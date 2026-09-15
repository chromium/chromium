// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/ahardwarebuffer_image_backing_factory.h"

#include <android/hardware_buffer.h>

#include <vector>

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_info.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_test_base.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/service/vulkan_context_provider.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_gl_framebuffer.h"

#if BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
#include <dawn/native/DawnNative.h>
#include <dawn/native/OpenGLBackend.h>
#include <dawn/webgpu_cpp.h>
#include <dawn/webgpu_cpp_print.h>
#endif

namespace gpu {
namespace {

class AHardwareBufferImageBackingFactoryTest
    : public SharedImageTestBase,
      public testing::WithParamInterface<GrContextType> {
 public:
  GrContextType GrContextType() const { return GetParam(); }

  bool IsGraphiteDawn() {
    return GrContextType() == GrContextType::kGraphiteDawn;
  }

  void SetUp() override {
    // AHardwareBuffer is only supported on ANDROID O+. Hence these tests
    // should not be run on android versions less that O.

    if (IsGraphiteDawn() && !IsGraphiteDawnSupported()) {
      GTEST_SKIP() << "Graphite/Dawn not supported";
    }

    ASSERT_NO_FATAL_FAILURE(InitializeContext(GrContextType()));

    backing_factory_ = std::make_unique<AHardwareBufferImageBackingFactory>(
        context_state_->feature_info(), gpu_preferences_,
        context_state_->vk_context_provider());
  }
};

class GlLegacySharedImage {
 public:
  GlLegacySharedImage(
      SharedImageBackingFactory* backing_factory,
      bool is_thread_safe,
      bool concurrent_read_write,
      SharedImageManager* shared_image_manager,
      MemoryTypeTracker* memory_type_tracker,
      SharedImageRepresentationFactory* shared_image_representation_factory);
  ~GlLegacySharedImage();

  gfx::Size size() { return size_; }
  Mailbox mailbox() { return mailbox_; }

 private:
  gfx::Size size_;
  Mailbox mailbox_;
  std::unique_ptr<SharedImageBacking> backing_;
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image_;
};

// Basic test to check creation and deletion of AHB backed shared image.
TEST_P(AHardwareBufferImageBackingFactoryTest, Basic) {
  GlLegacySharedImage gl_legacy_shared_image{
      backing_factory_.get(),          /*is_thread_safe=*/false,
      /*concurrent_read_write=*/false, &shared_image_manager_,
      &memory_type_tracker_,           &shared_image_representation_factory_};

  // Validate a SkiaImageRepresentation.
  auto skia_representation = shared_image_representation_factory_.ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());
  EXPECT_TRUE(skia_representation);
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
      scoped_write_access;
  scoped_write_access = skia_representation->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  EXPECT_TRUE(scoped_write_access);
  auto* surface = scoped_write_access->surface();
  EXPECT_TRUE(surface);
  EXPECT_EQ(gl_legacy_shared_image.size().width(), surface->width());
  EXPECT_EQ(gl_legacy_shared_image.size().height(), surface->height());
  EXPECT_EQ(0u, begin_semaphores.size());
  EXPECT_EQ(0u, end_semaphores.size());
  scoped_write_access.reset();

  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess> scoped_read_access;
  scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  EXPECT_TRUE(scoped_read_access);
  EXPECT_EQ(0u, begin_semaphores.size());
  EXPECT_EQ(0u, end_semaphores.size());
  if (IsGraphiteDawn()) {
    auto graphite_texture = scoped_read_access->graphite_texture();
    EXPECT_TRUE(graphite_texture.isValid());
    EXPECT_EQ(gl_legacy_shared_image.size(),
              gfx::SkISizeToSize(graphite_texture.dimensions()));
  } else {
    auto* promise_texture = scoped_read_access->promise_image_texture();
    ASSERT_TRUE(promise_texture);
    GrBackendTexture backend_texture = promise_texture->backendTexture();
    EXPECT_TRUE(backend_texture.isValid());
    EXPECT_EQ(gl_legacy_shared_image.size().width(), backend_texture.width());
    EXPECT_EQ(gl_legacy_shared_image.size().height(), backend_texture.height());
  }

