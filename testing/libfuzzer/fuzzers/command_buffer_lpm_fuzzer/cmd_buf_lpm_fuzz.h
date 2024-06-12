// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_LIBFUZZER_FUZZERS_COMMAND_BUFFER_LPM_FUZZER_CMD_BUF_LPM_FUZZ_H_
#define TESTING_LIBFUZZER_FUZZERS_COMMAND_BUFFER_LPM_FUZZER_CMD_BUF_LPM_FUZZ_H_

#include <dawn/native/DawnNative.h>
#include <dawn/webgpu_cpp.h>
#include <dawn/wire/WireServer.h>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_simple_task_runner.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "gpu/command_buffer/client/webgpu_cmd_helper.h"
#include "gpu/command_buffer/client/webgpu_implementation.h"
#include "gpu/command_buffer/service/webgpu_decoder.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/webgpu_in_process_context.h"
#include "ui/gl/buildflags.h"

#include "testing/libfuzzer/fuzzers/command_buffer_lpm_fuzzer/cmd_buf_lpm_fuzz.pb.h"
#include "testing/libfuzzer/fuzzers/command_buffer_lpm_fuzzer/webgpu_support.h"

namespace gpu::cmdbuf::fuzzing {

const uint32_t kCommandBufferSize = 16384;
const uint32_t kTransferBufferSize = 16384;
const uint32_t kSmallTransferBufferSize = 16;
const uint32_t kTinyTransferBufferSize = 3;

const base::TimeDelta kTinyTimeout = base::Milliseconds(100);

struct Config {};

class CmdBufFuzz : public base::TestSuite {
 public:
  CmdBufFuzz();
  ~CmdBufFuzz() override;
  void CommandLineInit();
  void GfxInit();

  void RuntimeInit();
  bool Reset();

  // CommandBuffer ops
  void RunCommandBuffer(cmdbuf::fuzzing::CmdBufSession);
  bool CmdBufReset();
  gpu::SyncToken SyncTokenFromProto(cmdbuf::fuzzing::SyncToken token_proto);
  void SignalSyncTokenCallback();

  // WebGPU ops
  void WebGPURequestAdapter();
  void WebGPURequestDevice();
  void WebGPUDestroyDevice();
  void WebGPUCreateBuffer();
  void WebGPUDestroyBuffer();
  void WebGPUReset();

  inline gpu::webgpu::WebGPUImplementation* webgpu() {
    return webgpu_context_->GetImplementation();
  }

  webgpu::WebGPUDecoder* GetDecoder() const {
    return webgpu_context_->GetCommandBufferForTest()
        ->GetWebGPUDecoderForTest();
  }

  void RunPendingTasks() {
    webgpu_context_->GetTaskRunner()->RunPendingTasks();
    gpu_service_holder_->ScheduleGpuMainTask(base::BindOnce(
        [](webgpu::WebGPUDecoder* decoder) {
          if (decoder->HasPollingWork()) {
            LOG(INFO) << "GPU has waiting work";
            decoder->PerformPollingWork();
          }
        },
        GetDecoder()));
  }

  void PollUntilIdle() {
    webgpu()->FlushCommands();
    base::WaitableEvent wait;
    gpu_service_holder_->ScheduleGpuMainTask(base::BindOnce(
        [](viz::TestGpuServiceHolder* service, base::WaitableEvent* wait,
           webgpu::WebGPUDecoder* decoder) {
          service->gpu_main_thread_task_runner()->RunsTasksInCurrentSequence();
          while (decoder->HasPollingWork()) {
            base::PlatformThread::Sleep(kTinyTimeout);
            decoder->PerformPollingWork();
          }
          wait->Signal();
        },
        gpu_service_holder_.get(), &wait, GetDecoder()));
    wait.Wait();
    webgpu_context_->GetTaskRunner()->RunPendingTasks();
  }

