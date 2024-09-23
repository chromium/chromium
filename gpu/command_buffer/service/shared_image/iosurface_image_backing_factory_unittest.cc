// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/shared_image/iosurface_image_backing_factory.h"

#include <dawn/native/DawnNative.h>
#include <dawn/webgpu_cpp.h>

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/dawn_image_representation_unittest_common.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_test_base.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/progress_reporter.h"

#if BUILDFLAG(SKIA_USE_DAWN)
#include "gpu/command_buffer/service/dawn_context_provider.h"
#endif

using testing::AtLeast;

namespace gpu {

namespace {

class IOSurfaceImageBackingFactoryTest : public SharedImageTestBase {
 public:
  void SetUp() override {
    ASSERT_TRUE(gpu_preferences_.use_passthrough_cmd_decoder);

    ASSERT_NO_FATAL_FAILURE(InitializeContext(GrContextType::kGL));

    backing_factory_ = std::make_unique<IOSurfaceImageBackingFactory>(
        context_state_->gr_context_type(), context_state_->GetMaxTextureSize(),
        context_state_->feature_info(), /*progress_reporter=*/nullptr,
#if BUILDFLAG(IS_MAC)
        GetTextureTargetForIOSurfaces());
#else
        GL_TEXTURE_2D);
#endif
  }

 protected:
  void CheckSkiaPixels(const Mailbox& mailbox,
                       const gfx::Size& size,
                       const std::vector<uint8_t> expected_color) {
    auto skia_representation = shared_image_representation_factory_.ProduceSkia(
        mailbox, context_state_);
    ASSERT_NE(skia_representation, nullptr);

    std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
        scoped_read_access =
            skia_representation->BeginScopedReadAccess(nullptr, nullptr);
    EXPECT_TRUE(scoped_read_access);

    auto* promise_texture = scoped_read_access->promise_image_texture();
    GrBackendTexture backend_texture = promise_texture->backendTexture();

    EXPECT_TRUE(backend_texture.isValid());
    EXPECT_EQ(size.width(), backend_texture.width());
    EXPECT_EQ(size.height(), backend_texture.height());

    // Create an Sk Image from GrBackendTexture.
    auto sk_image = SkImages::BorrowTextureFrom(
        gr_context(), backend_texture, kTopLeft_GrSurfaceOrigin,
        kRGBA_8888_SkColorType, kOpaque_SkAlphaType, nullptr);

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
      EXPECT_EQ(pixel[0], expected_color[0]);
      EXPECT_EQ(pixel[1], expected_color[1]);
      EXPECT_EQ(pixel[2], expected_color[2]);
      EXPECT_EQ(pixel[3], expected_color[3]);
    }
  }
};

// Test to check interaction between Gl and skia GL representations.
// We write to a GL texture using gl representation and then read from skia
// representation.
TEST_F(IOSurfaceImageBackingFactoryTest, GL_SkiaGL) {
  // Create a backing using mailbox.
  auto mailbox = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(1, 1);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  SharedImageUsageSet usage = {SHARED_IMAGE_USAGE_GLES2_WRITE,
                               SHARED_IMAGE_USAGE_DISPLAY_READ,
                               SHARED_IMAGE_USAGE_SCANOUT};
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, "TestLabel", /*is_thread_safe=*/false);
  EXPECT_TRUE(backing);
  backing->SetCleared();

  GLenum expected_target =
#if BUILDFLAG(IS_MAC)
      GetTextureTargetForIOSurfaces();
#else
      GL_TEXTURE_2D;
#endif
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);

  // Create a GLTextureImageRepresentation.
  {
    auto gl_representation =
        shared_image_representation_factory_.ProduceGLTexturePassthrough(
            mailbox);
    EXPECT_TRUE(gl_representation);
    EXPECT_EQ(expected_target,
              gl_representation->GetTexturePassthrough()->target());

    // Access the SharedImageRepresentationGLTexutre
    auto scoped_write_access = gl_representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);

    // Create an FBO.
    GLuint fbo = 0;
    gl::GLApi* api = gl::g_current_gl_context;
    api->glGenFramebuffersEXTFn(1, &fbo);
    api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, fbo);

    // Attach the texture to FBO.
    api->glFramebufferTexture2DEXTFn(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        gl_representation->GetTexturePassthrough()->target(),
        gl_representation->GetTexturePassthrough()->service_id(), 0);

    // Set the clear color to green.
    api->glClearColorFn(0.0f, 1.0f, 0.0f, 1.0f);
    api->glClearFn(GL_COLOR_BUFFER_BIT);
  }

  CheckSkiaPixels(mailbox, size, {0, 255, 0, 255});
  factory_ref.reset();
}

