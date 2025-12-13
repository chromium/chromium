// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_webgl_cubemap_swap_chain.h"

#include "third_party/blink/renderer/modules/webgl/webgl_framebuffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_texture.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"

namespace blink {

namespace {
constexpr GLushort kQuadIndices[] = {
    0, 1, 2,  // Triangle 1: bottom-left, bottom-right, top-left
    2, 1, 3   // Triangle 2: top-left, bottom-right, top-right
};
constexpr GLfloat kQuadVertices[] = {
    -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, -0.5f,
};

constexpr char kVertexShader[] = R"(#version 100
  precision highp float;

  attribute vec2 a_Position;
  // Use a float uniform for better ES 2.0 compatibility
  uniform float u_FaceIndex;
  varying vec3 v_TexCoord3D;

  void main() {
    vec2 uv = a_Position * 2.0;

    // We use comparisons (< 0.5, < 1.5, etc.) to safely detect the integer value
    // stored in the float.
    if (u_FaceIndex < 0.5) { // Face 0 (+X)
      v_TexCoord3D = vec3(1.0, -uv.y, -uv.x);
    } else if (u_FaceIndex < 1.5) { // Face 1 (-X)
      v_TexCoord3D = vec3(-1.0, -uv.y, uv.x);
    } else if (u_FaceIndex < 2.5) { // Face 2 (+Y)
      v_TexCoord3D = vec3(uv.x, 1.0, uv.y);
    } else if (u_FaceIndex < 3.5) { // Face 3 (-Y)
      v_TexCoord3D = vec3(uv.x, -1.0, -uv.y);
    } else if (u_FaceIndex < 4.5) { // Face 4 (+Z)
      v_TexCoord3D = vec3(uv.x, -uv.y, 1.0);
    } else { // Face 5 (-Z)
      v_TexCoord3D = vec3(-uv.x, -uv.y, -1.0);
    }

    gl_Position = vec4(uv.x, uv.y, 0.0, 1.0);
  })";

constexpr char kFragmentShader[] = R"(#version 100
  precision highp float;
  varying vec3 v_TexCoord3D;
  uniform highp samplerCube u_Cubemap;

  void main() {
    gl_FragColor = textureCube(u_Cubemap, v_TexCoord3D);
  })";

void VerifyShader(gpu::gles2::GLES2Interface* gl, GLuint shader_id) {
  GLint compile_status = 0;
  gl->GetShaderiv(shader_id, GL_COMPILE_STATUS, &compile_status);

  if (compile_status == GL_FALSE) {
    // Get the length of the error log
    GLint log_length = 0;
    gl->GetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);

    // Allocate a buffer and retrieve the log
    std::vector<char> info_log(log_length);
    gl->GetShaderInfoLog(shader_id, log_length, nullptr, info_log.data());

    CHECK(compile_status != GL_FALSE) << "Shader compilation failed:\n"
                                      << info_log.data();
  }
}

void VerifyProgram(gpu::gles2::GLES2Interface* gl, GLuint program_id) {
  GLint link_status = 0;
  gl->GetProgramiv(program_id, GL_LINK_STATUS, &link_status);

  if (link_status == GL_FALSE) {
    // Get the length of the error log
    GLint log_length = 0;
    gl->GetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);

    // Allocate a buffer and retrieve the log
    std::vector<char> info_log(log_length);
    gl->GetProgramInfoLog(program_id, log_length, nullptr, info_log.data());

    CHECK(link_status != GL_FALSE) << "Program linking failed:\n"
                                   << info_log.data();
  }
}

void AttachShader(gpu::gles2::GLES2Interface* gl,
                  GLuint program_id,
                  const char* shader_source,
                  GLenum shader_type) {
  const std::string_view source(shader_source);
  const GLchar* shader_data = source.data();
  const GLint shader_length = source.length();

  GLuint shader = gl->CreateShader(shader_type);
  gl->ShaderSource(shader, 1, &shader_data, &shader_length);
  gl->CompileShader(shader);
  VerifyShader(gl, shader);
  gl->AttachShader(program_id, shader);
  gl->DeleteShader(shader);
}

}  // namespace

