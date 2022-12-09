// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/d3d_image_backing_factory.h"

#include <memory>
#include <utility>

#include "base/bits.h"
#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/unguessable_token.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/compound_image_backing.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_d3d.h"
#include "ui/gl/gl_image_memory.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(USE_DAWN)
#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>
#include <dawn/webgpu_cpp.h>
#endif  // BUILDFLAG(USE_DAWN)

#define SCOPED_GL_CLEANUP_VAR(api, func, var)            \
  base::ScopedClosureRunner delete_##var(base::BindOnce( \
      [](gl::GLApi* api, GLuint var) { api->gl##func##Fn(var); }, api, var))

#define SCOPED_GL_CLEANUP_PTR(api, func, n, var)                           \
  base::ScopedClosureRunner delete_##var(base::BindOnce(                   \
      [](gl::GLApi* api, GLuint var) { api->gl##func##Fn(n, &var); }, api, \
      var))

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

void FillYUV(uint8_t* data,
             const gfx::Size& size,
             uint8_t y_fill_value,
             uint8_t u_fill_value,
             uint8_t v_fill_value) {
  const size_t kYPlaneSize = size.width() * size.height();
  memset(data, y_fill_value, kYPlaneSize);
  uint8_t* uv_data = data + kYPlaneSize;
  const size_t kUVPlaneSize = kYPlaneSize / 2;
  for (size_t i = 0; i < kUVPlaneSize; i += 2) {
    uv_data[i] = u_fill_value;
    uv_data[i + 1] = v_fill_value;
  }
}

void CheckYUV(const uint8_t* data,
              size_t stride,
              const gfx::Size& size,
              uint8_t y_fill_value,
              uint8_t u_fill_value,
              uint8_t v_fill_value) {
  const size_t kYPlaneSize = stride * size.height();
  const uint8_t* uv_data = data + kYPlaneSize;
  for (int i = 0; i < size.height(); i++) {
    for (int j = 0; j < size.width(); j++) {
      // ASSERT instead of EXPECT to exit on first failure to avoid log spam.
      ASSERT_EQ(*(data + i * stride + j), y_fill_value);
      if (i < size.height() / 2) {
        const uint8_t uv_value = (j % 2 == 0) ? u_fill_value : v_fill_value;
        ASSERT_EQ(*(uv_data + i * stride + j), uv_value);
      }
    }
  }
}

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

}  // anonymous namespace

class D3DImageBackingFactoryTestBase : public testing::Test {
 public:
  void SetUp() override {
    if (!IsD3DSharedImageSupported())
      return;
    surface_ = gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplayEGL(),
                                                  gfx::Size());
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
    shared_image_factory_ = std::make_unique<D3DImageBackingFactory>(
        gl::QueryD3D11DeviceObjectFromANGLE(),
        shared_image_manager_.dxgi_shared_handle_manager());
  }

 protected:
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  SharedImageManager shared_image_manager_;
  std::unique_ptr<MemoryTypeTracker> memory_type_tracker_;
  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  std::unique_ptr<D3DImageBackingFactory> shared_image_factory_;
};

class D3DImageBackingFactoryTestSwapChain
    : public D3DImageBackingFactoryTestBase {
 public:
  void SetUp() override {
    if (!D3DImageBackingFactory::IsSwapChainSupported())
      return;
    D3DImageBackingFactoryTestBase::SetUp();
  }
};

