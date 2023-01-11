// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/iosurface_image_backing_factory.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/progress_reporter.h"

#if BUILDFLAG(USE_DAWN)
#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>
#include <dawn/webgpu_cpp.h>
#endif  // BUILDFLAG(USE_DAWN)

using testing::AtLeast;

namespace gpu {
namespace {

bool IsGLSupported(viz::SharedImageFormat format) {
  return format.is_single_plane() && !format.IsLegacyMultiplanar() &&
         format != viz::SinglePlaneFormat::kBGR_565;
}

}  // namespace

class IOSurfaceImageBackingFactoryTest : public testing::Test {
 public:
  void SetUp() override {
    surface_ = gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplayEGL(),
                                                  gfx::Size());
    ASSERT_TRUE(surface_);
    context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                         gl::GLContextAttribs());
    ASSERT_TRUE(context_);
    bool result = context_->MakeCurrent(surface_.get());
    ASSERT_TRUE(result);

    GpuPreferences preferences;
    preferences.use_passthrough_cmd_decoder = true;
    preferences.texture_target_exception_list.push_back(
        gfx::BufferUsageAndFormat(gfx::BufferUsage::SCANOUT,
                                  gfx::BufferFormat::RGBA_8888));

    GpuDriverBugWorkarounds workarounds;
    scoped_refptr<gl::GLShareGroup> share_group = new gl::GLShareGroup();
    context_state_ = base::MakeRefCounted<SharedContextState>(
        std::move(share_group), surface_, context_,
        false /* use_virtualized_gl_contexts */, base::DoNothing());
    context_state_->InitializeGrContext(preferences, workarounds, nullptr);
    auto feature_info =
        base::MakeRefCounted<gles2::FeatureInfo>(workarounds, GpuFeatureInfo());
    context_state_->InitializeGL(preferences, std::move(feature_info));

    backing_factory_ = std::make_unique<IOSurfaceImageBackingFactory>(
        preferences, workarounds, context_state_->feature_info(),
        /*progress_reporter=*/nullptr);

