// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "build/build_config.h"
#include "cc/paint/color_space_transfer_cache_entry.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/raster_implementation.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/host/gpu_memory_buffer_support.h"
#include "gpu/ipc/in_process_gpu_thread_holder.h"
#include "gpu/ipc/raster_in_process_context.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

namespace gpu {

namespace {

constexpr gfx::BufferFormat kBufferFormat = gfx::BufferFormat::RGBA_8888;
constexpr gfx::BufferUsage kBufferUsage = gfx::BufferUsage::SCANOUT;
constexpr viz::ResourceFormat kResourceFormat = viz::RGBA_8888;
constexpr gfx::Size kBufferSize(100, 100);

class RasterInProcessCommandBufferTest : public ::testing::Test {
 public:
  RasterInProcessCommandBufferTest() {
    // Always enable gpu and oop raster, regardless of platform and blacklist.
    auto* gpu_feature_info = gpu_thread_holder_.GetGpuFeatureInfo();
    gpu_feature_info->status_values[gpu::GPU_FEATURE_TYPE_GPU_RASTERIZATION] =
        gpu::kGpuFeatureStatusEnabled;
    gpu_feature_info->status_values[gpu::GPU_FEATURE_TYPE_OOP_RASTERIZATION] =
        gpu::kGpuFeatureStatusEnabled;
  }

  std::unique_ptr<RasterInProcessContext> CreateRasterInProcessContext() {
    if (!RasterInProcessContext::SupportedInTest())
      return nullptr;

    ContextCreationAttribs attributes;
    attributes.bind_generates_resource = false;
    attributes.enable_oop_rasterization = true;
    attributes.enable_gles2_interface = false;
    attributes.enable_raster_interface = true;

    auto context = std::make_unique<RasterInProcessContext>();
    auto result = context->Initialize(
        gpu_thread_holder_.GetTaskExecutor(), attributes, SharedMemoryLimits(),
        gpu_memory_buffer_manager_.get(),
        gpu_memory_buffer_factory_->AsImageFactory(),
        /*gpu_channel_manager_delegate=*/nullptr, nullptr, nullptr);
    DCHECK_EQ(result, ContextResult::kSuccess);
    return context;
  }

  void SetUp() override {
    if (!RasterInProcessContext::SupportedInTest())
      return;
    gpu_memory_buffer_factory_ = GpuMemoryBufferFactory::CreateNativeType();
    gpu_memory_buffer_manager_ =
        std::make_unique<viz::TestGpuMemoryBufferManager>();
    context_ = CreateRasterInProcessContext();
    ri_ = context_->GetImplementation();
  }

  void TearDown() override {
    context_.reset();
    gpu_memory_buffer_manager_.reset();
    gpu_memory_buffer_factory_.reset();
  }

 protected:
  InProcessGpuThreadHolder gpu_thread_holder_;
  raster::RasterInterface* ri_;  // not owned
  std::unique_ptr<GpuMemoryBufferFactory> gpu_memory_buffer_factory_;
  std::unique_ptr<GpuMemoryBufferManager> gpu_memory_buffer_manager_;
  std::unique_ptr<RasterInProcessContext> context_;
};

}  // namespace

TEST_F(RasterInProcessCommandBufferTest, CreateImage) {
  if (!RasterInProcessContext::SupportedInTest())
    return;

  // Calling CreateImageCHROMIUM() should allocate an image id.
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer1 =
      gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
          kBufferSize, kBufferFormat, kBufferUsage, kNullSurfaceHandle);
  GLuint image_id1 = ri_->CreateImageCHROMIUM(
      gpu_memory_buffer1->AsClientBuffer(), kBufferSize.width(),
      kBufferSize.height(), GL_RGBA);

  EXPECT_GT(image_id1, 0u);

  // Create a second GLInProcessContext that is backed by a different
  // InProcessCommandBuffer. Calling CreateImageCHROMIUM() should return a
  // different id than the first call.
  std::unique_ptr<RasterInProcessContext> context2 =
      CreateRasterInProcessContext();
  std::unique_ptr<gfx::GpuMemoryBuffer> buffer2 =
      gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
          kBufferSize, kBufferFormat, kBufferUsage, kNullSurfaceHandle);
  GLuint image_id2 = context2->GetImplementation()->CreateImageCHROMIUM(
      buffer2->AsClientBuffer(), kBufferSize.width(), kBufferSize.height(),
      GL_RGBA);

  EXPECT_GT(image_id2, 0u);
  EXPECT_NE(image_id1, image_id2);
}