class IOSurfaceImageBackingFactoryDawnTest
    : public IOSurfaceImageBackingFactoryTest,
      public testing::WithParamInterface<wgpu::BackendType> {
 protected:
  void SetUp() override {
    IOSurfaceImageBackingFactoryTest::SetUp();

    wgpu::RequestAdapterOptions adapter_options;
    std::vector<const char*> adapter_enabled_toggles;

    if (backend_type() == wgpu::BackendType::Vulkan) {
      // Vulkan doesn't support IOSurface image backing, so we need
      // MultiPlanarFormatExtendedUsages to copy to/from multiplanar texture.
      // And this feature is currently experimental.
      adapter_enabled_toggles.push_back("allow_unsafe_apis");
    }

    adapter_options.backendType = backend_type();

    wgpu::DawnTogglesDescriptor adapter_toggles_desc;
    adapter_toggles_desc.enabledToggles = adapter_enabled_toggles.data();
    adapter_toggles_desc.enabledToggleCount = adapter_enabled_toggles.size();
    adapter_options.nextInChain = &adapter_toggles_desc;

    std::vector<dawn::native::Adapter> adapters =
        instance_.EnumerateAdapters(&adapter_options);
    EXPECT_FALSE(adapters.empty());
    adapter_ = wgpu::Adapter(adapters[0].Get());
  }

  wgpu::BackendType backend_type() { return GetParam(); }

  wgpu::Device CreateDevice() {
    std::vector<wgpu::FeatureName> features;

    if (adapter_.HasFeature(wgpu::FeatureName::DawnMultiPlanarFormats)) {
      features.push_back(wgpu::FeatureName::DawnMultiPlanarFormats);
    }
    if (adapter_.HasFeature(
            wgpu::FeatureName::MultiPlanarFormatExtendedUsages)) {
      features.push_back(wgpu::FeatureName::MultiPlanarFormatExtendedUsages);
    }
    if (adapter_.HasFeature(wgpu::FeatureName::MultiPlanarFormatP010)) {
      features.push_back(wgpu::FeatureName::MultiPlanarFormatP010);
    }

    if (adapter_.HasFeature(wgpu::FeatureName::SharedTextureMemoryIOSurface)) {
      CHECK(adapter_.HasFeature(wgpu::FeatureName::SharedFenceMTLSharedEvent));
      features.push_back(wgpu::FeatureName::SharedTextureMemoryIOSurface);
      features.push_back(wgpu::FeatureName::SharedFenceMTLSharedEvent);
    }

    // We need to request internal usage to be able to do operations with
    // internal methods that would need specific usages.
    features.push_back(wgpu::FeatureName::DawnInternalUsages);
    wgpu::DeviceDescriptor device_descriptor;
    device_descriptor.requiredFeatureCount = features.size();
    device_descriptor.requiredFeatures = features.data();

    wgpu::Device device = adapter_.CreateDevice(&device_descriptor);

    return device;
  }

  std::unique_ptr<SharedImageRepresentationFactoryRef> CreateSharedImage(
      const gfx::Size& size,
      SharedImageUsageSet usage) {
    // Create a backing using mailbox.
    auto mailbox = Mailbox::Generate();
    auto format = viz::SinglePlaneFormat::kRGBA_8888;
    auto color_space = gfx::ColorSpace::CreateSRGB();
    GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
    SkAlphaType alpha_type = kPremul_SkAlphaType;
    gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
    auto backing = backing_factory_->CreateSharedImage(
        mailbox, format, surface_handle, size, color_space, surface_origin,
        alpha_type, usage, "TestLabel", /*is_thread_safe=*/false);
    EXPECT_TRUE(backing);

    std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
        shared_image_manager_.Register(std::move(backing),
                                       &memory_type_tracker_);
    return factory_ref;
  }

  void ClearSharedImageWithDawn(wgpu::Device device,
                                const gpu::Mailbox& mailbox,
                                wgpu::Color color) {
    // Create a DawnImageRepresentation.
    auto dawn_representation = shared_image_representation_factory_.ProduceDawn(
        mailbox, device, backend_type(), {}, context_state_);
    EXPECT_TRUE(dawn_representation);

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

  auto CopySharedImageWithDawn(wgpu::Device device,
                               const gpu::Mailbox& src,
                               const gpu::Mailbox& dst) {
    auto src_rep = shared_image_representation_factory_.ProduceDawn(
        src, device, backend_type(), {}, context_state_);
    auto src_scoped_access = src_rep->BeginScopedAccess(
        wgpu::TextureUsage::CopySrc,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    wgpu::Texture src_texture(src_scoped_access->texture());

    auto dst_rep = shared_image_representation_factory_.ProduceDawn(
        dst, device, backend_type(), {}, context_state_);

    auto dst_scoped_access = dst_rep->BeginScopedAccess(
        wgpu::TextureUsage::CopyDst,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    wgpu::Texture dst_texture(dst_scoped_access->texture());

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ImageCopyTexture copy_src;
    copy_src.texture = src_texture;

    wgpu::ImageCopyTexture copy_dst;
    copy_dst.texture = dst_texture;

    wgpu::Extent3D copy_size;
    copy_size.width = src_rep->size().width();
    copy_size.height = src_rep->size().height();

    encoder.CopyTextureToTexture(&copy_src, &copy_dst, &copy_size);

    wgpu::CommandBuffer commands = encoder.Finish();
    wgpu::Queue queue = device.GetQueue();

    queue.Submit(1, &commands);

    return std::make_pair(std::move(src_rep), std::move(src_scoped_access));
  }

  static constexpr WGPUInstanceDescriptor instance_desc_ = {
      .features =
          {
              .timedWaitAnyEnable = true,
          },
  };
  dawn::native::Instance instance_ = dawn::native::Instance(&instance_desc_);
  wgpu::Adapter adapter_;
};

// Test to verify that different representations created via the same Device get
// different wgpu::Textures.
TEST_P(IOSurfaceImageBackingFactoryDawnTest,
       Dawn_MultipleRepresentationsFromSingleDevice) {
  wgpu::Device device = CreateDevice();

  gfx::Size size(1, 1);
  // TODO: crbug.com/349290188: This SCANOUT usage (and most of the others in
  // this test) are likely not needed. Try to remove them.
  SharedImageUsageSet usage = {SHARED_IMAGE_USAGE_WEBGPU_READ,
                               SHARED_IMAGE_USAGE_DISPLAY_READ,
                               SHARED_IMAGE_USAGE_SCANOUT};
  auto factory_ref = CreateSharedImage(size, usage);

  auto rep_0 = shared_image_representation_factory_.ProduceDawn(
      factory_ref->mailbox(), device, backend_type(), {}, context_state_);
  auto scoped_access_0 = rep_0->BeginScopedAccess(
      wgpu::TextureUsage::CopySrc,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);

  auto rep_1 = shared_image_representation_factory_.ProduceDawn(
      factory_ref->mailbox(), device, backend_type(), {}, context_state_);
  auto scoped_access_1 = rep_1->BeginScopedAccess(
      wgpu::TextureUsage::CopySrc,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);

  wgpu::Texture texture_0(scoped_access_0->texture());
  wgpu::Texture texture_1(scoped_access_1->texture());

  EXPECT_NE(texture_0.Get(), texture_1.Get());
}

// Test to verify handling of failure to begin access.
TEST_P(IOSurfaceImageBackingFactoryDawnTest, Dawn_FailureToBeginAccess) {
  wgpu::Device device = CreateDevice();

  gfx::Size size(1, 1);
  // It's necessary to add WEBGPU_WRITE access as the created SharedImage
  // will be uncleared and hence require lazy clearing on access.
  SharedImageUsageSet usage = {
      SHARED_IMAGE_USAGE_WEBGPU_READ, SHARED_IMAGE_USAGE_WEBGPU_WRITE,
      SHARED_IMAGE_USAGE_DISPLAY_READ, SHARED_IMAGE_USAGE_SCANOUT};
  auto factory_ref = CreateSharedImage(size, usage);

  auto rep = shared_image_representation_factory_.ProduceDawn(
      factory_ref->mailbox(), device, backend_type(), {}, context_state_);

  device.Destroy();

  auto scoped_access = rep->BeginScopedAccess(
      wgpu::TextureUsage::CopySrc,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);

  if (backend_type() == wgpu::BackendType::Metal) {
    // The BeginAccess() call should have returned an empty texture, in which
    // case SharedImageRepresentation will return null for the scoped access.
    EXPECT_FALSE(scoped_access);
  } else {
    // The BeginAccess() call should have created an error texture.
    EXPECT_TRUE(scoped_access);
    EXPECT_TRUE(scoped_access->texture());
    EXPECT_TRUE(
        dawn::native::CheckIsErrorForTesting(scoped_access->texture().Get()));
  }
}

// Test to check interaction between Dawn and skia GL representations.
TEST_P(IOSurfaceImageBackingFactoryDawnTest, Dawn_SkiaGL) {
  // Create a Dawn device
  wgpu::Device device = CreateDevice();
  ASSERT_NE(device, nullptr);

  gfx::Size size(1, 1);
  SharedImageUsageSet usage = {SHARED_IMAGE_USAGE_WEBGPU_WRITE,
                               SHARED_IMAGE_USAGE_DISPLAY_READ,
                               SHARED_IMAGE_USAGE_SCANOUT};
  auto factory_ref = CreateSharedImage(size, usage);

  // Clear the shared image to green using Dawn.
  ClearSharedImageWithDawn(device, factory_ref->mailbox(), {0, 255, 0, 255});

  CheckSkiaPixels(factory_ref->mailbox(), size, {0, 255, 0, 255});

  // Shut down Dawn
  device = {};

  factory_ref.reset();
}

// Test to check interaction between Dawn devices.
// Create 3 dawn devices, clear a shared image with devices[0], and then read it
// from devices[1] and devices[2].
TEST_P(IOSurfaceImageBackingFactoryDawnTest, Dawn_WriteReadReadOnThreeDevices) {
  // Create three Dawn devices
  auto device_0 = CreateDevice();
  ASSERT_NE(device_0, nullptr);
  auto device_1 = CreateDevice();
  ASSERT_NE(device_1, nullptr);
  auto device_2 = CreateDevice();
  ASSERT_NE(device_2, nullptr);

  gfx::Size size(256, 256);
  SharedImageUsageSet usage = {
      SHARED_IMAGE_USAGE_WEBGPU_READ, SHARED_IMAGE_USAGE_WEBGPU_WRITE,
      SHARED_IMAGE_USAGE_DISPLAY_READ, SHARED_IMAGE_USAGE_SCANOUT};
  auto factory_ref_0 = CreateSharedImage(size, usage);
  auto factory_ref_1 = CreateSharedImage(size, usage);
  auto factory_ref_2 = CreateSharedImage(size, usage);

  // Clear the shared image to green using Dawn.
  ClearSharedImageWithDawn(device_0, factory_ref_0->mailbox(),
                           {0, 255, 0, 255});

  auto [rep_1, access_1] = CopySharedImageWithDawn(
      device_1, factory_ref_0->mailbox(), factory_ref_1->mailbox());
  auto [rep_2, access_2] = CopySharedImageWithDawn(
      device_2, factory_ref_0->mailbox(), factory_ref_2->mailbox());

  CheckSkiaPixels(factory_ref_1->mailbox(), size, {0, 255, 0, 255});
  CheckSkiaPixels(factory_ref_2->mailbox(), size, {0, 255, 0, 255});

  // Release scoped accesses.
  access_2 = {};
  access_1 = {};
}

// 1. Draw a color to texture through GL
// 2. Do not call SetCleared so we can test Dawn Lazy clear
// 3. Begin render pass in Dawn, but do not do anything
// 4. Verify through CheckSkiaPixel that GL drawn color not seen
TEST_P(IOSurfaceImageBackingFactoryDawnTest, GL_Dawn_Skia_UnclearTexture) {
  gfx::Size size(1, 1);
  SharedImageUsageSet usage = {
      SHARED_IMAGE_USAGE_GLES2_WRITE, SHARED_IMAGE_USAGE_SCANOUT,
      SHARED_IMAGE_USAGE_WEBGPU_WRITE, SHARED_IMAGE_USAGE_DISPLAY_READ};
  auto factory_ref = CreateSharedImage(size, usage);

  {
    // Create a GLTextureImageRepresentation.
    auto gl_representation =
        shared_image_representation_factory_.ProduceGLTexturePassthrough(
            factory_ref->mailbox());
    GLenum expected_target =
#if BUILDFLAG(IS_MAC)
        GetTextureTargetForIOSurfaces();
#else
        GL_TEXTURE_2D;
#endif
    EXPECT_TRUE(gl_representation);
    EXPECT_EQ(expected_target,
              gl_representation->GetTexturePassthrough()->target());

    std::unique_ptr<GLTexturePassthroughImageRepresentation::ScopedAccess>
        gl_scoped_access = gl_representation->BeginScopedAccess(
            GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
            SharedImageRepresentation::AllowUnclearedAccess::kYes);
    EXPECT_TRUE(gl_scoped_access);

    // Create an FBO.
    GLuint fbo = 0;
    gl::GLApi* api = gl::g_current_gl_context;
    api->glGenFramebuffersEXTFn(1, &fbo);
    api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, fbo);

    // Attach the texture to FBO.
    api->glFramebufferTexture2DEXTFn(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        gl_representation->GetTexturePassthrough()->target(),
        gl_representation->GetTexturePassthrough()->service_id(), 0);

    // Set the clear color to green.
    api->glClearColorFn(0.0f, 1.0f, 0.0f, 1.0f);
    api->glClearFn(GL_COLOR_BUFFER_BIT);

    // Don't set cleared, we want to see if Dawn will lazy clear the texture
    EXPECT_FALSE(factory_ref->IsCleared());
  }

  // Create a Dawn device
  wgpu::Device device = CreateDevice();
  ASSERT_NE(device, nullptr);

  {
    auto dawn_representation = shared_image_representation_factory_.ProduceDawn(
        factory_ref->mailbox(), device, backend_type(), {}, context_state_);
    ASSERT_TRUE(dawn_representation);

    auto dawn_scoped_access = dawn_representation->BeginScopedAccess(
        wgpu::TextureUsage::RenderAttachment,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(dawn_scoped_access);

    wgpu::Texture texture(dawn_scoped_access->texture());
    wgpu::RenderPassColorAttachment color_desc;
    color_desc.view = texture.CreateView();
    color_desc.resolveTarget = nullptr;
    color_desc.loadOp = wgpu::LoadOp::Load;
    color_desc.storeOp = wgpu::StoreOp::Store;

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

  // Check skia pixels returns black since texture was lazy cleared in Dawn
  EXPECT_TRUE(factory_ref->IsCleared());
  CheckSkiaPixels(factory_ref->mailbox(), size, {0, 0, 0, 0});

  // Shut down Dawn
  device = wgpu::Device();

  factory_ref.reset();
}

// 1. Draw  a color to texture through Dawn
// 2. Set the renderpass storeOp = Discard
// 3. Texture in Dawn will stay as uninitialized
// 4. Expect skia to fail to access the texture because texture is not
// initialized
TEST_P(IOSurfaceImageBackingFactoryDawnTest, UnclearDawn_SkiaFails) {
  gfx::Size size(1, 1);
  SharedImageUsageSet usage = {SHARED_IMAGE_USAGE_SCANOUT,
                               SHARED_IMAGE_USAGE_WEBGPU_WRITE,
                               SHARED_IMAGE_USAGE_DISPLAY_READ};
  auto factory_ref = CreateSharedImage(size, usage);

  // Create dawn device
  wgpu::Device device = CreateDevice();
  ASSERT_NE(device, nullptr);

  {
    auto dawn_representation = shared_image_representation_factory_.ProduceDawn(
        factory_ref->mailbox(), device, backend_type(), {}, context_state_);
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
    color_desc.storeOp = wgpu::StoreOp::Discard;
    color_desc.clearValue = {0, 255, 0, 255};

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

  // Shut down Dawn
  device = wgpu::Device();

  EXPECT_FALSE(factory_ref->IsCleared());

  // Produce skia representation
  auto skia_representation = shared_image_representation_factory_.ProduceSkia(
      factory_ref->mailbox(), context_state_);
  ASSERT_NE(skia_representation, nullptr);

  // Expect BeginScopedReadAccess to fail because sharedImage is uninitialized
  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
      scoped_read_access =
          skia_representation->BeginScopedReadAccess(nullptr, nullptr);
  EXPECT_EQ(scoped_read_access, nullptr);
}

TEST_P(IOSurfaceImageBackingFactoryDawnTest, Dawn_SamplingVideoTexture) {
  const gfx::Size size(32, 32);

  const uint8_t kYFillValue = 0x12;
  const uint8_t kUFillValue = 0x23;
  const uint8_t kVFillValue = 0x34;

  // Create a Dawn device
  wgpu::Device device = CreateDevice();
  ASSERT_NE(device, nullptr);

  if (!device.HasFeature(wgpu::FeatureName::DawnMultiPlanarFormats)) {
    GTEST_SKIP();
  }
  if (backend_type() == wgpu::BackendType::Vulkan) {
    // Vulkan doesn't support IOSurface image backing, so we need
    // MultiPlanarFormatExtendedUsages to copy to/from multiplanar texture.
    ASSERT_TRUE(
        device.HasFeature(wgpu::FeatureName::MultiPlanarFormatExtendedUsages));
  }

  // Create a backing using mailbox.
  auto mailbox = Mailbox::Generate();
  const auto format = viz::MultiPlaneFormat::kNV12;
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  const SharedImageUsageSet usage = {SHARED_IMAGE_USAGE_SCANOUT,
                                     SHARED_IMAGE_USAGE_WEBGPU_READ};
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, "TestLabel", /*is_thread_safe=*/false);
  ASSERT_NE(backing, nullptr);

  // Fill the shared image's data
  std::array<std::vector<uint8_t>, 2> plane_datas = {
      std::vector<uint8_t>(size.width() * size.height(), kYFillValue),
      std::vector<uint8_t>(size.width() * size.height() / 2),
  };

  // U & V data are interleaved.
  for (size_t i = 0; i < plane_datas[1].size(); i += 2) {
    plane_datas[1][i] = kUFillValue;
    plane_datas[1][i + 1] = kVFillValue;
  }

  std::vector<SkPixmap> pixmaps(2);

  for (int plane_index = 0; plane_index < 2; ++plane_index) {
    gfx::Size plane_size = format.GetPlaneSize(plane_index, size);
    auto info =
        SkImageInfo::Make(gfx::SizeToSkISize(plane_size),
                          viz::ToClosestSkColorType(
                              /*gpu_compositing=*/true, format, plane_index),
                          alpha_type, color_space.ToSkColorSpace());
    pixmaps[plane_index] =
        SkPixmap(info, plane_datas[plane_index].data(), info.minRowBytes());
  }

  ASSERT_TRUE(backing->UploadFromMemory(pixmaps));
  backing->SetCleared();

  // Sampling the shared image in Dawn
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);

  auto dawn_image = shared_image_representation_factory_.ProduceDawn(
      mailbox, device, backend_type(), {}, context_state_);
  ASSERT_NE(dawn_image, nullptr);

  RunDawnVideoSamplingTest(instance_.Get(), device, dawn_image, kYFillValue,
                           kUFillValue, kVFillValue);
}

// Test that Skia trying to access uninitialized SharedImage will fail
TEST_F(IOSurfaceImageBackingFactoryTest, SkiaAccessFirstFails) {
  // Create a mailbox.
  auto mailbox = Mailbox::Generate();
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  SharedImageUsageSet usage = {SHARED_IMAGE_USAGE_SCANOUT,
                               SHARED_IMAGE_USAGE_DISPLAY_READ};
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, "TestLabel", /*is_thread_safe=*/false);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);

  auto skia_representation =
      shared_image_representation_factory_.ProduceSkia(mailbox, context_state_);
  ASSERT_NE(skia_representation, nullptr);
  EXPECT_FALSE(skia_representation->IsCleared());

  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
      scoped_read_access =
          skia_representation->BeginScopedReadAccess(nullptr, nullptr);
  // Expect BeginScopedReadAccess to fail because sharedImage is uninitialized
  EXPECT_EQ(scoped_read_access, nullptr);
}

class MockProgressReporter : public gl::ProgressReporter {
 public:
  MockProgressReporter() = default;
  ~MockProgressReporter() override = default;

  // gl::ProgressReporter implementation.
  MOCK_METHOD0(ReportProgress, void());
};

struct ContextTypeParams {
  ContextTypeParams(
      GrContextType context_type,
      wgpu::BackendType dawn_backend_type = wgpu::BackendType::Null)
      : context_type(context_type), dawn_backend_type(dawn_backend_type) {}
  GrContextType context_type;
  wgpu::BackendType dawn_backend_type;
};

class IOSurfaceImageBackingFactoryParameterizedTestBase
    : public SharedImageTestBase,
      public testing::WithParamInterface<
          std::tuple<viz::SharedImageFormat, ContextTypeParams>> {
 public:
  IOSurfaceImageBackingFactoryParameterizedTestBase() = default;
  ~IOSurfaceImageBackingFactoryParameterizedTestBase() override = default;

  void SetUp() override {
    ASSERT_TRUE(gpu_preferences_.use_passthrough_cmd_decoder);

    auto gr_context_type = get_gr_context_type();
    ASSERT_NO_FATAL_FAILURE(InitializeContext(gr_context_type));

    auto format = get_format();
    // Dawn does not support BGRA_1010102.
    if (gr_context_type == GrContextType::kGraphiteDawn &&
        format == viz::SinglePlaneFormat::kBGRA_1010102) {
      GTEST_SKIP();
    }

    auto* feature_info = context_state_->feature_info();
    // NV12 is always supported on Apple.
    ASSERT_TRUE(feature_info->feature_flags().chromium_image_ycbcr_420v);
    supports_etc1_ =
        feature_info->validators()->compressed_texture_format.IsValid(
            GL_ETC1_RGB8_OES);
    supports_ar30_ = feature_info->feature_flags().chromium_image_ar30;
    supports_ab30_ = feature_info->feature_flags().chromium_image_ab30;
    supports_ycbcr_p010_ =
        feature_info->feature_flags().chromium_image_ycbcr_p010;

    backing_factory_ = std::make_unique<IOSurfaceImageBackingFactory>(
        context_state_->gr_context_type(), context_state_->GetMaxTextureSize(),
        context_state_->feature_info(), &progress_reporter_,
#if BUILDFLAG(IS_MAC)
        GetTextureTargetForIOSurfaces());
#else
        GL_TEXTURE_2D);
#endif
  }

  viz::SharedImageFormat get_format() { return std::get<0>(GetParam()); }
  GrContextType get_gr_context_type() {
    return std::get<1>(GetParam()).context_type;
  }

#if BUILDFLAG(SKIA_USE_DAWN)
  // Override SharedImageTestBase
  wgpu::BackendType GetDawnBackendType() const override {
    return std::get<1>(GetParam()).dawn_backend_type;
  }

  bool DawnForceFallbackAdapter() const override {
    return GetDawnBackendType() == wgpu::BackendType::Vulkan;
  }
#else
  wgpu::BackendType GetDawnBackendType() const {
    NOTREACHED_IN_MIGRATION();
    return wgpu::BackendType::Undefined;
  }
#endif  // BUILDFLAG(SKIA_USE_DAWN)

 protected:
  ::testing::NiceMock<MockProgressReporter> progress_reporter_;
  bool supports_etc1_ = false;
  bool supports_ar30_ = false;
  bool supports_ab30_ = false;
  bool supports_ycbcr_p010_ = false;
};

