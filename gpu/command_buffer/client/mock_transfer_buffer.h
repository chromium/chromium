// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_MOCK_TRANSFER_BUFFER_H_
#define GPU_COMMAND_BUFFER_CLIENT_MOCK_TRANSFER_BUFFER_H_

#include "base/macros.h"
#include "gpu/command_buffer/client/ring_buffer.h"
#include "gpu/command_buffer/client/transfer_buffer.h"

namespace gpu {

class CommandBuffer;

class MockTransferBuffer : public TransferBufferInterface {
 public:
  struct ExpectedMemoryInfo {
    uint32_t offset;
    int32_t id;
    uint8_t* ptr;
  };

  MockTransferBuffer(CommandBuffer* command_buffer,
                     unsigned int size,
                     unsigned int result_size,
                     unsigned int alignment,
                     bool initialize_fail);

  ~MockTransferBuffer() override;

  base::UnguessableToken shared_memory_guid() const override;
  bool Initialize(unsigned int starting_buffer_size,
                  unsigned int result_size,
                  unsigned int /* min_buffer_size */,
                  unsigned int /* max_buffer_size */,
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
  void FreePendingToken(void* p, unsigned int /* token */) override;
  unsigned int GetSize() const override;
  unsigned int GetFreeSize() const override;
  unsigned int GetFragmentedFreeSize() const override;
  void ShrinkLastBlock(unsigned int new_size) override;
  unsigned int GetMaxSize() const override;

  uint32_t MaxTransferBufferSize();
  unsigned int RoundToAlignment(unsigned int size);
  bool InSync();
  ExpectedMemoryInfo GetExpectedMemory(uint32_t size);
  ExpectedMemoryInfo GetExpectedResultMemory(uint32_t size);

 private:
  static const int kNumBuffers = 2;

  uint8_t* actual_buffer() const {
    return static_cast<uint8_t*>(buffers_[actual_buffer_index_]->memory());
  }

  uint8_t* expected_buffer() const {
    return static_cast<uint8_t*>(buffers_[expected_buffer_index_]->memory());
  }

  uint32_t AllocateExpectedTransferBuffer(uint32_t size);
  void* GetExpectedTransferAddressFromOffset(uint32_t offset, uint32_t size);
  int GetExpectedResultBufferId();
  uint32_t GetExpectedResultBufferOffset();
  int GetExpectedTransferBufferId();

  CommandBuffer* command_buffer_;
  uint32_t size_;
  uint32_t result_size_;
  uint32_t alignment_;
  int buffer_ids_[kNumBuffers];
  scoped_refptr<Buffer> buffers_[kNumBuffers];
  int actual_buffer_index_;
  int expected_buffer_index_;
  void* last_alloc_;
  uint32_t expected_offset_;
  uint32_t actual_offset_;
  bool initialize_fail_;
  bool outstanding_result_pointer_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockTransferBuffer);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_MOCK_TRANSFER_BUFFER_H_