  scoped_read_access.reset();
  skia_representation.reset();
}

// Test to check interaction between GL and skia representations.
// We write to a GL texture using gl representation and then read from skia
// representation.
TEST_P(AHardwareBufferImageBackingFactoryTest, GLWriteSkiaRead) {
  // Create a backing using mailbox.
  auto mailbox = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(1, 1);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SharedImageUsageSet usage =
      SHARED_IMAGE_USAGE_GLES2_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox,
      {format, size, color_space, surface_origin, alpha_type, usage,
       "TestLabel"},
      surface_handle, /*is_thread_safe=*/false);
  EXPECT_TRUE(backing);

  GLenum expected_target = GL_TEXTURE_2D;
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);

  // Create a GLTextureImageRepresentation.
  auto gl_representation =
      shared_image_representation_factory_.ProduceGLTexture(mailbox);
  EXPECT_TRUE(gl_representation);
  EXPECT_EQ(expected_target, gl_representation->GetTexture()->target());

  // Create an FBO.
  gl::GLApi* api = gl::g_current_gl_context;
  gl::ScopedGLFramebuffer fbo = gl::CreateScopedGLFramebuffer(api);
  api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, fbo.get());

  // Attach the texture to FBO.
  api->glFramebufferTexture2DEXTFn(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      gl_representation->GetTexture()->target(),
      gl_representation->GetTexture()->service_id(), 0);

  // Set the clear color to red.
  api->glClearColorFn(1.0f, 0.0f, 0.0f, 1.0f);
  api->glClearFn(GL_COLOR_BUFFER_BIT);

  api->glFinishFn();

  // Mark the representation as cleared.
  gl_representation->SetCleared();
  gl_representation.reset();

  VerifyPixelsWithReadback(mailbox, AllocateRedBitmaps(format, size));

  factory_ref.reset();
}

// Test ProduceDawn via OpenGLES Compat backend
#if BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
TEST_P(AHardwareBufferImageBackingFactoryTest, ProduceDawnOpenGLES) {
  // Create a backing using mailbox.
  auto mailbox = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(1, 1);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SharedImageUsageSet usage = SHARED_IMAGE_USAGE_WEBGPU_WRITE |
                                   SHARED_IMAGE_USAGE_DISPLAY_READ |
                                   SHARED_IMAGE_USAGE_SCANOUT;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox,
      {format, size, color_space, surface_origin, alpha_type, usage,
       "TestLabel"},
      surface_handle, /*is_thread_safe=*/false);
  EXPECT_TRUE(backing);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);

  dawn::native::Instance instance;

  wgpu::RequestAdapterOptions adapter_options;
  adapter_options.backendType = wgpu::BackendType::OpenGLES;
  adapter_options.featureLevel = wgpu::FeatureLevel::Compatibility;

  dawn::native::opengl::RequestAdapterOptionsGetGLProc
      adapter_options_get_gl_proc = {};
  adapter_options_get_gl_proc.getProc = gl::GetGLProcAddress;
  gl::GLDisplayEGL* gl_display = gl::GLSurfaceEGL::GetGLDisplayEGL();
  if (gl_display) {
    adapter_options_get_gl_proc.display = gl_display->GetDisplay();
  } else {
    adapter_options_get_gl_proc.display = EGL_NO_DISPLAY;
  }
  adapter_options.nextInChain = &adapter_options_get_gl_proc;

  std::vector<dawn::native::Adapter> adapters =
      instance.EnumerateAdapters(&adapter_options);
  if (adapters.empty()) {
    GTEST_SKIP() << "Dawn OpenGLES backend not available";
  }
  wgpu::Adapter adapter = wgpu::Adapter(adapters[0].Get());

  std::array<wgpu::FeatureName, 3> required_features = {
      // We need to request internal usage to be able to do operations with
      // internal methods that would need specific usages.
      wgpu::FeatureName::DawnInternalUsages,

      // AHardwareBuffers are imported directly into Dawn and SyncFDs are used
      // to synchronize them.
      wgpu::FeatureName::SharedTextureMemoryAHardwareBuffer,
      wgpu::FeatureName::SharedFenceSyncFD};
  for (const wgpu::FeatureName& required_feature : required_features) {
    if (!adapter.HasFeature(required_feature)) {
      GTEST_SKIP() << "Required Dawn feature " << required_feature
                   << " is not available.";
    }
  }

  wgpu::DeviceDescriptor device_descriptor;
  device_descriptor.requiredFeatureCount = required_features.size();
  device_descriptor.requiredFeatures = required_features.data();

  wgpu::Device device = adapter.CreateDevice(&device_descriptor);

  auto dawn_representation = shared_image_representation_factory_.ProduceDawn(
      mailbox, device, wgpu::BackendType::OpenGLES, {}, context_state_);
  EXPECT_TRUE(dawn_representation);

  wgpu::Color color{255, 0, 0, 255};
  {
    auto scoped_access = dawn_representation->BeginScopedAccess(
        wgpu::TextureUsage::RenderAttachment,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(scoped_access);

    wgpu::Texture texture(scoped_access->texture());

    wgpu::RenderPassColorAttachment color_desc;
    color_desc.view = texture.CreateView();
    color_desc.resolveTarget = nullptr;
    color_desc.loadOp = wgpu::LoadOp::Clear;
    color_desc.storeOp = wgpu::StoreOp::Store;
    color_desc.clearValue = color;

    wgpu::RenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &color_desc;
    renderPassDesc.depthStencilAttachment = nullptr;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetQueue();
    queue.Submit(1, &commands);
  }
  VerifyPixelsWithReadback(mailbox, AllocateRedBitmaps(format, size));

  factory_ref.reset();
}
#endif  // BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)