// SharedImageFormat parameterized tests.
class IOSurfaceImageBackingFactoryScanoutTest
    : public IOSurfaceImageBackingFactoryParameterizedTestBase {
 public:
  bool can_create_scanout_shared_image(viz::SharedImageFormat format,
                                       bool has_pixel_data = false) const {
    if (format == viz::SinglePlaneFormat::kBGRA_1010102) {
      return supports_ar30_;
    } else if (format == viz::SinglePlaneFormat::kRGBA_1010102) {
      return supports_ab30_;
    } else if (format == viz::MultiPlaneFormat::kNV12) {
      return !has_pixel_data;
    } else if (format == viz::MultiPlaneFormat::kP010) {
      return supports_ycbcr_p010_ && !has_pixel_data;
    }
    return true;
  }
};

TEST_P(IOSurfaceImageBackingFactoryScanoutTest, Basic) {
  const bool should_succeed = can_create_scanout_shared_image(get_format());
  if (should_succeed) {
    EXPECT_CALL(progress_reporter_, ReportProgress).Times(AtLeast(1));
  }
  auto mailbox = Mailbox::Generate();
  auto format = get_format();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  SharedImageUsageSet usage{SHARED_IMAGE_USAGE_SCANOUT,
                            SHARED_IMAGE_USAGE_RASTER_READ,
                            SHARED_IMAGE_USAGE_RASTER_WRITE};
  if (get_gr_context_type() == GrContextType::kGL) {
    usage.PutAll({SHARED_IMAGE_USAGE_GLES2_READ});
  } else if constexpr (BUILDFLAG(SKIA_USE_DAWN)) {
    usage.PutAll({SHARED_IMAGE_USAGE_WEBGPU_READ});
  }
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, "TestLabel", /*is_thread_safe=*/false);

  if (!should_succeed) {
    EXPECT_FALSE(backing);
    return;
  }
  ASSERT_TRUE(backing);
  ::testing::Mock::VerifyAndClearExpectations(&progress_reporter_);

  // Check clearing.
  if (!backing->IsCleared()) {
    backing->SetCleared();
    EXPECT_TRUE(backing->IsCleared());
  }

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);
  EXPECT_TRUE(shared_image);

  if (get_gr_context_type() == GrContextType::kGL) {
    // First, validate a GLTexturePassthroughImageRepresentation.
    auto gl_representation =
        shared_image_representation_factory_.ProduceGLTexturePassthrough(
            mailbox);
    EXPECT_TRUE(gl_representation);
    for (auto i = 0; i < format.NumberOfPlanes(); i++) {
      EXPECT_TRUE(gl_representation->GetTexturePassthrough(i)->service_id());
    }
    EXPECT_EQ(size, gl_representation->size());
    EXPECT_EQ(format, gl_representation->format());
    EXPECT_EQ(color_space, gl_representation->color_space());
    EXPECT_EQ(usage, gl_representation->usage());
    gl_representation.reset();
  } else {
#if BUILDFLAG(SKIA_USE_DAWN)
    CHECK_EQ(get_gr_context_type(), GrContextType::kGraphiteDawn);
    // First, validate a DawnImageRepresentation.
    auto* context_provider = context_state_->dawn_context_provider();
    auto device = context_provider->GetDevice();
    auto dawn_representation = shared_image_representation_factory_.ProduceDawn(
        mailbox, device, context_provider->backend_type(), {}, context_state_);
    ASSERT_TRUE(dawn_representation);
    EXPECT_EQ(usage, dawn_representation->usage());
    EXPECT_EQ(color_space, dawn_representation->color_space());

    auto dawn_scoped_access = dawn_representation->BeginScopedAccess(
        wgpu::TextureUsage::TextureBinding,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(dawn_scoped_access);

    wgpu::Texture texture(dawn_scoped_access->texture());
    ASSERT_TRUE(texture);
    EXPECT_EQ(size.width(), static_cast<int>(texture.GetWidth()));
    EXPECT_EQ(size.height(), static_cast<int>(texture.GetHeight()));
    EXPECT_EQ(ToDawnFormat(format), texture.GetFormat());
    dawn_scoped_access.reset();
    dawn_representation.reset();
#endif
  }

  if (format == viz::SinglePlaneFormat::kBGRA_1010102 ||
      format == viz::MultiPlaneFormat::kP010) {
    // Producing SkSurface for these formats fails for some reason.
    return;
  }

  // TODO(dawn:1337): Enable once multi-planar rendering lands for Dawn
  // Vulkan-Swiftshader.
  if (get_gr_context_type() == GrContextType::kGraphiteDawn &&
      GetDawnBackendType() == wgpu::BackendType::Vulkan &&
      format.is_multi_plane()) {
    return;
  }

  // Finally, validate a SkiaImageRepresentation.
  auto skia_representation = shared_image_representation_factory_.ProduceSkia(
      mailbox, context_state_.get());
  EXPECT_TRUE(skia_representation);
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
      scoped_write_access;
  scoped_write_access = skia_representation->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  for (auto i = 0; i < format.NumberOfPlanes(); i++) {
    auto* surface = scoped_write_access->surface(i);
    EXPECT_TRUE(surface);
    auto expected_plane_size = format.GetPlaneSize(i, size);
    EXPECT_EQ(expected_plane_size.width(), surface->width());
    EXPECT_EQ(expected_plane_size.height(), surface->height());
  }
  scoped_write_access.reset();

  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess> scoped_read_access;
  scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  if (get_gr_context_type() == GrContextType::kGL) {
    EXPECT_TRUE(begin_semaphores.empty());
    EXPECT_TRUE(end_semaphores.empty());
    for (auto i = 0; i < format.NumberOfPlanes(); i++) {
      auto* promise_texture = scoped_read_access->promise_image_texture(i);
      EXPECT_TRUE(promise_texture);
      if (promise_texture) {
        GrBackendTexture backend_texture = promise_texture->backendTexture();
        EXPECT_TRUE(backend_texture.isValid());
        auto expected_plane_size = format.GetPlaneSize(i, size);
        EXPECT_EQ(expected_plane_size.width(), backend_texture.width());
        EXPECT_EQ(expected_plane_size.height(), backend_texture.height());
      }
    }
  } else {
    CHECK_EQ(get_gr_context_type(), GrContextType::kGraphiteDawn);
    EXPECT_TRUE(begin_semaphores.empty());
    EXPECT_TRUE(end_semaphores.empty());
    for (auto i = 0; i < format.NumberOfPlanes(); i++) {
      auto graphite_texture = scoped_read_access->graphite_texture(i);
      EXPECT_TRUE(graphite_texture.isValid());
      auto plane_size = format.GetPlaneSize(i, size);
      EXPECT_EQ(plane_size.width(), graphite_texture.dimensions().width());
      EXPECT_EQ(plane_size.height(), graphite_texture.dimensions().height());
    }
  }
  scoped_read_access.reset();
  skia_representation.reset();

  shared_image.reset();
}

