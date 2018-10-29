// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_channel_test_common.h"

#include "base/memory/shared_memory.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "url/gurl.h"

namespace gpu {

class TestGpuChannelManagerDelegate : public GpuChannelManagerDelegate {
 public:
  TestGpuChannelManagerDelegate() = default;
  ~TestGpuChannelManagerDelegate() override = default;

  // GpuChannelManagerDelegate implementation:
  void SetActiveURL(const GURL& url) override {}
  void DidCreateContextSuccessfully() override {}
  void DidCreateOffscreenContext(const GURL& active_url) override {}
  void DidDestroyChannel(int client_id) override {}
  void DidDestroyOffscreenContext(const GURL& active_url) override {}
  void DidLoseContext(bool offscreen,
                      error::ContextLostReason reason,
                      const GURL& active_url) override {}
  void StoreShaderToDisk(int32_t client_id,
                         const std::string& key,
                         const std::string& shader) override {}
  void ExitProcess() override {}
#if defined(OS_WIN)
  void SendCreatedChildWindow(SurfaceHandle parent_window,
                              SurfaceHandle child_window) override {}
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(TestGpuChannelManagerDelegate);
};

GpuChannelTestCommon::GpuChannelTestCommon()
    : task_runner_(new base::TestSimpleTaskRunner),
      io_task_runner_(new base::TestSimpleTaskRunner),
      sync_point_manager_(new SyncPointManager()),
      scheduler_(new Scheduler(task_runner_, sync_point_manager_.get())),
      channel_manager_delegate_(new TestGpuChannelManagerDelegate()) {
  // We need GL bindings to actually initialize command buffers.
  gl::GLSurfaceTestSupport::InitializeOneOffWithStubBindings();

  channel_manager_.reset(new GpuChannelManager(
      GpuPreferences(), channel_manager_delegate_.get(), nullptr, /* watchdog */
      task_runner_.get(), io_task_runner_.get(), scheduler_.get(),
      sync_point_manager_.get(), nullptr, /* gpu_memory_buffer_factory */
      GpuFeatureInfo(), GpuProcessActivityFlags(),
      gl::init::CreateOffscreenGLSurface(gfx::Size())));
}

GpuChannelTestCommon::~GpuChannelTestCommon() {
  // Command buffers can post tasks and run GL in destruction so do this first.
  channel_manager_ = nullptr;

  // Clear pending tasks to avoid refptr cycles that get flagged by ASAN.
  task_runner_->ClearPendingTasks();
  io_task_runner_->ClearPendingTasks();

  gl::init::ShutdownGL(false);
}

GpuChannel* GpuChannelTestCommon::CreateChannel(int32_t client_id,
                                                bool is_gpu_host) {
  uint64_t kClientTracingId = 1;
  GpuChannel* channel = channel_manager()->EstablishChannel(
      client_id, kClientTracingId, is_gpu_host, true);
  channel->InitForTesting(&sink_);
  base::ProcessId kProcessId = 1;
  channel->OnChannelConnected(kProcessId);
  return channel;
}

void GpuChannelTestCommon::HandleMessage(GpuChannel* channel,
                                         IPC::Message* msg) {
  // Some IPCs (such as GpuCommandBufferMsg_Initialize) will generate more
  // delayed responses, drop those if they exist.
  sink_.ClearMessages();

  // Needed to appease DCHECKs.
  msg->set_unblock(false);

  // Message filter gets message first on IO thread.
  channel->HandleMessageForTesting(*msg);

  // Run the HandleMessage task posted to the main thread.
  task_runner()->RunPendingTasks();

  // Replies are sent to the sink.
  if (msg->is_sync()) {
    const IPC::Message* reply_msg = sink_.GetMessageAt(0);
    ASSERT_TRUE(reply_msg);
    EXPECT_TRUE(!reply_msg->is_reply_error());

    EXPECT_TRUE(IPC::SyncMessage::IsMessageReplyTo(
        *reply_msg, IPC::SyncMessage::GetMessageId(*msg)));

    IPC::MessageReplyDeserializer* deserializer =
        static_cast<IPC::SyncMessage*>(msg)->GetReplyDeserializer();
    ASSERT_TRUE(deserializer);
    deserializer->SerializeOutputParameters(*reply_msg);

    delete deserializer;
  }

  sink_.ClearMessages();

  delete msg;
}

base::UnsafeSharedMemoryRegion GpuChannelTestCommon::GetSharedMemoryRegion() {
  return base::UnsafeSharedMemoryRegion::Create(
      sizeof(CommandBufferSharedState));
}

}  // namespace gpu
