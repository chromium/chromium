// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/raster_in_process_context.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/raster_implementation.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/in_process_gpu_thread_holder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

namespace gpu {

namespace {

constexpr viz::SharedImageFormat kSharedImageFormat =
    viz::SinglePlaneFormat::kRGBA_8888;
constexpr gfx::Size kBufferSize(100, 100);

class RasterInProcessCommandBufferTest : public ::testing::Test {
 public:
  RasterInProcessCommandBufferTest() {
    // Always enable gpu and oop raster, regardless of platform and blocklist.
    auto* gpu_feature_info = gpu_thread_holder_.GetGpuFeatureInfo();
    gpu_feature_info
        ->status_values[gpu::GPU_FEATURE_TYPE_GPU_TILE_RASTERIZATION] =
        gpu::kGpuFeatureStatusEnabled;
  }

  std::unique_ptr<RasterInProcessContext> CreateRasterInProcessContext() {
    if (!RasterInProcessContext::SupportedInTest())
      return nullptr;

    auto context = std::make_unique<RasterInProcessContext>();
    auto result = context->Initialize(gpu_thread_holder_.GetTaskExecutor(),
                                      /*gr_shader_cache=*/nullptr,
                                      /*use_shader_cache_shm_count=*/nullptr);
    DCHECK_EQ(result, ContextResult::kSuccess);
    return context;
  }

  void SetUp() override {
    if (!RasterInProcessContext::SupportedInTest())
      return;
    gpu_thread_holder_.GetGpuPreferences()->gr_context_type =
        GrContextType::kGL;
    context_ = CreateRasterInProcessContext();
    ri_ = context_->GetImplementation();
  }

  void TearDown() override { context_.reset(); }

 protected:
  InProcessGpuThreadHolder gpu_thread_holder_;
  raw_ptr<raster::RasterInterface> ri_;  // not owned
  std::unique_ptr<RasterInProcessContext> context_;
};

}  // namespace

TEST_F(RasterInProcessCommandBufferTest, AllowedBetweenBeginEndRasterCHROMIUM) {
  if (!RasterInProcessContext::SupportedInTest()) {
    GTEST_SKIP();
  }

  // Create shared image and allocate storage.
  auto* sii = context_->GetSharedImageInterface();
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  SharedImageUsageSet flags = gpu::SHARED_IMAGE_USAGE_RASTER_READ |
                              gpu::SHARED_IMAGE_USAGE_RASTER_WRITE;
  scoped_refptr<gpu::ClientSharedImage> shared_image = sii->CreateSharedImage(
      {kSharedImageFormat, kBufferSize, color_space, flags, "TestLabel"},
      kNullSurfaceHandle);
  auto ri_access = shared_image->BeginRasterAccess(
      ri_, shared_image->creation_sync_token(), /*readonly=*/false);

  // Call BeginRasterCHROMIUM.
  ri_->BeginRasterCHROMIUM(
      /*sk_color_4f=*/{0, 0, 0, 0}, /*needs_clear=*/true,
      /*msaa_sample_count=*/0, gpu::raster::kNoMSAA,
      /*can_use_lcd_text=*/false, /*visible=*/true, color_space,
      /*hdr_headroom=*/0.f, shared_image->mailbox().name);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ri_->GetError());

  // Should flag an error this command is not allowed between a Begin and
  // EndRasterCHROMIUM.
  GLuint id;
  ri_->GenQueriesEXT(1, &id);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), ri_->GetError());

  // Confirm that we skip over without error.
  ri_->EndRasterCHROMIUM();
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ri_->GetError());

  gpu::RasterScopedAccess::EndAccess(std::move(ri_access));
}

}  // namespace gpu