TEST_P(IOSurfaceImageBackingFactoryScanoutTest, InitialData) {
  auto format = get_format();
  const bool should_succeed =
      can_create_scanout_shared_image(format,
                                      /*has_pixel_data=*/true);
  if (should_succeed) {
    EXPECT_CALL(progress_reporter_, ReportProgress).Times(AtLeast(1));
  }

  auto mailbox = Mailbox::Generate();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  SharedImageUsageSet usage = {SHARED_IMAGE_USAGE_SCANOUT};
  if (get_gr_context_type() == GrContextType::kGL) {
    usage.PutAll({SHARED_IMAGE_USAGE_GLES2_READ});
  } else if constexpr (BUILDFLAG(SKIA_USE_DAWN)) {
    usage.PutAll({SHARED_IMAGE_USAGE_WEBGPU_READ});
  }
  std::vector<uint8_t> initial_data(format.EstimatedSizeInBytes(size));
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      "TestLabel", /*is_thread_safe=*/false, initial_data);
  ::testing::Mock::VerifyAndClearExpectations(&progress_reporter_);
  if (!should_succeed) {
    EXPECT_FALSE(backing);
    return;
  }
  ASSERT_TRUE(backing);
  EXPECT_TRUE(backing->IsCleared());

  // Validate via a GLTextureImageRepresentation(Passthrough).
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);
  EXPECT_TRUE(shared_image);
  GLenum expected_target =
