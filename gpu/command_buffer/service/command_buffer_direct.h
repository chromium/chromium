// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_DIRECT_H_
#define GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_DIRECT_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/gpu_export.h"

namespace gpu {

class AsyncAPIInterface;

class GPU_EXPORT CommandBufferDirect : public CommandBuffer,
                                       public CommandBufferServiceClient,
                                       public DecoderClient {
 public:
  CommandBufferDirect();
  ~CommandBufferDirect() override;

  void set_handler(AsyncAPIInterface* handler) { handler_ = handler; }
  CommandBufferService* service() { return &service_; }

  // CommandBuffer implementation:
  CommandBuffer::State GetLastState() override;
  void Flush(int32_t put_offset) override;
  void OrderingBarrier(int32_t put_offset) override;
  CommandBuffer::State WaitForTokenInRange(int32_t start, int32_t end) override;
  CommandBuffer::State WaitForGetOffsetInRange(uint32_t set_get_buffer_count,
                                               int32_t start,
                                               int32_t end) override;
  void SetGetBuffer(int32_t transfer_buffer_id) override;
  scoped_refptr<Buffer> CreateTransferBuffer(
      uint32_t size,
      int32_t* id,
      uint32_t alignment = 0,
      TransferBufferAllocationOption option =
          TransferBufferAllocationOption::kLoseContextOnOOM) override;
  void DestroyTransferBuffer(int32_t id) override;
  void ForceLostContext(error::ContextLostReason reason) override;

  // CommandBufferServiceClient implementation:
  CommandBatchProcessedResult OnCommandBatchProcessed() override;
  void OnParseError() override;

  // DecoderClient implementation
  void OnConsoleMessage(int32_t id, const std::string& message) override;
  void CacheBlob(gpu::GpuDiskCacheType type,
                 const std::string& key,
                 const std::string& shader) override;
  void OnFenceSyncRelease(uint64_t release) override;
  void OnDescheduleUntilFinished() override;
  void OnRescheduleAfterFinished() override;
  void OnSwapBuffers(uint64_t swap_id, uint32_t flags) override;
  void ScheduleGrContextCleanup() override {}
  void HandleReturnData(base::span<const uint8_t> data) override;
  bool ShouldYield() override;

  scoped_refptr<Buffer> CreateTransferBufferWithId(uint32_t size, int32_t id);

  void SetGetOffsetForTest(int32_t get_offset) {
    service_.SetGetOffsetForTest(get_offset);
  }

 private:
  CommandBufferService service_;
  raw_ptr<AsyncAPIInterface, DanglingUntriaged> handler_ = nullptr;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_DIRECT_H_
