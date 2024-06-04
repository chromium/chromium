// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_TRANSFER_BUFFER_H_
#define GPU_COMMAND_BUFFER_CLIENT_TRANSFER_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "gpu/command_buffer/client/ring_buffer.h"
#include "gpu/command_buffer/common/buffer.h"
#include "gpu/gpu_export.h"

namespace gpu {

class CommandBufferHelper;
template <typename>
class ScopedResultPtr;

// Interface for managing the transfer buffer.
class GPU_EXPORT TransferBufferInterface {
 public:
  TransferBufferInterface() = default;
  virtual ~TransferBufferInterface() = default;

  // Returns 128-bit GUID of the shared memory's region when the back end is
  // base::UnsafeSharedMemoryRegion. Otherwise, this returns an empty GUID.
  virtual base::UnguessableToken shared_memory_guid() const = 0;

  virtual bool Initialize(unsigned int buffer_size,
                          unsigned int result_size,
                          unsigned int min_buffer_size,
                          unsigned int max_buffer_size,
                          unsigned int alignment) = 0;

  virtual int GetShmId() = 0;

  virtual void Free() = 0;

  virtual bool HaveBuffer() const = 0;

  // Allocates up to size bytes.
  virtual void* AllocUpTo(unsigned int size, unsigned int* size_allocated) = 0;

  // Allocates size bytes.
  // Note: Alloc will fail if it can not return size bytes.
  virtual void* Alloc(unsigned int size) = 0;

  virtual RingBuffer::Offset GetOffset(void* pointer) const = 0;

  virtual void DiscardBlock(void* p) = 0;

  virtual void FreePendingToken(void* p, unsigned int token) = 0;

  virtual unsigned int GetSize() const = 0;

  virtual unsigned int GetFreeSize() const = 0;

  virtual unsigned int GetFragmentedFreeSize() const = 0;

  virtual void ShrinkLastBlock(unsigned int new_size) = 0;

  virtual unsigned int GetMaxSize() const = 0;

 protected:
  template <typename>
  friend class ScopedResultPtr;
  // Use ScopedResultPtr instead of calling these directly. The acquire/release
  // semantics allow TransferBuffer to detect if there is an outstanding result
  // pointer when the buffer is resized, which would otherwise cause a
  // use-after-free bug.
  virtual void* AcquireResultBuffer() = 0;
  virtual void ReleaseResultBuffer() = 0;
  virtual int GetResultOffset() = 0;
};

// Class that manages the transfer buffer.
class GPU_EXPORT TransferBuffer : public TransferBufferInterface {
 public:
  TransferBuffer(CommandBufferHelper* helper);
  ~TransferBuffer() override;

  // Overridden from TransferBufferInterface.
  base::UnguessableToken shared_memory_guid() const override;
  bool Initialize(unsigned int default_buffer_size,
                  unsigned int result_size,
                  unsigned int min_buffer_size,
                  unsigned int max_buffer_size,
                  unsigned int alignment) override;
  int GetShmId() override;
  void* AcquireResultBuffer() override;
  void ReleaseResultBuffer() override;
  int GetResultOffset() override;
  void Free() override;
  bool HaveBuffer() const override;
  void* AllocUpTo(unsigned int size, unsigned int* size_allocated) override;
  void* Alloc(unsigned int size) override;
  RingBuffer::Offset GetOffset(void* pointer) const override;
  void DiscardBlock(void* p) override;
  void FreePendingToken(void* p, unsigned int token) override;
  unsigned int GetSize() const override;
  unsigned int GetFreeSize() const override;
  unsigned int GetFragmentedFreeSize() const override;
  void ShrinkLastBlock(unsigned int new_size) override;
  unsigned int GetMaxSize() const override;

  // These are for testing.
  unsigned int GetCurrentMaxAllocationWithoutRealloc() const;

  // We will attempt to shrink the ring buffer once the number of bytes
  // allocated reaches this threshold times the high water mark.
  static const int kShrinkThreshold = 120;

 private:
  // Tries to reallocate the ring buffer if it's not large enough for size.
  void ReallocateRingBuffer(unsigned int size, bool shrink = false);

  void AllocateRingBuffer(unsigned int size);

  void ShrinkOrExpandRingBufferIfNecessary(unsigned int size);

  // Returns the number of bytes that are still in use in ring buffers that we
  // previously freed.
  unsigned int GetPreviousRingBufferUsedBytes();

  raw_ptr<CommandBufferHelper> helper_;
  std::unique_ptr<RingBuffer> ring_buffer_;
  base::circular_deque<std::unique_ptr<RingBuffer>> previous_ring_buffers_;

  // size reserved for results
  unsigned int result_size_;

  // default size. Size we want when starting or re-allocating
  unsigned int default_buffer_size_;

  // min size we'll consider successful
  unsigned int min_buffer_size_;

  // max size we'll let the buffer grow
  unsigned int max_buffer_size_;

  // Size of the currently allocated ring buffer.
  unsigned int last_allocated_size_ = 0;