TEST_P(AHardwareBufferImageBackingFactoryTest, InitialData) {
  auto mailbox = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(4, 4);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SharedImageUsageSet usage = SHARED_IMAGE_USAGE_DISPLAY_READ;

  auto image_info = SkImageInfo::Make(
      gfx::SizeToSkISize(size), viz::ToClosestSkColorType(format), alpha_type);
  SkBitmap expected_bitmap;
  expected_bitmap.allocPixels(image_info);

  base::span<uint8_t> UNSAFE_TODO(pixel_span(
      static_cast<uint8_t*>(expected_bitmap.pixmap().writable_addr()),
      expected_bitmap.computeByteSize()));
  for (size_t i = 0; i < pixel_span.size(); i++) {
    pixel_span[i] = static_cast<uint8_t>(i);
  }

  // Create a SharedImage whose contents will be read out by Skia.
  auto backing = backing_factory_->CreateSharedImage(
      mailbox,
      {format, size, color_space, surface_origin, alpha_type, usage,
       "TestLabel"},
      /*is_thread_safe=*/false, pixel_span);
  EXPECT_TRUE(backing);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);

  VerifyPixelsWithReadback(mailbox, {expected_bitmap});

  factory_ref.reset();
}

// Test to check invalid format support.
TEST_P(AHardwareBufferImageBackingFactoryTest, InvalidFormat) {
  auto mailbox = Mailbox::Generate();
  auto format = viz::MultiPlaneFormat::kP010;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  // NOTE: The specific usage here doesn't matter - the only important thing is
  // that it be a usage that the factory supports so that the test is exercising
  // the fact that the passed-in *format* is not supported.
  gpu::SharedImageUsageSet usage = SHARED_IMAGE_USAGE_GLES2_READ;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox,
      {format, size, color_space, surface_origin, alpha_type, usage,
       "TestLabel"},
      surface_handle, /*is_thread_safe=*/false);
  EXPECT_FALSE(backing);
}

TEST_P(AHardwareBufferImageBackingFactoryTest,
       CanCreateMultiplanarBackingWhenPassedGMBHandle) {
  auto mailbox = Mailbox::Generate();
  auto format = viz::MultiPlaneFormat::kNV12;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SharedImageUsageSet usage =
      SHARED_IMAGE_USAGE_CPU_READ | SHARED_IMAGE_USAGE_CPU_WRITE_ONLY;

  AHardwareBuffer* buffer = nullptr;
  AHardwareBuffer_Desc hwb_desc = {};
  hwb_desc.width = size.width();
  hwb_desc.height = size.height();
  hwb_desc.layers = 1;
  hwb_desc.format = AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420;
  hwb_desc.usage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN;
  AHardwareBuffer_allocate(&hwb_desc, &buffer);
  ASSERT_NE(buffer, nullptr);

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::ANDROID_HARDWARE_BUFFER;
  handle.android_hardware_buffer =
      base::android::ScopedHardwareBufferHandle::Adopt(buffer);

  auto backing = backing_factory_->CreateSharedImage(
      mailbox,
      {format, size, color_space, surface_origin, alpha_type, usage,
       "TestLabel"},
      /*is_thread_safe=*/false, std::move(handle));
  EXPECT_TRUE(backing);
}