#if BUILDFLAG(IS_MAC)
      GetTextureTargetForIOSurfaces();
#else
      GL_TEXTURE_2D;
#endif

  if (get_gr_context_type() == GrContextType::kGL) {
    // First, validate a GLTexturePassthroughImageRepresentation.
    auto gl_representation =
        shared_image_representation_factory_.ProduceGLTexturePassthrough(
            mailbox);
    EXPECT_TRUE(gl_representation);
    EXPECT_TRUE(gl_representation->GetTexturePassthrough()->service_id());
    EXPECT_EQ(expected_target,
              gl_representation->GetTexturePassthrough()->target());
    EXPECT_EQ(size, gl_representation->size());
    EXPECT_EQ(format, gl_representation->format());
    EXPECT_EQ(color_space, gl_representation->color_space());
    EXPECT_EQ(usage, gl_representation->usage());
    gl_representation.reset();
  } else {
#if BUILDFLAG(SKIA_USE_DAWN)
    CHECK_EQ(get_gr_context_type(), GrContextType::kGraphiteDawn);
    // First, validate a DawnImageRepresentation.
    auto* context_provider = context_state_->dawn_context_provider();
    auto device = context_provider->GetDevice();
    auto dawn_representation = shared_image_representation_factory_.ProduceDawn(
        mailbox, device, context_provider->backend_type(), {}, context_state_);
    ASSERT_TRUE(dawn_representation);
    EXPECT_EQ(usage, dawn_representation->usage());
    EXPECT_EQ(color_space, dawn_representation->color_space());

    auto dawn_scoped_access = dawn_representation->BeginScopedAccess(
        wgpu::TextureUsage::TextureBinding,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    ASSERT_TRUE(dawn_scoped_access);

    wgpu::Texture texture(dawn_scoped_access->texture());
    ASSERT_TRUE(texture);
    EXPECT_EQ(size.width(), static_cast<int>(texture.GetWidth()));
    EXPECT_EQ(size.height(), static_cast<int>(texture.GetHeight()));
    EXPECT_EQ(ToDawnFormat(format), texture.GetFormat());
    dawn_scoped_access.reset();
    dawn_representation.reset();
#endif
  }
}

TEST_P(IOSurfaceImageBackingFactoryScanoutTest, InitialDataImage) {
  const bool should_succeed =
      can_create_scanout_shared_image(get_format(), /*has_pixel_data=*/true);
  if (should_succeed) {
    EXPECT_CALL(progress_reporter_, ReportProgress).Times(AtLeast(1));
  }
  auto mailbox = Mailbox::Generate();
  auto format = get_format();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  SharedImageUsageSet usage = {SHARED_IMAGE_USAGE_SCANOUT};
  if (get_gr_context_type() == GrContextType::kGL) {
    usage.PutAll({SHARED_IMAGE_USAGE_GLES2_READ});
  } else if constexpr (BUILDFLAG(SKIA_USE_DAWN)) {
    usage.PutAll({SHARED_IMAGE_USAGE_WEBGPU_READ});
  }
  std::vector<uint8_t> initial_data(256 * 256 * 4);
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      "TestLabel", /*is_thread_safe=*/false, initial_data);
  if (!should_succeed) {
    EXPECT_FALSE(backing);
    return;
  }
  ASSERT_TRUE(backing);
  EXPECT_TRUE(backing->IsCleared());

  // Validate via a GLTextureImageRepresentation(Passthrough).
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);
  EXPECT_TRUE(shared_image);

  if (get_gr_context_type() == GrContextType::kGL) {
    // First, validate a GLTexturePassthroughImageRepresentation.
    auto gl_representation =
        shared_image_representation_factory_.ProduceGLTexturePassthrough(
            mailbox);
    EXPECT_TRUE(gl_representation);
    EXPECT_TRUE(gl_representation->GetTexturePassthrough()->service_id());
    EXPECT_EQ(size, gl_representation->size());
    EXPECT_EQ(format, gl_representation->format());
    EXPECT_EQ(color_space, gl_representation->color_space());
    EXPECT_EQ(usage, gl_representation->usage());
    gl_representation.reset();
  } else {
#if BUILDFLAG(SKIA_USE_DAWN)
    CHECK_EQ(get_gr_context_type(), GrContextType::kGraphiteDawn);
    // First, validate a DawnImageRepresentation.
    auto* context_provider = context_state_->dawn_context_provider();
    auto device = context_provider->GetDevice();
    auto dawn_representation = shared_image_representation_factory_.ProduceDawn(
        mailbox, device, context_provider->backend_type(), {}, context_state_);
    ASSERT_TRUE(dawn_representation);
    EXPECT_EQ(usage, dawn_representation->usage());
    EXPECT_EQ(color_space, dawn_representation->color_space());

    auto dawn_scoped_access = dawn_representation->BeginScopedAccess(
        wgpu::TextureUsage::TextureBinding,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    ASSERT_TRUE(dawn_scoped_access);

    wgpu::Texture texture(dawn_scoped_access->texture());
    ASSERT_TRUE(texture);
    EXPECT_EQ(size.width(), static_cast<int>(texture.GetWidth()));
    EXPECT_EQ(size.height(), static_cast<int>(texture.GetHeight()));
    EXPECT_EQ(ToDawnFormat(format), texture.GetFormat());
    dawn_scoped_access.reset();
    dawn_representation.reset();
#endif
  }
}

TEST_P(IOSurfaceImageBackingFactoryScanoutTest, InitialDataWrongSize) {
  auto mailbox = Mailbox::Generate();
  auto format = get_format();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  SharedImageUsageSet usage = {SHARED_IMAGE_USAGE_SCANOUT};
  std::vector<uint8_t> initial_data_small(256 * 128 * 4);
  std::vector<uint8_t> initial_data_large(256 * 512 * 4);
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      "TestLabel", /*is_thread_safe=*/false, initial_data_small);
  EXPECT_FALSE(backing);
  backing = backing_factory_->CreateSharedImage(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      "TestLabel", /*is_thread_safe=*/false, initial_data_large);
  EXPECT_FALSE(backing);
}

// Tests creation with a multiplanar format that would succeed if used with
// empty pixel data but should fail with non-empty pixel data.
TEST_P(IOSurfaceImageBackingFactoryScanoutTest,
       InvalidFormatForCreationWithPixelData) {
  auto mailbox = Mailbox::Generate();
  auto format = viz::MultiPlaneFormat::kNV12;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  SharedImageUsageSet usage = {SHARED_IMAGE_USAGE_SCANOUT};
  std::vector<uint8_t> initial_data(256 * 256 * 4);
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      "TestLabel", /*is_thread_safe=*/false, initial_data);
  EXPECT_FALSE(backing);
}

TEST_P(IOSurfaceImageBackingFactoryScanoutTest, InvalidSize) {
  auto mailbox = Mailbox::Generate();
  auto format = get_format();
  gfx::Size size(0, 0);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  SharedImageUsageSet usage = {SHARED_IMAGE_USAGE_SCANOUT};
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, "TestLabel", /*is_thread_safe=*/false);
  EXPECT_FALSE(backing);

  size = gfx::Size(INT_MAX, INT_MAX);
  backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, "TestLabel", /*is_thread_safe=*/false);
  EXPECT_FALSE(backing);
}

TEST_P(IOSurfaceImageBackingFactoryScanoutTest, EstimatedSize) {
  const bool should_succeed = can_create_scanout_shared_image(get_format());
  if (should_succeed) {
    EXPECT_CALL(progress_reporter_, ReportProgress).Times(AtLeast(1));
  }
  auto mailbox = Mailbox::Generate();
  auto format = get_format();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  SharedImageUsageSet usage = {SHARED_IMAGE_USAGE_SCANOUT};
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, "TestLabel", /*is_thread_safe=*/false);

  if (!should_succeed) {
    EXPECT_FALSE(backing);
    return;
  }
  ASSERT_TRUE(backing);

  size_t backing_estimated_size = backing->GetEstimatedSize();
  EXPECT_GT(backing_estimated_size, 0u);

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);
  EXPECT_EQ(backing_estimated_size, memory_type_tracker_.GetMemRepresented());

  shared_image.reset();
}

