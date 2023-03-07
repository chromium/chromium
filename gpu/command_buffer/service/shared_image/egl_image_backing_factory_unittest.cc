// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/egl_image_backing_factory.h"

#include <thread>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/test_utils.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>
#include <dawn/webgpu_cpp.h>
#endif  // BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)

using testing::AtLeast;

namespace gpu {
namespace {

bool IsEglImageSupported() {
  // Creating a context and making it current to initialize dynamic bindings
  // which is needed to query extensions from current gl driver.
  scoped_refptr<gl::GLSurface> surface = gl::init::CreateOffscreenGLSurface(
      gl::GetDefaultDisplayEGL(), gfx::Size());
  DCHECK(surface);
  scoped_refptr<gl::GLContext> context =
      gl::init::CreateGLContext(nullptr, surface.get(), gl::GLContextAttribs());
  DCHECK(context);
  bool result = context->MakeCurrent(surface.get());
  DCHECK(result);

  // Check the required extensions to support egl images.
  auto* egl_display = gl::GetDefaultDisplayEGL();
  if (egl_display && egl_display->ext->b_EGL_KHR_image_base &&
      egl_display->ext->b_EGL_KHR_gl_texture_2D_image &&
      egl_display->ext->b_EGL_KHR_fence_sync &&
      gl::g_current_gl_driver->ext.b_GL_OES_EGL_image) {
    return true;
  }
  return false;
}

void CreateSharedContext(const GpuDriverBugWorkarounds& workarounds,
                         scoped_refptr<gl::GLSurface>& surface,
                         scoped_refptr<gl::GLContext>& context,
                         scoped_refptr<SharedContextState>& context_state,
                         scoped_refptr<gles2::FeatureInfo>& feature_info) {
  surface = gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplayEGL(),
                                               gfx::Size());
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
      /*use_virtualized_gl_contexts=*/false, base::DoNothing());
  context_state->InitializeGrContext(GpuPreferences(), workarounds, nullptr);
  context_state->InitializeGL(GpuPreferences(), feature_info);
}

class EGLImageBackingFactoryThreadSafeTest
    : public testing::TestWithParam<std::tuple<bool, viz::SharedImageFormat>> {
 public:
  EGLImageBackingFactoryThreadSafeTest()
      : shared_image_manager_(std::make_unique<SharedImageManager>(true)) {}
  ~EGLImageBackingFactoryThreadSafeTest() override {
    // |context_state_| and |context_state2_| must be destroyed on its own
    // context.
    if (context_state2_) {
      context_state2_->MakeCurrent(surface2_.get(), /*needs_gl=*/true);
      context_state2_.reset();
    }
    if (context_state_) {
      context_state_->MakeCurrent(surface_.get(), /*needs_gl=*/true);
      context_state_.reset();
    }
  }

  void SetUp() override {
    if (!IsEglImageSupported())
      return;

    GpuDriverBugWorkarounds workarounds;

    scoped_refptr<gles2::FeatureInfo> feature_info;
    CreateSharedContext(workarounds, surface_, context_, context_state_,
                        feature_info);

    GpuPreferences preferences;
    preferences.use_passthrough_cmd_decoder = use_passthrough();
    backing_factory_ = std::make_unique<EGLImageBackingFactory>(
        preferences, workarounds, context_state_->feature_info());

    memory_type_tracker_ = std::make_unique<MemoryTypeTracker>(nullptr);
    shared_image_representation_factory_ =
        std::make_unique<SharedImageRepresentationFactory>(
            shared_image_manager_.get(), nullptr);

    // Create 2nd context/context_state which are not part of same shared group.
    scoped_refptr<gles2::FeatureInfo> feature_info2;
    CreateSharedContext(workarounds, surface2_, context2_, context_state2_,
                        feature_info2);
    feature_info2.reset();
  }

  bool use_passthrough() {
    return std::get<0>(GetParam()) &&
           gles2::PassthroughCommandDecoderSupported();
  }

  viz::SharedImageFormat get_format() { return std::get<1>(GetParam()); }

 protected:
#if BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
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
        context_state_->gr_context(), backend_texture, kTopLeft_GrSurfaceOrigin,
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
#endif  // BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<SharedContextState> context_state_;
  std::unique_ptr<EGLImageBackingFactory> backing_factory_;
  gles2::MailboxManagerImpl mailbox_manager_;
  std::unique_ptr<SharedImageManager> shared_image_manager_;
  std::unique_ptr<MemoryTypeTracker> memory_type_tracker_;
  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;

  scoped_refptr<gl::GLSurface> surface2_;
  scoped_refptr<gl::GLContext> context2_;
  scoped_refptr<SharedContextState> context_state2_;
};

