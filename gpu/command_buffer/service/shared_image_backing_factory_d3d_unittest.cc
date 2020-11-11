// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_d3d.h"

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing_d3d.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_d3d.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(USE_DAWN)
#include <dawn/dawn_proc.h>
#include <dawn/webgpu_cpp.h>
#include <dawn_native/DawnNative.h>
#endif  // BUILDFLAG(USE_DAWN)

namespace gpu {
namespace {

static const char* kVertexShaderSrc =
    "attribute vec2 a_position;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "  gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);\n"
    "  v_texCoord = (a_position + vec2(1.0, 1.0)) * 0.5;\n"
    "}\n";

static const char* kFragmentShaderSrc =
    "precision mediump float;\n"
    "uniform mediump sampler2D u_texture;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(u_texture, v_texCoord);"
    "}\n";

GLuint MakeTextureAndSetParameters(gl::GLApi* api, GLenum target, bool fbo) {
  GLuint texture_id = 0;
  api->glGenTexturesFn(1, &texture_id);
  api->glBindTextureFn(target, texture_id);
  api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  if (fbo) {
    api->glTexParameteriFn(target, GL_TEXTURE_USAGE_ANGLE,
                           GL_FRAMEBUFFER_ATTACHMENT_ANGLE);
  }
  return texture_id;
}

bool IsD3DSharedImageSupported() {
  // D3D shared images with the current group of flags only works on Win8+
  // OSes. If we need shared images on Win7, we can create them but a more
  // insecure group of flags is required.
  if (GPUTestBotConfig::CurrentConfigMatches("Win7"))
    return false;
  return true;
}

class SharedImageBackingFactoryD3DTestBase : public testing::Test {
 public:
  void SetUp() override {
    surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
    ASSERT_TRUE(surface_);
    context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                         gl::GLContextAttribs());
    ASSERT_TRUE(context_);
    bool result = context_->MakeCurrent(surface_.get());
    ASSERT_TRUE(result);

    memory_type_tracker_ = std::make_unique<MemoryTypeTracker>(nullptr);
    shared_image_representation_factory_ =
        std::make_unique<SharedImageRepresentationFactory>(
            &shared_image_manager_, nullptr);
    shared_image_factory_ = std::make_unique<SharedImageBackingFactoryD3D>();
  }

 protected:
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  SharedImageManager shared_image_manager_;
  std::unique_ptr<MemoryTypeTracker> memory_type_tracker_;
  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  std::unique_ptr<SharedImageBackingFactoryD3D> shared_image_factory_;
};

class SharedImageBackingFactoryD3DTestSwapChain
    : public SharedImageBackingFactoryD3DTestBase {
 public:
  void SetUp() override {
    if (!SharedImageBackingFactoryD3D::IsSwapChainSupported())
      return;
    SharedImageBackingFactoryD3DTestBase::SetUp();
  }
};