  // The size to shrink the ring buffer to next time shrinking happens.
  unsigned int high_water_mark_ = 0;

  // alignment for allocations
  unsigned int alignment_;

  // Number of bytes since we last attempted to shrink the ring buffer.
  unsigned int bytes_since_last_shrink_ = 0;

  // the current buffer.
  scoped_refptr<gpu::Buffer> buffer_;

  // id of buffer. -1 = no buffer
  int32_t buffer_id_;

  // address of result area
  raw_ptr<void> result_buffer_;

  // offset to result area
  uint32_t result_shm_offset_;

  // false if we failed to allocate min_buffer_size
  bool usable_;

  // While a ScopedResultPtr exists, we can't resize the transfer buffer. Only
  // one ScopedResultPtr should exist at a time. This tracks whether one exists.
  bool outstanding_result_pointer_ = false;
};

// A class that will manage the lifetime of a transferbuffer allocation.
class GPU_EXPORT ScopedTransferBufferPtr {
 public:
  ScopedTransferBufferPtr(unsigned int size,
                          CommandBufferHelper* helper,
                          TransferBufferInterface* transfer_buffer)
      : buffer_(nullptr),
        size_(0),
        helper_(helper),
        transfer_buffer_(transfer_buffer) {
    Reset(size);
  }

  // Constructs an empty and invalid allocation that should be Reset() later.
  ScopedTransferBufferPtr(CommandBufferHelper* helper,
                          TransferBufferInterface* transfer_buffer)
      : buffer_(nullptr),
        size_(0),
        helper_(helper),
        transfer_buffer_(transfer_buffer) {}

  ScopedTransferBufferPtr(const ScopedTransferBufferPtr&) = delete;
  ScopedTransferBufferPtr& operator=(const ScopedTransferBufferPtr&) = delete;

  ~ScopedTransferBufferPtr() {
    Release();
  }

  ScopedTransferBufferPtr(ScopedTransferBufferPtr&& other);

  bool valid() const { return buffer_ != nullptr; }

  unsigned int size() const {
    return size_;
  }

  int shm_id() const {
    return transfer_buffer_->GetShmId();
  }

  RingBuffer::Offset offset() const {
    return transfer_buffer_->GetOffset(buffer_);
  }

  void* address() const {
    return buffer_;
  }

  // Returns true if |memory| lies inside this buffer.
  bool BelongsToBuffer(uint8_t* memory) const;

  void Release();

  void Discard();

  void Reset(unsigned int new_size);

  // Shrinks this transfer buffer to a given size.
  void Shrink(unsigned int new_size);

 private:
  raw_ptr<void> buffer_;
  unsigned int size_;

  // Found dangling on `linux-rel` in
  // `gpu_tests.trace_integration_test.TraceIntegrationTest.
  // WebGPUCachingTraceTest_ComputePipelineMainThread`.
  raw_ptr<CommandBufferHelper, DanglingUntriaged> helper_;

  raw_ptr<TransferBufferInterface, DanglingUntriaged> transfer_buffer_;
};

template <typename T>
class ScopedTransferBufferArray : public ScopedTransferBufferPtr {
 public:
  ScopedTransferBufferArray(
      unsigned int num_elements,
      CommandBufferHelper* helper, TransferBufferInterface* transfer_buffer)
      : ScopedTransferBufferPtr(
          num_elements * sizeof(T), helper, transfer_buffer) {
  }

  T* elements() {
    return static_cast<T*>(address());
  }

  unsigned int num_elements() const {
    return size() / sizeof(T);
  }
};

// ScopedResultPtr is a move-only smart pointer that calls AcquireResultBuffer
// and ReleaseResultBuffer for you.
template <typename T>
class ScopedResultPtr {
 public:
  explicit ScopedResultPtr(TransferBufferInterface* tb)
      : result_(static_cast<T*>(tb->AcquireResultBuffer())),
        transfer_buffer_(tb) {}

  ScopedResultPtr(const ScopedResultPtr&) = delete;
  ScopedResultPtr& operator=(const ScopedResultPtr&) = delete;

  ~ScopedResultPtr() {
    if (transfer_buffer_)
      transfer_buffer_->ReleaseResultBuffer();
  }

  int offset() const { return transfer_buffer_->GetResultOffset(); }

  // Make this a move-only class like unique_ptr.
  ScopedResultPtr(ScopedResultPtr<T>&& other) { *this = std::move(other); }
  ScopedResultPtr& operator=(ScopedResultPtr<T>&& other) {
    this->result_ = other.result_;
    this->transfer_buffer_ = other.transfer_buffer_;
    other.result_ = nullptr;
    other.transfer_buffer_ = nullptr;
    return *this;
  }

  // Dereferencing behaviors
  T& operator*() const { return *result_; }
  T* operator->() const { return result_; }
  explicit operator bool() { return result_; }

 private:
  raw_ptr<T> result_;
  raw_ptr<TransferBufferInterface> transfer_buffer_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_TRANSFER_BUFFER_H_
