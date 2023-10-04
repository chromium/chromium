// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_CHANNEL_TEST_COMMON_H_
#define GPU_IPC_SERVICE_GPU_CHANNEL_TEST_COMMON_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_display.h"

namespace base {
namespace trace_event {
class MemoryDumpManager;
}  // namespace trace_event
}  // namespace base

namespace gpu {
class GpuChannel;
class GpuChannelManager;
class Scheduler;
class SyncPointManager;
class SharedImageManager;
class TestGpuChannelManagerDelegate;

class GpuChannelTestCommon : public testing::Test {
 public:
  explicit GpuChannelTestCommon(bool use_stub_bindings);
  // Constructor which allows a custom set of GPU driver bug workarounds.
  GpuChannelTestCommon(std::vector<int32_t> enabled_workarounds,
                       bool use_stub_bindings);

  GpuChannelTestCommon(const GpuChannelTestCommon&) = delete;
  GpuChannelTestCommon& operator=(const GpuChannelTestCommon&) = delete;

  ~GpuChannelTestCommon() override;

 protected:
  Scheduler* scheduler() const { return scheduler_.get(); }
  GpuChannelManager* channel_manager() const { return channel_manager_.get(); }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  GpuChannel* CreateChannel(int32_t client_id, bool is_gpu_host);

  void CreateCommandBuffer(GpuChannel& channel,
                           mojom::CreateCommandBufferParamsPtr init_params,
                           int32_t routing_id,
                           base::UnsafeSharedMemoryRegion shared_state,
                           ContextResult* out_result,
                           Capabilities* out_capabilities,
                           GLCapabilities* out_gl_capabilities);

  base::UnsafeSharedMemoryRegion GetSharedMemoryRegion();

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::trace_event::MemoryDumpManager> memory_dump_manager_;
  std::unique_ptr<SyncPointManager> sync_point_manager_;
  std::unique_ptr<SharedImageManager> shared_image_manager_;
  std::unique_ptr<Scheduler> scheduler_;
  std::unique_ptr<TestGpuChannelManagerDelegate> channel_manager_delegate_;
  std::unique_ptr<GpuChannelManager> channel_manager_;
  raw_ptr<gl::GLDisplay> display_ = nullptr;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_CHANNEL_TEST_COMMON_H_
