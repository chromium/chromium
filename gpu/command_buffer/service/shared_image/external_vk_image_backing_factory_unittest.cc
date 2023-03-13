// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/config/gpu_test_config.h"
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(USE_DAWN)
#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>
#include <dawn/webgpu_cpp.h>
#endif  // BUILDFLAG(USE_DAWN)

namespace gpu {
namespace {

class ExternalVkImageBackingFactoryTest : public testing::Test {
 protected:
  bool use_passthrough() const {
    return gles2::UsePassthroughCommandDecoder(
               base::CommandLine::ForCurrentProcess()) &&
           gles2::PassthroughCommandDecoderSupported();
  }

  GrDirectContext* gr_context() { return context_state_->gr_context(); }

  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS)
    GTEST_SKIP() << "Chrome OS Vulkan initialization fails";
#else
    // Set up the Vulkan implementation and context provider.
    vulkan_implementation_ = gpu::CreateVulkanImplementation();
    ASSERT_TRUE(vulkan_implementation_);

    auto initialize_vulkan = vulkan_implementation_->InitializeVulkanInstance();
    ASSERT_TRUE(initialize_vulkan);

    vulkan_context_provider_ = viz::VulkanInProcessContextProvider::Create(
        vulkan_implementation_.get());
    ASSERT_TRUE(vulkan_context_provider_);

    // Set up a GL context. We don't actually need it, but we can't make
    // a SharedContextState without one.
    gl_surface_ = gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplayEGL(),
                                                     gfx::Size());
    ASSERT_TRUE(gl_surface_);
    gl_context_ = gl::init::CreateGLContext(nullptr, gl_surface_.get(),
                                            gl::GLContextAttribs());
    ASSERT_TRUE(gl_context_);
    bool make_current_result = gl_context_->MakeCurrent(gl_surface_.get());
    ASSERT_TRUE(make_current_result);

    scoped_refptr<gl::GLShareGroup> share_group = new gl::GLShareGroup();
    context_state_ = base::MakeRefCounted<SharedContextState>(
        std::move(share_group), gl_surface_, gl_context_,
        /*use_virtualized_gl_contexts=*/false, base::DoNothing(),
        GrContextType::kVulkan, vulkan_context_provider_.get());

    context_state_->InitializeGL(
        GpuPreferences(), base::MakeRefCounted<gles2::FeatureInfo>(
                              GpuDriverBugWorkarounds(), GpuFeatureInfo()));

    GpuPreferences gpu_preferences = {};
    GpuDriverBugWorkarounds workarounds = {};
    context_state_->InitializeGrContext(gpu_preferences, workarounds, nullptr);

    memory_type_tracker_ = std::make_unique<MemoryTypeTracker>(nullptr);
    shared_image_representation_factory_ =
        std::make_unique<SharedImageRepresentationFactory>(
            &shared_image_manager_, nullptr);
    backing_factory_ =
        std::make_unique<ExternalVkImageBackingFactory>(context_state_);
#endif
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
  std::unique_ptr<ExternalVkImageBackingFactory> backing_factory_;
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

    // Create a Dawn Vulkan device
    dawn_instance_.DiscoverDefaultAdapters();

    std::vector<dawn::native::Adapter> adapters = dawn_instance_.GetAdapters();
    auto adapter_it = base::ranges::find(adapters, wgpu::BackendType::Vulkan,
                                         [](dawn::native::Adapter adapter) {
                                           wgpu::AdapterProperties properties;
                                           adapter.GetProperties(&properties);
                                           return properties.backendType;
                                         });
    ASSERT_NE(adapter_it, adapters.end());

    DawnProcTable procs = dawn::native::GetProcs();
    dawnProcSetProcs(&procs);

    dawn::native::DawnDeviceDescriptor device_descriptor;
    // We need to request internal usage to be able to do operations with
    // internal methods that would need specific usages.
    device_descriptor.requiredFeatures.push_back("dawn-internal-usages");

    dawn_device_ =
        wgpu::Device::Acquire(adapter_it->CreateDevice(&device_descriptor));
    DCHECK(dawn_device_) << "Failed to create Dawn device";
  }

  void TearDown() override {
    dawn_device_ = wgpu::Device();
    dawnProcSetProcs(nullptr);
  }

 protected:
  dawn::native::Instance dawn_instance_;
  wgpu::Device dawn_device_;
};

