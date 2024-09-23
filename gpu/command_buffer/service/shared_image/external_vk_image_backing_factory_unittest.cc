// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/shared_image/external_vk_image_backing_factory.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/external_vk_image_dawn_representation.h"
#include "gpu/command_buffer/service/shared_image/external_vk_image_skia_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_test_base.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/config/gpu_test_config.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gl/buildflags.h"

#if BUILDFLAG(USE_DAWN)
#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>
#include <dawn/webgpu_cpp.h>
#endif  // BUILDFLAG(USE_DAWN)

namespace gpu {
namespace {

class ExternalVkImageBackingFactoryTest : public SharedImageTestBase {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(InitializeContext(GrContextType::kVulkan));
    backing_factory_ =
        std::make_unique<ExternalVkImageBackingFactory>(context_state_);
  }
};

#if BUILDFLAG(USE_DAWN)

class ExternalVkImageBackingFactoryDawnTest
    : public ExternalVkImageBackingFactoryTest {
 public:
  void SetUp() override {
    // crbug.com(941685, 1139366): Vulkan driver crashes on Linux FYI Release
    // (AMD R7 240).
    if (GPUTestBotConfig::CurrentConfigMatches("Linux AMD")) {
      GTEST_SKIP();
    }

    ExternalVkImageBackingFactoryTest::SetUp();

    dawnProcSetProcs(&dawn::native::GetProcs());

    // Find a Dawn Vulkan adapter
    wgpu::RequestAdapterOptions adapter_options;
    adapter_options.backendType = wgpu::BackendType::Vulkan;
    std::vector<dawn::native::Adapter> adapters =
        dawn_instance_.EnumerateAdapters(&adapter_options);
    ASSERT_GT(adapters.size(), 0u);

    // We need to request internal usage to be able to do operations with
    // internal methods that would need specific usages.
    wgpu::FeatureName dawn_internal_usage =
        wgpu::FeatureName::DawnInternalUsages;
    wgpu::DeviceDescriptor device_descriptor;
    device_descriptor.requiredFeatureCount = 1;
    device_descriptor.requiredFeatures = &dawn_internal_usage;

    dawn_device_ =
        wgpu::Device::Acquire(adapters[0].CreateDevice(&device_descriptor));
    DCHECK(dawn_device_) << "Failed to create Dawn device";
  }

  void TearDown() override {
    dawn_device_ = wgpu::Device();
    dawnProcSetProcs(nullptr);
  }

 protected:
  static constexpr WGPUInstanceDescriptor dawn_instance_desc_ = {
      .features =
          {
              .timedWaitAnyEnable = true,
          },
  };
  dawn::native::Instance dawn_instance_ =
      dawn::native::Instance(&dawn_instance_desc_);
  wgpu::Device dawn_device_;
};

TEST_F(ExternalVkImageBackingFactoryDawnTest, DawnWrite_SkiaVulkanRead) {
  // Create a backing using mailbox.
  auto mailbox = Mailbox::Generate();
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size size(4, 4);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const gpu::SharedImageUsageSet usage =
      SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_WEBGPU_WRITE;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage, "TestLabel",
      /*is_thread_safe=*/false);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);

  {
    // Create a Dawn representation to clear the texture contents to a green.
    auto dawn_representation = shared_image_representation_factory_.ProduceDawn(
        mailbox, dawn_device_, wgpu::BackendType::Vulkan, {}, context_state_);
    ASSERT_TRUE(dawn_representation);

    auto dawn_scoped_access = dawn_representation->BeginScopedAccess(
        wgpu::TextureUsage::RenderAttachment,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(dawn_scoped_access);

    wgpu::Texture texture(dawn_scoped_access->texture());
    wgpu::RenderPassColorAttachment color_desc;
    color_desc.view = texture.CreateView();
    color_desc.resolveTarget = nullptr;
    color_desc.loadOp = wgpu::LoadOp::Clear;
    color_desc.storeOp = wgpu::StoreOp::Store;
    color_desc.clearValue = {0, 255, 0, 255};

    wgpu::RenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &color_desc;
    renderPassDesc.depthStencilAttachment = nullptr;

    wgpu::CommandEncoder encoder = dawn_device_.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = dawn_device_.GetQueue();
    queue.Submit(1, &commands);
  }

  EXPECT_TRUE(factory_ref->IsCleared());

  {
    auto skia_representation = shared_image_representation_factory_.ProduceSkia(
        mailbox, context_state_.get());

    std::vector<GrBackendSemaphore> begin_semaphores;
    std::vector<GrBackendSemaphore> end_semaphores;
    auto skia_scoped_access = skia_representation->BeginScopedReadAccess(
        &begin_semaphores, &end_semaphores);

    gr_context()->wait(begin_semaphores.size(), begin_semaphores.data(),
                       /*deleteSemaphoresAfterWait=*/false);

    EXPECT_TRUE(skia_scoped_access);

    auto* promise_texture = skia_scoped_access->promise_image_texture();
    GrBackendTexture backend_texture = promise_texture->backendTexture();

    EXPECT_TRUE(backend_texture.isValid());
    EXPECT_EQ(size.width(), backend_texture.width());
    EXPECT_EQ(size.height(), backend_texture.height());

    // Create an Sk Image from GrBackendTexture.
    auto sk_image = SkImages::BorrowTextureFrom(
        gr_context(), backend_texture, kTopLeft_GrSurfaceOrigin,
        kRGBA_8888_SkColorType, kOpaque_SkAlphaType, nullptr);
    EXPECT_TRUE(sk_image);

    const SkImageInfo dst_info =
        SkImageInfo::Make(size.width(), size.height(), kRGBA_8888_SkColorType,
                          kOpaque_SkAlphaType, nullptr);

    const int num_pixels = size.width() * size.height();
    std::vector<uint8_t> dst_pixels(num_pixels * 4);

    // Read back pixels from Sk Image.
    EXPECT_TRUE(sk_image->readPixels(dst_info, dst_pixels.data(),
                                     dst_info.minRowBytes(), 0, 0));

    for (int i = 0; i < num_pixels; i++) {
      // Compare the pixel values.
      const uint8_t* pixel = dst_pixels.data() + (i * 4);
      EXPECT_EQ(pixel[0], 0);
      EXPECT_EQ(pixel[1], 255);
      EXPECT_EQ(pixel[2], 0);
      EXPECT_EQ(pixel[3], 255);
    }

    skia_scoped_access->ApplyBackendSurfaceEndState();

    GrFlushInfo flush_info;
    flush_info.fNumSemaphores = end_semaphores.size();
    flush_info.fSignalSemaphores = end_semaphores.data();
    gpu::AddVulkanCleanupTaskForSkiaFlush(vulkan_context_provider_.get(),
                                          &flush_info);

    gr_context()->flush(flush_info);
    gr_context()->submit();
  }
}

