// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_webgl_texture_array_swap_chain.h"

#include "third_party/blink/renderer/modules/webgl/webgl_framebuffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_texture.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"

namespace blink {

namespace {

XRWebGLSwapChain::Descriptor MakeLayerDescriptor(
    XRWebGLSwapChain* wrapped_swap_chain,
    uint32_t layers) {
  // Copy the wrapped swap chain's descriptor and divide its width by the
  // number of requested layers.
  XRWebGLSwapChain::Descriptor descriptor = wrapped_swap_chain->descriptor();

  CHECK_EQ(descriptor.width % layers, 0ul);
  descriptor.width /= layers;
  descriptor.layers = layers;
  return descriptor;
}

}  // namespace

XRWebGLTextureArraySwapChain::XRWebGLTextureArraySwapChain(
    XRWebGLSwapChain* wrapped_swap_chain,
    uint32_t layers)
    : XRWebGLSwapChain(wrapped_swap_chain->context(),
                       MakeLayerDescriptor(wrapped_swap_chain, layers),
                       wrapped_swap_chain->webgl2()),
      wrapped_swap_chain_(wrapped_swap_chain) {
  CHECK(wrapped_swap_chain_);
  CHECK(webgl2());  // Texture arrays are only available in WebGL 2
}

XRWebGLTextureArraySwapChain::~XRWebGLTextureArraySwapChain() {
  if (owned_texture_) {
    gpu::gles2::GLES2Interface* gl = context()->ContextGL();
    if (!gl) {
      return;
    }

    gl->DeleteTextures(1, &owned_texture_);
  }
}

WebGLUnownedTexture* XRWebGLTextureArraySwapChain::ProduceTexture() {
  gpu::gles2::GLES2Interface* gl = context()->ContextGL();
  if (!gl) {
    return nullptr;
  }

  gl->GenTextures(1, &owned_texture_);
  gl->BindTexture(GL_TEXTURE_2D_ARRAY, owned_texture_);
  gl->TexStorage3D(GL_TEXTURE_2D_ARRAY, 1, descriptor().internal_format,
                   descriptor().width, descriptor().height,
                   descriptor().layers);
  gl->TexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl->TexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // WebGLRenderingContextBase inherits from DrawingBuffer::Client, but makes
  // all the methods private. Downcasting allows us to access them.
  DrawingBuffer::Client* client =
      static_cast<DrawingBuffer::Client*>(context());
  client->DrawingBufferClientRestoreTexture2DArrayBinding();

  return MakeGarbageCollected<WebGLUnownedTexture>(context(), owned_texture_,
                                                   GL_TEXTURE_2D_ARRAY);
}

void XRWebGLTextureArraySwapChain::SetLayer(XRCompositionLayer* layer) {
  XRWebGLSwapChain::SetLayer(layer);
  wrapped_swap_chain_->SetLayer(layer);
}

void XRWebGLTextureArraySwapChain::OnFrameStart() {
  wrapped_swap_chain_->OnFrameStart();
}

void XRWebGLTextureArraySwapChain::OnFrameEnd() {
  if (!texture_was_queried()) {
    wrapped_swap_chain_->OnFrameEnd();
    return;
  }

  gpu::gles2::GLES2Interface* gl = context()->ContextGL();
  if (!gl) {
    return;
  }

  // Copy from the layers texture to the side-by-side wrapped texture.
  // Note: This could be done with less state-shifting but a bug on Qualcomm
  // devices prevents copying from non-zero layers of an array textures.
  // See crbug.com/391919452
  WebGLUnownedTexture* source_texture = GetCurrentTexture();
  WebGLUnownedTexture* wrapped_texture =
      wrapped_swap_chain_->GetCurrentTexture();

  gl->BindFramebuffer(GL_FRAMEBUFFER, GetFramebuffer()->Object());
  gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           wrapped_texture->Object(), 0);

  // It would be more efficient to track this state in the WebGL context and
  // restore to the tracked values, but this is a temporary solution for an edge
  // case that should no longer be needed once array SharedImages are available
  // so in the meantime we'll simply query the values.
  std::array<GLint, 4> curr_viewport = {0, 0, 0, 0};
  gl->GetIntegerv(GL_VIEWPORT, curr_viewport.data());

  const bool depth_test_enabled = gl->IsEnabled(GL_DEPTH_TEST);
  const bool stencil_test_enabled = gl->IsEnabled(GL_STENCIL_TEST);
  const bool culling_enabled = gl->IsEnabled(GL_CULL_FACE);
  const bool blend_enabled = gl->IsEnabled(GL_BLEND);
  const bool dither_enabled = gl->IsEnabled(GL_DITHER);

