// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_d3d.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_d3d.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

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

class SharedImageBackingFactoryD3DTest : public testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    if (!SharedImageBackingFactoryD3D::IsSwapChainSupported())
      return;

    use_passthrough_texture_ = GetParam();

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
    shared_image_factory_ = std::make_unique<SharedImageBackingFactoryD3D>(
        use_passthrough_texture_);
  }

 protected:
  bool use_passthrough_texture_ = false;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  SharedImageManager shared_image_manager_;
  std::unique_ptr<MemoryTypeTracker> memory_type_tracker_;
  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  std::unique_ptr<SharedImageBackingFactoryD3D> shared_image_factory_;
};

TEST_P(SharedImageBackingFactoryD3DTest, InvalidFormat) {
  if (!SharedImageBackingFactoryD3D::IsSwapChainSupported())
    return;

  auto front_buffer_mailbox = Mailbox::GenerateForSharedImage();
  auto back_buffer_mailbox = Mailbox::GenerateForSharedImage();
  gfx::Size size(1, 1);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = gpu::SHARED_IMAGE_USAGE_SCANOUT;
  {
    auto valid_format = viz::RGBA_8888;
    auto backings = shared_image_factory_->CreateSwapChain(
        front_buffer_mailbox, back_buffer_mailbox, valid_format, size,
        color_space, usage);
    EXPECT_TRUE(backings.front_buffer);
    EXPECT_TRUE(backings.back_buffer);
    backings.front_buffer->Destroy();
    backings.back_buffer->Destroy();
  }
  {
    auto valid_format = viz::BGRA_8888;
    auto backings = shared_image_factory_->CreateSwapChain(
        front_buffer_mailbox, back_buffer_mailbox, valid_format, size,
        color_space, usage);
    EXPECT_TRUE(backings.front_buffer);
    EXPECT_TRUE(backings.back_buffer);
    backings.front_buffer->Destroy();
    backings.back_buffer->Destroy();
  }
  {
    auto valid_format = viz::RGBA_F16;
    auto backings = shared_image_factory_->CreateSwapChain(
        front_buffer_mailbox, back_buffer_mailbox, valid_format, size,
        color_space, usage);
    EXPECT_TRUE(backings.front_buffer);
    EXPECT_TRUE(backings.back_buffer);
    backings.front_buffer->Destroy();
    backings.back_buffer->Destroy();
  }
  {
    auto invalid_format = viz::RGBA_4444;
    auto backings = shared_image_factory_->CreateSwapChain(
        front_buffer_mailbox, back_buffer_mailbox, invalid_format, size,
        color_space, usage);
    EXPECT_FALSE(backings.front_buffer);
    EXPECT_FALSE(backings.back_buffer);
  }
}

TEST_P(SharedImageBackingFactoryD3DTest, CreateAndPresentSwapChain) {
  if (!SharedImageBackingFactoryD3D::IsSwapChainSupported())
    return;

  auto front_buffer_mailbox = Mailbox::GenerateForSharedImage();
  auto back_buffer_mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::RGBA_8888;
  gfx::Size size(1, 1);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = gpu::SHARED_IMAGE_USAGE_GLES2 |
                   gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT |
                   gpu::SHARED_IMAGE_USAGE_DISPLAY |
                   gpu::SHARED_IMAGE_USAGE_SCANOUT;

  auto backings = shared_image_factory_->CreateSwapChain(
      front_buffer_mailbox, back_buffer_mailbox, format, size, color_space,
      usage);
  EXPECT_TRUE(backings.front_buffer);
  EXPECT_TRUE(backings.back_buffer);

  std::unique_ptr<SharedImageRepresentationFactoryRef> back_factory_ref =
      shared_image_manager_.Register(std::move(backings.back_buffer),
                                     memory_type_tracker_.get());
  std::unique_ptr<SharedImageRepresentationFactoryRef> front_factory_ref =
      shared_image_manager_.Register(std::move(backings.front_buffer),
                                     memory_type_tracker_.get());

  GLuint back_texture_id, front_texture_id = 0u;
  gl::GLImageD3D *back_image, *front_image = 0u;
  if (use_passthrough_texture_) {
    auto back_texture = shared_image_representation_factory_
                            ->ProduceGLTexturePassthrough(back_buffer_mailbox)
                            ->GetTexturePassthrough();
    ASSERT_TRUE(back_texture);
    EXPECT_EQ(back_texture->target(), static_cast<unsigned>(GL_TEXTURE_2D));

    back_texture_id = back_texture->service_id();
    EXPECT_NE(back_texture_id, 0u);

    back_image = gl::GLImageD3D::FromGLImage(
        back_texture->GetLevelImage(GL_TEXTURE_2D, 0));

    auto front_texture = shared_image_representation_factory_
                             ->ProduceGLTexturePassthrough(front_buffer_mailbox)
                             ->GetTexturePassthrough();
    ASSERT_TRUE(front_texture);
    EXPECT_EQ(front_texture->target(), static_cast<unsigned>(GL_TEXTURE_2D));

    front_texture_id = front_texture->service_id();
    EXPECT_NE(front_texture_id, 0u);

    front_image = gl::GLImageD3D::FromGLImage(
        front_texture->GetLevelImage(GL_TEXTURE_2D, 0));
  } else {
    auto* back_texture = shared_image_representation_factory_
                             ->ProduceGLTexture(back_buffer_mailbox)
                             ->GetTexture();
    ASSERT_TRUE(back_texture);
    EXPECT_EQ(back_texture->target(), static_cast<unsigned>(GL_TEXTURE_2D));

    back_texture_id = back_texture->service_id();
    EXPECT_NE(back_texture_id, 0u);

    gles2::Texture::ImageState image_state = gles2::Texture::UNBOUND;
    back_image = gl::GLImageD3D::FromGLImage(
        back_texture->GetLevelImage(GL_TEXTURE_2D, 0, &image_state));
    EXPECT_EQ(image_state, gles2::Texture::BOUND);

    auto* front_texture = shared_image_representation_factory_
                              ->ProduceGLTexture(front_buffer_mailbox)
                              ->GetTexture();
    ASSERT_TRUE(front_texture);
    EXPECT_EQ(front_texture->target(), static_cast<unsigned>(GL_TEXTURE_2D));

    front_texture_id = front_texture->service_id();
    EXPECT_NE(front_texture_id, 0u);

    image_state = gles2::Texture::UNBOUND;
    front_image = gl::GLImageD3D::FromGLImage(
        front_texture->GetLevelImage(GL_TEXTURE_2D, 0, &image_state));
    EXPECT_EQ(image_state, gles2::Texture::BOUND);
  }

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

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         SharedImageBackingFactoryD3DTest,
                         testing::Bool());

}  // anonymous namespace
}  // namespace gpu