TEST_F(SharedImageBackingFactoryD3DTestSwapChain, InvalidFormat) {
  if (!SharedImageBackingFactoryD3D::IsSwapChainSupported())
    return;

  auto front_buffer_mailbox = Mailbox::GenerateForSharedImage();
  auto back_buffer_mailbox = Mailbox::GenerateForSharedImage();
  gfx::Size size(1, 1);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  auto surface_origin = kTopLeft_GrSurfaceOrigin;
  auto alpha_type = kPremul_SkAlphaType;
  uint32_t usage = gpu::SHARED_IMAGE_USAGE_SCANOUT;
  {
    auto valid_format = viz::RGBA_8888;
    auto backings = shared_image_factory_->CreateSwapChain(
        front_buffer_mailbox, back_buffer_mailbox, valid_format, size,
        color_space, surface_origin, alpha_type, usage);
    EXPECT_TRUE(backings.front_buffer);
    EXPECT_TRUE(backings.back_buffer);
  }
  {
    auto valid_format = viz::BGRA_8888;
    auto backings = shared_image_factory_->CreateSwapChain(
        front_buffer_mailbox, back_buffer_mailbox, valid_format, size,
        color_space, surface_origin, alpha_type, usage);
    EXPECT_TRUE(backings.front_buffer);
    EXPECT_TRUE(backings.back_buffer);
  }
  {
    auto valid_format = viz::RGBA_F16;
    auto backings = shared_image_factory_->CreateSwapChain(
        front_buffer_mailbox, back_buffer_mailbox, valid_format, size,
        color_space, surface_origin, alpha_type, usage);
    EXPECT_TRUE(backings.front_buffer);
    EXPECT_TRUE(backings.back_buffer);
  }
  {
    auto invalid_format = viz::RGBA_4444;
    auto backings = shared_image_factory_->CreateSwapChain(
        front_buffer_mailbox, back_buffer_mailbox, invalid_format, size,
        color_space, surface_origin, alpha_type, usage);
    EXPECT_FALSE(backings.front_buffer);
    EXPECT_FALSE(backings.back_buffer);
  }
}