TEST_F(ExternalVkImageBackingFactoryDawnTest, SkiaVulkanWrite_DawnRead) {
  // Create a backing using mailbox.
  auto mailbox = Mailbox::Generate();
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size size(4, 4);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const gpu::SharedImageUsageSet usage =
      SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_WEBGPU_READ;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage, "TestLabel",
      /*is_thread_safe=*/false);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);

  {
    // Create a SkiaImageRepresentation
    auto skia_representation = shared_image_representation_factory_.ProduceSkia(
        mailbox, context_state_.get());

    // Begin access for writing
    std::vector<GrBackendSemaphore> begin_semaphores;
    std::vector<GrBackendSemaphore> end_semaphores;
    auto skia_scoped_access = skia_representation->BeginScopedWriteAccess(
        /*final_msaa_count=*/1,
        SkSurfaceProps(/*flags=*/0, kUnknown_SkPixelGeometry),
        &begin_semaphores, &end_semaphores,
        gpu::SharedImageRepresentation::AllowUnclearedAccess::kYes);

    SkSurface* dest_surface = skia_scoped_access->surface();
    dest_surface->wait(begin_semaphores.size(), begin_semaphores.data(),
                       /*deleteSemaphoresAfterWait=*/false);
    SkCanvas* dest_canvas = dest_surface->getCanvas();

    // Color the top half blue, and the bottom half green
    dest_canvas->drawRect(
        SkRect{0, 0, static_cast<SkScalar>(size.width()), size.height() / 2.0f},
        SkPaint(SkColors::kBlue));
    dest_canvas->drawRect(
        SkRect{0, size.height() / 2.0f, static_cast<SkScalar>(size.width()),
               static_cast<SkScalar>(size.height())},
        SkPaint(SkColors::kGreen));
    skia_representation->SetCleared();

    GrFlushInfo flush_info;
    flush_info.fNumSemaphores = end_semaphores.size();
    flush_info.fSignalSemaphores = end_semaphores.data();
    gpu::AddVulkanCleanupTaskForSkiaFlush(vulkan_context_provider_.get(),
                                          &flush_info);
    gr_context()->flush(dest_surface, flush_info, nullptr);
    skia_scoped_access->ApplyBackendSurfaceEndState();
    gr_context()->submit();
  }

  {
    // Create a Dawn representation
    auto dawn_representation = shared_image_representation_factory_.ProduceDawn(
        mailbox, dawn_device_, wgpu::BackendType::Vulkan, {}, context_state_);
    ASSERT_TRUE(dawn_representation);

    // Begin access to copy the data out. Skia should have initialized the
    // contents.
    auto dawn_scoped_access = dawn_representation->BeginScopedAccess(
        wgpu::TextureUsage::CopySrc,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    ASSERT_TRUE(dawn_scoped_access);

    wgpu::Texture src_texture(dawn_scoped_access->texture());

    // Create a buffer to read back the texture data
    wgpu::BufferDescriptor dst_buffer_desc = {};
    dst_buffer_desc.usage =
        wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    dst_buffer_desc.size = 256 * size.height();
    wgpu::Buffer dst_buffer = dawn_device_.CreateBuffer(&dst_buffer_desc);

    // Encode the buffer copy
    wgpu::CommandEncoder encoder = dawn_device_.CreateCommandEncoder();
    {
      wgpu::ImageCopyTexture src_copy_view = {};
      src_copy_view.origin = {0, 0, 0};
      src_copy_view.texture = src_texture;

      wgpu::ImageCopyBuffer dst_copy_view = {};
      dst_copy_view.buffer = dst_buffer;
      dst_copy_view.layout.bytesPerRow = 256;
      dst_copy_view.layout.offset = 0;

      wgpu::Extent3D copy_extent = {static_cast<uint32_t>(size.width()),
                                    static_cast<uint32_t>(size.height()), 1};

      encoder.CopyTextureToBuffer(&src_copy_view, &dst_copy_view, &copy_extent);
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    wgpu::Queue queue = dawn_device_.GetQueue();
    queue.Submit(1, &commands);

    // Map the buffer to read back data
    wgpu::FutureWaitInfo wait_info{
        dst_buffer.MapAsync(wgpu::MapMode::Read, 0, 256 * size.height(),
                            wgpu::CallbackMode::WaitAnyOnly,
                            [](wgpu::MapAsyncStatus status, const char*) {
                              ASSERT_EQ(status, wgpu::MapAsyncStatus::Success);
                            })};

    wgpu::WaitStatus status =
        wgpu::Instance(dawn_instance_.Get())
            .WaitAny(1, &wait_info, std::numeric_limits<uint64_t>::max());
    DCHECK(status == wgpu::WaitStatus::Success);

    // Check the pixel data
    const uint8_t* pixel_data =
        static_cast<const uint8_t*>(dst_buffer.GetConstMappedRange());
    for (int h = 0; h < size.height(); ++h) {
      for (int w = 0; w < size.width(); ++w) {
        const uint8_t* pixel = (pixel_data + h * 256) + w * 4;
        if (h < size.height() / 2) {
          EXPECT_EQ(pixel[0], 0);
          EXPECT_EQ(pixel[1], 0);
          EXPECT_EQ(pixel[2], 255);
          EXPECT_EQ(pixel[3], 255);
        } else {
          EXPECT_EQ(pixel[0], 0);
          EXPECT_EQ(pixel[1], 255);
          EXPECT_EQ(pixel[2], 0);
          EXPECT_EQ(pixel[3], 255);
        }
      }
    }
  }
}

#endif  // BUILDFLAG(USE_DAWN)

class ExternalVkImageBackingFactoryWithFormatTest
    : public ExternalVkImageBackingFactoryTest,
      public testing::WithParamInterface<viz::SharedImageFormat> {
 public:
  viz::SharedImageFormat get_format() { return GetParam(); }
};

TEST_P(ExternalVkImageBackingFactoryWithFormatTest, Basic) {
  viz::SharedImageFormat format = get_format();
  auto mailbox = Mailbox::Generate();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SharedImageUsageSet usage = SHARED_IMAGE_USAGE_DISPLAY_READ |
                                   SHARED_IMAGE_USAGE_GLES2_READ |
                                   SHARED_IMAGE_USAGE_GLES2_WRITE;

  bool supported = backing_factory_->CanCreateSharedImage(
      usage, format, size, /*thread_safe=*/false, gfx::EMPTY_BUFFER,
      GrContextType::kVulkan, {});
  ASSERT_TRUE(supported);

  // Verify backing can be created.
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, gpu::kNullSurfaceHandle, size, color_space,
      surface_origin, alpha_type, usage, "TestLabel", /*is_thread_safe=*/false);
  ASSERT_TRUE(backing);

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);
  EXPECT_TRUE(shared_image);

  auto skia_representation = shared_image_representation_factory_.ProduceSkia(
      mailbox, context_state_.get());
  ASSERT_TRUE(skia_representation);

  {
    // Verify Skia write access works.
    std::vector<GrBackendSemaphore> begin_semaphores;
    std::vector<GrBackendSemaphore> end_semaphores;
    auto scoped_write_access = skia_representation->BeginScopedWriteAccess(
        &begin_semaphores, &end_semaphores,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(scoped_write_access);
    EXPECT_TRUE(begin_semaphores.empty());

    for (int plane = 0; plane < format.NumberOfPlanes(); ++plane) {
      auto* surface = scoped_write_access->surface(plane);
      ASSERT_TRUE(surface);

      auto plane_size = format.GetPlaneSize(plane, size);
      EXPECT_EQ(plane_size.width(), surface->width());
      EXPECT_EQ(plane_size.height(), surface->height());
    }

    scoped_write_access->ApplyBackendSurfaceEndState();
    // Handle end state and semaphores.
    if (!end_semaphores.empty()) {
      GrFlushInfo flush_info;
      if (!end_semaphores.empty()) {
        flush_info.fNumSemaphores = end_semaphores.size();
        flush_info.fSignalSemaphores = end_semaphores.data();
      }
      for (int plane = 0; plane < format.NumberOfPlanes(); ++plane) {
        gr_context()->flush(scoped_write_access->surface(plane), flush_info,
                            nullptr);
      }
      gr_context()->submit();
    }
  }

  // Must set cleared before read access.
  skia_representation->SetCleared();

  {
    // Verify Skia read access works.
    std::vector<GrBackendSemaphore> begin_semaphores;
    std::vector<GrBackendSemaphore> end_semaphores;
    auto scoped_read_access = skia_representation->BeginScopedReadAccess(
        &begin_semaphores, &end_semaphores);
    ASSERT_TRUE(scoped_read_access);

    for (int plane = 0; plane < format.NumberOfPlanes(); ++plane) {
      auto* promise_texture = scoped_read_access->promise_image_texture(plane);
      ASSERT_TRUE(promise_texture);
      GrBackendTexture backend_texture = promise_texture->backendTexture();
      EXPECT_TRUE(backend_texture.isValid());

      auto plane_size = format.GetPlaneSize(plane, size);
      EXPECT_EQ(plane_size.width(), backend_texture.width());
      EXPECT_EQ(plane_size.height(), backend_texture.height());
    }

    // Handle end state and semaphores.
    scoped_read_access->ApplyBackendSurfaceEndState();
    if (!end_semaphores.empty()) {
      GrFlushInfo flush_info = {
          .fNumSemaphores = end_semaphores.size(),
          .fSignalSemaphores = end_semaphores.data(),
      };
      gr_context()->flush(flush_info);
      gr_context()->submit();
    }
  }
  skia_representation.reset();

  // Verify GL access works.
  if (use_passthrough()) {
    auto gl_representation =
        shared_image_representation_factory_.ProduceGLTexturePassthrough(
            mailbox);
    ASSERT_TRUE(gl_representation);
    auto scoped_access = gl_representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    ASSERT_TRUE(scoped_access);

    for (int plane = 0; plane < format.NumberOfPlanes(); ++plane) {
      auto texture = gl_representation->GetTexturePassthrough(plane);
      ASSERT_TRUE(texture);
      EXPECT_NE(texture->service_id(), 0u);
    }
  } else {
    auto gl_representation =
        shared_image_representation_factory_.ProduceGLTexture(mailbox);
    ASSERT_TRUE(gl_representation);
    auto scoped_access = gl_representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    ASSERT_TRUE(scoped_access);

    for (int plane = 0; plane < format.NumberOfPlanes(); ++plane) {
      auto* texture = gl_representation->GetTexture(plane);
      ASSERT_TRUE(texture);
      EXPECT_NE(texture->service_id(), 0u);
    }
  }
}

