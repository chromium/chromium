// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_MAPPED_MEMORY_H_
#define GPU_COMMAND_BUFFER_CLIENT_MAPPED_MEMORY_H_

#include <stddef.h>
#include <stdint.h>

#include <bit>
#include <memory>
#include <type_traits>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/numerics/checked_math.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/command_buffer/client/fenced_allocator.h"
#include "gpu/command_buffer/client/gpu_command_buffer_client_export.h"
#include "gpu/command_buffer/common/buffer.h"
#include "gpu/command_buffer/common/constants.h"

namespace gpu {

class CommandBufferHelper;
class ScopedDedicatedChunk;

// Manages a shared memory segment.
class GPU_COMMAND_BUFFER_CLIENT_EXPORT MemoryChunk {
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
  //   the span to the allocated memory block, or an empty span if out of
  //   memory.
  base::span<uint8_t> Alloc(uint32_t size) { return allocator_.Alloc(size); }

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
    return pointer >= shm_->memory() && pointer <= &shm_->as_byte_span().back();
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
class GPU_COMMAND_BUFFER_CLIENT_EXPORT MappedMemoryManager {
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

  // Caps the total size of dedicated chunks. AllocDedicatedChunk() fails once
  // the cap is reached so callers can fall back to pooled chunks.
  void set_max_dedicated_bytes_for_testing(size_t max_dedicated_bytes) {
    max_dedicated_bytes_ = max_dedicated_bytes;
  }

  // Allocates a block of memory
  // Parameters:
  //   size: size of memory to allocate.
  //   shm_id: pointer to variable to receive the shared memory id.
  //   shm_offset: pointer to variable to receive the shared memory offset.
  //   option: defaults to kLoseContextOnOOM, but may be kReturnNullOnOOM.
  //           Passing kReturnNullOnOOM will gracefully fail and return empty
  //           span on OOM instead of losing the context. Callers should be
  //           careful to check error conditions.
  // Returns:
  //   span of allocated block of memory. Empty span if failure.
  base::span<uint8_t> Alloc(
      uint32_t size,
      int32_t* shm_id,
      uint32_t* shm_offset,
      TransferBufferAllocationOption option =
          TransferBufferAllocationOption::kLoseContextOnOOM);

  // Allocates a block of typed memory, using reinterpret_span to convert
  // the raw byte buffer to the desired type.
  //
  // Parameters:
  //   count: the number of T-typed elements to allocate.
  //   shm_id: pointer to variable to receive the shared memory id.
  //   shm_offset: pointer to variable to receive the shared memory offset.
  //   option: defaults to kLoseContextOnOOM, but may be kReturnNullOnOOM.
  //           Passing kReturnNullOnOOM will gracefully fail and return empty
  //           span on OOM instead of losing the context. Callers should be
  //           careful to check error conditions.
  //
  // Returns:
  //   span of allocated block of typed memory. Empty span if failure.
  template <typename T>
  base::span<T> AllocTyped(
      uint32_t count,
      int32_t* shm_id,
      uint32_t* shm_offset,
      TransferBufferAllocationOption option =
          TransferBufferAllocationOption::kLoseContextOnOOM) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "AllocTyped only supports trivially copyable types");
    uint32_t byte_size = 0;
    if (!base::CheckMul(count, sizeof(T)).AssignIfValid(&byte_size)) {
      return {};
    }
    base::span<uint8_t> buffer = Alloc(byte_size, shm_id, shm_offset, option);
    if (buffer.empty()) {
      return {};
    }
    return base::subtle::reinterpret_span<T>(buffer);
  }

  // Allocates a whole new chunk dedicated to a single allocation, guaranteeing
  // that the shared memory offset is always 0.
  //
  // Parameters:
  //   size: size of memory to allocate.
  // Returns:
  //   an RAII handle to the allocated chunk that releases it (and destroys
  //   its transfer buffer) on destruction. Invalid handle on OOM; callers
  //   should be careful to check error conditions.
  ScopedDedicatedChunk AllocDedicatedChunk(uint32_t size);

  // Frees a block of memory.
  //
  // Parameters:
  //   pointer: the pointer to the memory block to free.
  void Free(void* pointer);

