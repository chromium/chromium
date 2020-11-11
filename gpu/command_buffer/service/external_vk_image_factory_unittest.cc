// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_factory.h"

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"
#include "gpu/command_buffer/service/external_vk_image_dawn_representation.h"
#include "gpu/command_buffer/service/external_vk_image_skia_representation.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/config/gpu_test_config.h"
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(USE_DAWN)
#include <dawn/dawn_proc.h>
#include <dawn/webgpu_cpp.h>
#include <dawn_native/DawnNative.h>
#endif  // BUILDFLAG(USE_DAWN)

namespace gpu {
namespace {

class ExternalVkImageFactoryTest : public testing::Test {
 protected:
  bool VulkanSupported() const {
    // crbug.com(941685, 1139366): Vulkan driver crashes on Linux FYI Release
    // (AMD R7 240).
    return !GPUTestBotConfig::CurrentConfigMatches("Linux AMD");
  }
  void SetUp() override {
    if (!VulkanSupported()) {
      return;
    }
    // Set up the Vulkan implementation and context provider.
    vulkan_implementation_ = gpu::CreateVulkanImplementation();
    DCHECK(vulkan_implementation_) << "Failed to create Vulkan implementation";

    auto initialize_vulkan = vulkan_implementation_->InitializeVulkanInstance();
    DCHECK(initialize_vulkan) << "Failed to initialize Vulkan implementation.";

    vulkan_context_provider_ = viz::VulkanInProcessContextProvider::Create(
        vulkan_implementation_.get());
    DCHECK(vulkan_context_provider_)
        << "Failed to create Vulkan context provider";

    // Set up a GL context. We don't actually need it, but we can't make
    // a SharedContextState without one.
    gl_surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
    DCHECK(gl_surface_);
    gl_context_ = gl::init::CreateGLContext(nullptr, gl_surface_.get(),
                                            gl::GLContextAttribs());
    DCHECK(gl_context_);
    bool make_current_result = gl_context_->MakeCurrent(gl_surface_.get());
    DCHECK(make_current_result);

    scoped_refptr<gl::GLShareGroup> share_group = new gl::GLShareGroup();
    context_state_ = base::MakeRefCounted<SharedContextState>(
        std::move(share_group), gl_surface_, gl_context_,
        false /* use_virtualized_gl_contexts */, base::DoNothing(),
        GrContextType::kVulkan, vulkan_context_provider_.get());

    GpuPreferences gpu_preferences = {};
    GpuDriverBugWorkarounds workarounds = {};
    context_state_->InitializeGrContext(gpu_preferences, workarounds, nullptr);

    memory_type_tracker_ = std::make_unique<MemoryTypeTracker>(nullptr);
    shared_image_representation_factory_ =
        std::make_unique<SharedImageRepresentationFactory>(
            &shared_image_manager_, nullptr);
    shared_image_factory_ =
        std::make_unique<ExternalVkImageFactory>(context_state_);

#if BUILDFLAG(USE_DAWN)
    // Create a Dawn Vulkan device
    dawn_instance_.DiscoverDefaultAdapters();

    std::vector<dawn_native::Adapter> adapters = dawn_instance_.GetAdapters();
    auto adapter_it = std::find_if(
        adapters.begin(), adapters.end(), [](dawn_native::Adapter adapter) {
          return adapter.GetBackendType() == dawn_native::BackendType::Vulkan;
        });
    ASSERT_NE(adapter_it, adapters.end());

    DawnProcTable procs = dawn_native::GetProcs();
    dawnProcSetProcs(&procs);

    dawn_device_ = wgpu::Device::Acquire(adapter_it->CreateDevice());
    DCHECK(dawn_device_) << "Failed to create Dawn device";
#endif  // BUILDFLAG(USE_DAWN)
  }

  void TearDown() override {
#if BUILDFLAG(USE_DAWN)
    dawn_device_ = wgpu::Device();
    dawnProcSetProcs(nullptr);
#endif  // BUILDFLAG(USE_DAWN)
  }