// Verify that pixel upload works as expected.
TEST_P(ExternalVkImageBackingFactoryWithFormatTest, Upload) {
  viz::SharedImageFormat format = get_format();
  auto mailbox = Mailbox::Generate();
  gfx::Size size(30, 30);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SharedImageUsageSet usage =
      SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_CPU_UPLOAD;

  // Verify backing can be created.
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, gpu::kNullSurfaceHandle, size, color_space,
      surface_origin, alpha_type, usage, "TestLabel", /*is_thread_safe=*/false);
  ASSERT_TRUE(backing);

  std::vector<SkBitmap> bitmaps = AllocateRedBitmaps(format, size);

  // Upload pixels and set cleared.
  ASSERT_TRUE(backing->UploadFromMemory(GetSkPixmaps(bitmaps)));
  backing->SetCleared();

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image_ref =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);
  ASSERT_TRUE(shared_image_ref);

  VerifyPixelsWithReadbackGanesh(mailbox, bitmaps);
}

std::string TestParamToString(
    const testing::TestParamInfo<viz::SharedImageFormat>& param_info) {
  return param_info.param.ToTestParamString();
}

const auto kSharedImageFormats =
    ::testing::Values(viz::SinglePlaneFormat::kRGBA_8888,
                      viz::SinglePlaneFormat::kBGRA_8888,
                      viz::SinglePlaneFormat::kR_8,
                      viz::SinglePlaneFormat::kRG_88,
                      viz::MultiPlaneFormat::kNV12,
                      viz::MultiPlaneFormat::kYV12,
                      viz::MultiPlaneFormat::kI420);

INSTANTIATE_TEST_SUITE_P(,
                         ExternalVkImageBackingFactoryWithFormatTest,
                         kSharedImageFormats,
                         TestParamToString);

}  // anonymous namespace
}  // namespace gpu