    memory_type_tracker_ = std::make_unique<MemoryTypeTracker>(nullptr);
    shared_image_representation_factory_ =
        std::make_unique<SharedImageRepresentationFactory>(
            &shared_image_manager_, nullptr);
  }

  GrDirectContext* gr_context() { return context_state_->gr_context(); }

 protected:
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<SharedContextState> context_state_;
  std::unique_ptr<IOSurfaceImageBackingFactory> backing_factory_;
  SharedImageManager shared_image_manager_;
  std::unique_ptr<MemoryTypeTracker> memory_type_tracker_;
  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;

  void CheckSkiaPixels(const Mailbox& mailbox,
                       const gfx::Size& size,
                       const std::vector<uint8_t> expected_color) {
    auto skia_representation =
        shared_image_representation_factory_->ProduceSkia(mailbox,
                                                          context_state_);
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
    auto sk_image = SkImage::MakeFromTexture(
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
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(1, 1);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_SCANOUT;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);
  EXPECT_TRUE(backing);
  backing->SetCleared();

  GLenum expected_target = GL_TEXTURE_RECTANGLE;
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  // Create a GLTextureImageRepresentation.
  {
    auto gl_representation =
        shared_image_representation_factory_->ProduceGLTexturePassthrough(
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

#if BUILDFLAG(USE_DAWN)
// Test to check interaction between Dawn and skia GL representations.
TEST_F(IOSurfaceImageBackingFactoryTest, Dawn_SkiaGL) {
  // Create a Dawn Metal device
  dawn::native::Instance instance;
  instance.DiscoverDefaultAdapters();

  std::vector<dawn::native::Adapter> adapters = instance.GetAdapters();
  auto adapter_it = base::ranges::find(adapters, wgpu::BackendType::Metal,
                                       [](dawn::native::Adapter adapter) {
                                         wgpu::AdapterProperties properties;
                                         adapter.GetProperties(&properties);
                                         return properties.backendType;
                                       });
  ASSERT_NE(adapter_it, adapters.end());

  dawn::native::DawnDeviceDescriptor device_descriptor;
  // We need to request internal usage to be able to do operations with
  // internal methods that would need specific usages.
  device_descriptor.requiredFeatures.push_back("dawn-internal-usages");

  wgpu::Device device =
      wgpu::Device::Acquire(adapter_it->CreateDevice(&device_descriptor));
  DawnProcTable procs = dawn::native::GetProcs();
  dawnProcSetProcs(&procs);

  // Create a backing using mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(1, 1);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  uint32_t usage = SHARED_IMAGE_USAGE_WEBGPU | SHARED_IMAGE_USAGE_SCANOUT;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);
  EXPECT_TRUE(backing);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  // Create a DawnImageRepresentation.
  auto dawn_representation = shared_image_representation_factory_->ProduceDawn(
      mailbox, device.Get(), WGPUBackendType_Metal, {});
  EXPECT_TRUE(dawn_representation);

  // Clear the shared image to green using Dawn.
  {
    auto scoped_access = dawn_representation->BeginScopedAccess(
        WGPUTextureUsage_RenderAttachment,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(scoped_access);
    wgpu::Texture texture(scoped_access->texture());

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

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.EndPass();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetQueue();
    queue.Submit(1, &commands);
  }

  CheckSkiaPixels(mailbox, size, {0, 255, 0, 255});

  // Shut down Dawn
  device = wgpu::Device();
  dawnProcSetProcs(nullptr);

  factory_ref.reset();
}

// 1. Draw a color to texture through GL
// 2. Do not call SetCleared so we can test Dawn Lazy clear
// 3. Begin render pass in Dawn, but do not do anything
// 4. Verify through CheckSkiaPixel that GL drawn color not seen
TEST_F(IOSurfaceImageBackingFactoryTest, GL_Dawn_Skia_UnclearTexture) {
  // Create a backing using mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  const uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_SCANOUT |
                         SHARED_IMAGE_USAGE_WEBGPU;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);
  EXPECT_TRUE(backing);

  GLenum expected_target = GL_TEXTURE_RECTANGLE;
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  {
    // Create a GLTextureImageRepresentation.
    auto gl_representation =
        shared_image_representation_factory_->ProduceGLTexturePassthrough(
            mailbox);
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

  // Create a Dawn Metal device
  dawn::native::Instance instance;
  instance.DiscoverDefaultAdapters();

  std::vector<dawn::native::Adapter> adapters = instance.GetAdapters();
  auto adapter_it = base::ranges::find(adapters, wgpu::BackendType::Metal,
                                       [](dawn::native::Adapter adapter) {
                                         wgpu::AdapterProperties properties;
                                         adapter.GetProperties(&properties);
                                         return properties.backendType;
                                       });
  ASSERT_NE(adapter_it, adapters.end());

  dawn::native::DawnDeviceDescriptor device_descriptor;
  // We need to request internal usage to be able to do operations with
  // internal methods that would need specific usages.
  device_descriptor.requiredFeatures.push_back("dawn-internal-usages");

  wgpu::Device device =
      wgpu::Device::Acquire(adapter_it->CreateDevice(&device_descriptor));
  DawnProcTable procs = dawn::native::GetProcs();
  dawnProcSetProcs(&procs);
  {
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(
            mailbox, device.Get(), WGPUBackendType_Metal, {});
    ASSERT_TRUE(dawn_representation);

    auto dawn_scoped_access = dawn_representation->BeginScopedAccess(
        WGPUTextureUsage_RenderAttachment,
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
    pass.EndPass();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetQueue();
    queue.Submit(1, &commands);
  }

  // Check skia pixels returns black since texture was lazy cleared in Dawn
  EXPECT_TRUE(factory_ref->IsCleared());
  CheckSkiaPixels(mailbox, size, {0, 0, 0, 0});

  // Shut down Dawn
  device = wgpu::Device();
  dawnProcSetProcs(nullptr);

  factory_ref.reset();
}

// 1. Draw  a color to texture through Dawn
// 2. Set the renderpass storeOp = Clear
// 3. Texture in Dawn will stay as uninitialized
// 4. Expect skia to fail to access the texture because texture is not
// initialized
TEST_F(IOSurfaceImageBackingFactoryTest, UnclearDawn_SkiaFails) {
  // Create a backing using mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  const uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_SCANOUT |
                         SHARED_IMAGE_USAGE_WEBGPU;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  // Create dawn device
  dawn::native::Instance instance;
  instance.DiscoverDefaultAdapters();

  std::vector<dawn::native::Adapter> adapters = instance.GetAdapters();
  auto adapter_it = base::ranges::find(adapters, wgpu::BackendType::Metal,
                                       [](dawn::native::Adapter adapter) {
                                         wgpu::AdapterProperties properties;
                                         adapter.GetProperties(&properties);
                                         return properties.backendType;
                                       });
  ASSERT_NE(adapter_it, adapters.end());

  dawn::native::DawnDeviceDescriptor device_descriptor;
  // We need to request internal usage to be able to do operations with
  // internal methods that would need specific usages.
  device_descriptor.requiredFeatures.push_back("dawn-internal-usages");

  wgpu::Device device =
      wgpu::Device::Acquire(adapter_it->CreateDevice(&device_descriptor));
  DawnProcTable procs = dawn::native::GetProcs();
  dawnProcSetProcs(&procs);
  {
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(
            mailbox, device.Get(), WGPUBackendType_Metal, {});
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
    color_desc.storeOp = wgpu::StoreOp::Discard;
    color_desc.clearValue = {0, 255, 0, 255};

    wgpu::RenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &color_desc;
    renderPassDesc.depthStencilAttachment = nullptr;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.EndPass();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetQueue();
    queue.Submit(1, &commands);
  }

  // Shut down Dawn
  device = wgpu::Device();
  dawnProcSetProcs(nullptr);

  EXPECT_FALSE(factory_ref->IsCleared());

  // Produce skia representation
  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      mailbox, context_state_);
  ASSERT_NE(skia_representation, nullptr);

  // Expect BeginScopedReadAccess to fail because sharedImage is uninitialized
  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
      scoped_read_access =
          skia_representation->BeginScopedReadAccess(nullptr, nullptr);
  EXPECT_EQ(scoped_read_access, nullptr);
}
#endif  // BUILDFLAG(USE_DAWN)

// Test that Skia trying to access uninitialized SharedImage will fail
TEST_F(IOSurfaceImageBackingFactoryTest, SkiaAccessFirstFails) {
  // Create a mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  const uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_SCANOUT;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      mailbox, context_state_);
  ASSERT_NE(skia_representation, nullptr);
  EXPECT_FALSE(skia_representation->IsCleared());

  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
      scoped_read_access =
          skia_representation->BeginScopedReadAccess(nullptr, nullptr);
  // Expect BeginScopedReadAccess to fail because sharedImage is uninitialized
  EXPECT_EQ(scoped_read_access, nullptr);
}

void CreateSharedContext(const GpuDriverBugWorkarounds& workarounds,
                         scoped_refptr<gl::GLSurface>& surface,
                         scoped_refptr<gl::GLContext>& context,
                         scoped_refptr<SharedContextState>& context_state,
                         scoped_refptr<gles2::FeatureInfo>& feature_info) {
  surface =
      gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(), gfx::Size());
  ASSERT_TRUE(surface);
  context =
      gl::init::CreateGLContext(nullptr, surface.get(), gl::GLContextAttribs());
  ASSERT_TRUE(context);
  bool result = context->MakeCurrent(surface.get());
  ASSERT_TRUE(result);

  scoped_refptr<gl::GLShareGroup> share_group = new gl::GLShareGroup();
  feature_info =
      base::MakeRefCounted<gles2::FeatureInfo>(workarounds, GpuFeatureInfo());
  context_state = base::MakeRefCounted<SharedContextState>(
      std::move(share_group), surface, context,
      false /* use_virtualized_gl_contexts */, base::DoNothing());
  context_state->InitializeGrContext(GpuPreferences(), workarounds, nullptr);
  context_state->InitializeGL(GpuPreferences(), feature_info);
}

class MockProgressReporter : public gl::ProgressReporter {
 public:
  MockProgressReporter() = default;
  ~MockProgressReporter() override = default;

  // gl::ProgressReporter implementation.
  MOCK_METHOD0(ReportProgress, void());
};

class IOSurfaceImageBackingFactoryWithFormatTestBase
    : public testing::TestWithParam<viz::SharedImageFormat> {
 public:
  explicit IOSurfaceImageBackingFactoryWithFormatTestBase()
      : shared_image_manager_(
            std::make_unique<SharedImageManager>(/*thread_safe=*/false)) {}
  ~IOSurfaceImageBackingFactoryWithFormatTestBase() override {
    // |context_state_| must be destroyed on its own context.
    context_state_->MakeCurrent(surface_.get(), true /* needs_gl */);
  }

  void SetUp() override {
    GpuDriverBugWorkarounds workarounds;
    scoped_refptr<gles2::FeatureInfo> feature_info;
    CreateSharedContext(workarounds, surface_, context_, context_state_,
                        feature_info);
    supports_etc1_ =
        feature_info->validators()->compressed_texture_format.IsValid(
            GL_ETC1_RGB8_OES);
    supports_ar30_ = feature_info->feature_flags().chromium_image_ar30;
    supports_ab30_ = feature_info->feature_flags().chromium_image_ab30;
    supports_ycbcr_420v_ =
        feature_info->feature_flags().chromium_image_ycbcr_420v;
    supports_ycbcr_p010_ =
        feature_info->feature_flags().chromium_image_ycbcr_p010;

    GpuPreferences preferences;
    preferences.use_passthrough_cmd_decoder = true;
    backing_factory_ = std::make_unique<IOSurfaceImageBackingFactory>(
        preferences, workarounds, context_state_->feature_info(),
        &progress_reporter_);

    memory_type_tracker_ = std::make_unique<MemoryTypeTracker>(nullptr);
    shared_image_representation_factory_ =
        std::make_unique<SharedImageRepresentationFactory>(
            shared_image_manager_.get(), nullptr);
  }

  viz::SharedImageFormat get_format() { return GetParam(); }

 protected:
  ::testing::NiceMock<MockProgressReporter> progress_reporter_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<SharedContextState> context_state_;
  std::unique_ptr<IOSurfaceImageBackingFactory> backing_factory_;
  std::unique_ptr<SharedImageManager> shared_image_manager_;
  std::unique_ptr<MemoryTypeTracker> memory_type_tracker_;
  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  bool supports_etc1_ = false;
  bool supports_ar30_ = false;
  bool supports_ab30_ = false;
  bool supports_ycbcr_p010_ = false;
  bool supports_ycbcr_420v_ = false;
};

// SharedImageFormat parameterized tests.
class IOSurfaceImageBackingFactoryScanoutTest
    : public IOSurfaceImageBackingFactoryWithFormatTestBase {
 public:
  bool can_create_scanout_shared_image(viz::SharedImageFormat format) const {
    if (format.is_multi_plane()) {
      return false;
    } else if (format == viz::SinglePlaneFormat::kBGRA_1010102) {
      return supports_ar30_;
    } else if (format == viz::SinglePlaneFormat::kRGBA_1010102) {
      return supports_ab30_;
    }
    return true;
  }
};

TEST_P(IOSurfaceImageBackingFactoryScanoutTest, Basic) {
  const bool should_succeed = can_create_scanout_shared_image(get_format());
  if (should_succeed) {
    EXPECT_CALL(progress_reporter_, ReportProgress).Times(AtLeast(1));
  }
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = get_format();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_SCANOUT;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);

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

  // First, validate a GLTexturePassthroughImageRepresentation.
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_->Register(std::move(backing),
                                      memory_type_tracker_.get());
  EXPECT_TRUE(shared_image);
  {
    auto gl_representation =
        shared_image_representation_factory_->ProduceGLTexturePassthrough(
            mailbox);
    EXPECT_TRUE(gl_representation);
    EXPECT_TRUE(gl_representation->GetTexturePassthrough()->service_id());
    EXPECT_EQ(size, gl_representation->size());
    EXPECT_EQ(format, gl_representation->format());
    EXPECT_EQ(color_space, gl_representation->color_space());
    EXPECT_EQ(usage, gl_representation->usage());
    gl_representation.reset();
  }

  if (format == viz::SinglePlaneFormat::kBGRA_1010102) {
    // Producing SkSurface for BGRA_1010102 fails for some reason.
    return;
  }

  // Finally, validate a SkiaImageRepresentation.
  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      mailbox, context_state_.get());
  EXPECT_TRUE(skia_representation);
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
      scoped_write_access;
  scoped_write_access = skia_representation->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  auto* surface = scoped_write_access->surface();
  EXPECT_TRUE(surface);
  EXPECT_EQ(size.width(), surface->width());
  EXPECT_EQ(size.height(), surface->height());
  scoped_write_access.reset();

  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess> scoped_read_access;
  scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  auto* promise_texture = scoped_read_access->promise_image_texture();
  EXPECT_TRUE(promise_texture);
  EXPECT_TRUE(begin_semaphores.empty());
  EXPECT_TRUE(end_semaphores.empty());
  if (promise_texture) {
    GrBackendTexture backend_texture = promise_texture->backendTexture();
    EXPECT_TRUE(backend_texture.isValid());
    EXPECT_EQ(size.width(), backend_texture.width());
    EXPECT_EQ(size.height(), backend_texture.height());
  }
  scoped_read_access.reset();
  skia_representation.reset();

  shared_image.reset();
}

TEST_P(IOSurfaceImageBackingFactoryScanoutTest, InitialData) {
  auto format = GetParam();
  const bool should_succeed = can_create_scanout_shared_image(format);
  if (should_succeed) {
    EXPECT_CALL(progress_reporter_, ReportProgress).Times(AtLeast(1));
  }

  auto mailbox = Mailbox::GenerateForSharedImage();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_SCANOUT;
  std::vector<uint8_t> initial_data(
      viz::ResourceSizes::CheckedSizeInBytes<unsigned int>(size, format));

  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      initial_data);
  ::testing::Mock::VerifyAndClearExpectations(&progress_reporter_);
  if (!should_succeed) {
    EXPECT_FALSE(backing);
    return;
  }
  ASSERT_TRUE(backing);
  EXPECT_TRUE(backing->IsCleared());

  // Validate via a GLTextureImageRepresentation(Passthrough).
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_->Register(std::move(backing),
                                      memory_type_tracker_.get());
  EXPECT_TRUE(shared_image);
  GLenum expected_target = GL_TEXTURE_RECTANGLE;

  {
    auto gl_representation =
        shared_image_representation_factory_->ProduceGLTexturePassthrough(
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
  }
}

TEST_P(IOSurfaceImageBackingFactoryScanoutTest, InitialDataImage) {
  const bool should_succeed = can_create_scanout_shared_image(get_format());
  if (should_succeed) {
    EXPECT_CALL(progress_reporter_, ReportProgress).Times(AtLeast(1));
  }
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = get_format();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_SCANOUT;
  std::vector<uint8_t> initial_data(256 * 256 * 4);
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      initial_data);
  if (!should_succeed) {
    EXPECT_FALSE(backing);
    return;
  }
  ASSERT_TRUE(backing);

  // Validate via a GLTextureImageRepresentation(Passthrough).
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_->Register(std::move(backing),
                                      memory_type_tracker_.get());
  EXPECT_TRUE(shared_image);
  {
    auto gl_representation =
        shared_image_representation_factory_->ProduceGLTexturePassthrough(
            mailbox);
    EXPECT_TRUE(gl_representation);
    EXPECT_TRUE(gl_representation->GetTexturePassthrough()->service_id());
    EXPECT_EQ(size, gl_representation->size());
    EXPECT_EQ(format, gl_representation->format());
    EXPECT_EQ(color_space, gl_representation->color_space());
    EXPECT_EQ(usage, gl_representation->usage());
    gl_representation.reset();
  }
}