TEST_F(SharedImageBackingFactoryD3DTestSwapChain, CreateAndPresentSwapChain) {
  if (!SharedImageBackingFactoryD3D::IsSwapChainSupported())
    return;

  auto front_buffer_mailbox = Mailbox::GenerateForSharedImage();
  auto back_buffer_mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::RGBA_8888;
  gfx::Size size(1, 1);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  auto surface_origin = kTopLeft_GrSurfaceOrigin;
  auto alpha_type = kPremul_SkAlphaType;
  uint32_t usage = gpu::SHARED_IMAGE_USAGE_GLES2 |
                   gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT |
                   gpu::SHARED_IMAGE_USAGE_DISPLAY |
                   gpu::SHARED_IMAGE_USAGE_SCANOUT;

  auto backings = shared_image_factory_->CreateSwapChain(
      front_buffer_mailbox, back_buffer_mailbox, format, size, color_space,
      surface_origin, alpha_type, usage);
  ASSERT_TRUE(backings.front_buffer);
  EXPECT_TRUE(backings.front_buffer->IsCleared());

  ASSERT_TRUE(backings.back_buffer);
  EXPECT_TRUE(backings.back_buffer->IsCleared());

  std::unique_ptr<SharedImageRepresentationFactoryRef> back_factory_ref =
      shared_image_manager_.Register(std::move(backings.back_buffer),
                                     memory_type_tracker_.get());
  std::unique_ptr<SharedImageRepresentationFactoryRef> front_factory_ref =
      shared_image_manager_.Register(std::move(backings.front_buffer),
                                     memory_type_tracker_.get());

  auto back_texture = shared_image_representation_factory_
                          ->ProduceGLTexturePassthrough(back_buffer_mailbox)
                          ->GetTexturePassthrough();
  ASSERT_TRUE(back_texture);
  EXPECT_EQ(back_texture->target(), static_cast<unsigned>(GL_TEXTURE_2D));

  GLuint back_texture_id = back_texture->service_id();
  EXPECT_NE(back_texture_id, 0u);

  auto* back_image = gl::GLImageD3D::FromGLImage(
      back_texture->GetLevelImage(GL_TEXTURE_2D, 0));

  auto front_texture = shared_image_representation_factory_
                           ->ProduceGLTexturePassthrough(front_buffer_mailbox)
                           ->GetTexturePassthrough();
  ASSERT_TRUE(front_texture);
  EXPECT_EQ(front_texture->target(), static_cast<unsigned>(GL_TEXTURE_2D));

  GLuint front_texture_id = front_texture->service_id();
  EXPECT_NE(front_texture_id, 0u);

  auto* front_image = gl::GLImageD3D::FromGLImage(
      front_texture->GetLevelImage(GL_TEXTURE_2D, 0));

  ASSERT_TRUE(back_image);
  EXPECT_EQ(back_image->ShouldBindOrCopy(), gl::GLImage::BIND);

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  EXPECT_EQ(S_OK, back_image->swap_chain()->GetBuffer(
                      0 /* buffer_index */, IID_PPV_ARGS(&d3d11_texture)));
  EXPECT_TRUE(d3d11_texture);
  EXPECT_EQ(d3d11_texture, back_image->texture());
  d3d11_texture.Reset();

  ASSERT_TRUE(front_image);
  EXPECT_EQ(front_image->ShouldBindOrCopy(), gl::GLImage::BIND);

  EXPECT_EQ(S_OK, front_image->swap_chain()->GetBuffer(
                      1 /* buffer_index */, IID_PPV_ARGS(&d3d11_texture)));
  EXPECT_TRUE(d3d11_texture);
  EXPECT_EQ(d3d11_texture, front_image->texture());
  d3d11_texture.Reset();

  gl::GLApi* api = gl::g_current_gl_context;
  // Create a multisampled FBO.
  GLuint multisample_fbo, renderbuffer = 0u;
  api->glGenFramebuffersEXTFn(1, &multisample_fbo);
  api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, multisample_fbo);
  api->glGenRenderbuffersEXTFn(1, &renderbuffer);
  api->glBindRenderbufferEXTFn(GL_RENDERBUFFER, renderbuffer);
  ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

  api->glRenderbufferStorageMultisampleFn(GL_RENDERBUFFER, 4 /* sample_count */,
                                          GL_RGBA8_OES, 1, 1);
  api->glFramebufferRenderbufferEXTFn(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      GL_RENDERBUFFER, renderbuffer);
  EXPECT_EQ(api->glCheckFramebufferStatusEXTFn(GL_FRAMEBUFFER),
            static_cast<unsigned>(GL_FRAMEBUFFER_COMPLETE));
  ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

  // Set the clear color to green.
  api->glViewportFn(0, 0, size.width(), size.height());
  api->glClearColorFn(0.0f, 1.0f, 0.0f, 1.0f);
  api->glClearFn(GL_COLOR_BUFFER_BIT);
  ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

  api->glBindFramebufferEXTFn(GL_READ_FRAMEBUFFER, multisample_fbo);

  // Attach the back buffer texture to an FBO.
  GLuint fbo = 0u;
  api->glGenFramebuffersEXTFn(1, &fbo);
  api->glBindFramebufferEXTFn(GL_DRAW_FRAMEBUFFER, fbo);
  api->glBindTextureFn(GL_TEXTURE_2D, back_texture_id);
  api->glFramebufferTexture2DEXTFn(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, back_texture_id, 0);
  EXPECT_EQ(api->glCheckFramebufferStatusEXTFn(GL_DRAW_FRAMEBUFFER),
            static_cast<unsigned>(GL_FRAMEBUFFER_COMPLETE));
  ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

  api->glBlitFramebufferFn(0, 0, 1, 1, 0, 0, 1, 1, GL_COLOR_BUFFER_BIT,
                           GL_NEAREST);
  ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

  api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, fbo);

  // Checks if rendering to back buffer was successful.
  {
    GLubyte pixel_color[4];
    const uint8_t expected_color[4] = {0, 255, 0, 255};
    api->glReadPixelsFn(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel_color);
    EXPECT_EQ(expected_color[0], pixel_color[0]);
    EXPECT_EQ(expected_color[1], pixel_color[1]);
    EXPECT_EQ(expected_color[2], pixel_color[2]);
    EXPECT_EQ(expected_color[3], pixel_color[3]);
  }

  EXPECT_TRUE(back_factory_ref->PresentSwapChain());

  // After present, back buffer should now have a clear texture.
  {
    GLubyte pixel_color[4];
    const uint8_t expected_color[4] = {0, 0, 0, 255};
    api->glReadPixelsFn(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel_color);
    EXPECT_EQ(expected_color[0], pixel_color[0]);
    EXPECT_EQ(expected_color[1], pixel_color[1]);
    EXPECT_EQ(expected_color[2], pixel_color[2]);
    EXPECT_EQ(expected_color[3], pixel_color[3]);
  }

  // And front buffer should have the rendered contents.  Test that binding
  // front buffer as a sampler works.
  {
    // Create a destination texture to render into since we can't bind front
    // buffer to an FBO.
    GLuint dest_texture_id =
        MakeTextureAndSetParameters(api, GL_TEXTURE_2D, true);
    api->glTexImage2DFn(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA,
                        GL_UNSIGNED_BYTE, nullptr);
    api->glFramebufferTexture2DEXTFn(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_2D, dest_texture_id, 0);
    EXPECT_EQ(api->glCheckFramebufferStatusEXTFn(GL_FRAMEBUFFER),
              static_cast<unsigned>(GL_FRAMEBUFFER_COMPLETE));
    api->glClearColorFn(0.0f, 0.0f, 0.0f, 0.0f);
    api->glClearFn(GL_COLOR_BUFFER_BIT);
    ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

    GLint status = 0;
    GLuint vertex_shader = api->glCreateShaderFn(GL_VERTEX_SHADER);
    ASSERT_NE(vertex_shader, 0u);
    api->glShaderSourceFn(vertex_shader, 1, &kVertexShaderSrc, nullptr);
    api->glCompileShaderFn(vertex_shader);
    api->glGetShaderivFn(vertex_shader, GL_COMPILE_STATUS, &status);
    ASSERT_NE(status, 0);

    GLuint fragment_shader = api->glCreateShaderFn(GL_FRAGMENT_SHADER);
    ASSERT_NE(fragment_shader, 0u);
    api->glShaderSourceFn(fragment_shader, 1, &kFragmentShaderSrc, nullptr);
    api->glCompileShaderFn(fragment_shader);
    api->glGetShaderivFn(fragment_shader, GL_COMPILE_STATUS, &status);
    ASSERT_NE(status, 0);

    GLuint program = api->glCreateProgramFn();
    ASSERT_NE(program, 0u);
    api->glAttachShaderFn(program, vertex_shader);
    api->glAttachShaderFn(program, fragment_shader);
    api->glLinkProgramFn(program);
    api->glGetProgramivFn(program, GL_LINK_STATUS, &status);
    ASSERT_NE(status, 0);

    GLuint vbo = 0u;
    api->glGenBuffersARBFn(1, &vbo);
    ASSERT_NE(vbo, 0u);
    api->glBindBufferFn(GL_ARRAY_BUFFER, vbo);
    static const float vertices[] = {
        1.0f, 1.0f, -1.0f, 1.0f,  -1.0f, -1.0f,
        1.0f, 1.0f, -1.0f, -1.0f, 1.0f,  -1.0f,
    };
    api->glBufferDataFn(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
                        GL_STATIC_DRAW);
    GLint vertex_location = api->glGetAttribLocationFn(program, "a_position");
    ASSERT_NE(vertex_location, -1);
    api->glEnableVertexAttribArrayFn(vertex_location);
    api->glVertexAttribPointerFn(vertex_location, 2, GL_FLOAT, GL_FALSE, 0, 0);

    GLint sampler_location = api->glGetUniformLocationFn(program, "u_texture");
    ASSERT_NE(sampler_location, -1);
    api->glActiveTextureFn(GL_TEXTURE0);
    // ExpectUnboundAndBindOrCopyTexImage(front_buffer_mailbox);
    api->glBindTextureFn(GL_TEXTURE_2D, front_texture_id);
    api->glUniform1iFn(sampler_location, 0);

    api->glUseProgramFn(program);
    api->glDrawArraysFn(GL_TRIANGLES, 0, 6);

    {
      GLubyte pixel_color[4];
      const uint8_t expected_color[4] = {0, 255, 0, 255};
      api->glReadPixelsFn(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel_color);
      EXPECT_EQ(expected_color[0], pixel_color[0]);
      EXPECT_EQ(expected_color[1], pixel_color[1]);
      EXPECT_EQ(expected_color[2], pixel_color[2]);
      EXPECT_EQ(expected_color[3], pixel_color[3]);
    }

    api->glDeleteProgramFn(program);
    api->glDeleteShaderFn(vertex_shader);
    api->glDeleteShaderFn(fragment_shader);
    api->glDeleteBuffersARBFn(1, &vbo);
  }

  api->glDeleteFramebuffersEXTFn(1, &fbo);
}