// Ensures that the various conversion functions used w/ TexStorage2D match
// their TexImage2D equivalents, allowing us to minimize the amount of parallel
// data tracked in the SharedImageFactoryGLImage.
TEST_P(IOSurfaceImageBackingFactoryScanoutTest, TexImageTexStorageEquivalence) {
  // GraphiteDawn does not support tex storage.
  if (get_gr_context_type() == GrContextType::kGraphiteDawn) {
    GTEST_SKIP();
  }

  scoped_refptr<gles2::FeatureInfo> feature_info =
      new gles2::FeatureInfo(GpuDriverBugWorkarounds(), GpuFeatureInfo());
  feature_info->Initialize(ContextType::CONTEXT_TYPE_OPENGLES2,
                           /*is_passthrough_cmd_decoder=*/true,
                           gles2::DisallowedFeatures());
  const gles2::Validators* validators = feature_info->validators();

  for (auto format : viz::SinglePlaneFormat::kAll) {
    if (format == viz::SinglePlaneFormat::kBGR_565 || format.IsCompressed()) {
      continue;
    }

    GLFormatCaps caps(feature_info.get());
    GLFormatDesc format_desc = caps.ToGLFormatDesc(format, /*plane_index=*/0);
    int storage_format = format_desc.storage_internal_format;
    int image_gl_format = format_desc.data_format;
    int storage_gl_format =
        gles2::TextureManager::ExtractFormatFromStorageFormat(storage_format);
    EXPECT_EQ(image_gl_format, storage_gl_format);

    int image_gl_type = format_desc.data_type;
    int storage_gl_type =
        gles2::TextureManager::ExtractTypeFromStorageFormat(storage_format);

    // Ignore the HALF_FLOAT / HALF_FLOAT_OES discrepancy for now.
    // TODO(ericrk): Figure out if we need additional action to support
    // HALF_FLOAT.
    if (!(image_gl_type == GL_HALF_FLOAT_OES &&
          storage_gl_type == GL_HALF_FLOAT)) {
      EXPECT_EQ(image_gl_type, storage_gl_type);
    }

    // confirm that we support TexStorage2D only if we support TexImage2D:
    int image_internal_format = format_desc.image_internal_format;
    bool supports_tex_image =
        validators->texture_internal_format.IsValid(image_internal_format) &&
        validators->texture_format.IsValid(image_gl_format) &&
        validators->pixel_type.IsValid(image_gl_type);
    bool supports_tex_storage =
        validators->texture_internal_format_storage.IsValid(storage_format);
    if (supports_tex_storage) {
      EXPECT_TRUE(supports_tex_image);
    }
  }
}

// SharedImageFormat parameterized tests.
class IOSurfaceImageBackingFactoryGMBTest
    : public IOSurfaceImageBackingFactoryParameterizedTestBase {
 public:
  bool can_create_gmb_shared_image(viz::SharedImageFormat format) const {
    if (format == viz::SinglePlaneFormat::kBGRA_1010102) {
      return supports_ar30_;
    } else if (format == viz::SinglePlaneFormat::kRGBA_1010102) {
      return supports_ab30_;
    } else if (format == viz::MultiPlaneFormat::kP010) {
      return supports_ycbcr_p010_;
    }
    return true;
  }

  std::unique_ptr<SharedImageRepresentationFactoryRef> CreateSharedImage(
      gfx::Size size,
      viz::SharedImageFormat format,
      SharedImageUsageSet usage,
      gfx::ColorSpace color_space) {
    const bool should_succeed = can_create_gmb_shared_image(get_format());
    auto mailbox = Mailbox::Generate();
    GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
    SkAlphaType alpha_type = kPremul_SkAlphaType;
    bool override_rgba_to_bgra = get_gr_context_type() == GrContextType::kGL;

    gfx::BufferFormat buffer_format = gpu::ToBufferFormat(format);
    gfx::GpuMemoryBufferHandle handle;
    gfx::GpuMemoryBufferId kBufferId(1);
    handle.type = gfx::IO_SURFACE_BUFFER;
    handle.id = kBufferId;
    handle.io_surface = gfx::CreateIOSurface(
        size, buffer_format, /*should_clear=*/true, override_rgba_to_bgra);
    DCHECK(handle.io_surface);

    auto backing = backing_factory_->CreateSharedImage(
        mailbox, format, size, color_space, surface_origin, alpha_type, usage,
        "TestLabel", std::move(handle));

    if (!should_succeed) {
      return nullptr;
    }

    // Check clearing.
    if (!backing->IsCleared()) {
      backing->SetCleared();
      EXPECT_TRUE(backing->IsCleared());
    }

    return shared_image_manager_.Register(std::move(backing),
                                          &memory_type_tracker_);
  }
};