TEST_F(D3DImageBackingFactoryTestSwapChain, InvalidFormat) {
  if (!D3DImageBackingFactory::IsSwapChainSupported())
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

TEST_F(D3DImageBackingFactoryTestSwapChain, CreateAndPresentSwapChain) {
  if (!D3DImageBackingFactory::IsSwapChainSupported())
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
                   gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
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

  auto* back_image =
      gl::GLImage::ToGLImageD3D(back_texture->GetLevelImage(GL_TEXTURE_2D, 0));

  auto front_texture = shared_image_representation_factory_
                           ->ProduceGLTexturePassthrough(front_buffer_mailbox)
                           ->GetTexturePassthrough();
  ASSERT_TRUE(front_texture);
  EXPECT_EQ(front_texture->target(), static_cast<unsigned>(GL_TEXTURE_2D));

  GLuint front_texture_id = front_texture->service_id();
  EXPECT_NE(front_texture_id, 0u);

  auto* front_image =
      gl::GLImage::ToGLImageD3D(front_texture->GetLevelImage(GL_TEXTURE_2D, 0));

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
    api->glVertexAttribPointerFn(vertex_location, 2, GL_FLOAT, GL_FALSE, 0,
                                 nullptr);

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

class D3DImageBackingFactoryTest : public D3DImageBackingFactoryTestBase {
 public:
  void SetUp() override {
    if (!IsD3DSharedImageSupported())
      return;

    D3DImageBackingFactoryTestBase::SetUp();
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

  void CheckSkiaPixels(
      SkiaImageRepresentation::ScopedReadAccess* scoped_read_access,
      const gfx::Size& size,
      const std::vector<uint8_t>& expected_color) const {
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

  void CheckSkiaPixels(const Mailbox& mailbox,
                       const gfx::Size& size,
                       const std::vector<uint8_t>& expected_color) const {
    auto skia_representation =
        shared_image_representation_factory_->ProduceSkia(mailbox,
                                                          context_state_);
    ASSERT_NE(skia_representation, nullptr);

    std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
        scoped_read_access =
            skia_representation->BeginScopedReadAccess(nullptr, nullptr);
    EXPECT_TRUE(scoped_read_access);

    CheckSkiaPixels(scoped_read_access.get(), size, expected_color);
  }

  void CheckDawnPixels(
      DawnImageRepresentation::ScopedAccess* scoped_read_access,
      const wgpu::Device& device,
      const gfx::Size& size,
      const std::vector<uint8_t>& expected_color) const;

  // Helper for opening multiple Dawn and Skia scoped access on given mailbox,
  // and checking for the expected color using both APIs concurrently.
  void DawnConcurrentReadTestHelper(const Mailbox& mailbox,
                                    const wgpu::Device& device,
                                    const gfx::Size& size,
                                    const std::vector<uint8_t>& expected_color);

  std::vector<std::unique_ptr<SharedImageRepresentationFactoryRef>>
  CreateVideoImages(const gfx::Size& size,
                    uint8_t y_fill_value,
                    uint8_t u_fill_value,
                    uint8_t v_fill_value,
                    bool use_shared_handle,
                    bool use_factory);
  void RunVideoTest(bool use_shared_handle, bool use_factory);
  void RunOverlayTest(bool use_shared_handle, bool use_factory);
  void RunCreateSharedImageFromHandleTest(DXGI_FORMAT dxgi_format);

  scoped_refptr<SharedContextState> context_state_;
};

// Test to check interaction between Gl and skia GL representations.
// We write to a GL texture using gl representation and then read from skia
// representation.
TEST_F(D3DImageBackingFactoryTest, GL_SkiaGL) {
  if (!IsD3DSharedImageSupported())
    return;

  // Create a backing using mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::SharedImageFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const uint32_t usage =
      SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_DISPLAY_READ;
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

  // Create a GLTextureImageRepresentation.
  auto gl_representation =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          mailbox);
  EXPECT_EQ(expected_target,
            gl_representation->GetTexturePassthrough()->target());

  std::unique_ptr<GLTexturePassthroughImageRepresentation::ScopedAccess>
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
TEST_F(D3DImageBackingFactoryTest, Dawn_SkiaGL) {
  if (!IsD3DSharedImageSupported())
    return;

  // Create a Dawn D3D12 device
  dawn::native::Instance instance;
  instance.DiscoverDefaultAdapters();

  std::vector<dawn::native::Adapter> adapters = instance.GetAdapters();
  auto adapter_it = base::ranges::find(adapters, wgpu::BackendType::D3D12,
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
  const auto format = viz::SharedImageFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  const uint32_t usage =
      SHARED_IMAGE_USAGE_WEBGPU | SHARED_IMAGE_USAGE_DISPLAY_READ;
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
    // Create a DawnImageRepresentation.
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(
            mailbox, device.Get(), WGPUBackendType_D3D12, {});
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

void D3DImageBackingFactoryTest::CheckDawnPixels(
    DawnImageRepresentation::ScopedAccess* scoped_read_access,
    const wgpu::Device& device,
    const gfx::Size& size,
    const std::vector<uint8_t>& expected_color) const {
  wgpu::Texture texture(scoped_read_access->texture());

  uint32_t buffer_stride =
      static_cast<uint32_t>(base::bits::AlignUp(size.width() * 4, 256));
  size_t buffer_size = static_cast<size_t>(size.height()) * buffer_stride;
  wgpu::BufferDescriptor buffer_desc{
      .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead,
      .size = buffer_size};
  wgpu::Buffer buffer = device.CreateBuffer(&buffer_desc);

  wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
  auto src = wgpu::ImageCopyTexture{.texture = texture, .origin = {0, 0, 0}};
  auto dst = wgpu::ImageCopyBuffer{.layout = {.bytesPerRow = buffer_stride},
                                   .buffer = buffer};
  auto copy_size = wgpu::Extent3D{static_cast<uint32_t>(size.width()),
                                  static_cast<uint32_t>(size.height(), 1)};
  encoder.CopyTextureToBuffer(&src, &dst, &copy_size);
  wgpu::CommandBuffer commands = encoder.Finish();

  wgpu::Queue queue = device.GetQueue();
  queue.Submit(1, &commands);

  WGPUBufferMapAsyncStatus map_status = WGPUBufferMapAsyncStatus_Unknown;
  auto map_callback = [](WGPUBufferMapAsyncStatus status, void* userdata) {
    WGPUBufferMapAsyncStatus* status_out =
        reinterpret_cast<WGPUBufferMapAsyncStatus*>(userdata);
    *status_out = status;
  };
  buffer.MapAsync(wgpu::MapMode::Read, 0, buffer_desc.size, map_callback,
                  &map_status);
  // Tick device until async map operation completes.
  while (dawn::native::DeviceTick(device.Get())) {
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  }

  const uint8_t* dst_pixels =
      reinterpret_cast<const uint8_t*>(buffer.GetConstMappedRange());
  for (int row = 0; row < size.height(); row++) {
    for (int col = 0; col < size.width(); col++) {
      // Compare the pixel values.
      const uint8_t* pixel = dst_pixels + (row * buffer_stride) + col * 4;
      EXPECT_EQ(pixel[0], expected_color[0]);
      EXPECT_EQ(pixel[1], expected_color[1]);
      EXPECT_EQ(pixel[2], expected_color[2]);
      EXPECT_EQ(pixel[3], expected_color[3]);
    }
  }
}

// Opens two Skia and two Dawn ScopedReadAccess and performs concurrent reads
// using both APIs.
void D3DImageBackingFactoryTest::DawnConcurrentReadTestHelper(
    const Mailbox& mailbox,
    const wgpu::Device& device,
    const gfx::Size& size,
    const std::vector<uint8_t>& expected_color) {
  auto dawn_representation1 = shared_image_representation_factory_->ProduceDawn(
      mailbox, device.Get(), WGPUBackendType_D3D12, {});
  ASSERT_TRUE(dawn_representation1);

  auto dawn_access1 = dawn_representation1->BeginScopedAccess(
      WGPUTextureUsage_CopySrc,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);
  ASSERT_TRUE(dawn_access1);

  auto dawn_representation2 = shared_image_representation_factory_->ProduceDawn(
      mailbox, device.Get(), WGPUBackendType_D3D12, {});
  ASSERT_TRUE(dawn_representation2);

  auto dawn_access2 = dawn_representation2->BeginScopedAccess(
      WGPUTextureUsage_CopySrc,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);
  ASSERT_TRUE(dawn_access2);

  auto skia_representation1 = shared_image_representation_factory_->ProduceSkia(
      mailbox, context_state_);
  ASSERT_NE(skia_representation1, nullptr);

  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess> skia_access1 =
      skia_representation1->BeginScopedReadAccess(nullptr, nullptr);
  EXPECT_TRUE(skia_access1);

  auto skia_representation2 = shared_image_representation_factory_->ProduceSkia(
      mailbox, context_state_);
  ASSERT_NE(skia_representation2, nullptr);

  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess> skia_access2 =
      skia_representation2->BeginScopedReadAccess(nullptr, nullptr);
  EXPECT_TRUE(skia_access2);

  CheckDawnPixels(dawn_access1.get(), device, size, expected_color);
  CheckDawnPixels(dawn_access2.get(), device, size, expected_color);
  CheckSkiaPixels(skia_access1.get(), size, expected_color);
  CheckSkiaPixels(skia_access2.get(), size, expected_color);
}

TEST_F(D3DImageBackingFactoryTest, Dawn_ConcurrentReads) {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      shared_image_factory_->GetDeviceForTesting();
  if (!IsD3DSharedImageSupported() ||
      !D3DSharedFence::IsSupported(d3d11_device.Get())) {
    return;
  }

  // Create a Dawn D3D12 device
  dawn::native::Instance instance;
  instance.DiscoverDefaultAdapters();

  std::vector<dawn::native::Adapter> adapters = instance.GetAdapters();
  auto adapter_it = base::ranges::find(adapters, wgpu::BackendType::D3D12,
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
  const auto format = viz::SharedImageFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  const uint32_t usage = SHARED_IMAGE_USAGE_WEBGPU |
                         SHARED_IMAGE_USAGE_DISPLAY_READ |
                         SHARED_IMAGE_USAGE_DISPLAY_WRITE;
  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage,
      false /* is_thread_safe */);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  // Clear to red using Skia.
  {
    auto skia_representation =
        shared_image_representation_factory_->ProduceSkia(mailbox,
                                                          context_state_);
    ASSERT_NE(skia_representation, nullptr);

    auto scoped_write_access = skia_representation->BeginScopedWriteAccess(
        /*begin_semaphores=*/nullptr, /*end_semaphores=*/nullptr,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    EXPECT_TRUE(scoped_write_access);

    SkCanvas* canvas = scoped_write_access->surface()->getCanvas();
    canvas->clear(SkColors::kRed);
    scoped_write_access->surface()->flush();

    skia_representation->SetCleared();
  }

  // Check if pixels are red using concurrent reads.
  DawnConcurrentReadTestHelper(mailbox, device, size, {255, 0, 0, 255});

  // Clear the shared image to green using Dawn.
  {
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(
            mailbox, device.Get(), WGPUBackendType_D3D12, {});
    ASSERT_TRUE(dawn_representation);

    auto scoped_access = dawn_representation->BeginScopedAccess(
        WGPUTextureUsage_RenderAttachment,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    ASSERT_TRUE(scoped_access);

    wgpu::Texture texture(scoped_access->texture());

    wgpu::RenderPassColorAttachment color_desc;
    color_desc.view = texture.CreateView();
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

    wgpu::Queue queue = device.GetQueue();
    queue.Submit(1, &commands);
  }

  // Check if pixels are green using concurrent reads.
  DawnConcurrentReadTestHelper(mailbox, device, size, {0, 255, 0, 255});
}

// 1. Draw a color to texture through GL
// 2. Do not call SetCleared so we can test Dawn Lazy clear
// 3. Begin render pass in Dawn, but do not do anything
// 4. Verify through CheckSkiaPixel that GL drawn color not seen
TEST_F(D3DImageBackingFactoryTest, GL_Dawn_Skia_UnclearTexture) {
  if (!IsD3DSharedImageSupported())
    return;

  // Create a backing using mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::SharedImageFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const uint32_t usage = SHARED_IMAGE_USAGE_GLES2 |
                         SHARED_IMAGE_USAGE_DISPLAY_READ |
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
    // Create a GLTextureImageRepresentation.
    auto gl_representation =
        shared_image_representation_factory_->ProduceGLTexturePassthrough(
            mailbox);
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

    // Don't call SetCleared, we want to see if Dawn will lazy clear the texture
    EXPECT_FALSE(factory_ref->IsCleared());
  }

  // Create a Dawn D3D12 device
  dawn::native::Instance instance;
  instance.DiscoverDefaultAdapters();

  std::vector<dawn::native::Adapter> adapters = instance.GetAdapters();
  auto adapter_it = base::ranges::find(adapters, wgpu::BackendType::D3D12,
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
            mailbox, device.Get(), WGPUBackendType_D3D12, {});
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
// 2. Set the renderpass storeOp = Discard
// 3. Texture in Dawn will stay as uninitialized
// 3. Expect skia to fail to access the texture because texture is not
// initialized
TEST_F(D3DImageBackingFactoryTest, UnclearDawn_SkiaFails) {
  if (!IsD3DSharedImageSupported())
    return;

  // Create a backing using mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::SharedImageFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const uint32_t usage = SHARED_IMAGE_USAGE_GLES2 |
                         SHARED_IMAGE_USAGE_DISPLAY_READ |
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
  dawn::native::Instance instance;
  instance.DiscoverDefaultAdapters();

  std::vector<dawn::native::Adapter> adapters = instance.GetAdapters();
  auto adapter_it = base::ranges::find(adapters, wgpu::BackendType::D3D12,
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
            mailbox, device.Get(), WGPUBackendType_D3D12, {});
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
TEST_F(D3DImageBackingFactoryTest, SkiaAccessFirstFails) {
  if (!IsD3DSharedImageSupported())
    return;

  // Create a mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::SharedImageFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const uint32_t usage =
      SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_DISPLAY_READ;
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
  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
      scoped_read_access =
          skia_representation->BeginScopedReadAccess(nullptr, nullptr);
  EXPECT_EQ(scoped_read_access, nullptr);
}

void D3DImageBackingFactoryTest::RunCreateSharedImageFromHandleTest(
    DXGI_FORMAT dxgi_format) {
  if (!IsD3DSharedImageSupported())
    return;

  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto buffer_format = gfx::BufferFormat::RGBA_8888;
  const auto format = viz::SharedImageFormat::SinglePlane(
      viz::GetResourceFormat(buffer_format));
  const gfx::Size size(1, 1);
  const auto plane = gfx::BufferPlane::DEFAULT;
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const uint32_t usage =
      SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_DISPLAY_READ;
  const GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  const SkAlphaType alpha_type = kPremul_SkAlphaType;

  EXPECT_TRUE(shared_image_factory_->IsSupported(
      usage, format, size, /*thread_safe=*/false, gfx::DXGI_SHARED_HANDLE,
      GrContextType::kGL, /*pixel_data=*/{}));

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      shared_image_factory_->GetDeviceForTesting();

  D3D11_TEXTURE2D_DESC desc;
  desc.Width = size.width();
  desc.Height = size.height();
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = dxgi_format;
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
  gpu_memory_buffer_handle.dxgi_token = gfx::DXGIHandleToken();
  gpu_memory_buffer_handle.type = gfx::DXGI_SHARED_HANDLE;

  // Clone before moving the handle in CreateSharedImage.
  auto dup_handle = gpu_memory_buffer_handle.Clone();

  auto backing = shared_image_factory_->CreateSharedImage(
      mailbox, 0, std::move(gpu_memory_buffer_handle), buffer_format, plane,
      size, color_space, surface_origin, alpha_type, usage);
  ASSERT_NE(backing, nullptr);

  EXPECT_EQ(backing->format(), format);
  EXPECT_EQ(backing->size(), size);
  EXPECT_EQ(backing->color_space(), color_space);
  EXPECT_EQ(backing->surface_origin(), surface_origin);
  EXPECT_EQ(backing->alpha_type(), alpha_type);
  EXPECT_EQ(backing->mailbox(), mailbox);
  EXPECT_TRUE(backing->IsCleared());

  D3DImageBacking* backing_d3d = static_cast<D3DImageBacking*>(backing.get());
  EXPECT_EQ(
      backing_d3d->dxgi_shared_handle_state_for_testing()->GetSharedHandle(),
      shared_handle);

  // Check that a second backing created from the duplicated handle shares the
  // shared handle state and texture with the first backing.
  auto dup_mailbox = Mailbox::GenerateForSharedImage();
  auto dup_backing = shared_image_factory_->CreateSharedImage(
      dup_mailbox, 0, std::move(dup_handle), buffer_format, plane, size,
      color_space, surface_origin, alpha_type, usage);
  ASSERT_NE(dup_backing, nullptr);

  EXPECT_EQ(dup_backing->format(), format);
  EXPECT_EQ(dup_backing->size(), size);
  EXPECT_EQ(dup_backing->color_space(), color_space);
  EXPECT_EQ(dup_backing->surface_origin(), surface_origin);
  EXPECT_EQ(dup_backing->alpha_type(), alpha_type);
  EXPECT_EQ(dup_backing->mailbox(), dup_mailbox);
  EXPECT_TRUE(dup_backing->IsCleared());

  D3DImageBacking* dup_backing_d3d =
      static_cast<D3DImageBacking*>(dup_backing.get());
  EXPECT_EQ(dup_backing_d3d->dxgi_shared_handle_state_for_testing(),
            backing_d3d->dxgi_shared_handle_state_for_testing());
  EXPECT_EQ(dup_backing_d3d->d3d11_texture_for_testing(),
            backing_d3d->d3d11_texture_for_testing());

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  std::unique_ptr<SharedImageRepresentationFactoryRef> dup_factory_ref =
      shared_image_manager_.Register(std::move(dup_backing),
                                     memory_type_tracker_.get());

  // Check that concurrent read access using the duplicated handle works.
  auto gl_representation =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          mailbox);
  EXPECT_TRUE(gl_representation);

  std::unique_ptr<GLTexturePassthroughImageRepresentation::ScopedAccess>
      scoped_access = gl_representation->BeginScopedAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
          SharedImageRepresentation::AllowUnclearedAccess::kYes);
  EXPECT_TRUE(scoped_access);

  auto dup_gl_representation =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          dup_mailbox);
  EXPECT_TRUE(dup_gl_representation);

  std::unique_ptr<GLTexturePassthroughImageRepresentation::ScopedAccess>
      dup_scoped_access = dup_gl_representation->BeginScopedAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
          SharedImageRepresentation::AllowUnclearedAccess::kYes);
  EXPECT_TRUE(dup_scoped_access);
}

TEST_F(D3DImageBackingFactoryTest, CreateSharedImageFromHandleFormatUNORM) {
  RunCreateSharedImageFromHandleTest(DXGI_FORMAT_R8G8B8A8_UNORM);
}

TEST_F(D3DImageBackingFactoryTest, CreateSharedImageFromHandleFormatTYPELESS) {
  RunCreateSharedImageFromHandleTest(DXGI_FORMAT_R8G8B8A8_TYPELESS);
}

#if BUILDFLAG(USE_DAWN)
// Test to check external image stored in the backing can be reused
TEST_F(D3DImageBackingFactoryTest, Dawn_ReuseExternalImage) {
  if (!IsD3DSharedImageSupported())
    return;

  // Create a backing using mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::SharedImageFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const uint32_t usage = SHARED_IMAGE_USAGE_GLES2 |
                         SHARED_IMAGE_USAGE_DISPLAY_READ |
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

  // Create a Dawn D3D12 device
  dawn::native::Instance instance;
  instance.DiscoverDefaultAdapters();

  std::vector<dawn::native::Adapter> adapters = instance.GetAdapters();
  auto adapter_it = base::ranges::find(adapters, wgpu::BackendType::D3D12,
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

  const WGPUTextureUsage texture_usage = WGPUTextureUsage_RenderAttachment;

  // Create the first Dawn texture then clear it to green.
  {
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(
            mailbox, device.Get(), WGPUBackendType_D3D12, {});
    ASSERT_TRUE(dawn_representation);

    auto scoped_access = dawn_representation->BeginScopedAccess(
        texture_usage, SharedImageRepresentation::AllowUnclearedAccess::kYes);
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

  // Create another Dawn texture then clear it with another color.
  {
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(
            mailbox, device.Get(), WGPUBackendType_D3D12, {});
    ASSERT_TRUE(dawn_representation);

    // Check again that the texture is still green
    CheckSkiaPixels(mailbox, size, {0, 255, 0, 255});

    auto scoped_access = dawn_representation->BeginScopedAccess(
        texture_usage, SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(scoped_access);

    wgpu::Texture texture(scoped_access->texture());

    wgpu::RenderPassColorAttachment color_desc;
    color_desc.view = texture.CreateView();
    color_desc.resolveTarget = nullptr;
    color_desc.loadOp = wgpu::LoadOp::Clear;
    color_desc.storeOp = wgpu::StoreOp::Store;
    color_desc.clearValue = {255, 0, 0, 255};

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

  CheckSkiaPixels(mailbox, size, {255, 0, 0, 255});

  // Shut down Dawn
  device = wgpu::Device();
  dawnProcSetProcs(nullptr);

  factory_ref.reset();
}

// Check if making Dawn have the last ref works without a current GL context.
TEST_F(D3DImageBackingFactoryTest, Dawn_HasLastRef) {
  if (!IsD3DSharedImageSupported())
    return;

  // Create a backing using mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::SharedImageFormat::kRGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  const uint32_t usage = SHARED_IMAGE_USAGE_GLES2 |
                         SHARED_IMAGE_USAGE_DISPLAY_READ |
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

  // Create a Dawn D3D12 device
  dawn::native::Instance instance;
  instance.DiscoverDefaultAdapters();

  std::vector<dawn::native::Adapter> adapters = instance.GetAdapters();
  auto adapter_it = base::ranges::find(adapters, wgpu::BackendType::D3D12,
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

  auto dawn_representation = shared_image_representation_factory_->ProduceDawn(
      mailbox, device.Get(), WGPUBackendType_D3D12, {});
  ASSERT_NE(dawn_representation, nullptr);

  // Creating the Skia representation will also create a temporary GL texture.
  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      mailbox, context_state_);
  ASSERT_NE(skia_representation, nullptr);

  // Drop Skia representation and factory ref so that the Dawn representation
  // has the last ref.
  skia_representation.reset();
  factory_ref.reset();

  // Ensure no GL context is current.
  context_->ReleaseCurrent(surface_.get());

  // This shouldn't crash due to no GL context being current.
  dawn_representation.reset();

  // Shut down Dawn
  device = wgpu::Device();
  dawnProcSetProcs(nullptr);

  // Make context current so that it can be destroyed.
  context_->MakeCurrent(surface_.get());
}
#endif  // BUILDFLAG(USE_DAWN)

std::vector<std::unique_ptr<SharedImageRepresentationFactoryRef>>
D3DImageBackingFactoryTest::CreateVideoImages(const gfx::Size& size,
                                              uint8_t y_fill_value,
                                              uint8_t u_fill_value,
                                              uint8_t v_fill_value,
                                              bool use_shared_handle,
                                              bool use_factory) {
  DCHECK(IsD3DSharedImageSupported());

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      shared_image_factory_->GetDeviceForTesting();

  const size_t kDataSize = size.width() * size.height() * 3 / 2;

  std::vector<uint8_t> video_data(kDataSize);
  FillYUV(video_data.data(), size, y_fill_value, u_fill_value, v_fill_value);

  D3D11_SUBRESOURCE_DATA data = {};
  data.pSysMem = static_cast<const void*>(video_data.data());
  data.SysMemPitch = static_cast<UINT>(size.width());

  CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_NV12, size.width(), size.height(), 1,
                             1, D3D11_BIND_SHADER_RESOURCE);
  if (use_shared_handle) {
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                     D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
  }

  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;
  HRESULT hr = d3d11_device->CreateTexture2D(&desc, &data, &d3d11_texture);
  if (FAILED(hr))
    return {};

  uint32_t usage =
      gpu::SHARED_IMAGE_USAGE_VIDEO_DECODE | gpu::SHARED_IMAGE_USAGE_GLES2 |
      gpu::SHARED_IMAGE_USAGE_RASTER | gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
      gpu::SHARED_IMAGE_USAGE_SCANOUT;

  base::win::ScopedHandle shared_handle;
  if (use_shared_handle) {
    Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
    hr = d3d11_texture.As(&dxgi_resource);
    DCHECK_EQ(hr, S_OK);

    HANDLE handle = nullptr;
    hr = dxgi_resource->CreateSharedHandle(
        nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        nullptr, &handle);
    if (FAILED(hr))
      return {};

    shared_handle.Set(handle);
    DCHECK(shared_handle.is_valid());

    usage |= gpu::SHARED_IMAGE_USAGE_WEBGPU;
  }

  const size_t kNumPlanes = 2;
  const gpu::Mailbox mailboxes[kNumPlanes] = {
      gpu::Mailbox::GenerateForSharedImage(),
      gpu::Mailbox::GenerateForSharedImage()};
  const gfx::BufferPlane planes[kNumPlanes] = {gfx::BufferPlane::Y,
                                               gfx::BufferPlane::UV};

  std::vector<std::unique_ptr<SharedImageBacking>> shared_image_backings;
  if (use_factory) {
    HANDLE dup_handle = nullptr;
    if (!::DuplicateHandle(::GetCurrentProcess(), shared_handle.get(),
                           ::GetCurrentProcess(), &dup_handle, 0, false,
                           DUPLICATE_SAME_ACCESS)) {
      return {};
    }

    gfx::GpuMemoryBufferHandle gmb_handles[kNumPlanes];

    gmb_handles[0].type = gfx::DXGI_SHARED_HANDLE;
    gmb_handles[1].type = gfx::DXGI_SHARED_HANDLE;

    gmb_handles[0].dxgi_handle = std::move(shared_handle);
    DCHECK(gmb_handles[0].dxgi_handle.IsValid());

    gmb_handles[1].dxgi_handle.Set(dup_handle);
    DCHECK(gmb_handles[1].dxgi_handle.IsValid());

    gmb_handles[0].dxgi_token = gfx::DXGIHandleToken();
    gmb_handles[1].dxgi_token = gmb_handles[0].dxgi_token;

    for (size_t plane = 0; plane < kNumPlanes; plane++) {
      auto backing = shared_image_factory_->CreateSharedImage(
          mailboxes[plane], 0, std::move(gmb_handles[plane]),
          gfx::BufferFormat::YUV_420_BIPLANAR, planes[plane], size,
          gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
          usage);
      if (!backing)
        return {};
      shared_image_backings.push_back(std::move(backing));
    }
  } else {
    scoped_refptr<DXGISharedHandleState> dxgi_shared_handle_state;
    if (use_shared_handle) {
      dxgi_shared_handle_state =
          shared_image_manager_.dxgi_shared_handle_manager()
              ->CreateAnonymousSharedHandleState(std::move(shared_handle),
                                                 d3d11_texture);
    }
    shared_image_backings = D3DImageBacking::CreateFromVideoTexture(
        mailboxes, DXGI_FORMAT_NV12, size, usage, d3d11_texture,
        /*array_slice=*/0, std::move(dxgi_shared_handle_state));
  }
  EXPECT_EQ(shared_image_backings.size(), kNumPlanes);

  const gfx::Size plane_sizes[kNumPlanes] = {
      size, gfx::Size(size.width() / 2, size.height() / 2)};
  const viz::ResourceFormat plane_formats[kNumPlanes] = {viz::RED_8,
                                                         viz::RG_88};

  std::vector<std::unique_ptr<SharedImageRepresentationFactoryRef>>
      shared_image_refs;
  for (size_t i = 0; i < std::min(shared_image_backings.size(), kNumPlanes);
       i++) {
    auto& backing = shared_image_backings[i];

    EXPECT_EQ(backing->mailbox(), mailboxes[i]);
    EXPECT_EQ(backing->size(), plane_sizes[i]);
    EXPECT_EQ(backing->format(),
              viz::SharedImageFormat::SinglePlane(plane_formats[i]));
    EXPECT_EQ(backing->color_space(), gfx::ColorSpace());
    EXPECT_EQ(backing->surface_origin(), kTopLeft_GrSurfaceOrigin);
    EXPECT_EQ(backing->alpha_type(), kPremul_SkAlphaType);
    EXPECT_EQ(backing->usage(), usage);
    EXPECT_TRUE(backing->IsCleared());

    shared_image_refs.push_back(shared_image_manager_.Register(
        std::move(backing), memory_type_tracker_.get()));
  }

  return shared_image_refs;
}

void D3DImageBackingFactoryTest::RunVideoTest(bool use_shared_handle,
                                              bool use_factory) {
  if (!IsD3DSharedImageSupported())
    return;

  const gfx::Size size(32, 32);

  const uint8_t kYFillValue = 0x12;
  const uint8_t kUFillValue = 0x23;
  const uint8_t kVFillValue = 0x34;

  auto shared_image_refs =
      CreateVideoImages(size, kYFillValue, kUFillValue, kVFillValue,
                        use_shared_handle, use_factory);
  ASSERT_EQ(shared_image_refs.size(), 2u);

  // Setup GL shaders, framebuffers, uniforms, etc.
  static const char* kVideoFragmentShaderSrcTextureExternal =
      "#extension GL_OES_EGL_image_external : require\n"
      "precision mediump float;\n"
      "uniform samplerExternalOES u_texture_y;\n"
      "uniform samplerExternalOES u_texture_uv;\n"
      "varying vec2 v_texCoord;\n"
      "void main() {\n"
      "  gl_FragColor.r = texture2D(u_texture_y, v_texCoord).r;\n"
      "  gl_FragColor.gb = texture2D(u_texture_uv, v_texCoord).rg;\n"
      "  gl_FragColor.a = 1.0;\n"
      "}\n";
  static const char* kVideoFragmentShaderSrcTexture2D =
      "precision mediump float;\n"
      "uniform sampler2D u_texture_y;\n"
      "uniform sampler2D u_texture_uv;\n"
      "varying vec2 v_texCoord;\n"
      "void main() {\n"
      "  gl_FragColor.r = texture2D(u_texture_y, v_texCoord).r;\n"
      "  gl_FragColor.gb = texture2D(u_texture_uv, v_texCoord).rg;\n"
      "  gl_FragColor.a = 1.0;\n"
      "}\n";

  gl::GLApi* api = gl::g_current_gl_context;

  GLint status = 0;
  GLuint vertex_shader = api->glCreateShaderFn(GL_VERTEX_SHADER);
  SCOPED_GL_CLEANUP_VAR(api, DeleteShader, vertex_shader);
  ASSERT_NE(vertex_shader, 0u);
  api->glShaderSourceFn(vertex_shader, 1, &kVertexShaderSrc, nullptr);
  api->glCompileShaderFn(vertex_shader);
  api->glGetShaderivFn(vertex_shader, GL_COMPILE_STATUS, &status);
  ASSERT_NE(status, 0);

  GLuint fragment_shader = api->glCreateShaderFn(GL_FRAGMENT_SHADER);
  SCOPED_GL_CLEANUP_VAR(api, DeleteShader, fragment_shader);
  ASSERT_NE(fragment_shader, 0u);
  api->glShaderSourceFn(fragment_shader, 1,
                        use_factory ? &kVideoFragmentShaderSrcTexture2D
                                    : &kVideoFragmentShaderSrcTextureExternal,
                        nullptr);
  api->glCompileShaderFn(fragment_shader);
  api->glGetShaderivFn(fragment_shader, GL_COMPILE_STATUS, &status);
  ASSERT_NE(status, 0);

  GLuint program = api->glCreateProgramFn();
  ASSERT_NE(program, 0u);
  SCOPED_GL_CLEANUP_VAR(api, DeleteProgram, program);
  api->glAttachShaderFn(program, vertex_shader);
  api->glAttachShaderFn(program, fragment_shader);
  api->glLinkProgramFn(program);
  api->glGetProgramivFn(program, GL_LINK_STATUS, &status);
  ASSERT_NE(status, 0);

  GLint vertex_location = api->glGetAttribLocationFn(program, "a_position");
  ASSERT_NE(vertex_location, -1);

  GLint y_texture_location =
      api->glGetUniformLocationFn(program, "u_texture_y");
  ASSERT_NE(y_texture_location, -1);

  GLint uv_texture_location =
      api->glGetUniformLocationFn(program, "u_texture_uv");
  ASSERT_NE(uv_texture_location, -1);

  GLuint fbo, renderbuffer = 0u;
  api->glGenFramebuffersEXTFn(1, &fbo);
  ASSERT_NE(fbo, 0u);
  SCOPED_GL_CLEANUP_PTR(api, DeleteFramebuffersEXT, 1, fbo);
  api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, fbo);

  api->glGenRenderbuffersEXTFn(1, &renderbuffer);
  ASSERT_NE(renderbuffer, 0u);
  SCOPED_GL_CLEANUP_PTR(api, DeleteRenderbuffersEXT, 1, renderbuffer);
  api->glBindRenderbufferEXTFn(GL_RENDERBUFFER, renderbuffer);

  api->glRenderbufferStorageEXTFn(GL_RENDERBUFFER, GL_RGBA8_OES, size.width(),
                                  size.height());
  api->glFramebufferRenderbufferEXTFn(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      GL_RENDERBUFFER, renderbuffer);
  ASSERT_EQ(api->glCheckFramebufferStatusEXTFn(GL_FRAMEBUFFER),
            static_cast<unsigned>(GL_FRAMEBUFFER_COMPLETE));

  // Set the clear color to green.
  api->glViewportFn(0, 0, size.width(), size.height());
  api->glClearColorFn(0.0f, 1.0f, 0.0f, 1.0f);
  api->glClearFn(GL_COLOR_BUFFER_BIT);

  GLuint vbo = 0u;
  api->glGenBuffersARBFn(1, &vbo);
  ASSERT_NE(vbo, 0u);
  SCOPED_GL_CLEANUP_PTR(api, DeleteBuffersARB, 1, vbo);
  api->glBindBufferFn(GL_ARRAY_BUFFER, vbo);
  static const float vertices[] = {
      1.0f, 1.0f, -1.0f, 1.0f,  -1.0f, -1.0f,
      1.0f, 1.0f, -1.0f, -1.0f, 1.0f,  -1.0f,
  };
  api->glBufferDataFn(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
                      GL_STATIC_DRAW);

  ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

  // Create the representations for the planes, get the texture ids, bind to
  // samplers, and draw.
  {
    auto y_texture =
        shared_image_representation_factory_->ProduceGLTexturePassthrough(
            shared_image_refs[0]->mailbox());
    ASSERT_NE(y_texture, nullptr);

    auto uv_texture =
        shared_image_representation_factory_->ProduceGLTexturePassthrough(
            shared_image_refs[1]->mailbox());
    ASSERT_NE(uv_texture, nullptr);

    auto y_texture_access = y_texture->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    ASSERT_NE(y_texture_access, nullptr);

    auto uv_texture_access = uv_texture->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    ASSERT_NE(uv_texture_access, nullptr);

    api->glActiveTextureFn(GL_TEXTURE0);
    api->glBindTextureFn(use_factory ? GL_TEXTURE_2D : GL_TEXTURE_EXTERNAL_OES,
                         y_texture->GetTexturePassthrough()->service_id());
    ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

    api->glActiveTextureFn(GL_TEXTURE1);
    api->glBindTextureFn(use_factory ? GL_TEXTURE_2D : GL_TEXTURE_EXTERNAL_OES,
                         uv_texture->GetTexturePassthrough()->service_id());
    ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

    api->glUseProgramFn(program);

    api->glEnableVertexAttribArrayFn(vertex_location);
    api->glVertexAttribPointerFn(vertex_location, 2, GL_FLOAT, GL_FALSE, 0,
                                 nullptr);

    api->glUniform1iFn(y_texture_location, 0);
    api->glUniform1iFn(uv_texture_location, 1);

    api->glDrawArraysFn(GL_TRIANGLES, 0, 6);
    ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

    GLubyte pixel_color[4];
    api->glReadPixelsFn(size.width() / 2, size.height() / 2, 1, 1, GL_RGBA,
                        GL_UNSIGNED_BYTE, pixel_color);
    EXPECT_EQ(kYFillValue, pixel_color[0]);
    EXPECT_EQ(kUFillValue, pixel_color[1]);
    EXPECT_EQ(kVFillValue, pixel_color[2]);
    EXPECT_EQ(255, pixel_color[3]);
  }
  // TODO(dawn:551): Test Dawn access after multi-planar support lands in Dawn.
}

TEST_F(D3DImageBackingFactoryTest, CreateFromVideoTexture) {
  RunVideoTest(/*use_shared_handle=*/false, /*use_factory=*/false);
}

TEST_F(D3DImageBackingFactoryTest, CreateFromVideoTextureSharedHandle) {
  RunVideoTest(/*use_shared_handle=*/true, /*use_factory=*/false);
}

TEST_F(D3DImageBackingFactoryTest, CreateFromVideoTextureViaFactory) {
  RunVideoTest(/*use_shared_handle=*/true, /*use_factory=*/true);
}

void D3DImageBackingFactoryTest::RunOverlayTest(bool use_shared_handle,
                                                bool use_factory) {
  if (!IsD3DSharedImageSupported())
    return;

  constexpr gfx::Size size(32, 32);

  constexpr uint8_t kYFillValue = 0x12;
  constexpr uint8_t kUFillValue = 0x23;
  constexpr uint8_t kVFillValue = 0x34;

  auto shared_image_refs =
      CreateVideoImages(size, kYFillValue, kUFillValue, kVFillValue,
                        use_shared_handle, use_factory);
  ASSERT_EQ(shared_image_refs.size(), 2u);

  auto overlay_representation =
      shared_image_representation_factory_->ProduceOverlay(
          shared_image_refs[0]->mailbox());

  auto scoped_read_access = overlay_representation->BeginScopedReadAccess();
  ASSERT_TRUE(scoped_read_access);

  auto* gl_image_d3d =
      gl::GLImage::ToGLImageD3D(scoped_read_access->gl_image());
  ASSERT_TRUE(gl_image_d3d);

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      shared_image_factory_->GetDeviceForTesting();

  CD3D11_TEXTURE2D_DESC staging_desc(
      DXGI_FORMAT_NV12, size.width(), size.height(), 1, 1, 0,
      D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ);

  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture;
  HRESULT hr =
      d3d11_device->CreateTexture2D(&staging_desc, nullptr, &staging_texture);
  ASSERT_EQ(hr, S_OK);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
  d3d11_device->GetImmediateContext(&device_context);

  device_context->CopyResource(staging_texture.Get(),
                               gl_image_d3d->texture().Get());
  D3D11_MAPPED_SUBRESOURCE mapped_resource = {};
  hr = device_context->Map(staging_texture.Get(), 0, D3D11_MAP_READ, 0,
                           &mapped_resource);
  ASSERT_EQ(hr, S_OK);

  CheckYUV(static_cast<const uint8_t*>(mapped_resource.pData),
           mapped_resource.RowPitch, size, kYFillValue, kUFillValue,
           kVFillValue);

  device_context->Unmap(staging_texture.Get(), 0);
}

TEST_F(D3DImageBackingFactoryTest, CreateFromVideoTextureOverlay) {
  RunOverlayTest(/*use_shared_handle=*/false, /*use_factory=*/false);
}

TEST_F(D3DImageBackingFactoryTest, CreateFromVideoTextureSharedHandleOverlay) {
  RunOverlayTest(/*use_shared_handle=*/true, /*use_factory=*/false);
}

TEST_F(D3DImageBackingFactoryTest, CreateFromVideoTextureViaFactoryOverlay) {
  RunOverlayTest(/*use_shared_handle=*/true, /*use_factory=*/true);
}

TEST_F(D3DImageBackingFactoryTest, CreateFromSharedMemory) {
  if (!IsD3DSharedImageSupported())
    return;

  constexpr gfx::Size size(32, 32);
  constexpr size_t kDataSize = size.width() * size.height() * 3 / 2;

  base::UnsafeSharedMemoryRegion shm_region =
      base::UnsafeSharedMemoryRegion::Create(kDataSize);
  {
    base::WritableSharedMemoryMapping shm_mapping = shm_region.Map();
    FillYUV(shm_mapping.GetMemoryAs<uint8_t>(), size, 255, 255, 255);
  }

  constexpr size_t kNumPlanes = 2;
  const gpu::Mailbox mailboxes[kNumPlanes] = {
      gpu::Mailbox::GenerateForSharedImage(),
      gpu::Mailbox::GenerateForSharedImage()};
  const gfx::BufferPlane planes[kNumPlanes] = {gfx::BufferPlane::Y,
                                               gfx::BufferPlane::UV};
  constexpr uint32_t usage =
      gpu::SHARED_IMAGE_USAGE_VIDEO_DECODE | gpu::SHARED_IMAGE_USAGE_GLES2 |
      gpu::SHARED_IMAGE_USAGE_RASTER | gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
      gpu::SHARED_IMAGE_USAGE_SCANOUT;
  std::vector<std::unique_ptr<SharedImageBacking>> shared_image_backings;
  for (size_t i = 0; i < kNumPlanes; i++) {
    gfx::GpuMemoryBufferHandle shm_gmb_handle;
    shm_gmb_handle.type = gfx::SHARED_MEMORY_BUFFER;
    shm_gmb_handle.region = shm_region.Duplicate();
    DCHECK(shm_gmb_handle.region.IsValid());
    shm_gmb_handle.stride = size.width();

    // CompoundImageBacking wrapping D3DImageBacking is required for shared
    // memory support.
    auto backing = CompoundImageBacking::CreateSharedMemory(
        shared_image_factory_.get(), /*allow_shm_overlays=*/true, mailboxes[i],
        std::move(shm_gmb_handle), gfx::BufferFormat::YUV_420_BIPLANAR,
        planes[i], size, gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin,
        kPremul_SkAlphaType, usage);
    EXPECT_NE(backing, nullptr);

    shared_image_backings.push_back(std::move(backing));
  }
  EXPECT_EQ(shared_image_backings.size(), kNumPlanes);

  const gfx::Size plane_sizes[kNumPlanes] = {
      size, gfx::Size(size.width() / 2, size.height() / 2)};
  const viz::ResourceFormat plane_formats[kNumPlanes] = {viz::RED_8,
                                                         viz::RG_88};

  std::vector<std::unique_ptr<SharedImageRepresentationFactoryRef>>
      shared_image_refs;
  for (size_t i = 0; i < shared_image_backings.size(); i++) {
    auto& backing = shared_image_backings[i];

    EXPECT_EQ(backing->mailbox(), mailboxes[i]);
    EXPECT_EQ(backing->size(), plane_sizes[i]);
    EXPECT_EQ(backing->format(),
              viz::SharedImageFormat::SinglePlane(plane_formats[i]));
    EXPECT_EQ(backing->color_space(), gfx::ColorSpace());
    EXPECT_EQ(backing->surface_origin(), kTopLeft_GrSurfaceOrigin);
    EXPECT_EQ(backing->alpha_type(), kPremul_SkAlphaType);
    EXPECT_EQ(backing->usage(), usage);
    EXPECT_TRUE(backing->IsCleared());

    shared_image_refs.push_back(shared_image_manager_.Register(
        std::move(backing), memory_type_tracker_.get()));
  }
  ASSERT_EQ(shared_image_refs.size(), 2u);

  constexpr uint8_t kYClearValue = 0x12;
  constexpr uint8_t kUClearValue = 0x23;
  constexpr uint8_t kVClearValue = 0x34;

  gl::GLApi* api = gl::g_current_gl_context;

  GLuint fbo;
  api->glGenFramebuffersEXTFn(1, &fbo);
  ASSERT_NE(fbo, 0u);
  SCOPED_GL_CLEANUP_PTR(api, DeleteFramebuffersEXT, 1, fbo);
  api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, fbo);

  auto y_texture =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          shared_image_refs[0]->mailbox());
  ASSERT_NE(y_texture, nullptr);

  auto y_access = y_texture->BeginScopedAccess(
      GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);
  ASSERT_NE(y_access, nullptr);

  GLuint y_texture_id = y_texture->GetTexturePassthrough()->service_id();
  api->glBindTextureFn(GL_TEXTURE_2D, y_texture_id);
  api->glFramebufferTexture2DEXTFn(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, y_texture_id, 0);
  ASSERT_EQ(api->glCheckFramebufferStatusEXTFn(GL_FRAMEBUFFER),
            static_cast<unsigned>(GL_FRAMEBUFFER_COMPLETE));
  ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

  GLubyte y_value;
  api->glReadPixelsFn(size.width() / 2, size.height() / 2, 1, 1, GL_RED,
                      GL_UNSIGNED_BYTE, &y_value);
  EXPECT_EQ(255, y_value);

  api->glViewportFn(0, 0, size.width(), size.height());
  api->glClearColorFn(kYClearValue / 255.0f, 0, 0, 0);
  api->glClearFn(GL_COLOR_BUFFER_BIT);

  api->glReadPixelsFn(size.width() / 2, size.height() / 2, 1, 1, GL_RED,
                      GL_UNSIGNED_BYTE, &y_value);
  EXPECT_EQ(kYClearValue, y_value);

  y_access.reset();
  y_texture.reset();
  EXPECT_TRUE(shared_image_refs[0]->CopyToGpuMemoryBuffer());

  auto uv_texture =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          shared_image_refs[1]->mailbox());
  ASSERT_NE(uv_texture, nullptr);

  auto uv_access = uv_texture->BeginScopedAccess(
      GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);
  ASSERT_NE(uv_access, nullptr);

  GLuint uv_texture_id = uv_texture->GetTexturePassthrough()->service_id();
  api->glBindTextureFn(GL_TEXTURE_2D, uv_texture_id);
  api->glFramebufferTexture2DEXTFn(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, uv_texture_id, 0);
  ASSERT_EQ(api->glCheckFramebufferStatusEXTFn(GL_FRAMEBUFFER),
            static_cast<unsigned>(GL_FRAMEBUFFER_COMPLETE));
  ASSERT_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

  GLubyte uv_value[2];
  api->glReadPixelsFn(size.width() / 4, size.height() / 4, 1, 1, GL_RG,
                      GL_UNSIGNED_BYTE, uv_value);
  EXPECT_EQ(255, uv_value[0]);
  EXPECT_EQ(255, uv_value[1]);

  api->glViewportFn(0, 0, size.width(), size.height());
  api->glClearColorFn(kUClearValue / 255.0f, kVClearValue / 255.0f, 0, 0);
  api->glClearFn(GL_COLOR_BUFFER_BIT);

  api->glReadPixelsFn(size.width() / 4, size.height() / 4, 1, 1, GL_RG,
                      GL_UNSIGNED_BYTE, uv_value);
  EXPECT_EQ(kUClearValue, uv_value[0]);
  EXPECT_EQ(kVClearValue, uv_value[1]);

  uv_access.reset();
  uv_texture.reset();
  EXPECT_TRUE(shared_image_refs[1]->CopyToGpuMemoryBuffer());

  {
    base::WritableSharedMemoryMapping shm_mapping = shm_region.Map();
    CheckYUV(shm_mapping.GetMemoryAs<uint8_t>(), size.width(), size,
             kYClearValue, kUClearValue, kVClearValue);
  }

  // Both planes use the same underlying shared memory buffer with different
  // offsets. Accessing via the Y plane overlay allows reading both planes.
  {
    auto overlay_representation =
        shared_image_representation_factory_->ProduceOverlay(
            shared_image_refs[0]->mailbox());

    auto scoped_read_access = overlay_representation->BeginScopedReadAccess();
    ASSERT_TRUE(scoped_read_access);

    auto* gl_image_memory =
        gl::GLImage::ToGLImageMemory(scoped_read_access->gl_image());
    ASSERT_TRUE(gl_image_memory);

    CheckYUV(gl_image_memory->memory(), gl_image_memory->stride(), size,
             kYClearValue, kUClearValue, kVClearValue);
  }
}

}  // namespace gpu