class SharedImageBackingFactoryD3DTest
    : public SharedImageBackingFactoryD3DTestBase {
 public:
  void SetUp() override {
    if (!IsD3DSharedImageSupported())
      return;

    SharedImageBackingFactoryD3DTestBase::SetUp();
    GpuDriverBugWorkarounds workarounds;
    scoped_refptr<gl::GLShareGroup> share_group = new gl::GLShareGroup();
    context_state_ = base::MakeRefCounted<SharedContextState>(
        std::move(share_group), surface_, context_,
        /*use_virtualized_gl_contexts=*/false, base::DoNothing());
    context_state_->InitializeGrContext(GpuPreferences(), workarounds, nullptr);
    auto feature_info =
        base::MakeRefCounted<gles2::FeatureInfo>(workarounds, GpuFeatureInfo());
    context_state_->InitializeGL(GpuPreferences(), std::move(feature_info));
  }

 protected:
  GrDirectContext* gr_context() const { return context_state_->gr_context(); }

  void CheckSkiaPixels(const Mailbox& mailbox,
                       const gfx::Size& size,
                       const std::vector<uint8_t> expected_color) const {
    auto skia_representation =
        shared_image_representation_factory_->ProduceSkia(mailbox,
                                                          context_state_);
    ASSERT_NE(skia_representation, nullptr);

    std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
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

  scoped_refptr<SharedContextState> context_state_;
};

// Test to check interaction between Gl and skia GL representations.
// We write to a GL texture using gl representation and then read from skia
// representation.
TEST_F(SharedImageBackingFactoryD3DTest, GL_SkiaGL) {
  if (!IsD3DSharedImageSupported())
    return;

  // Create a backing using mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::ResourceFormat::RGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_DISPLAY;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage,
      false /* is_thread_safe */);
  ASSERT_NE(backing, nullptr);

  GLenum expected_target = GL_TEXTURE_2D;
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  // Create a SharedImageRepresentationGLTexture.
  auto gl_representation =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          mailbox);
  EXPECT_EQ(expected_target,
            gl_representation->GetTexturePassthrough()->target());

  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough::ScopedAccess>
      scoped_access = gl_representation->BeginScopedAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
          SharedImageRepresentation::AllowUnclearedAccess::kYes);
  EXPECT_TRUE(scoped_access);

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
  gl_representation->SetCleared();

  scoped_access.reset();
  gl_representation.reset();

  CheckSkiaPixels(mailbox, size, {0, 255, 0, 255});

  factory_ref.reset();
}