TEST_P(AHardwareBufferImageBackingFactoryTest, MultiplanarAHB) {
  if (IsGraphiteDawn()) {
    GTEST_SKIP() << "Graphite/Dawn not supported";
  }

  auto mailbox = Mailbox::Generate();
  auto format = viz::MultiPlaneFormat::kNV12;
  format.SetPrefersExternalSampler();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SharedImageUsageSet usage = SHARED_IMAGE_USAGE_DISPLAY_READ;

  AHardwareBuffer* buffer = nullptr;
  AHardwareBuffer_Desc hwb_desc = {};
  hwb_desc.width = size.width();
  hwb_desc.height = size.height();
  hwb_desc.layers = 1;
  hwb_desc.format = AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420;
  hwb_desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
  AHardwareBuffer_allocate(&hwb_desc, &buffer);
  ASSERT_NE(buffer, nullptr);

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::ANDROID_HARDWARE_BUFFER;
  handle.android_hardware_buffer =
      base::android::ScopedHardwareBufferHandle::Adopt(buffer);

  auto backing = backing_factory_->CreateSharedImage(
      mailbox,
      {format, size, color_space, surface_origin, alpha_type, usage,
       "TestLabel"},
      /*is_thread_safe=*/false, std::move(handle));
  EXPECT_TRUE(backing);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);

  auto skia_representation = shared_image_representation_factory_.ProduceSkia(
      mailbox, context_state_.get());
  EXPECT_TRUE(skia_representation);
}

// Test to check invalid size support.
TEST_P(AHardwareBufferImageBackingFactoryTest, InvalidSize) {
  auto mailbox = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(0, 0);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  // NOTE: The specific usage here doesn't matter - the only important thing is
  // that it be a usage that the factory supports so that the test is exercising
  // the fact that the passed-in *size* is not supported.
  gpu::SharedImageUsageSet usage = SHARED_IMAGE_USAGE_GLES2_READ;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox,
      {format, size, color_space, surface_origin, alpha_type, usage,
       "TestLabel"},
      surface_handle, /*is_thread_safe=*/false);
  EXPECT_FALSE(backing);

  size = gfx::Size(INT_MAX, INT_MAX);
  backing = backing_factory_->CreateSharedImage(
      mailbox,
      {format, size, color_space, surface_origin, alpha_type, usage,
       "TestLabel"},
      surface_handle, /*is_thread_safe=*/false);
  EXPECT_FALSE(backing);
}

TEST_P(AHardwareBufferImageBackingFactoryTest, EstimatedSize) {
  auto mailbox = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  // NOTE: The specific usage does not matter here as long as it is supported by
  // the factory.
  gpu::SharedImageUsageSet usage = SHARED_IMAGE_USAGE_GLES2_READ;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox,
      {format, size, color_space, surface_origin, alpha_type, usage,
       "TestLabel"},
      surface_handle, /*is_thread_safe=*/false);
  EXPECT_TRUE(backing);

  size_t backing_estimated_size = backing->GetEstimatedSize();
  EXPECT_GT(backing_estimated_size, 0u);

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);
  EXPECT_EQ(backing_estimated_size, memory_type_tracker_.GetMemRepresented());

  shared_image.reset();
}

// Test to check that only one context can write at a time
TEST_P(AHardwareBufferImageBackingFactoryTest, OnlyOneWriter) {
  GlLegacySharedImage gl_legacy_shared_image{
      backing_factory_.get(),          /*is_thread_safe=*/true,
      /*concurrent_read_write=*/false, &shared_image_manager_,
      &memory_type_tracker_,           &shared_image_representation_factory_};

  auto skia_representation = shared_image_representation_factory_.ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
      scoped_write_access;
  scoped_write_access = skia_representation->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  EXPECT_TRUE(scoped_write_access);
  EXPECT_EQ(0u, begin_semaphores.size());
  EXPECT_EQ(0u, end_semaphores.size());

  auto skia_representation2 = shared_image_representation_factory_.ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());
  std::vector<GrBackendSemaphore> begin_semaphores2;
  std::vector<GrBackendSemaphore> end_semaphores2;
  std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
      scoped_write_access2;
  scoped_write_access2 = skia_representation2->BeginScopedWriteAccess(
      &begin_semaphores2, &end_semaphores2,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  EXPECT_FALSE(scoped_write_access2);
  EXPECT_EQ(0u, begin_semaphores2.size());
  EXPECT_EQ(0u, end_semaphores2.size());
  skia_representation2.reset();

  scoped_write_access.reset();
  skia_representation.reset();
}