  void PostTaskToGpuThread(base::OnceClosure callback) {
    gpu_service_holder_->ScheduleGpuMainTask(std::move(callback));
  }

  void WaitForCompletion(wgpu::Instance instance, wgpu::Device device) {
    // Wait for any work submitted to the queue to be finished. The guarantees
    // of Dawn are that all previous operations will have been completed and
    // more importantly the callbacks will have been called.
    wgpu::Queue queue = device.GetQueue();
    wgpu::FutureWaitInfo wait_info = {queue.OnSubmittedWorkDone(
        wgpu::CallbackMode::WaitAnyOnly, [](wgpu::QueueWorkDoneStatus) {})};

    while (!wait_info.completed) {
      instance.WaitAny(1, &wait_info, 0u);
      webgpu()->FlushCommands();
      RunPendingTasks();
      base::PlatformThread::Sleep(kTinyTimeout);
    }
  }

 private:
  GpuPreferences gpu_preferences_;
  std::unique_ptr<viz::TestGpuServiceHolder> gpu_service_holder_;
  std::unique_ptr<WebGPUInProcessContext> webgpu_context_;
  raw_ptr<gl::GLDisplay> gl_display_;
  scoped_refptr<gl::GLSurface> surface_;
  // std::unique_ptr<CommandBufferDirect> command_buffer_;
  raw_ptr<InProcessCommandBuffer> command_buffer_ = nullptr;
  std::unique_ptr<CommandBufferService> command_buffer_service_;
  raw_ptr<CommandBufferHelper> cmd_helper_ = nullptr;
  std::unique_ptr<webgpu::WebGPUCmdHelper> webgpu_cmd_helper_ = nullptr;
  // std::unique_ptr<gpu::CommandBufferProxyImpl> command_buffer_proxy_impl_;
  //  scoped_refptr<viz::ContextProviderCommandBuffer> provider_;
  wgpu::Instance webgpu_instance_;
  raw_ptr<webgpu::WebGPUDecoder> decoder_;
  wgpu::Adapter webgpu_adapter_;
  std::unique_ptr<gpu::TransferBuffer> transfer_buffer_;
  raw_ptr<gpu::webgpu::WebGPUImplementation> webgpu_impl_;
  std::unique_ptr<dawn::wire::WireServer> wire_server_;
  scoped_refptr<webgpu::APIChannel> wire_channel_;
  std::unique_ptr<dawn::native::Instance> dawn_instance_;
  std::unique_ptr<dawn::wire::WireServerDescriptor> wire_descriptor_;
  scoped_refptr<Buffer> buffer_;
  int32_t command_buffer_id_ = -1;
  std::unique_ptr<SharedMemoryLimits> shared_memory_limits_;
  raw_ptr<CommandBufferTaskExecutor> task_executor_;
  // scoped_refptr<SharedContextState> shared_context_state_;
  raw_ptr<webgpu::DawnWireServices> dawn_wire_services_;
  raw_ptr<webgpu::DawnClientSerializer> dawn_wire_serializer_;
  wgpu::Device webgpu_device_ = {};
  std::vector<wgpu::Buffer> wgpu_buffers_ = {};

  // Actions
  int action_index_ = 0;
  base::Lock cmd_buf_lock_;

  // TODO(bookholt): enable to support GL fuzzing
  // gpu::gles2::GLES2Interface* gl_interface_;
  // TODO(bookholt): this looks useful
  // raw_ptr<gpu::ContextSupport, DanglingUntriaged> context_support_;
  // TODO(bookohlt)
  // gpu::GpuChannelEstablishFactory* factory_;
  // TODO(bookholt)
  // scoped_refptr<gpu::GpuChannelHost> gpu_channel_host_;
};
}  // namespace gpu::cmdbuf::fuzzing

#endif  // TESTING_LIBFUZZER_FUZZERS_COMMAND_BUFFER_LPM_FUZZER_CMD_BUF_LPM_FUZZ_H_
