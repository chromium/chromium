// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef GPU_COMMAND_BUFFER_CLIENT_MAPPED_MEMORY_H_
#define GPU_COMMAND_BUFFER_CLIENT_MAPPED_MEMORY_H_

#include <stddef.h>
#include <stdint.h>

#include <bit>
#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/command_buffer/client/fenced_allocator.h"
#include "gpu/command_buffer/common/buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/gpu_export.h"

namespace gpu {

class CommandBufferHelper;

// Manages a shared memory segment.
class GPU_EXPORT MemoryChunk {
 public:
  MemoryChunk(int32_t shm_id,
              scoped_refptr<gpu::Buffer> shm,
              CommandBufferHelper* helper);

  MemoryChunk(const MemoryChunk&) = delete;
  MemoryChunk& operator=(const MemoryChunk&) = delete;

  ~MemoryChunk();

  // Gets the size of the largest free block that is available without waiting.
  uint32_t GetLargestFreeSizeWithoutWaiting() {
    return allocator_.GetLargestFreeSize();
  }

  // Gets the size of the largest free block that can be allocated if the
  // caller can wait.
  uint32_t GetLargestFreeSizeWithWaiting() {
    return allocator_.GetLargestFreeOrPendingSize();
  }

  // Gets the size of the chunk.
  uint32_t GetSize() const { return shm_->size(); }

  // The shared memory id for this chunk.
  int32_t shm_id() const { return shm_id_; }

  gpu::Buffer* shared_memory() const { return shm_.get(); }

  // Allocates a block of memory. If the buffer is out of directly available
  // memory, this function may wait until memory that was freed "pending a
  // token" can be re-used.
  //
  // Parameters:
  //   size: the size of the memory block to allocate.
  //
  // Returns:
  //   the pointer to the allocated memory block, or nullptr if out of
  //   memory.
  void* Alloc(uint32_t size) { return allocator_.Alloc(size); }

  // Gets the offset to a memory block given the base memory and the address.
  // It translates nullptr to FencedAllocator::kInvalidOffset.
  uint32_t GetOffset(void* pointer) { return allocator_.GetOffset(pointer); }

  // Frees a block of memory.
  //
  // Parameters:
  //   pointer: the pointer to the memory block to free.
  void Free(void* pointer) {
    allocator_.Free(pointer);
  }

  // Frees a block of memory, pending the passage of a token. That memory won't
  // be re-allocated until the token has passed through the command stream.
  //
  // Parameters:
  //   pointer: the pointer to the memory block to free.
  //   token: the token value to wait for before re-using the memory.
  void FreePendingToken(void* pointer, uint32_t token) {
    allocator_.FreePendingToken(pointer, token);
  }

  // Frees any blocks whose tokens have passed.
  void FreeUnused() {
    allocator_.FreeUnused();
  }

  // Gets the free size of the chunk.
  uint32_t GetFreeSize() { return allocator_.GetFreeSize(); }

  // Returns true if pointer is in the range of this block.
  bool IsInChunk(void* pointer) const {
    return pointer >= shm_->memory() &&
           pointer < static_cast<const int8_t*>(shm_->memory()) + shm_->size();
  }

  // Returns true of any memory in this chunk is in use or free pending token.
  bool InUseOrFreePending() { return allocator_.InUseOrFreePending(); }

  uint32_t bytes_in_use() const { return allocator_.bytes_in_use(); }

  FencedAllocator::State GetPointerStatusForTest(void* pointer,
                                                 int32_t* token_if_pending) {
    return allocator_.GetPointerStatusForTest(pointer, token_if_pending);
  }

 private:
  int32_t shm_id_;
  scoped_refptr<gpu::Buffer> shm_;
  FencedAllocatorWrapper allocator_;
};

// Manages MemoryChunks.
class GPU_EXPORT MappedMemoryManager {
 public:
  enum MemoryLimit {
    kNoLimit = 0,
  };

  // |unused_memory_reclaim_limit|: When exceeded this causes pending memory
  // to be reclaimed before allocating more memory.
  MappedMemoryManager(CommandBufferHelper* helper,
                      size_t unused_memory_reclaim_limit);

  MappedMemoryManager(const MappedMemoryManager&) = delete;
  MappedMemoryManager& operator=(const MappedMemoryManager&) = delete;

  ~MappedMemoryManager();

  uint32_t chunk_size_multiple() const { return chunk_size_multiple_; }