// Test to check that multiple readers are allowed
TEST_P(AHardwareBufferImageBackingFactoryTest, CanHaveMultipleReaders) {
  GlLegacySharedImage gl_legacy_shared_image{
      backing_factory_.get(),          /*is_thread_safe=*/true,
      /*concurrent_read_write=*/false, &shared_image_manager_,
      &memory_type_tracker_,           &shared_image_representation_factory_};

  auto skia_representation = shared_image_representation_factory_.ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());
  auto skia_representation2 = shared_image_representation_factory_.ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess> scoped_read_access;
  scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  EXPECT_TRUE(scoped_read_access);
  EXPECT_EQ(0u, begin_semaphores.size());
  EXPECT_EQ(0u, end_semaphores.size());

  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
      scoped_read_access2;
  scoped_read_access2 = skia_representation2->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  EXPECT_TRUE(scoped_read_access2);
  EXPECT_EQ(0u, begin_semaphores.size());
  EXPECT_EQ(0u, end_semaphores.size());

  scoped_read_access2.reset();
  skia_representation2.reset();
  scoped_read_access.reset();
  skia_representation.reset();
}

// Test to check that a context cannot write while another context is reading
TEST_P(AHardwareBufferImageBackingFactoryTest, CannotWriteWhileReading) {
  GlLegacySharedImage gl_legacy_shared_image{
      backing_factory_.get(),          /*is_thread_safe=*/true,
      /*concurrent_read_write=*/false, &shared_image_manager_,
      &memory_type_tracker_,           &shared_image_representation_factory_};

  auto skia_representation = shared_image_representation_factory_.ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;

  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess> scoped_read_access;
  scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  EXPECT_TRUE(scoped_read_access);
  EXPECT_EQ(0u, begin_semaphores.size());
  EXPECT_EQ(0u, end_semaphores.size());

  auto skia_representation2 = shared_image_representation_factory_.ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());

  std::vector<GrBackendSemaphore> begin_semaphores2;
  std::vector<GrBackendSemaphore> end_semaphores2;

  std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
      scoped_write_access;
  scoped_write_access = skia_representation2->BeginScopedWriteAccess(
      &begin_semaphores2, &end_semaphores2,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  EXPECT_FALSE(scoped_write_access);
  EXPECT_EQ(0u, begin_semaphores2.size());
  EXPECT_EQ(0u, end_semaphores2.size());
  skia_representation2.reset();

  scoped_read_access.reset();
  skia_representation.reset();
}

// Test to check that a context cannot read while another context is writing
TEST_P(AHardwareBufferImageBackingFactoryTest, CannotReadWhileWriting) {
  GlLegacySharedImage gl_legacy_shared_image{
      backing_factory_.get(),          /*is_thread_safe=*/true,
      /*concurrent_read_write=*/false, &shared_image_manager_,
      &memory_type_tracker_,           &shared_image_representation_factory_};

  auto skia_representation = shared_image_representation_factory_.ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
      scoped_write_access;
  scoped_write_access = skia_representation->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  EXPECT_TRUE(scoped_write_access);
  EXPECT_EQ(0u, begin_semaphores.size());
  EXPECT_EQ(0u, end_semaphores.size());

  auto skia_representation2 = shared_image_representation_factory_.ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());
  std::vector<GrBackendSemaphore> begin_semaphores2;
  std::vector<GrBackendSemaphore> end_semaphores2;
  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess> scoped_read_access;
  scoped_read_access = skia_representation2->BeginScopedReadAccess(
      &begin_semaphores2, &end_semaphores2);
  EXPECT_FALSE(scoped_read_access);
  EXPECT_EQ(0u, begin_semaphores2.size());
  EXPECT_EQ(0u, end_semaphores2.size());
  skia_representation2.reset();

  scoped_write_access.reset();
  skia_representation.reset();
}