TEST_F(RasterInProcessCommandBufferTest, SetColorSpaceMetadata) {
  if (!RasterInProcessContext::SupportedInTest())
    return;

  GLuint texture_id =
      ri_->CreateTexture(/*use_buffer=*/true, kBufferUsage, kResourceFormat);

  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer1 =
      gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
          kBufferSize, kBufferFormat, kBufferUsage, kNullSurfaceHandle);
  GLuint image_id = ri_->CreateImageCHROMIUM(
      gpu_memory_buffer1->AsClientBuffer(), kBufferSize.width(),
      kBufferSize.height(), GL_RGBA);

  ri_->BindTexImage2DCHROMIUM(texture_id, image_id);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ri_->GetError());

  gfx::ColorSpace color_space;
  ri_->SetColorSpaceMetadata(texture_id,
                             reinterpret_cast<GLColorSpace>(&color_space));
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ri_->GetError());
}

TEST_F(RasterInProcessCommandBufferTest, TexStorage2DImage) {
  if (!RasterInProcessContext::SupportedInTest())
    return;

  // Check for GPU and driver support
  if (!context_->GetCapabilities().texture_storage_image) {
    return;
  }
  std::vector<gfx::BufferUsageAndFormat> supported_formats =
      CreateBufferUsageAndFormatExceptionList();
  if (supported_formats.empty()) {
    return;
  }

  // Find a supported_format with a matching resource_format.
  bool found = false;
  gfx::BufferUsageAndFormat supported_format = supported_formats[0];
  viz::ResourceFormat resource_format = static_cast<viz::ResourceFormat>(0);
  for (size_t i = 0; !found && i < supported_formats.size(); ++i) {
    supported_format = supported_formats[i];
    for (size_t j = 0; !found && j <= viz::RESOURCE_FORMAT_MAX; ++j) {
      resource_format = static_cast<viz::ResourceFormat>(j);
      if (supported_format.format == viz::BufferFormat(resource_format)) {
        found = true;
      }
    }
  }

  if (!found) {
    return;
  }

  // Create a buffer backed texture and allocate storage.
  GLuint texture_id = ri_->CreateTexture(
      /*use_buffer=*/true, supported_format.usage, resource_format);
  ri_->TexStorage2D(texture_id, kBufferSize.width(), kBufferSize.height());

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ri_->GetError());
}

TEST_F(RasterInProcessCommandBufferTest,
       WhitelistBetweenBeginEndRasterCHROMIUM) {
  if (!RasterInProcessContext::SupportedInTest())
    return;

  // Check for GPU and driver support
  if (!context_->GetCapabilities().supports_oop_raster) {
    return;
  }

  // Create shared image and allocate storage.
  auto* sii = context_->GetSharedImageInterface();
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t flags = gpu::SHARED_IMAGE_USAGE_RASTER |
                   gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
  gpu::Mailbox mailbox =
      sii->CreateSharedImage(kResourceFormat, kBufferSize, color_space, flags);
  ri_->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());

  // Call BeginRasterCHROMIUM.
  cc::RasterColorSpace raster_color_space(color_space, 0);
  ri_->BeginRasterCHROMIUM(/*sk_color=*/0, /*msaa_sample_count=*/0,
                           /*can_use_lcd_text=*/false,
                           viz::ResourceFormatToClosestSkColorType(
                               /*gpu_compositing=*/true, kResourceFormat),
                           raster_color_space, mailbox.name);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ri_->GetError());

  // Should flag an error this command is not allowed between a Begin and
  // EndRasterCHROMIUM.
  ri_->CreateTexture(/*use_buffer=*/false, kBufferUsage, kResourceFormat);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), ri_->GetError());

  // Confirm that we skip over without error.
  ri_->EndRasterCHROMIUM();
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ri_->GetError());
}

}  // namespace gpu