  void set_chunk_size_multiple(uint32_t multiple) {
    DCHECK(std::has_single_bit(multiple));
    DCHECK_GE(multiple, FencedAllocator::kAllocAlignment);
    chunk_size_multiple_ = multiple;
  }

  size_t max_allocated_bytes() const {
    return max_allocated_bytes_;
  }

  void set_max_allocated_bytes(size_t max_allocated_bytes) {
    max_allocated_bytes_ = max_allocated_bytes;
  }

  // Allocates a block of memory
  // Parameters:
  //   size: size of memory to allocate.
  //   shm_id: pointer to variable to receive the shared memory id.
  //   shm_offset: pointer to variable to receive the shared memory offset.
  //   option: defaults to kLoseContextOnOOM, but may be kReturnNullOnOOM.
  //           Passing kReturnNullOnOOM will gracefully fail and return nullptr
  //           on OOM instead of losing the context. Callers should be careful
  //           to check error conditions.
  // Returns:
  //   pointer to allocated block of memory. nullptr if failure.
  void* Alloc(uint32_t size,
              int32_t* shm_id,
              uint32_t* shm_offset,
              TransferBufferAllocationOption option =
                  TransferBufferAllocationOption::kLoseContextOnOOM);

  // Frees a block of memory.
  //
  // Parameters:
  //   pointer: the pointer to the memory block to free.
  void Free(void* pointer);

  // Frees a block of memory, pending the passage of a token. That memory won't
  // be re-allocated until the token has passed through the command stream.
  //
  // Parameters:
  //   pointer: the pointer to the memory block to free.
  //   token: the token value to wait for before re-using the memory.
  void FreePendingToken(void* pointer, int32_t token);

  // Free Any Shared memory that is not in use.
  void FreeUnused();

  // Dump memory usage - called from GLES2Implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd);

  // Used for testing
  size_t num_chunks() const {
    return chunks_.size();
  }

  size_t bytes_in_use() const {
    size_t bytes_in_use = 0;
    for (size_t ii = 0; ii < chunks_.size(); ++ii) {
      bytes_in_use += chunks_[ii]->bytes_in_use();
    }
    return bytes_in_use;
  }

  // Used for testing
  size_t allocated_memory() const {
    return allocated_memory_;
  }

  // Gets the status of a previous allocation, as well as the corresponding
  // token if FREE_PENDING_TOKEN (and token_if_pending is not null).
  FencedAllocator::State GetPointerStatusForTest(void* pointer,
                                                 int32_t* token_if_pending);

 private:
  typedef std::vector<std::unique_ptr<MemoryChunk>> MemoryChunkVector;

  // size a chunk is rounded up to.
  uint32_t chunk_size_multiple_;
  raw_ptr<CommandBufferHelper> helper_;
  MemoryChunkVector chunks_;
  size_t allocated_memory_;
  size_t max_free_bytes_;
  size_t max_allocated_bytes_;
  // A process-unique ID used for disambiguating memory dumps from different
  // mapped memory manager.
  int tracing_id_;
};

// A class that will manage the lifetime of a mapped memory allocation
class GPU_EXPORT ScopedMappedMemoryPtr {
 public:
  ScopedMappedMemoryPtr(uint32_t size,
                        CommandBufferHelper* helper,
                        MappedMemoryManager* mapped_memory_manager)
      : buffer_(nullptr),
        size_(0),
        shm_id_(0),
        shm_offset_(0),
        flush_after_release_(false),
        helper_(helper),
        mapped_memory_manager_(mapped_memory_manager) {
    Reset(size);
  }

  ScopedMappedMemoryPtr(const ScopedMappedMemoryPtr&) = delete;
  ScopedMappedMemoryPtr& operator=(const ScopedMappedMemoryPtr&) = delete;

  ~ScopedMappedMemoryPtr() {
    Release();
  }

  bool valid() const { return buffer_ != nullptr; }

  void SetFlushAfterRelease(bool flush_after_release) {
    flush_after_release_ = flush_after_release;
  }

  uint32_t size() const {
    return size_;
  }

  int32_t shm_id() const {
    return shm_id_;
  }

  uint32_t offset() const {
    return shm_offset_;
  }

  void* address() const {
    return buffer_;
  }

  void Release();

  void Reset(uint32_t new_size);

 private:
  raw_ptr<void> buffer_;
  uint32_t size_;
  int32_t shm_id_;
  uint32_t shm_offset_;
  bool flush_after_release_;
  raw_ptr<CommandBufferHelper> helper_;
  raw_ptr<MappedMemoryManager> mapped_memory_manager_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_MAPPED_MEMORY_H_