TEST_P(AHardwareBufferImageBackingFactoryTest, ConcurrentReadWrite) {
  GlLegacySharedImage gl_legacy_shared_image{
      backing_factory_.get(),         /*is_thread_safe=*/true,
      /*concurrent_read_write=*/true, &shared_image_manager_,
      &memory_type_tracker_,          &shared_image_representation_factory_};

  auto skia_representation = shared_image_representation_factory_.ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess> scoped_read_access;
  scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  EXPECT_TRUE(scoped_read_access);
  EXPECT_EQ(0u, begin_semaphores.size());
  EXPECT_EQ(0u, end_semaphores.size());

  auto skia_representation2 = shared_image_representation_factory_.ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());
  std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
      scoped_write_access;
  scoped_write_access = skia_representation2->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  EXPECT_TRUE(scoped_write_access);
  EXPECT_EQ(0u, begin_semaphores.size());
  EXPECT_EQ(0u, end_semaphores.size());
}

GlLegacySharedImage::GlLegacySharedImage(
    SharedImageBackingFactory* backing_factory,
    bool is_thread_safe,
    bool concurrent_read_write,
    SharedImageManager* shared_image_manager,
    MemoryTypeTracker* memory_type_tracker,
    SharedImageRepresentationFactory* shared_image_representation_factory)
    : size_(256, 256) {
  mailbox_ = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  GLenum expected_target = GL_TEXTURE_2D;

  // Provide usage settings to model an SI that is written via raster, read
  // via GL (e.g., for canvas import into WebGL), and used as an overlay. Add
  // SHARED_IMAGE_USAGE_DISPLAY_READ if modeling the display compositor being on
  // the same thread as raster.
  SharedImageUsageSet usage = {SHARED_IMAGE_USAGE_GLES2_READ,
                               SHARED_IMAGE_USAGE_RASTER_WRITE,
                               SHARED_IMAGE_USAGE_SCANOUT};
  if (!is_thread_safe) {
    usage |= SharedImageUsageSet({SHARED_IMAGE_USAGE_DISPLAY_READ});
  }
  if (concurrent_read_write) {
    usage |= SharedImageUsageSet({SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE});
  }
  backing_ = backing_factory->CreateSharedImage(
      mailbox_,
      {format, size_, color_space, surface_origin, alpha_type, usage,
       "TestLabel"},
      surface_handle, is_thread_safe);
  EXPECT_TRUE(backing_);

  // Check clearing.
  if (!backing_->IsCleared()) {
    backing_->SetCleared();
    EXPECT_TRUE(backing_->IsCleared());
  }

  shared_image_ =
      shared_image_manager->Register(std::move(backing_), memory_type_tracker);

  auto gl_representation =
      shared_image_representation_factory->ProduceGLTexture(mailbox_);

  EXPECT_TRUE(gl_representation);
  EXPECT_TRUE(gl_representation->GetTexture()->service_id());
  EXPECT_EQ(expected_target, gl_representation->GetTexture()->target());
  EXPECT_EQ(size_, gl_representation->size());
  EXPECT_EQ(format, gl_representation->format());
  EXPECT_EQ(color_space, gl_representation->color_space());
  EXPECT_EQ(usage, gl_representation->usage());
  gl_representation.reset();
}

GlLegacySharedImage::~GlLegacySharedImage() {
  shared_image_.reset();
}

TEST_P(AHardwareBufferImageBackingFactoryTest, Overlay) {
  GlLegacySharedImage gl_legacy_shared_image{
      backing_factory_.get(),          /*is_thread_safe=*/false,
      /*concurrent_read_write=*/false, &shared_image_manager_,
      &memory_type_tracker_,           &shared_image_representation_factory_};

  auto skia_representation = shared_image_representation_factory_.ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  auto scoped_write_access = skia_representation->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  EXPECT_TRUE(scoped_write_access);
  EXPECT_EQ(0u, begin_semaphores.size());
  EXPECT_EQ(0u, end_semaphores.size());
  scoped_write_access.reset();

  auto overlay_representation =
      shared_image_representation_factory_.ProduceOverlay(
          gl_legacy_shared_image.mailbox());
  EXPECT_TRUE(overlay_representation);

  auto scoped_read_access = overlay_representation->BeginScopedReadAccess();
  EXPECT_TRUE(scoped_read_access);
  auto buffer = scoped_read_access->GetAHardwareBufferFenceSync();
  DCHECK(buffer);
  scoped_read_access.reset();
  skia_representation.reset();
}