  // Ensure that all possible state that could interfere with the draw is reset.
  gl->Viewport(0, 0, wrapped_swap_chain_->descriptor().width,
               wrapped_swap_chain_->descriptor().height);

  gl->Disable(GL_DEPTH_TEST);
  gl->Disable(GL_STENCIL_TEST);
  gl->Disable(GL_CULL_FACE);
  gl->Disable(GL_BLEND);
  gl->Disable(GL_DITHER);
  gl->Disable(GL_SCISSOR_TEST);

  gl->ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  gl->DepthMask(GL_FALSE);
  gl->BindVertexArrayOES(vao_);

  gl->UseProgram(GetCopyProgram());
  gl->Uniform1i(texture_uniform_, 0);
  gl->Uniform1f(layer_count_uniform_, descriptor().layers);

  gl->ActiveTexture(GL_TEXTURE0);
  gl->BindTexture(GL_TEXTURE_2D_ARRAY, source_texture->Object());

  // Draw one quad for each layer.
  gl->DrawArraysInstancedANGLE(GL_TRIANGLES, 0, 6, descriptor().layers);

  // ClearCurrentTexture resets the framebuffer binding and mask/clear values
  // prior to returning.
  ClearCurrentTexture();

  // Restore manually tracked state
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
  client->DrawingBufferClientRestoreTexture2DArrayBinding();
  client->DrawingBufferClientRestoreScissorTest();

  context()->RestoreVertexArrayObjectBinding();
  context()->RestoreProgram();
  context()->RestoreActiveTexture();

  wrapped_swap_chain_->OnFrameEnd();

  // Intentionally not calling ResetCurrentTexture() here to keep the previously
  // produced texture for the next frame.
}

// Gets a program that does a copy from a TEXTURE_2D_ARRAY to a side-by-side
// framebuffer. The copy is done with a single instanced draw call, with each
// instance blitting a different layer to an offset position in the framebuffer.
GLuint XRWebGLTextureArraySwapChain::GetCopyProgram() {
  // Check to see if we already have a copy program, and if not create one.
  if (!copy_program_) {
    gpu::gles2::GLES2Interface* gl = context()->ContextGL();
    if (!gl) {
      return 0;
    }

    // Internal shader used to copy between two textures
    copy_program_ = gl->CreateProgram();
    std::string vert_source = R"(#version 300 es
      out vec3 v_texcoord;
      uniform float u_layer_count;

      void main() {
          const vec2 quad[6] = vec2[6]
          (
              vec2(0.0f, 0.0f),
              vec2(0.0f, 1.0f),
              vec2(1.0f, 0.0f),

              vec2(0.0f, 1.0f),
              vec2(1.0f, 0.0f),
              vec2(1.0f, 1.0f)
          );

          vec2 pos = quad[gl_VertexID] + vec2(float(gl_InstanceID), 0.0f);
          pos /= vec2(u_layer_count, 1.0f);
          gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
          v_texcoord = vec3(quad[gl_VertexID], gl_InstanceID);
      }
    )";

    const GLchar* vert_shader_data = vert_source.c_str();
    const GLint vert_shader_length = vert_source.length();

    GLuint vs = gl->CreateShader(GL_VERTEX_SHADER);
    gl->ShaderSource(vs, 1, &vert_shader_data, &vert_shader_length);
    gl->CompileShader(vs);
    gl->AttachShader(copy_program_, vs);
    gl->DeleteShader(vs);

    std::string frag_source = R"(#version 300 es
      precision highp float;
      precision highp sampler2DArray;
      uniform sampler2DArray u_source_texture;

      in vec3 v_texcoord;
      out vec4 output_color;

      void main() {
        output_color = texture(u_source_texture, v_texcoord);
      }
    )";

    const GLchar* frag_shader_data = frag_source.c_str();
    const GLint frag_shader_length = frag_source.length();

    GLuint fs = gl->CreateShader(GL_FRAGMENT_SHADER);
    gl->ShaderSource(fs, 1, &frag_shader_data, &frag_shader_length);
    gl->CompileShader(fs);
    gl->AttachShader(copy_program_, fs);
    gl->DeleteShader(fs);

    gl->LinkProgram(copy_program_);
    texture_uniform_ =
        gl->GetUniformLocation(copy_program_, "u_source_texture");
    layer_count_uniform_ =
        gl->GetUniformLocation(copy_program_, "u_layer_count");
  }

  return copy_program_;
}

scoped_refptr<StaticBitmapImage>
XRWebGLTextureArraySwapChain::TransferToStaticBitmapImage() {
  return wrapped_swap_chain_->TransferToStaticBitmapImage();
}

void XRWebGLTextureArraySwapChain::Trace(Visitor* visitor) const {
  visitor->Trace(wrapped_swap_chain_);
  XRWebGLSwapChain::Trace(visitor);
}

}  // namespace blink