TEST_P(IOSurfaceImageBackingFactoryGMBTest, Basic) {
  auto format = get_format();
  gfx::Size size(256, 256);
  SharedImageUsageSet usage = {SHARED_IMAGE_USAGE_SCANOUT,
                               SHARED_IMAGE_USAGE_DISPLAY_READ,
                               SHARED_IMAGE_USAGE_DISPLAY_WRITE};
  if (get_gr_context_type() == GrContextType::kGL) {
    usage.PutAll({SHARED_IMAGE_USAGE_GLES2_READ});
  } else if constexpr (BUILDFLAG(SKIA_USE_DAWN)) {
    usage.PutAll({SHARED_IMAGE_USAGE_WEBGPU_READ});
  }
  auto color_space = gfx::ColorSpace::CreateSRGB();

  const bool should_succeed = can_create_gmb_shared_image(get_format());
  auto shared_image = CreateSharedImage(size, format, usage, color_space);
  if (!should_succeed) {
    EXPECT_FALSE(shared_image);
    return;
  }
  ASSERT_TRUE(shared_image);
  auto mailbox = shared_image->mailbox();

  if (get_gr_context_type() == GrContextType::kGL) {
    // First, validate a GLTexturePassthroughImageRepresentation.
    auto gl_representation =
        shared_image_representation_factory_.ProduceGLTexturePassthrough(
            mailbox);
    EXPECT_TRUE(gl_representation);
    for (int plane = 0; plane < format.NumberOfPlanes(); plane++) {
      EXPECT_TRUE(
          gl_representation->GetTexturePassthrough(plane)->service_id());
    }
    EXPECT_EQ(size, gl_representation->size());
    EXPECT_EQ(format, gl_representation->format());
    EXPECT_EQ(color_space, gl_representation->color_space());
    EXPECT_EQ(usage, gl_representation->usage());
    gl_representation.reset();
  } else {
#if BUILDFLAG(SKIA_USE_DAWN)
    CHECK_EQ(get_gr_context_type(), GrContextType::kGraphiteDawn);
    // First, validate a DawnImageRepresentation.
    auto* context_provider = context_state_->dawn_context_provider();
    auto device = context_provider->GetDevice();
    auto dawn_representation = shared_image_representation_factory_.ProduceDawn(
        mailbox, device, context_provider->backend_type(), {}, context_state_);
    ASSERT_TRUE(dawn_representation);
    EXPECT_EQ(usage, dawn_representation->usage());
    EXPECT_EQ(color_space, dawn_representation->color_space());

    auto dawn_scoped_access = dawn_representation->BeginScopedAccess(
        wgpu::TextureUsage::TextureBinding,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    ASSERT_TRUE(dawn_scoped_access);

    // TODO(crbug.com/40266937): Check for TextureViews for multiplanar formats.
    wgpu::Texture texture(dawn_scoped_access->texture());
    ASSERT_TRUE(texture);
    EXPECT_EQ(size.width(), static_cast<int>(texture.GetWidth()));
    EXPECT_EQ(size.height(), static_cast<int>(texture.GetHeight()));
    EXPECT_EQ(ToDawnFormat(format), texture.GetFormat());
    dawn_scoped_access.reset();
    dawn_representation.reset();
#endif
  }

  // Finally, validate a SkiaImageRepresentation.
  auto skia_representation = shared_image_representation_factory_.ProduceSkia(
      mailbox, context_state_.get());
  EXPECT_TRUE(skia_representation);
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess> scoped_read_access;
  scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  EXPECT_TRUE(begin_semaphores.empty());
  EXPECT_TRUE(end_semaphores.empty());
  if (get_gr_context_type() == GrContextType::kGL) {
    for (int plane = 0; plane < format.NumberOfPlanes(); plane++) {
      auto* promise_texture = scoped_read_access->promise_image_texture(plane);
      EXPECT_TRUE(promise_texture);
      if (promise_texture) {
        GrBackendTexture backend_texture = promise_texture->backendTexture();
        EXPECT_TRUE(backend_texture.isValid());
        auto plane_size = format.GetPlaneSize(plane, size);
        EXPECT_EQ(plane_size.width(), backend_texture.width());
        EXPECT_EQ(plane_size.height(), backend_texture.height());
      }
    }
  } else {
    CHECK_EQ(get_gr_context_type(), GrContextType::kGraphiteDawn);
    for (int plane = 0; plane < format.NumberOfPlanes(); plane++) {
      auto graphite_texture = scoped_read_access->graphite_texture(plane);
      EXPECT_TRUE(graphite_texture.isValid());
      auto plane_size = format.GetPlaneSize(plane, size);
      EXPECT_EQ(plane_size.width(), graphite_texture.dimensions().width());
      EXPECT_EQ(plane_size.height(), graphite_texture.dimensions().height());
    }
  }

  scoped_read_access.reset();

  // Producing SkSurface for BGRA_1010102 or P010 fails as Skia uses A16, RG16
  // formats as read-only for now. See
  // GrRecordingContext::colorTypeSupportedAsSurface() for all unsupported
  // types.
  // TODO(crbug.com/40266937): Check supported formats for graphite and update.
  if (format == viz::SinglePlaneFormat::kBGRA_1010102 ||
      format == viz::MultiPlaneFormat::kP010) {
    return;
  }

  // TODO(dawn:1337): Enable once multi-planar rendering lands for Dawn
  // Vulkan-Swiftshader.
  if (get_gr_context_type() == GrContextType::kGraphiteDawn &&
      GetDawnBackendType() == wgpu::BackendType::Vulkan &&
      format.is_multi_plane()) {
    return;
  }

  std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
      scoped_write_access;
  scoped_write_access = skia_representation->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  for (int plane = 0; plane < format.NumberOfPlanes(); plane++) {
    auto* surface = scoped_write_access->surface(plane);
    EXPECT_TRUE(surface);
    auto plane_size = format.GetPlaneSize(plane, size);
    EXPECT_EQ(plane_size.width(), surface->width());
    EXPECT_EQ(plane_size.height(), surface->height());
  }
  scoped_write_access.reset();
  skia_representation.reset();

  shared_image.reset();
}

// Tests that multiple representations created from Graphite's Dawn device use
// the same wgpu::Texture for accesses created with the same usage.
TEST_P(IOSurfaceImageBackingFactoryGMBTest,
       Dawn_MultipleRepresentationsWithSameUsageFromGraphiteDevice) {
  if ((get_gr_context_type() != GrContextType::kGraphiteDawn) ||
      GetDawnBackendType() != wgpu::BackendType::Metal) {
    GTEST_SKIP();
  }

  auto format = get_format();
  gfx::Size size(256, 256);
  SharedImageUsageSet usage = {SHARED_IMAGE_USAGE_SCANOUT,
                               SHARED_IMAGE_USAGE_WEBGPU_READ};
  auto color_space = gfx::ColorSpace::CreateSRGB();

  const bool should_succeed = can_create_gmb_shared_image(get_format());
  auto shared_image = CreateSharedImage(size, format, usage, color_space);
  if (!should_succeed) {
    EXPECT_FALSE(shared_image);
    return;
  }
  ASSERT_TRUE(shared_image);
  auto mailbox = shared_image->mailbox();

  auto* context_provider = context_state_->dawn_context_provider();
  auto device = context_provider->GetDevice();

  auto dawn_representation_0 = shared_image_representation_factory_.ProduceDawn(
      mailbox, device, context_provider->backend_type(), {}, context_state_);
  auto dawn_scoped_access_0 = dawn_representation_0->BeginScopedAccess(
      wgpu::TextureUsage::TextureBinding,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);

  auto dawn_representation_1 = shared_image_representation_factory_.ProduceDawn(
      mailbox, device, context_provider->backend_type(), {}, context_state_);
  auto dawn_scoped_access_1 = dawn_representation_1->BeginScopedAccess(
      wgpu::TextureUsage::TextureBinding,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);

  wgpu::Texture texture_0(dawn_scoped_access_0->texture());
  wgpu::Texture texture_1(dawn_scoped_access_1->texture());

  // The texture created for the first access should be cached and reused by the
  // second access.
  EXPECT_EQ(texture_0.Get(), texture_1.Get());
}

// Tests that multiple representations created from Graphite's Dawn device use
// different wgpu::Textures for accesses created with different usages.
TEST_P(IOSurfaceImageBackingFactoryGMBTest,
       Dawn_MultipleRepresentationsWithDifferentUsagesFromGraphiteDevice) {
  if ((get_gr_context_type() != GrContextType::kGraphiteDawn) ||
      GetDawnBackendType() != wgpu::BackendType::Metal) {
    GTEST_SKIP();
  }

  auto format = get_format();
  gfx::Size size(256, 256);
  SharedImageUsageSet usage = {SHARED_IMAGE_USAGE_SCANOUT,
                               SHARED_IMAGE_USAGE_WEBGPU_READ};
  auto color_space = gfx::ColorSpace::CreateSRGB();

  const bool should_succeed = can_create_gmb_shared_image(get_format());
  auto shared_image = CreateSharedImage(size, format, usage, color_space);
  if (!should_succeed) {
    EXPECT_FALSE(shared_image);
    return;
  }
  ASSERT_TRUE(shared_image);
  auto mailbox = shared_image->mailbox();

  auto* context_provider = context_state_->dawn_context_provider();
  auto device = context_provider->GetDevice();

  auto dawn_representation_0 = shared_image_representation_factory_.ProduceDawn(
      mailbox, device, context_provider->backend_type(), {}, context_state_);
  auto dawn_scoped_access_0 = dawn_representation_0->BeginScopedAccess(
      wgpu::TextureUsage::TextureBinding,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);

  auto dawn_representation_1 = shared_image_representation_factory_.ProduceDawn(
      mailbox, device, context_provider->backend_type(), {}, context_state_);
  auto dawn_scoped_access_1 = dawn_representation_1->BeginScopedAccess(
      wgpu::TextureUsage::CopySrc,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);

  wgpu::Texture texture_0(dawn_scoped_access_0->texture());
  wgpu::Texture texture_1(dawn_scoped_access_1->texture());

  // The texture created for the first access should be distinct from that of
  // the second access.
  EXPECT_NE(texture_0.Get(), texture_1.Get());
}

// Tests that sequential accesses to a Dawn representation created from the
// Graphite device use the same wgpu::Texture iff the usage is the same.
TEST_P(IOSurfaceImageBackingFactoryGMBTest,
       Dawn_SequentialAccessesOnSingleRepresentationFromGraphiteDevice) {
  if ((get_gr_context_type() != GrContextType::kGraphiteDawn) ||
      GetDawnBackendType() != wgpu::BackendType::Metal) {
    GTEST_SKIP();
  }

  auto format = get_format();
  gfx::Size size(256, 256);
  SharedImageUsageSet usage = {SHARED_IMAGE_USAGE_SCANOUT,
                               SHARED_IMAGE_USAGE_WEBGPU_READ};
  auto color_space = gfx::ColorSpace::CreateSRGB();

  const bool should_succeed = can_create_gmb_shared_image(get_format());
  auto shared_image = CreateSharedImage(size, format, usage, color_space);
  if (!should_succeed) {
    EXPECT_FALSE(shared_image);
    return;
  }
  ASSERT_TRUE(shared_image);
  auto mailbox = shared_image->mailbox();

  auto* context_provider = context_state_->dawn_context_provider();
  auto device = context_provider->GetDevice();

  auto dawn_representation = shared_image_representation_factory_.ProduceDawn(
      mailbox, device, context_provider->backend_type(), {}, context_state_);
  auto dawn_scoped_access_0 = dawn_representation->BeginScopedAccess(
      wgpu::TextureUsage::TextureBinding,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);
  wgpu::Texture texture_0(dawn_scoped_access_0->texture());

  // The texture created for the first access should be reused for a new
  // access with the same usage.
  dawn_scoped_access_0.reset();
  auto dawn_scoped_access_1 = dawn_representation->BeginScopedAccess(
      wgpu::TextureUsage::TextureBinding,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);
  wgpu::Texture texture_1(dawn_scoped_access_1->texture());
  EXPECT_EQ(texture_0.Get(), texture_1.Get());

  // The texture created for the first access should not be reused for a new
  // access with different usage.
  dawn_scoped_access_1.reset();
  auto dawn_scoped_access_2 = dawn_representation->BeginScopedAccess(
      wgpu::TextureUsage::CopySrc,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);
  wgpu::Texture texture_2(dawn_scoped_access_2->texture());
  EXPECT_NE(texture_0.Get(), texture_2.Get());
}

// Tests that sequential accesses to distinct Dawn representations created from
// the Graphite device use the same wgpu::Texture iff the usage is the same.
TEST_P(IOSurfaceImageBackingFactoryGMBTest,
       Dawn_SequentialAccessesOnDifferentRepresentationsFromGraphiteDevice) {
  if ((get_gr_context_type() != GrContextType::kGraphiteDawn) ||
      GetDawnBackendType() != wgpu::BackendType::Metal) {
    GTEST_SKIP();
  }

  auto format = get_format();
  gfx::Size size(256, 256);
  SharedImageUsageSet usage = {SHARED_IMAGE_USAGE_SCANOUT,
                               SHARED_IMAGE_USAGE_WEBGPU_READ};
  auto color_space = gfx::ColorSpace::CreateSRGB();

  const bool should_succeed = can_create_gmb_shared_image(get_format());
  auto shared_image = CreateSharedImage(size, format, usage, color_space);
  if (!should_succeed) {
    EXPECT_FALSE(shared_image);
    return;
  }
  ASSERT_TRUE(shared_image);
  auto mailbox = shared_image->mailbox();

  auto* context_provider = context_state_->dawn_context_provider();
  auto device = context_provider->GetDevice();

  auto dawn_representation_0 = shared_image_representation_factory_.ProduceDawn(
      mailbox, device, context_provider->backend_type(), {}, context_state_);
  auto dawn_scoped_access_0 = dawn_representation_0->BeginScopedAccess(
      wgpu::TextureUsage::TextureBinding,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);
  wgpu::Texture texture_0(dawn_scoped_access_0->texture());

  // The texture created for the first access should be reused for a new
  // access created from a new Dawn representation but with the same usage.
  dawn_scoped_access_0.reset();
  dawn_representation_0.reset();
  auto dawn_representation_1 = shared_image_representation_factory_.ProduceDawn(
      mailbox, device, context_provider->backend_type(), {}, context_state_);
  auto dawn_scoped_access_1 = dawn_representation_1->BeginScopedAccess(
      wgpu::TextureUsage::TextureBinding,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);
  wgpu::Texture texture_1(dawn_scoped_access_1->texture());
  EXPECT_EQ(texture_0.Get(), texture_1.Get());

  // The texture created for the first access should not be reused for a new
  // access from a new representation with different usage.
  dawn_scoped_access_1.reset();
  dawn_representation_1.reset();
  auto dawn_representation_2 = shared_image_representation_factory_.ProduceDawn(
      mailbox, device, context_provider->backend_type(), {}, context_state_);
  auto dawn_scoped_access_2 = dawn_representation_2->BeginScopedAccess(
      wgpu::TextureUsage::CopySrc,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);
  wgpu::Texture texture_2(dawn_scoped_access_2->texture());
  EXPECT_NE(texture_0.Get(), texture_2.Get());
}

// Tests that destroying an access/representation from the Graphite device does
// not end the underlying access on Dawn's SharedTextureMemory if there is a
// second access still open with the same usage.
TEST_P(IOSurfaceImageBackingFactoryGMBTest,
       Dawn_SecondAccessFromGraphiteDeviceStaysOpenWhenFirstDestroyed) {
  if ((get_gr_context_type() != GrContextType::kGraphiteDawn) ||
      GetDawnBackendType() != wgpu::BackendType::Metal) {
    GTEST_SKIP();
  }

  auto format = get_format();
  if (format.is_multi_plane()) {
    // This test does a copy from one Dawn texture to another, which is not
    // supported with a multiplanar texture as the source texture.
    GTEST_SKIP();
  }

  gfx::Size size(256, 256);
  SharedImageUsageSet usage = {SHARED_IMAGE_USAGE_SCANOUT,
                               SHARED_IMAGE_USAGE_WEBGPU_READ};
  auto color_space = gfx::ColorSpace::CreateSRGB();

  const bool should_succeed = can_create_gmb_shared_image(get_format());
  auto shared_image = CreateSharedImage(size, format, usage, color_space);
  if (!should_succeed) {
    EXPECT_FALSE(shared_image);
    return;
  }
  ASSERT_TRUE(shared_image);
  auto mailbox = shared_image->mailbox();

  auto* context_provider = context_state_->dawn_context_provider();
  auto device = context_provider->GetDevice();

  auto dawn_representation_0 = shared_image_representation_factory_.ProduceDawn(
      mailbox, device, context_provider->backend_type(), {}, context_state_);
  auto dawn_scoped_access_0 = dawn_representation_0->BeginScopedAccess(
      wgpu::TextureUsage::CopySrc,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);

  auto dawn_representation_1 = shared_image_representation_factory_.ProduceDawn(
      mailbox, device, context_provider->backend_type(), {}, context_state_);
  auto dawn_scoped_access_1 = dawn_representation_1->BeginScopedAccess(
      wgpu::TextureUsage::CopySrc,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);

  wgpu::Texture texture_0(dawn_scoped_access_0->texture());
  wgpu::Texture texture_1(dawn_scoped_access_1->texture());

  // The texture created for the first access should be cached and reused by the
  // second access.
  EXPECT_EQ(texture_0.Get(), texture_1.Get());

  // Destroy the first access and representation.
  dawn_scoped_access_0.reset();
  dawn_representation_0.reset();

  // Do a Dawn submit using the texture to verify that the the destruction of
  // the first access and representation should not have resulted in
  // SharedTextureMemory::EndAccess() being called.
  auto dst = CreateSharedImage(
      size, format,
      usage | SharedImageUsageSet({SHARED_IMAGE_USAGE_WEBGPU_WRITE}),
      color_space);
  auto dst_rep = shared_image_representation_factory_.ProduceDawn(
      dst->mailbox(), device, context_provider->backend_type(), {},
      context_state_);
  auto dst_scoped_access = dst_rep->BeginScopedAccess(
      wgpu::TextureUsage::CopyDst,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  wgpu::Texture dst_texture(dst_scoped_access->texture());

  wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

  wgpu::ImageCopyTexture copy_src;
  copy_src.texture = texture_1;
  wgpu::ImageCopyTexture copy_dst;
  copy_dst.texture = dst_texture;
  wgpu::Extent3D copy_size;
  copy_size.width = size.width();
  copy_size.height = size.height();

  encoder.CopyTextureToTexture(&copy_src, &copy_dst, &copy_size);
  wgpu::CommandBuffer commands = encoder.Finish();

  // There should have been no errors signaled by Dawn before the submit.
  ASSERT_FALSE(context_provider->GetResetStatus());

  // Do the submit and verify that it did not result in a Dawn validation error
  // (which it will if the destruction of the first scoped access representation
  // has resulted in SharedTextureMemory::EndAccess() being called on the
  // texture).
  wgpu::Queue queue = device.GetQueue();
  queue.Submit(1, &commands);
  EXPECT_FALSE(context_provider->GetResetStatus());
}

const auto kScanoutFormats =
    ::testing::Values(viz::SinglePlaneFormat::kRGBA_8888,
                      viz::SinglePlaneFormat::kBGRA_8888,
                      viz::SinglePlaneFormat::kBGRA_1010102,
                      viz::SinglePlaneFormat::kRGBA_1010102,
                      viz::MultiPlaneFormat::kNV12,
                      viz::MultiPlaneFormat::kP010);

const auto kGMBFormats =
    ::testing::Values(viz::SinglePlaneFormat::kRGBA_8888,
                      viz::SinglePlaneFormat::kBGRA_8888,
                      viz::SinglePlaneFormat::kBGRA_1010102,
                      viz::MultiPlaneFormat::kNV12,
                      viz::MultiPlaneFormat::kP010);

std::string TestBackendTypeParamToString(
    const testing::TestParamInfo<wgpu::BackendType>& param_info) {
  // We only support metal & vulkan on mac. And for vulkan, the only driver
  // supported is swiftshader.
  std::string adapter_type = (param_info.param == wgpu::BackendType::Metal)
                                 ? "Metal"
                                 : "Vulkan_SwiftShader";
  return adapter_type;
}

std::string TestContextTypeFormatParamsToString(
    const testing::TestParamInfo<
        std::tuple<viz::SharedImageFormat, ContextTypeParams>>& param_info) {
  std::string format = std::get<0>(param_info.param).ToTestParamString();
  std::string context_type;

  if ((std::get<1>(param_info.param).context_type == GrContextType::kGL)) {
    context_type = "GL";
  } else {
    context_type = "GraphiteDawn";

    if (std::get<1>(param_info.param).dawn_backend_type ==
        wgpu::BackendType::Metal) {
      context_type += "_Metal";
    } else {
      context_type += "_Vulkan_SwiftShader";
    }
  }

  return context_type + "_" + format;
}

}  // namespace

INSTANTIATE_TEST_SUITE_P(,
                         IOSurfaceImageBackingFactoryDawnTest,
                         testing::Values(wgpu::BackendType::Metal,
                                         wgpu::BackendType::Vulkan),
                         TestBackendTypeParamToString);

INSTANTIATE_TEST_SUITE_P(
    ,
    IOSurfaceImageBackingFactoryScanoutTest,
    testing::Combine(
        kScanoutFormats,
        testing::Values(ContextTypeParams(GrContextType::kGL),
                        ContextTypeParams(GrContextType::kGraphiteDawn,
                                          wgpu::BackendType::Metal),
                        ContextTypeParams(GrContextType::kGraphiteDawn,
                                          wgpu::BackendType::Vulkan))),
    TestContextTypeFormatParamsToString);
INSTANTIATE_TEST_SUITE_P(
    ,
    IOSurfaceImageBackingFactoryGMBTest,
    testing::Combine(
        kGMBFormats,
        testing::Values(ContextTypeParams(GrContextType::kGL),
                        ContextTypeParams(GrContextType::kGraphiteDawn,
                                          wgpu::BackendType::Metal),
                        ContextTypeParams(GrContextType::kGraphiteDawn,
                                          wgpu::BackendType::Vulkan))),
    TestContextTypeFormatParamsToString);

}  // namespace gpu