INSTANTIATE_TEST_SUITE_P(,
                         AHardwareBufferImageBackingFactoryTest,
                         testing::Values(GrContextType::kGL,
                                         GrContextType::kGraphiteDawn),
                         testing::PrintToStringParamName());

// Usage bits of a real NV12 buffer that is read back to shared memory: it is
// sampled by the GPU and read by the CPU. Allocating with GPU usage matters,
// since a CPU-only Y8Cb8Cr8_420 allocation is free to come back as a planar
// (YV12) layout, which this code path does not support.
constexpr uint64_t kNV12ReadbackUsage =
    AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
    AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN;

// Allocates an NV12 AHardwareBuffer of `size`, or returns an empty handle if
// the platform refuses the allocation (e.g. for odd dimensions).
base::android::ScopedHardwareBufferHandle AllocateNV12Buffer(
    const gfx::Size& size,
    uint64_t usage = kNV12ReadbackUsage) {
  AHardwareBuffer_Desc desc = {
      .width = static_cast<uint32_t>(size.width()),
      .height = static_cast<uint32_t>(size.height()),
      .layers = 1,
      .format = AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420,
      .usage = usage,
  };
  AHardwareBuffer* buffer = nullptr;
  AHardwareBuffer_allocate(&desc, &buffer);
  if (!buffer) {
    return base::android::ScopedHardwareBufferHandle();
  }
  return base::android::ScopedHardwareBufferHandle::Adopt(buffer);
}

// Whether the platform allocator hands back the interleaved NV12 plane layout
// that CopyNativeBufferToSharedMemoryAsync() supports. Gralloc is free to pick
// a planar layout instead, in which case the copy legitimately fails and the
// tests below that expect success have nothing to assert.
bool SupportsNV12PlaneLayout(AHardwareBuffer* buffer) {
  AHardwareBuffer_Planes planes;
  if (AHardwareBuffer_lockPlanes(buffer, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN,
                                 /*fence=*/-1, nullptr, &planes) != 0) {
    return false;
  }
  const bool supported =
      planes.planeCount == 3 && planes.planes[0].pixelStride == 1 &&
      planes.planes[1].pixelStride == 2 && planes.planes[2].pixelStride == 2 &&
      planes.planes[1].rowStride == planes.planes[2].rowStride &&
      (static_cast<uint8_t*>(planes.planes[2].data) -
       static_cast<uint8_t*>(planes.planes[1].data)) == 1;
  AHardwareBuffer_unlock(buffer, nullptr);
  return supported;
}

// Writes a distinct value into every byte the copy is supposed to read, and
// returns those bytes in the order the destination is supposed to hold them:
// `height` rows of `width` Y bytes, followed by `height / 2` rows of `width`
// interleaved UV bytes. Returns an empty vector if the buffer can't be locked.
std::vector<uint8_t> FillNV12Buffer(AHardwareBuffer* buffer,
                                    const gfx::Size& size) {
  AHardwareBuffer_Planes planes;
  if (AHardwareBuffer_lockPlanes(buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,
                                 /*fence=*/-1, nullptr, &planes) != 0) {
    return {};
  }

  const size_t width = base::checked_cast<size_t>(size.width());
  const size_t height = base::checked_cast<size_t>(size.height());
  std::vector<uint8_t> expected;
  auto fill_plane = [&](const AHardwareBuffer_Plane& src, size_t rows,
                        uint8_t seed) {
    const size_t row_stride = src.rowStride;
    // SAFETY: A locked plane holds `rows` rows at `rowStride` byte intervals,
    // the last of which is at least `width` bytes long.
    base::span<uint8_t> plane = UNSAFE_BUFFERS(base::span(
        static_cast<uint8_t*>(src.data), (rows - 1) * row_stride + width));
    for (size_t row = 0; row < rows; ++row) {
      base::span<uint8_t> src_row = plane.subspan(row * row_stride, width);
      for (size_t col = 0; col < width; ++col) {
        src_row[col] = static_cast<uint8_t>(seed + row * 7 + col);
        expected.push_back(src_row[col]);
      }
    }
  };
  fill_plane(planes.planes[0], height, /*seed=*/0);
  fill_plane(planes.planes[1], height / 2, /*seed=*/128);

  AHardwareBuffer_unlock(buffer, nullptr);
  return expected;
}