  std::unique_ptr<VulkanImplementation> vulkan_implementation_;
  scoped_refptr<viz::VulkanInProcessContextProvider> vulkan_context_provider_;

  scoped_refptr<gl::GLSurface> gl_surface_;
  scoped_refptr<gl::GLContext> gl_context_;
  scoped_refptr<SharedContextState> context_state_;

  SharedImageManager shared_image_manager_;
  std::unique_ptr<MemoryTypeTracker> memory_type_tracker_;
  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  std::unique_ptr<ExternalVkImageFactory> shared_image_factory_;

#if BUILDFLAG(USE_DAWN)
  dawn_native::Instance dawn_instance_;
  wgpu::Device dawn_device_;
#endif  // BUILDFLAG(USE_DAWN)
};

#if BUILDFLAG(USE_DAWN)

TEST_F(ExternalVkImageFactoryTest, DawnWrite_SkiaVulkanRead) {
  if (!VulkanSupported()) {
    DLOG(ERROR) << "Test skipped because Vulkan isn't supported.";
    return;
  }
  // Create a backing using mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::ResourceFormat::RGBA_8888;
  const gfx::Size size(4, 4);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const uint32_t usage = SHARED_IMAGE_USAGE_DISPLAY | SHARED_IMAGE_USAGE_WEBGPU;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage,
      false /* is_thread_safe */);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  {
    // Create a Dawn representation to clear the texture contents to a green.
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(mailbox,
                                                          dawn_device_.Get());
    ASSERT_TRUE(dawn_representation);

    auto dawn_scoped_access = dawn_representation->BeginScopedAccess(
        WGPUTextureUsage_OutputAttachment,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(dawn_scoped_access);

    wgpu::Texture texture(dawn_scoped_access->texture());
    wgpu::RenderPassColorAttachmentDescriptor color_desc;
    color_desc.attachment = texture.CreateView();
    color_desc.resolveTarget = nullptr;
    color_desc.loadOp = wgpu::LoadOp::Clear;
    color_desc.storeOp = wgpu::StoreOp::Store;
    color_desc.clearColor = {0, 255, 0, 255};

    wgpu::RenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &color_desc;
    renderPassDesc.depthStencilAttachment = nullptr;

    wgpu::CommandEncoder encoder = dawn_device_.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.EndPass();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = dawn_device_.GetDefaultQueue();
    queue.Submit(1, &commands);
  }

  EXPECT_TRUE(factory_ref->IsCleared());

  {
    auto skia_representation =
        shared_image_representation_factory_->ProduceSkia(mailbox,
                                                          context_state_.get());

    std::vector<GrBackendSemaphore> begin_semaphores;
    std::vector<GrBackendSemaphore> end_semaphores;
    auto skia_scoped_access = skia_representation->BeginScopedReadAccess(
        &begin_semaphores, &end_semaphores);

    context_state_->gr_context()->wait(begin_semaphores.size(),
                                       begin_semaphores.data());

    EXPECT_TRUE(skia_scoped_access);

    auto* promise_texture = skia_scoped_access->promise_image_texture();
    GrBackendTexture backend_texture = promise_texture->backendTexture();

    EXPECT_TRUE(backend_texture.isValid());
    EXPECT_EQ(size.width(), backend_texture.width());
    EXPECT_EQ(size.height(), backend_texture.height());

    // Create an Sk Image from GrBackendTexture.
    auto sk_image = SkImage::MakeFromTexture(
        context_state_->gr_context(), backend_texture, kTopLeft_GrSurfaceOrigin,
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

    GrFlushInfo flush_info;
    flush_info.fNumSemaphores = end_semaphores.size();
    flush_info.fSignalSemaphores = end_semaphores.data();
    gpu::AddVulkanCleanupTaskForSkiaFlush(vulkan_context_provider_.get(),
                                          &flush_info);

    context_state_->gr_context()->flush(flush_info);
    context_state_->gr_context()->submit();
  }
}

TEST_F(ExternalVkImageFactoryTest, SkiaVulkanWrite_DawnRead) {
  if (!VulkanSupported()) {
    DLOG(ERROR) << "Test skipped because Vulkan isn't supported.";
    return;
  }
  // Create a backing using mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::ResourceFormat::RGBA_8888;
  const gfx::Size size(4, 4);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const uint32_t usage = SHARED_IMAGE_USAGE_DISPLAY | SHARED_IMAGE_USAGE_WEBGPU;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage,
      false /* is_thread_safe */);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  {
    // Create a SkiaRepresentation
    auto skia_representation =
        shared_image_representation_factory_->ProduceSkia(mailbox,
                                                          context_state_.get());

    // Begin access for writing
    std::vector<GrBackendSemaphore> begin_semaphores;
    std::vector<GrBackendSemaphore> end_semaphores;
    auto skia_scoped_access = skia_representation->BeginScopedWriteAccess(
        1 /* final_msaa_count */,
        SkSurfaceProps(0 /* flags */, kUnknown_SkPixelGeometry),
        &begin_semaphores, &end_semaphores,
        gpu::SharedImageRepresentation::AllowUnclearedAccess::kYes);

    SkSurface* dest_surface = skia_scoped_access->surface();
    dest_surface->wait(begin_semaphores.size(), begin_semaphores.data());
    SkCanvas* dest_canvas = dest_surface->getCanvas();

    // Color the top half blue, and the bottom half green
    dest_canvas->drawRect(SkRect{0, 0, size.width(), size.height() / 2},
                          SkPaint(SkColors::kBlue));
    dest_canvas->drawRect(
        SkRect{0, size.height() / 2, size.width(), size.height()},
        SkPaint(SkColors::kGreen));
    skia_representation->SetCleared();

    GrFlushInfo flush_info;
    flush_info.fNumSemaphores = end_semaphores.size();
    flush_info.fSignalSemaphores = end_semaphores.data();
    gpu::AddVulkanCleanupTaskForSkiaFlush(vulkan_context_provider_.get(),
                                          &flush_info);
    dest_surface->flush(flush_info, skia_scoped_access->end_state());
    if (skia_scoped_access->end_state()) {
      context_state_->gr_context()->setBackendTextureState(
          dest_surface->getBackendTexture(
              SkSurface::BackendHandleAccess::kFlushRead_BackendHandleAccess),
          *skia_scoped_access->end_state());
    }
    context_state_->gr_context()->submit();
  }

  {
    // Create a Dawn representation
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(mailbox,
                                                          dawn_device_.Get());
    ASSERT_TRUE(dawn_representation);

    // Begin access to copy the data out. Skia should have initialized the
    // contents.
    auto dawn_scoped_access = dawn_representation->BeginScopedAccess(
        WGPUTextureUsage_CopySrc,
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
      wgpu::TextureCopyView src_copy_view = {};
      src_copy_view.origin = {0, 0, 0};
      src_copy_view.texture = src_texture;

      wgpu::BufferCopyView dst_copy_view = {};
      dst_copy_view.buffer = dst_buffer;
      dst_copy_view.layout.bytesPerRow = 256;
      dst_copy_view.layout.offset = 0;
      dst_copy_view.layout.rowsPerImage = 0;

      wgpu::Extent3D copy_extent = {size.width(), size.height(), 1};

      encoder.CopyTextureToBuffer(&src_copy_view, &dst_copy_view, &copy_extent);
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    wgpu::Queue queue = dawn_device_.GetDefaultQueue();
    queue.Submit(1, &commands);

    // Map the buffer to read back data
    bool done = false;
    dst_buffer.MapAsync(
        wgpu::MapMode::Read, 0, 256 * size.height(),
        [](WGPUBufferMapAsyncStatus status, void* userdata) {
          EXPECT_EQ(status, WGPUBufferMapAsyncStatus_Success);
          *static_cast<bool*>(userdata) = true;
        },
        &done);

    while (!done) {
      base::PlatformThread::Sleep(base::TimeDelta::FromMicroseconds(100));
      dawn_device_.Tick();
    }

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

}  // anonymous namespace
}  // namespace gpu