#if BUILDFLAG(USE_DAWN)
// Test to check interaction between Dawn and skia GL representations.
TEST_F(SharedImageBackingFactoryD3DTest, Dawn_SkiaGL) {
  if (!IsD3DSharedImageSupported())
    return;

  // Create a Dawn D3D12 device
  dawn_native::Instance instance;
  instance.DiscoverDefaultAdapters();

  std::vector<dawn_native::Adapter> adapters = instance.GetAdapters();
  auto adapter_it = std::find_if(
      adapters.begin(), adapters.end(), [](dawn_native::Adapter adapter) {
        return adapter.GetBackendType() == dawn_native::BackendType::D3D12;
      });
  ASSERT_NE(adapter_it, adapters.end());

  wgpu::Device device = wgpu::Device::Acquire(adapter_it->CreateDevice());
  DawnProcTable procs = dawn_native::GetProcs();
  dawnProcSetProcs(&procs);

  // Create a backing using mailbox.
  const auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::ResourceFormat::RGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  const uint32_t usage = SHARED_IMAGE_USAGE_WEBGPU | SHARED_IMAGE_USAGE_DISPLAY;
  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage,
      false /* is_thread_safe */);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  // Clear the shared image to green using Dawn.
  {
    // Create a SharedImageRepresentationDawn.
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(mailbox,
                                                          device.Get());
    ASSERT_TRUE(dawn_representation);

    auto scoped_access = dawn_representation->BeginScopedAccess(
        WGPUTextureUsage_OutputAttachment,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(scoped_access);

    wgpu::Texture texture(scoped_access->texture());

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

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.EndPass();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetDefaultQueue();
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
TEST_F(SharedImageBackingFactoryD3DTest, GL_Dawn_Skia_UnclearTexture) {
  if (!IsD3DSharedImageSupported())
    return;

  // Create a backing using mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::ResourceFormat::RGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_DISPLAY |
                         SHARED_IMAGE_USAGE_WEBGPU;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage,
      false /* is_thread_safe */);
  ASSERT_NE(backing, nullptr);

  GLenum expected_target = GL_TEXTURE_2D;
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());
  {
    // Create a SharedImageRepresentationGLTexture.
    auto gl_representation =
        shared_image_representation_factory_->ProduceGLTexturePassthrough(
            mailbox);
    EXPECT_EQ(expected_target,
              gl_representation->GetTexturePassthrough()->target());

    std::unique_ptr<SharedImageRepresentationGLTexturePassthrough::ScopedAccess>
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

    // Don't call SetCleared, we want to see if Dawn will lazy clear the texture
    EXPECT_FALSE(factory_ref->IsCleared());
  }

  // Create a Dawn D3D12 device
  dawn_native::Instance instance;
  instance.DiscoverDefaultAdapters();

  std::vector<dawn_native::Adapter> adapters = instance.GetAdapters();
  auto adapter_it = std::find_if(
      adapters.begin(), adapters.end(), [](dawn_native::Adapter adapter) {
        return adapter.GetBackendType() == dawn_native::BackendType::D3D12;
      });
  ASSERT_NE(adapter_it, adapters.end());

  wgpu::Device device = wgpu::Device::Acquire(adapter_it->CreateDevice());
  DawnProcTable procs = dawn_native::GetProcs();
  dawnProcSetProcs(&procs);
  {
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(mailbox,
                                                          device.Get());
    ASSERT_TRUE(dawn_representation);

    auto dawn_scoped_access = dawn_representation->BeginScopedAccess(
        WGPUTextureUsage_OutputAttachment,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(dawn_scoped_access);

    wgpu::Texture texture(dawn_scoped_access->texture());
    wgpu::RenderPassColorAttachmentDescriptor color_desc;
    color_desc.attachment = texture.CreateView();
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

    wgpu::Queue queue = device.GetDefaultQueue();
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
// 3. Expect skia to fail to access the texture because texture is not
// initialized
TEST_F(SharedImageBackingFactoryD3DTest, UnclearDawn_SkiaFails) {
  if (!IsD3DSharedImageSupported())
    return;

  // Create a backing using mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::ResourceFormat::RGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_DISPLAY |
                         SHARED_IMAGE_USAGE_WEBGPU;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage,
      false /* is_thread_safe */);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  // Create dawn device
  dawn_native::Instance instance;
  instance.DiscoverDefaultAdapters();

  std::vector<dawn_native::Adapter> adapters = instance.GetAdapters();
  auto adapter_it = std::find_if(
      adapters.begin(), adapters.end(), [](dawn_native::Adapter adapter) {
        return adapter.GetBackendType() == dawn_native::BackendType::D3D12;
      });
  ASSERT_NE(adapter_it, adapters.end());

  wgpu::Device device = wgpu::Device::Acquire(adapter_it->CreateDevice());
  DawnProcTable procs = dawn_native::GetProcs();
  dawnProcSetProcs(&procs);
  {
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(mailbox,
                                                          device.Get());
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
    color_desc.storeOp = wgpu::StoreOp::Clear;
    color_desc.clearColor = {0, 255, 0, 255};

    wgpu::RenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &color_desc;
    renderPassDesc.depthStencilAttachment = nullptr;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.EndPass();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetDefaultQueue();
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
  std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
      scoped_read_access =
          skia_representation->BeginScopedReadAccess(nullptr, nullptr);
  EXPECT_EQ(scoped_read_access, nullptr);
}
#endif  // BUILDFLAG(USE_DAWN)

// Test that Skia trying to access uninitialized SharedImage will fail
TEST_F(SharedImageBackingFactoryD3DTest, SkiaAccessFirstFails) {
  if (!IsD3DSharedImageSupported())
    return;

  // Create a mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::ResourceFormat::RGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_DISPLAY;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage,
      false /* is_thread_safe */);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  // Produce skia representation
  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      mailbox, context_state_);
  ASSERT_NE(skia_representation, nullptr);
  EXPECT_FALSE(skia_representation->IsCleared());

  // Expect BeginScopedReadAccess to fail because sharedImage is uninitialized
  std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
      scoped_read_access =
          skia_representation->BeginScopedReadAccess(nullptr, nullptr);
  EXPECT_EQ(scoped_read_access, nullptr);
}

TEST_F(SharedImageBackingFactoryD3DTest, CreateSharedImageFromHandle) {
  if (!IsD3DSharedImageSupported())
    return;

  EXPECT_TRUE(
      shared_image_factory_->CanImportGpuMemoryBuffer(gfx::DXGI_SHARED_HANDLE));

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      shared_image_factory_->GetDeviceForTesting();

  const gfx::Size size(1, 1);
  D3D11_TEXTURE2D_DESC desc;
  desc.Width = size.width();
  desc.Height = size.height();
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags =
      D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  HRESULT hr = d3d11_device->CreateTexture2D(&desc, nullptr, &d3d11_texture);
  ASSERT_EQ(hr, S_OK);

  Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
  hr = d3d11_texture.As(&dxgi_resource);
  ASSERT_EQ(hr, S_OK);

  HANDLE shared_handle;
  hr = dxgi_resource->CreateSharedHandle(
      nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
      &shared_handle);
  ASSERT_EQ(hr, S_OK);

  gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle;
  gpu_memory_buffer_handle.dxgi_handle.Set(shared_handle);
  gpu_memory_buffer_handle.type = gfx::DXGI_SHARED_HANDLE;

  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = gfx::BufferFormat::RGBA_8888;
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_DISPLAY;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  const GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  const SkAlphaType alpha_type = kPremul_SkAlphaType;
  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, 0, std::move(gpu_memory_buffer_handle), format, surface_handle,
      size, color_space, surface_origin, alpha_type, usage);
  ASSERT_NE(backing, nullptr);

  EXPECT_EQ(backing->format(), viz::RGBA_8888);
  EXPECT_EQ(backing->size(), size);
  EXPECT_EQ(backing->color_space(), color_space);
  EXPECT_EQ(backing->surface_origin(), surface_origin);
  EXPECT_EQ(backing->alpha_type(), alpha_type);
  EXPECT_EQ(backing->mailbox(), mailbox);
  EXPECT_TRUE(backing->IsCleared());

  SharedImageBackingD3D* backing_d3d =
      static_cast<SharedImageBackingD3D*>(backing.get());
  EXPECT_EQ(backing_d3d->GetSharedHandle(), shared_handle);
}

}  // anonymous namespace
}  // namespace gpu