// Regression test for the out-of-bounds write reachable from a compromised
// renderer via the CopyNativeGmbToSharedMemoryAsync IPC. The destination size
// used to be computed as `width * height * 3 / 2`, which rounds *down* for odd
// dimensions, while libyuv::NV12Copy() writes ceil-rounded chroma planes. A
// buffer with odd dimensions must be rejected outright.
TEST(AHardwareBufferImageBackingFactoryCopyTest, RejectsOddDimensions) {
  constexpr gfx::Size kOddSize(65, 65);
  auto ahb_handle = AllocateNV12Buffer(kOddSize);
  if (!ahb_handle.is_valid()) {
    GTEST_SKIP() << "Odd-sized NV12 AHardwareBuffer allocation unsupported";
  }

  // This is the size that the old (truncating) check would have accepted; it is
  // smaller than what NV12Copy() would write, so accepting it overflows.
  const size_t undersized =
      static_cast<size_t>(kOddSize.width()) * kOddSize.height() * 3 / 2;
  ASSERT_LT(undersized, viz::SharedMemorySizeForSharedImageFormat(
                            viz::MultiPlaneFormat::kNV12, kOddSize)
                            .value());
  auto region = base::UnsafeSharedMemoryRegion::Create(undersized);
  ASSERT_TRUE(region.IsValid());

  EXPECT_FALSE(
      AHardwareBufferImageBackingFactory::CopyNativeBufferToSharedMemoryAsync(
          gfx::GpuMemoryBufferHandle(std::move(ahb_handle)),
          std::move(region)));
}

// A destination smaller than the full NV12 image must be rejected.
TEST(AHardwareBufferImageBackingFactoryCopyTest, RejectsUndersizedDestination) {
  constexpr gfx::Size kSize(64, 64);
  auto ahb_handle = AllocateNV12Buffer(kSize);
  if (!ahb_handle.is_valid()) {
    GTEST_SKIP() << "NV12 AHardwareBuffer allocation unsupported";
  }
  const size_t required = viz::SharedMemorySizeForSharedImageFormat(
                              viz::MultiPlaneFormat::kNV12, kSize)
                              .value();
  auto region = base::UnsafeSharedMemoryRegion::Create(required - 1);
  ASSERT_TRUE(region.IsValid());

  EXPECT_FALSE(
      AHardwareBufferImageBackingFactory::CopyNativeBufferToSharedMemoryAsync(
          gfx::GpuMemoryBufferHandle(std::move(ahb_handle)),
          std::move(region)));
}

// A correctly sized destination must receive the whole image in the layout the
// client expects: a tightly packed Y plane followed by a tightly packed UV
// plane, both with a row size of `width` bytes.
TEST(AHardwareBufferImageBackingFactoryCopyTest, CopiesBothPlanes) {
  constexpr gfx::Size kSize(64, 64);
  auto ahb_handle = AllocateNV12Buffer(
      kSize, kNV12ReadbackUsage | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN);
  if (!ahb_handle.is_valid()) {
    GTEST_SKIP() << "NV12 AHardwareBuffer allocation unsupported";
  }
  if (!SupportsNV12PlaneLayout(ahb_handle.get())) {
    GTEST_SKIP() << "Platform does not provide an interleaved NV12 layout";
  }

  const std::vector<uint8_t> expected = FillNV12Buffer(ahb_handle.get(), kSize);
  ASSERT_EQ(expected.size(), viz::SharedMemorySizeForSharedImageFormat(
                                 viz::MultiPlaneFormat::kNV12, kSize)
                                 .value());

  auto region = base::UnsafeSharedMemoryRegion::Create(expected.size());
  ASSERT_TRUE(region.IsValid());
  base::UnsafeSharedMemoryRegion region_for_reading = region.Duplicate();
  ASSERT_TRUE(region_for_reading.IsValid());

  ASSERT_TRUE(
      AHardwareBufferImageBackingFactory::CopyNativeBufferToSharedMemoryAsync(
          gfx::GpuMemoryBufferHandle(std::move(ahb_handle)),
          std::move(region)));

  base::WritableSharedMemoryMapping mapping = region_for_reading.Map();
  ASSERT_TRUE(mapping.IsValid());
  EXPECT_EQ(mapping.GetMemoryAsSpan<uint8_t>().first(expected.size()),
            base::span(expected));
}

}  // anonymous namespace
}  // namespace gpu