XRWebGLCubemapSwapChain::XRWebGLCubemapSwapChain(
    XRWebGLSwapChain* wrapped_swapchain)
    : XRWebGLSwapChain(wrapped_swapchain->context(),
                       wrapped_swapchain->descriptor(),
                       wrapped_swapchain->webgl2()),
      wrapped_swapchain_(wrapped_swapchain) {}

XRWebGLCubemapSwapChain::~XRWebGLCubemapSwapChain() {
  gpu::gles2::GLES2Interface* gl = context()->ContextGL();
  if (!gl) {
    return;
  }
  if (vertex_buffer_) {
    gl->DeleteBuffers(1, &vertex_buffer_);
  }
  if (index_buffer_) {
    gl->DeleteBuffers(1, &index_buffer_);
  }
  if (copy_program_) {
    gl->DeleteProgram(copy_program_);
  }
}

WebGLUnownedTexture* XRWebGLCubemapSwapChain::ProduceTexture() {
  if (owned_texture_) {
    return owned_texture_;
  }

  gpu::gles2::GLES2Interface* gl = context()->ContextGL();
  if (!gl) {
    return nullptr;
  }

  GLuint cubemap_texture = 0;
  gl->GenTextures(1, &cubemap_texture);
  gl->BindTexture(GL_TEXTURE_CUBE_MAP, cubemap_texture);
  gl->TexStorage2DEXT(GL_TEXTURE_CUBE_MAP, 1, descriptor().internal_format,
                      descriptor().width, descriptor().height);
  gl->TexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl->TexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl->BindTexture(GL_TEXTURE_CUBE_MAP, 0);

  // WebGLRenderingContextBase inherits from DrawingBuffer::Client, but makes
  // all the methods private. Downcasting allows us to access them.
  DrawingBuffer::Client* client =
      static_cast<DrawingBuffer::Client*>(context());
  client->DrawingBufferClientRestoreTextureCubeMapBinding();

  owned_texture_ = MakeGarbageCollected<WebGLUnownedTexture>(
      context(), cubemap_texture, GL_TEXTURE_CUBE_MAP);

  copy_program_ = gl->CreateProgram();

  AttachShader(gl, copy_program_, kVertexShader, GL_VERTEX_SHADER);
  AttachShader(gl, copy_program_, kFragmentShader, GL_FRAGMENT_SHADER);

  gl->LinkProgram(copy_program_);
  VerifyProgram(gl, copy_program_);

  position_handle_ = gl->GetAttribLocation(copy_program_, "a_Position");
  CHECK_NE(position_handle_, -1);
  face_index_uniform_ = gl->GetUniformLocation(copy_program_, "u_FaceIndex");
  CHECK_NE(face_index_uniform_, -1);
  texture_uniform_ = gl->GetUniformLocation(copy_program_, "u_Cubemap");
  CHECK_NE(texture_uniform_, -1);

  gl->GenBuffers(1, &vertex_buffer_);
  gl->GenBuffers(1, &index_buffer_);

  gl->BindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  gl->BufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices,
                 GL_STATIC_DRAW);

  gl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  gl->BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kQuadIndices), kQuadIndices,
                 GL_STATIC_DRAW);

  gl->BindBuffer(GL_ARRAY_BUFFER, 0);
  gl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  return owned_texture_;
}

void XRWebGLCubemapSwapChain::SetLayer(XRCompositionLayer* layer) {
  XRWebGLSwapChain::SetLayer(layer);
  wrapped_swapchain_->SetLayer(layer);
}

void XRWebGLCubemapSwapChain::OnFrameStart() {
  wrapped_swapchain_->OnFrameStart();
}