  // Removes a dedicated chunk and destroys its transfer buffer. The caller
  // must have already written all commands referencing `shm_id` into the
  // command buffer.
  //
  // Normally called internally by ScopedDedicatedChunk's destructor; exposed
  // for callers that need to release a chunk early.
  void RemoveDedicatedChunk(int32_t shm_id);

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
    return chunks_.size() + dedicated_chunks_.size();
  }

  size_t bytes_in_use() const {
    size_t bytes_in_use = 0;
    for (size_t ii = 0; ii < chunks_.size(); ++ii) {
      bytes_in_use += chunks_[ii]->bytes_in_use();
    }
    for (const auto& entry : dedicated_chunks_) {
      bytes_in_use += entry.second->size();
    }
    return bytes_in_use;
  }

  // Used for testing
  size_t allocated_memory() const {
    return allocated_memory_;
  }

  size_t dedicated_memory_for_testing() const {
    return dedicated_memory_for_testing_;
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
  // Chunks that can be sub-allocated from and reused; never dedicated.
  MemoryChunkVector chunks_;
  // Chunks allocated via AllocDedicatedChunk(), keyed by shm_id.
  base::flat_map<int32_t, scoped_refptr<gpu::Buffer>> dedicated_chunks_;
  size_t allocated_memory_;
  size_t max_free_bytes_;
  size_t max_allocated_bytes_;
  size_t dedicated_memory_for_testing_;
  size_t max_dedicated_bytes_;
  // A process-unique ID used for disambiguating memory dumps from different
  // mapped memory manager.
  int tracing_id_;
};

// RAII handle for a chunk allocated via
// MappedMemoryManager::AllocDedicatedChunk(). Releases the chunk (and
// destroys its transfer buffer) when destroyed, moved-from, or Reset().
//
// Must not outlive the MappedMemoryManager it was allocated from: it holds a
// non-owning pointer back to the manager and calls RemoveDedicatedChunk() on
// it, so destroying the manager first would leave that pointer dangling.
// (MappedMemoryManager's destructor DCHECKs that no dedicated chunk is still
// outstanding.)
class GPU_COMMAND_BUFFER_CLIENT_EXPORT ScopedDedicatedChunk {
 public:
  ScopedDedicatedChunk();
  ScopedDedicatedChunk(base::span<uint8_t> span,
                       int32_t shm_id,
                       MappedMemoryManager* manager);

  ScopedDedicatedChunk(const ScopedDedicatedChunk&) = delete;
  ScopedDedicatedChunk& operator=(const ScopedDedicatedChunk&) = delete;

  ScopedDedicatedChunk(ScopedDedicatedChunk&& other);
  ScopedDedicatedChunk& operator=(ScopedDedicatedChunk&& other);

  ~ScopedDedicatedChunk();

  bool valid() const { return manager_ != nullptr; }
  int32_t shm_id() const { return shm_id_; }
  base::span<uint8_t> span() const { return span_; }

  // Releases the chunk early. Safe to call on an already-empty instance.
  void Reset();

 private:
  base::raw_span<uint8_t> span_;
  int32_t shm_id_ = -1;
  raw_ptr<MappedMemoryManager> manager_ = nullptr;
};

// A class that will manage the lifetime of a mapped memory allocation
class GPU_COMMAND_BUFFER_CLIENT_EXPORT ScopedMappedMemoryPtr {
 public:
  ScopedMappedMemoryPtr(uint32_t size,
                        CommandBufferHelper* helper,
                        MappedMemoryManager* mapped_memory_manager)
      : helper_(helper), mapped_memory_manager_(mapped_memory_manager) {
    Reset(size);
  }

  ScopedMappedMemoryPtr(const ScopedMappedMemoryPtr&) = delete;
  ScopedMappedMemoryPtr& operator=(const ScopedMappedMemoryPtr&) = delete;

  ~ScopedMappedMemoryPtr() {
    Release();
  }

  bool valid() const { return buffer_.data() != nullptr; }

  void SetFlushAfterRelease(bool flush_after_release) {
    flush_after_release_ = flush_after_release;
  }

  uint32_t size() const { return buffer_.size(); }

  int32_t shm_id() const {
    return shm_id_;
  }

  uint32_t offset() const {
    return shm_offset_;
  }

  void* address() const { return buffer_.data(); }

  base::span<uint8_t> as_byte_span() { return buffer_; }

  base::span<const uint8_t> as_byte_span() const { return buffer_; }

  void Release();

  void Reset(uint32_t new_size);

 private:
  base::raw_span<uint8_t> buffer_;
  int32_t shm_id_ = 0;
  uint32_t shm_offset_ = 0;
  bool flush_after_release_ = false;
  raw_ptr<CommandBufferHelper> helper_;
  raw_ptr<MappedMemoryManager> mapped_memory_manager_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_MAPPED_MEMORY_H_