class CreateAndValidateSharedImageRepresentations {
 public:
  CreateAndValidateSharedImageRepresentations(
      EGLImageBackingFactory* backing_factory,
      viz::SharedImageFormat format,
      bool is_thread_safe,
      gles2::MailboxManagerImpl* mailbox_manager,
      SharedImageManager* shared_image_manager,
      MemoryTypeTracker* memory_type_tracker,
      SharedImageRepresentationFactory* shared_image_representation_factory,
      SharedContextState* context_state,
      bool upload_initial_data);
  ~CreateAndValidateSharedImageRepresentations();

  gfx::Size size() { return size_; }
  Mailbox mailbox() { return mailbox_; }

 private:
  raw_ptr<gles2::MailboxManagerImpl> mailbox_manager_;
  gfx::Size size_;
  Mailbox mailbox_;
  std::unique_ptr<SharedImageBacking> backing_;
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image_;
};

// Intent of this test is to create at thread safe backing and test if all
// representations are working.
TEST_P(EGLImageBackingFactoryThreadSafeTest, BasicThreadSafe) {
  if (!IsEglImageSupported())
    return;

  CreateAndValidateSharedImageRepresentations shared_image(
      backing_factory_.get(), get_format(), /*is_thread_safe=*/true,
      &mailbox_manager_, shared_image_manager_.get(),
      memory_type_tracker_.get(), shared_image_representation_factory_.get(),
      context_state_.get(), /*upload_initial_data=*/false);
}

// Intent of this test is to create at thread safe backing with initial pixel
// data and test if all representations are working.
TEST_P(EGLImageBackingFactoryThreadSafeTest, BasicInitialData) {
  if (!IsEglImageSupported())
    return;

  CreateAndValidateSharedImageRepresentations shared_image(
      backing_factory_.get(), get_format(), /*is_thread_safe=*/true,
      &mailbox_manager_, shared_image_manager_.get(),
      memory_type_tracker_.get(), shared_image_representation_factory_.get(),
      context_state_.get(), /*upload_initial_data=*/true);
}

// Intent of this test is to use the shared image mailbox system by 2 different
// threads each running their own GL context which are not part of same shared
// group. One thread will be writing to the backing and other thread will be
// reading from it.
TEST_P(EGLImageBackingFactoryThreadSafeTest, OneWriterOneReader) {
  if (!IsEglImageSupported())
    return;

  // Create it on 1st SharedContextState |context_state_|.
  CreateAndValidateSharedImageRepresentations shared_image(
      backing_factory_.get(), get_format(), /*is_thread_safe=*/true,
      &mailbox_manager_, shared_image_manager_.get(),
      memory_type_tracker_.get(), shared_image_representation_factory_.get(),
      context_state_.get(), /*upload_initial_data=*/false);

  auto mailbox = shared_image.mailbox();
  auto size = shared_image.size();

  // Writer will write to the backing. We will create a GLTexture representation
  // and write green color to it.
  auto gl_representation =
      shared_image_representation_factory_->ProduceGLTexture(mailbox);
  EXPECT_TRUE(gl_representation);

  // Begin writing to the underlying texture of the backing via ScopedAccess.
  std::unique_ptr<GLTextureImageRepresentation::ScopedAccess>
      writer_scoped_access = gl_representation->BeginScopedAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
          SharedImageRepresentation::AllowUnclearedAccess::kNo);

  DCHECK(writer_scoped_access);

  // Create an FBO.
  GLuint fbo = 0;
  gl::GLApi* api = gl::g_current_gl_context;
  api->glGenFramebuffersEXTFn(1, &fbo);
  api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, fbo);

  // Attach the texture to FBO.
  api->glFramebufferTexture2DEXTFn(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      gl_representation->GetTexture()->target(),
      gl_representation->GetTexture()->service_id(), 0);

  // Set the clear color to green.
  api->glClearColorFn(0.0f, 1.0f, 0.0f, 1.0f);
  api->glClearFn(GL_COLOR_BUFFER_BIT);
  gl_representation->GetTexture()->SetLevelCleared(
      gl_representation->GetTexture()->target(), 0, true);

  // End writing.
  writer_scoped_access.reset();
  gl_representation.reset();

  // Read from the backing in a separate thread. Read is done via
  // SkiaGLImageRepresentation. ReadPixels() creates/produces a
  // SkiaGLImageRepresentation which in turn wraps a
  // GLTextureImageRepresentation when for GL mode. Hence testing reading via
  // SkiaGLImageRepresentation is equivalent to testing via
  // GLTextureImageRepresentation.
  std::vector<uint8_t> dst_pixels;

  // Launch 2nd thread.
  std::thread second_thread([&]() {
    // Do ReadPixels() on 2nd SharedContextState |context_state2_|.
    dst_pixels = ReadPixels(mailbox, size, context_state2_.get(),
                            shared_image_representation_factory_.get());
  });

  // Wait for this thread to be done.
  second_thread.join();

  // Compare the pixel values.
  EXPECT_EQ(dst_pixels[0], 0);
  EXPECT_EQ(dst_pixels[1], 255);
  EXPECT_EQ(dst_pixels[2], 0);
  EXPECT_EQ(dst_pixels[3], 255);
}

