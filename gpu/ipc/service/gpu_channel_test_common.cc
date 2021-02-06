// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_channel_test_common.h"

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "components/viz/common/features.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/ipc/service/context_url.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "url/gurl.h"

namespace gpu {

class TestGpuChannelManagerDelegate : public GpuChannelManagerDelegate {
 public:
  TestGpuChannelManagerDelegate(Scheduler* scheduler) : scheduler_(scheduler) {}
  ~TestGpuChannelManagerDelegate() override = default;

  // GpuChannelManagerDelegate implementation:
  void RegisterDisplayContext(gpu::DisplayContext* context) override {}
  void UnregisterDisplayContext(gpu::DisplayContext* context) override {}
  void LoseAllContexts() override {}
  void DidCreateContextSuccessfully() override {}
  void DidCreateOffscreenContext(const GURL& active_url) override {}
  void DidDestroyChannel(int client_id) override {}
  void DidDestroyAllChannels() override {}
  void DidDestroyOffscreenContext(const GURL& active_url) override {}
  void DidLoseContext(bool offscreen,
                      error::ContextLostReason reason,
                      const GURL& active_url) override {}
  void StoreShaderToDisk(int32_t client_id,
                         const std::string& key,
                         const std::string& shader) override {}
  void MaybeExitOnContextLost() override { is_exiting_ = true; }
  bool IsExiting() const override { return is_exiting_; }
#if defined(OS_WIN)
  void DidUpdateOverlayInfo(const gpu::OverlayInfo& overlay_info) override {}
  void DidUpdateHDRStatus(bool hdr_enabled) override {}
  void SendCreatedChildWindow(SurfaceHandle parent_window,
                              SurfaceHandle child_window) override {}
#endif

  Scheduler* GetGpuScheduler() override { return scheduler_; }

 private:
  bool is_exiting_ = false;
  Scheduler* const scheduler_;

  DISALLOW_COPY_AND_ASSIGN(TestGpuChannelManagerDelegate);
};

GpuChannelTestCommon::GpuChannelTestCommon(bool use_stub_bindings)
    : GpuChannelTestCommon(std::vector<int32_t>(), use_stub_bindings) {}

GpuChannelTestCommon::GpuChannelTestCommon(
    std::vector<int32_t> enabled_workarounds,
    bool use_stub_bindings)
    : memory_dump_manager_(
          base::trace_event::MemoryDumpManager::CreateInstanceForTesting()),
      task_runner_(new base::TestSimpleTaskRunner),
      io_task_runner_(new base::TestSimpleTaskRunner),
      sync_point_manager_(new SyncPointManager()),
      shared_image_manager_(new SharedImageManager(false /* thread_safe */)),
      scheduler_(new Scheduler(task_runner_,
                               sync_point_manager_.get(),
                               GpuPreferences())),
      channel_manager_delegate_(
          new TestGpuChannelManagerDelegate(scheduler_.get())) {
  // We need GL bindings to actually initialize command buffers.
  if (use_stub_bindings) {
    gl::GLSurfaceTestSupport::InitializeOneOffWithStubBindings();
    // GrContext cannot be created with stub bindings.
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndDisableFeature(features::kUseSkiaRenderer);
  } else {
    gl::GLSurfaceTestSupport::InitializeOneOff();
  }

  GpuFeatureInfo feature_info;
  feature_info.enabled_gpu_driver_bug_workarounds =
      std::move(enabled_workarounds);

  channel_manager_.reset(new GpuChannelManager(
      GpuPreferences(), channel_manager_delegate_.get(), nullptr, /* watchdog */
      task_runner_.get(), io_task_runner_.get(), scheduler_.get(),
      sync_point_manager_.get(), shared_image_manager_.get(),
      nullptr, /* gpu_memory_buffer_factory */
      std::move(feature_info), GpuProcessActivityFlags(),
      gl::init::CreateOffscreenGLSurface(gfx::Size()),
      nullptr /* image_decode_accelerator_worker */));
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