void XRWebGLCubemapSwapChain::OnFrameEnd() {
  if (!texture_was_queried()) {
    wrapped_swapchain_->OnFrameEnd();
    return;
  }

  gpu::gles2::GLES2Interface* gl = context()->ContextGL();
  if (!gl) {
    return;
  }

  // Copy from the layers texture to the side-by-side wrapped texture.
  WebGLUnownedTexture* source_texture = GetCurrentTexture();
  WebGLUnownedTexture* target_texture = wrapped_swapchain_->GetCurrentTexture();

  if (!source_texture || !target_texture) {
    return;
  }

  CHECK_EQ(descriptor().width, descriptor().height);
  CHECK_EQ(wrapped_swapchain_->descriptor().width, descriptor().width);
  CHECK_EQ(wrapped_swapchain_->descriptor().height, descriptor().height);

  // Read the old state.
  std::array<GLint, 4> curr_viewport = {0, 0, 0, 0};
  gl->GetIntegerv(GL_VIEWPORT, curr_viewport.data());

  const bool depth_test_enabled = gl->IsEnabled(GL_DEPTH_TEST);
  const bool stencil_test_enabled = gl->IsEnabled(GL_STENCIL_TEST);
  const bool culling_enabled = gl->IsEnabled(GL_CULL_FACE);
  const bool blend_enabled = gl->IsEnabled(GL_BLEND);
  const bool dither_enabled = gl->IsEnabled(GL_DITHER);

  gl->Disable(GL_DEPTH_TEST);
  gl->Disable(GL_STENCIL_TEST);
  gl->Disable(GL_CULL_FACE);
  gl->Disable(GL_BLEND);
  gl->Disable(GL_DITHER);
  gl->Disable(GL_SCISSOR_TEST);
  gl->ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  gl->DepthMask(GL_FALSE);

  gl->ActiveTexture(GL_TEXTURE0);
  gl->BindTexture(GL_TEXTURE_CUBE_MAP,
                  source_texture->Object());  // Source cubemap

  gl->UseProgram(copy_program_);
  gl->Uniform1i(texture_uniform_, 0);

  gl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  gl->BindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  // Set up position attribute.
  gl->VertexAttribPointer(position_handle_, 2, GL_FLOAT, false, 0, nullptr);
  gl->EnableVertexAttribArray(position_handle_);

  gl->BindFramebuffer(GL_FRAMEBUFFER, GetFramebuffer()->Object());
  gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           target_texture->Object(), 0);

  const GLenum draw_buffers[] = {GL_COLOR_ATTACHMENT0};
  gl->DrawBuffersEXT(1, draw_buffers);

  // 6 faces are placed as 3 tiles per row.
  for (int i = 0; i < 6; ++i) {
    gl->Viewport(descriptor().width * (i % 3), descriptor().height * (i / 3),
                 descriptor().width, descriptor().height);
    gl->Uniform1f(face_index_uniform_, i);
    gl->DrawElements(GL_TRIANGLES, std::size(kQuadIndices), GL_UNSIGNED_SHORT,
                     nullptr);
  }

  gl->DisableVertexAttribArray(position_handle_);
  gl->BindBuffer(GL_ARRAY_BUFFER, 0);
  gl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  // ClearCurrentTexture resets the framebuffer binding and mask/clear values
  // prior to returning.
  ClearCurrentTexture();

  // Restore the saved old state
  gl->Viewport(curr_viewport[0], curr_viewport[1], curr_viewport[2],
               curr_viewport[3]);
  if (depth_test_enabled) {
    gl->Enable(GL_DEPTH_TEST);
  }
  if (stencil_test_enabled) {
    gl->Enable(GL_STENCIL_TEST);
  }
  if (culling_enabled) {
    gl->Enable(GL_CULL_FACE);
  }
  if (blend_enabled) {
    gl->Enable(GL_BLEND);
  }
  if (dither_enabled) {
    gl->Enable(GL_DITHER);
  }

  // WebGLRenderingContextBase inherits from DrawingBuffer::Client, but makes
  // all the methods private. Downcasting allows us to access them.
  DrawingBuffer::Client* client =
      static_cast<DrawingBuffer::Client*>(context());
  client->DrawingBufferClientRestoreTextureCubeMapBinding();
  client->DrawingBufferClientRestoreScissorTest();

  context()->RestoreVertexArrayObjectBinding();
  context()->RestoreProgram();
  context()->RestoreActiveTexture();

  wrapped_swapchain_->OnFrameEnd();

  // Intentionally not calling ResetCurrentTexture() here to keep the previously
  // produced texture for the next frame.
}

scoped_refptr<StaticBitmapImage>
XRWebGLCubemapSwapChain::TransferToStaticBitmapImage() {
  return {};
}

void XRWebGLCubemapSwapChain::Trace(Visitor* visitor) const {
  visitor->Trace(owned_texture_);
  visitor->Trace(wrapped_swapchain_);
  XRWebGLSwapChain::Trace(visitor);
}

}  // namespace blink