TEST_F(ExternalVkImageBackingFactoryDawnTest, DawnWrite_SkiaVulkanRead) {
  // Create a backing using mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size size(4, 4);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const uint32_t usage =
      SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_WEBGPU;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage,
      /*is_thread_safe=*/false);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  {
    // Create a Dawn representation to clear the texture contents to a green.
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(
            mailbox, dawn_device_.Get(), WGPUBackendType_Vulkan, {});
    ASSERT_TRUE(dawn_representation);

    auto dawn_scoped_access = dawn_representation->BeginScopedAccess(
        WGPUTextureUsage_RenderAttachment,
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
    auto skia_representation =
        shared_image_representation_factory_->ProduceSkia(mailbox,
                                                          context_state_.get());

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
    auto sk_image = SkImage::MakeFromTexture(
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

    if (auto end_state = skia_scoped_access->TakeEndState()) {
      gr_context()->setBackendTextureState(backend_texture, *end_state);
    }

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
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size size(4, 4);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const uint32_t usage =
      SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_WEBGPU;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage,
      /*is_thread_safe=*/false);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  {
    // Create a SkiaImageRepresentation
    auto skia_representation =
        shared_image_representation_factory_->ProduceSkia(mailbox,
                                                          context_state_.get());

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
    auto end_state = skia_scoped_access->TakeEndState();
    dest_surface->flush(flush_info, end_state.get());
    gr_context()->submit();
  }

  {
    // Create a Dawn representation
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(
            mailbox, dawn_device_.Get(), WGPUBackendType_Vulkan, {});
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
    bool done = false;
    dst_buffer.MapAsync(
        wgpu::MapMode::Read, 0, 256 * size.height(),
        [](WGPUBufferMapAsyncStatus status, void* userdata) {
          EXPECT_EQ(status, WGPUBufferMapAsyncStatus_Success);
          *static_cast<bool*>(userdata) = true;
        },
        &done);

    while (!done) {
      base::PlatformThread::Sleep(base::Microseconds(100));
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

class ExternalVkImageBackingFactoryWithFormatTest
    : public ExternalVkImageBackingFactoryTest,
      public testing::WithParamInterface<viz::SharedImageFormat> {
 public:
  viz::SharedImageFormat get_format() { return GetParam(); }
};

TEST_P(ExternalVkImageBackingFactoryWithFormatTest, Basic) {
  viz::SharedImageFormat format = get_format();
  auto mailbox = Mailbox::GenerateForSharedImage();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_GLES2;

  bool supported = backing_factory_->IsSupported(
      usage, format, size, /*thread_safe=*/false, gfx::EMPTY_BUFFER,
      GrContextType::kVulkan, {});
  ASSERT_TRUE(supported);

  // Verify backing can be created.
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, gpu::kNullSurfaceHandle, size, color_space,
      surface_origin, alpha_type, usage, /*is_thread_safe=*/false);
  ASSERT_TRUE(backing);

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());
  EXPECT_TRUE(shared_image);

  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
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

    auto* surface = scoped_write_access->surface(/*plane_index=*/0);
    ASSERT_TRUE(surface);
    EXPECT_EQ(size.width(), surface->width());
    EXPECT_EQ(size.height(), surface->height());

    // Handle end state and semaphores.
    auto end_state = scoped_write_access->TakeEndState();
    if (!end_semaphores.empty() || end_state) {
      GrFlushInfo flush_info;
      if (!end_semaphores.empty()) {
        flush_info.fNumSemaphores = end_semaphores.size();
        flush_info.fSignalSemaphores = end_semaphores.data();
      }
      scoped_write_access->surface()->flush(flush_info, end_state.get());
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

    auto* promise_texture =
        scoped_read_access->promise_image_texture(/*plane_index=*/0);
    ASSERT_TRUE(promise_texture);
    GrBackendTexture backend_texture = promise_texture->backendTexture();
    EXPECT_TRUE(backend_texture.isValid());
    EXPECT_EQ(size.width(), backend_texture.width());
    EXPECT_EQ(size.height(), backend_texture.height());

    // Handle end state and semaphores.
    if (auto end_state = scoped_read_access->TakeEndState()) {
      gr_context()->setBackendTextureState(
          scoped_read_access->promise_image_texture(0)->backendTexture(),
          *end_state);
    }
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
        shared_image_representation_factory_->ProduceGLTexturePassthrough(
            mailbox);
    ASSERT_TRUE(gl_representation);
    auto scoped_access = gl_representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    ASSERT_TRUE(scoped_access);

    auto texture = gl_representation->GetTexturePassthrough(/*plane_index=*/0);
    ASSERT_TRUE(texture);
    EXPECT_NE(texture->service_id(), 0u);
  } else {
    auto gl_representation =
        shared_image_representation_factory_->ProduceGLTexture(mailbox);
    ASSERT_TRUE(gl_representation);
    auto scoped_access = gl_representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    ASSERT_TRUE(scoped_access);

    auto* texture = gl_representation->GetTexture(/*plane_index=*/0);
    ASSERT_TRUE(texture);
    EXPECT_NE(texture->service_id(), 0u);
  }
}

std::string TestParamToString(
    const testing::TestParamInfo<viz::SharedImageFormat>& param_info) {
  return param_info.param.ToTestParamString();
}

const auto kSharedImageFormats =
    ::testing::Values(viz::SinglePlaneFormat::kRGBA_8888,
                      viz::SinglePlaneFormat::kBGRA_8888,
                      viz::SinglePlaneFormat::kR_8,
                      viz::SinglePlaneFormat::kRG_88);

INSTANTIATE_TEST_SUITE_P(,
                         ExternalVkImageBackingFactoryWithFormatTest,
                         kSharedImageFormats,
                         TestParamToString);

}  // anonymous namespace
}  // namespace gpu