#if BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
// Test to check interaction between Dawn and skia GL representations.
TEST_F(EGLImageBackingFactoryThreadSafeTest, Dawn_SkiaGL) {
  if (!IsEglImageSupported()) {
    return;
  }

  // Create a Dawm OpenGLES device.
  dawn::native::Instance instance;
  instance.DiscoverDefaultAdapters();

  std::vector<dawn::native::Adapter> adapters = instance.GetAdapters();

  // Using wgpu::BackendType::OpenGLES.
  auto adapter_it = base::ranges::find(adapters, wgpu::BackendType::OpenGLES,
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
  const auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::SinglePlaneFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  const uint32_t usage =
      SHARED_IMAGE_USAGE_WEBGPU | SHARED_IMAGE_USAGE_DISPLAY_READ;

  // Note that this backing is always thread safe by default even if it is not
  // requested to be.
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage,
      /* is_thread_safe=*/true);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_->Register(std::move(backing),
                                      memory_type_tracker_.get());

  // Clear the shared image to green using Dawn.
  {
    // Create a DawnImageRepresentation using WGPUBackendType_OpenGLES backend.
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(
            mailbox, device.Get(), WGPUBackendType_OpenGLES, {});
    ASSERT_TRUE(dawn_representation);

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
#endif  // BUILDFLAG(USE_DAWN) && BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)

CreateAndValidateSharedImageRepresentations::
    CreateAndValidateSharedImageRepresentations(
        EGLImageBackingFactory* backing_factory,
        viz::SharedImageFormat format,
        bool is_thread_safe,
        gles2::MailboxManagerImpl* mailbox_manager,
        SharedImageManager* shared_image_manager,
        MemoryTypeTracker* memory_type_tracker,
        SharedImageRepresentationFactory* shared_image_representation_factory,
        SharedContextState* context_state,
        bool upload_initial_data)
    : mailbox_manager_(mailbox_manager), size_(256, 256) {
  // Make the context current.
  DCHECK(context_state);
  EXPECT_TRUE(
      context_state->MakeCurrent(context_state->surface(), true /* needs_gl*/));
  mailbox_ = Mailbox::GenerateForSharedImage();
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;

  // SHARED_IMAGE_USAGE_DISPLAY_READ for skia read and SHARED_IMAGE_USAGE_RASTER
  // for skia write.
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_RASTER;
  if (!is_thread_safe)
    usage |= SHARED_IMAGE_USAGE_DISPLAY_READ;
  if (upload_initial_data) {
    std::vector<uint8_t> initial_data(
        viz::ResourceSizes::CheckedSizeInBytes<unsigned int>(size_, format));
    backing_ = backing_factory->CreateSharedImage(
        mailbox_, format, size_, color_space, surface_origin, alpha_type, usage,
        initial_data);
  } else {
    backing_ = backing_factory->CreateSharedImage(
        mailbox_, format, surface_handle, size_, color_space, surface_origin,
        alpha_type, usage, is_thread_safe);
  }

  // As long as either |chromium_image_ar30| or |chromium_image_ab30| is
  // enabled, we can create a non-scanout SharedImage with format
  // viz::SinglePlaneFormat::{BGRA,RGBA}_1010102.
  const bool supports_ar30 =
      context_state->feature_info()->feature_flags().chromium_image_ar30;
  const bool supports_ab30 =
      context_state->feature_info()->feature_flags().chromium_image_ab30;
  if ((format == viz::SinglePlaneFormat::kBGRA_1010102 ||
       format == viz::SinglePlaneFormat::kRGBA_1010102) &&
      !supports_ar30 && !supports_ab30) {
    EXPECT_FALSE(backing_);
    return;
  }
  EXPECT_TRUE(backing_);
  if (!backing_)
    return;

  // Check clearing.
  if (!backing_->IsCleared()) {
    backing_->SetCleared();
    EXPECT_TRUE(backing_->IsCleared());
  }

  GLenum expected_target = GL_TEXTURE_2D;
  shared_image_ =
      shared_image_manager->Register(std::move(backing_), memory_type_tracker);

  // Create and validate GLTexture representation.
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

  // Create and Validate Skia Representations.
  auto skia_representation =
      shared_image_representation_factory->ProduceSkia(mailbox_, context_state);
  EXPECT_TRUE(skia_representation);
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;

  std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
      scoped_write_access;
  scoped_write_access = skia_representation->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);
  // We use |supports_ar30| and |supports_ab30| to detect RGB10A2/BGR10A2
  // support. It's possible Skia might support these formats even if the Chrome
  // feature flags are false. We just check here that the feature flags don't
  // allow Chrome to do something that Skia doesn't support.
  if ((format != viz::SinglePlaneFormat::kBGRA_1010102 || supports_ar30) &&
      (format != viz::SinglePlaneFormat::kRGBA_1010102 || supports_ab30)) {
    EXPECT_TRUE(scoped_write_access);
    if (!scoped_write_access)
      return;
    auto* surface = scoped_write_access->surface();
    EXPECT_TRUE(surface);
    if (!surface)
      return;
    EXPECT_EQ(size_.width(), surface->width());
    EXPECT_EQ(size_.height(), surface->height());
  }
  EXPECT_TRUE(begin_semaphores.empty());
  EXPECT_TRUE(end_semaphores.empty());
  scoped_write_access.reset();

  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess> scoped_read_access;
  scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  auto* promise_texture = scoped_read_access->promise_image_texture();
  EXPECT_TRUE(promise_texture);
  EXPECT_TRUE(begin_semaphores.empty());
  EXPECT_TRUE(end_semaphores.empty());
  GrBackendTexture backend_texture = promise_texture->backendTexture();
  EXPECT_TRUE(backend_texture.isValid());
  EXPECT_EQ(size_.width(), backend_texture.width());
  EXPECT_EQ(size_.height(), backend_texture.height());
  scoped_read_access.reset();
  skia_representation.reset();
}

CreateAndValidateSharedImageRepresentations::
    ~CreateAndValidateSharedImageRepresentations() {
  shared_image_.reset();
  EXPECT_FALSE(mailbox_manager_->ConsumeTexture(mailbox_));
}

// High bit depth rendering is not supported on Android.
const auto kSharedImageFormats =
    ::testing::Values(viz::SinglePlaneFormat::kRGBA_8888);

std::string TestParamToString(
    const testing::TestParamInfo<std::tuple<bool, viz::SharedImageFormat>>&
        param_info) {
  const bool allow_passthrough = std::get<0>(param_info.param);
  const viz::SharedImageFormat format = std::get<1>(param_info.param);
  return base::StringPrintf(
      "%s_%s", (allow_passthrough ? "AllowPassthrough" : "DisallowPassthrough"),
      format.ToString().c_str());
}

INSTANTIATE_TEST_SUITE_P(Service,
                         EGLImageBackingFactoryThreadSafeTest,
                         ::testing::Combine(::testing::Bool(),
                                            kSharedImageFormats),
                         TestParamToString);

}  // anonymous namespace
}  // namespace gpu
