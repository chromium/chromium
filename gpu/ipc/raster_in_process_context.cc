// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/raster_in_process_context.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/test/test_simple_task_runner.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/raster_cmd_helper.h"
#include "gpu/command_buffer/client/raster_implementation.h"
#include "gpu/command_buffer/client/raster_implementation_gles.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/common/surface_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

RasterInProcessContext::RasterInProcessContext() = default;

RasterInProcessContext::~RasterInProcessContext() {
  // Trigger any pending lost contexts. First do a full sync between client
  // and service threads. Then execute any pending tasks.
  if (raster_implementation_) {
    raster_implementation_->Finish();
    client_task_runner_->RunUntilIdle();
    raster_implementation_.reset();
  }
  transfer_buffer_.reset();
  helper_.reset();
  command_buffer_.reset();
}

ContextResult RasterInProcessContext::Initialize(
    CommandBufferTaskExecutor* task_executor,
    const ContextCreationAttribs& attribs,
    const SharedMemoryLimits& memory_limits,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    ImageFactory* image_factory,
    GpuChannelManagerDelegate* gpu_channel_manager_delegate,
    gpu::raster::GrShaderCache* gr_shader_cache,
    GpuProcessActivityFlags* activity_flags) {
  DCHECK(attribs.enable_raster_interface);
  if (!attribs.enable_raster_interface) {
    return ContextResult::kFatalFailure;
  }
  DCHECK(!attribs.enable_gles2_interface);
  if (attribs.enable_gles2_interface) {
    return ContextResult::kFatalFailure;
  }

  client_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  command_buffer_ =
      std::make_unique<InProcessCommandBuffer>(task_executor, GURL());
  auto result = command_buffer_->Initialize(
      nullptr /* surface */, true /* is_offscreen */, kNullSurfaceHandle,
      attribs, gpu_memory_buffer_manager, image_factory,
      gpu_channel_manager_delegate, client_task_runner_, gr_shader_cache,
      activity_flags);
  if (result != ContextResult::kSuccess) {
    DLOG(ERROR) << "Failed to initialize InProcessCommmandBuffer";
    return result;
  }

  // Check for consistency.
  DCHECK(!attribs.bind_generates_resource);
  constexpr bool bind_generates_resource = false;

  // TODO(https://crbug.com/829469): Remove check once we fuzz RasterDecoder.
  // enable_oop_rasterization is currently necessary to create RasterDecoder
  // in InProcessCommandBuffer.
  DCHECK(attribs.enable_oop_rasterization);

  // Create the RasterCmdHelper, which writes the command buffer protocol.
  auto raster_helper =
      std::make_unique<raster::RasterCmdHelper>(command_buffer_.get());
  result = raster_helper->Initialize(memory_limits.command_buffer_size);
  if (result != ContextResult::kSuccess) {
    LOG(ERROR) << "Failed to initialize RasterCmdHelper";
    return result;
  }
  transfer_buffer_ = std::make_unique<TransferBuffer>(raster_helper.get());

  raster_implementation_ = std::make_unique<raster::RasterImplementation>(
      raster_helper.get(), transfer_buffer_.get(), bind_generates_resource,
      attribs.lose_context_when_out_of_memory, command_buffer_.get(),
      nullptr /* image_decode_accelerator */);
  result = raster_implementation_->Initialize(memory_limits);
  raster_implementation_->SetLostContextCallback(base::BindOnce(
      []() { EXPECT_TRUE(false) << "Unexpected lost context."; }));
  helper_ = std::move(raster_helper);
  return result;
}

const Capabilities& RasterInProcessContext::GetCapabilities() const {
  return command_buffer_->GetCapabilities();
}

const GpuFeatureInfo& RasterInProcessContext::GetGpuFeatureInfo() const {
  return command_buffer_->GetGpuFeatureInfo();
}

raster::RasterInterface* RasterInProcessContext::GetImplementation() {
  return raster_implementation_.get();
}

ContextSupport* RasterInProcessContext::GetContextSupport() {
  return raster_implementation_.get();
}

SharedImageInterface* RasterInProcessContext::GetSharedImageInterface() {
  return command_buffer_->GetSharedImageInterface();
}

ServiceTransferCache* RasterInProcessContext::GetTransferCacheForTest() const {
  return command_buffer_->GetTransferCacheForTest();
}

InProcessCommandBuffer* RasterInProcessContext::GetCommandBufferForTest()
    const {
  return command_buffer_.get();
}

int RasterInProcessContext::GetRasterDecoderIdForTest() const {
  return command_buffer_->GetRasterDecoderIdForTest();
}

// static
bool RasterInProcessContext::SupportedInTest() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  GpuPreferences gpu_preferences = gles2::ParseGpuPreferences(command_line);
  return !gpu_preferences.use_passthrough_cmd_decoder ||
         !gles2::PassthroughCommandDecoderSupported();
}

}  // namespace gpu