TEST_P(IOSurfaceImageBackingFactoryScanoutTest, InitialDataWrongSize) {
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = get_format();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_SCANOUT;
  std::vector<uint8_t> initial_data_small(256 * 128 * 4);
  std::vector<uint8_t> initial_data_large(256 * 512 * 4);
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      initial_data_small);
  EXPECT_FALSE(backing);
  backing = backing_factory_->CreateSharedImage(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      initial_data_large);
  EXPECT_FALSE(backing);
}

TEST_P(IOSurfaceImageBackingFactoryScanoutTest, InvalidFormat) {
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::SharedImageFormat::SinglePlane(
      viz::ResourceFormat::YUV_420_BIPLANAR);
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  uint32_t usage = SHARED_IMAGE_USAGE_SCANOUT;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);
  EXPECT_FALSE(backing);
}

TEST_P(IOSurfaceImageBackingFactoryScanoutTest, InvalidSize) {
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = get_format();
  gfx::Size size(0, 0);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  uint32_t usage = SHARED_IMAGE_USAGE_SCANOUT;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);
  EXPECT_FALSE(backing);

  size = gfx::Size(INT_MAX, INT_MAX);
  backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);
  EXPECT_FALSE(backing);
}

TEST_P(IOSurfaceImageBackingFactoryScanoutTest, EstimatedSize) {
  const bool should_succeed = can_create_scanout_shared_image(get_format());
  if (should_succeed) {
    EXPECT_CALL(progress_reporter_, ReportProgress).Times(AtLeast(1));
  }
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = get_format();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  uint32_t usage = SHARED_IMAGE_USAGE_SCANOUT;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);

  if (!should_succeed) {
    EXPECT_FALSE(backing);
    return;
  }
  ASSERT_TRUE(backing);

  size_t backing_estimated_size = backing->GetEstimatedSize();
  EXPECT_GT(backing_estimated_size, 0u);

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_->Register(std::move(backing),
                                      memory_type_tracker_.get());
  EXPECT_EQ(backing_estimated_size, memory_type_tracker_->GetMemRepresented());

  shared_image.reset();
}

