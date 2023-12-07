/*
 * © Copyright 2019 Alyssa Rosenzweig
 * © Copyright 2019 Collabora, Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef __PAN_BO_H__
#define __PAN_BO_H__

#include "util/list.h"
#include "util/u_dynarray.h"
#include "panfrost-job.h"
#include <time.h>

/* Flags for allocated memory */

/* This memory region is executable */
#define PAN_BO_EXECUTE            (1 << 0)

/* This memory region should be lazily allocated and grow-on-page-fault. Must
 * be used in conjunction with INVISIBLE */
#define PAN_BO_GROWABLE           (1 << 1)

/* This memory region should not be mapped to the CPU */
#define PAN_BO_INVISIBLE          (1 << 2)

/* This region may not be used immediately and will not mmap on allocate
 * (semantically distinct from INVISIBLE, which cannot never be mmaped) */
#define PAN_BO_DELAY_MMAP         (1 << 3)

/* BO is shared across processes (imported or exported) and therefore cannot be
 * cached locally */
#define PAN_BO_SHARED             (1 << 4)

/* Use event memory, required for CSF events to be signaled to the kernel */
#define PAN_BO_EVENT              (1 << 5)

/* Use the caching policy for resource BOs */
#define PAN_BO_CACHEABLE          (1 << 6)

/* GPU access flags */

/* BO is either shared (can be accessed by more than one GPU batch) or private
 * (reserved by a specific GPU job). */
#define PAN_BO_ACCESS_PRIVATE         (0 << 0)
#define PAN_BO_ACCESS_SHARED          (1 << 0)

/* BO is being read/written by the GPU */
#define PAN_BO_ACCESS_READ            (1 << 1)
#define PAN_BO_ACCESS_WRITE           (1 << 2)
#define PAN_BO_ACCESS_RW              (PAN_BO_ACCESS_READ | PAN_BO_ACCESS_WRITE)

/* BO is accessed by the vertex/tiler job. */
#define PAN_BO_ACCESS_VERTEX_TILER    (1 << 3)

/* BO is accessed by the fragment job. */
#define PAN_BO_ACCESS_FRAGMENT        (1 << 4)

typedef uint8_t pan_bo_access;

struct panfrost_device;

struct panfrost_ptr {
        /* CPU address */
        void *cpu;

        /* GPU address */
        mali_ptr gpu;
};

struct panfrost_usage {
        uint32_t queue;
        bool write;
        uint64_t seqnum;
};

struct panfrost_bo {
        /* Must be first for casting */
        struct list_head bucket_link;

        /* Used to link the BO to the BO cache LRU list. */
        struct list_head lru_link;

        /* Store the time this BO was use last, so the BO cache logic can evict
         * stale BOs.
         */
        time_t last_used;

        /* Atomic reference count */
        int32_t refcnt;

        /* Reference count for GPU jobs */
        int32_t gpu_refcnt;

        struct panfrost_device *dev;

        /* Mapping for the entire object (all levels) */
        struct panfrost_ptr ptr;

        struct util_dynarray usage;

        /* Size of all entire trees */
        size_t size;

        int gem_handle;

        uint32_t flags;

        /* Combination of PAN_BO_ACCESS_{READ,WRITE} flags encoding pending
         * GPU accesses to this BO. Useful to avoid calling the WAIT_BO ioctl
         * when the BO is idle.
         */
        uint32_t gpu_access;

        /* Human readable description of the BO for debugging. */
        const char *label;

        /* Sometimes we don't access the BO through kbase's mapping of the
         * memory, in that case we need to save the pointer to pass to
         * munmap to avoid leaking memory. */
        void *munmap_ptr;

        /* For 32-bit applications we may not even be able to that, because
         * the VA may be too high for kbase to map to an equivalent CPU
         * address, in which case we must use the memory free icotl. */
        bool free_ioctl;

        /* Is the BO cached CPU-side? */
        bool cached;

        /* File descriptor for the dma-buf */
        int dmabuf_fd;
};

bool
panfrost_bo_wait(struct panfrost_bo *bo, int64_t timeout_ns, bool wait_readers);
void
panfrost_bo_mem_invalidate(struct panfrost_bo *bo, size_t offset, size_t length);
void
panfrost_bo_mem_clean(struct panfrost_bo *bo, size_t offset, size_t length);
void
panfrost_bo_reference(struct panfrost_bo *bo);
void
panfrost_bo_unreference(struct panfrost_bo *bo);
struct panfrost_bo *
panfrost_bo_create(struct panfrost_device *dev, size_t size,
                   uint32_t flags, const char *label);
void
panfrost_bo_mmap(struct panfrost_bo *bo);
struct panfrost_bo *
panfrost_bo_import(struct panfrost_device *dev, int fd);
int
panfrost_bo_export(struct panfrost_bo *bo);
void
panfrost_bo_cache_evict_all(struct panfrost_device *dev);

#endif /* __PAN_BO_H__ */