// Ensures that the various conversion functions used w/ TexStorage2D match
// their TexImage2D equivalents, allowing us to minimize the amount of parallel
// data tracked in the SharedImageFactoryGLImage.
TEST_F(IOSurfaceImageBackingFactoryScanoutTest, TexImageTexStorageEquivalence) {
  scoped_refptr<gles2::FeatureInfo> feature_info =
      new gles2::FeatureInfo(GpuDriverBugWorkarounds(), GpuFeatureInfo());
  feature_info->Initialize(ContextType::CONTEXT_TYPE_OPENGLES2,
                           /*is_passthrough_cmd_decoder=*/true,
                           gles2::DisallowedFeatures());
  const gles2::Validators* validators = feature_info->validators();

  for (int i = 0; i <= viz::RESOURCE_FORMAT_MAX; ++i) {
    auto format = viz::SharedImageFormat::SinglePlane(
        static_cast<viz::ResourceFormat>(i));
    if (!IsGLSupported(format) || format.IsCompressed()) {
      continue;
    }
    int storage_format = TextureStorageFormat(
        format, feature_info->feature_flags().angle_rgbx_internal_format);

    int image_gl_format = GLDataFormat(format);
    int storage_gl_format =
        gles2::TextureManager::ExtractFormatFromStorageFormat(storage_format);
    EXPECT_EQ(image_gl_format, storage_gl_format);

    int image_gl_type = GLDataType(format);
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
    int image_internal_format = GLInternalFormat(format);
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
    : public IOSurfaceImageBackingFactoryWithFormatTestBase {
 public:
  bool can_create_gmb_shared_image(viz::SharedImageFormat format) const {
    if (format == viz::SinglePlaneFormat::kBGRA_1010102) {
      return supports_ar30_;
    } else if (format == viz::SinglePlaneFormat::kRGBA_1010102) {
      return supports_ab30_;
    } else if (format == viz::MultiPlaneFormat::kYUV_420_BIPLANAR) {
      return supports_ycbcr_420v_;
    } else if (format == viz::MultiPlaneFormat::kP010) {
      return supports_ycbcr_p010_;
    }
    return true;
  }
};

TEST_P(IOSurfaceImageBackingFactoryGMBTest, Basic) {
  const bool should_succeed = can_create_gmb_shared_image(get_format());
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = get_format();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_SCANOUT;

  gfx::BufferFormat buffer_format = gpu::ToBufferFormat(format);
  gfx::GpuMemoryBufferHandle handle;
  gfx::GpuMemoryBufferId kBufferId(1);
  handle.type = gfx::IO_SURFACE_BUFFER;
  handle.id = kBufferId;
  handle.io_surface.reset(gfx::CreateIOSurface(size, buffer_format));
  DCHECK(handle.io_surface);

  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(handle));

  if (!should_succeed) {
    EXPECT_FALSE(backing);
    return;
  }
  ASSERT_TRUE(backing);

  // Check clearing.
  if (!backing->IsCleared()) {
    backing->SetCleared();
    EXPECT_TRUE(backing->IsCleared());
  }

  // First, validate a GLTexturePassthroughImageRepresentation.
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_->Register(std::move(backing),
                                      memory_type_tracker_.get());
  EXPECT_TRUE(shared_image);
  {
    auto gl_representation =
        shared_image_representation_factory_->ProduceGLTexturePassthrough(
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
  }

  // Finally, validate a SkiaImageRepresentation.
  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      mailbox, context_state_.get());
  EXPECT_TRUE(skia_representation);
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess> scoped_read_access;
  scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  EXPECT_TRUE(begin_semaphores.empty());
  EXPECT_TRUE(end_semaphores.empty());
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
  scoped_read_access.reset();

  // Producing SkSurface for BGRA_1010102 or P010 fails as Skia uses A16, RG16
  // formats as read-only for now. See
  // GrRecordingContext::colorTypeSupportedAsSurface() for all unsupported
  // types.
  if (format == viz::SinglePlaneFormat::kBGRA_1010102 ||
      format == viz::MultiPlaneFormat::kP010) {
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

const auto kSinglePlaneFormats =
    ::testing::Values(viz::SinglePlaneFormat::kRGBA_8888,
                      viz::SinglePlaneFormat::kBGRA_1010102,
                      viz::SinglePlaneFormat::kRGBA_1010102);

const auto kGMBFormats =
    ::testing::Values(viz::SinglePlaneFormat::kRGBA_8888,
                      viz::SinglePlaneFormat::kBGRA_1010102,
                      viz::MultiPlaneFormat::kYUV_420_BIPLANAR,
                      viz::MultiPlaneFormat::kP010);

std::string TestParamToString(
    const testing::TestParamInfo<viz::SharedImageFormat>& param_info) {
  return param_info.param.ToTestParamString();
}

INSTANTIATE_TEST_SUITE_P(,
                         IOSurfaceImageBackingFactoryScanoutTest,
                         kSinglePlaneFormats,
                         TestParamToString);
INSTANTIATE_TEST_SUITE_P(,
                         IOSurfaceImageBackingFactoryGMBTest,
                         kGMBFormats,
                         TestParamToString);

}  // namespace gpu
