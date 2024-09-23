// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webxr/mailbox_to_surface_bridge_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/gpu/context_provider.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_surface_tracker.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/android/surface_texture.h"

#define VOID_OFFSET(x) reinterpret_cast<void*>(x)
#define SHADER(Src) #Src

namespace {

/* clang-format off */
const char kQuadCopyVertex[] = SHADER(
    precision mediump float;
    attribute vec4 a_Position;
    attribute vec2 a_TexCoordinate;
    varying highp vec2 v_TexCoordinate;
    uniform mat4 u_UvTransform;
    void main() {
      highp vec4 uv_in = vec4(a_TexCoordinate.x, a_TexCoordinate.y, 0, 1);
      v_TexCoordinate = (u_UvTransform * uv_in).xy;
      gl_Position = a_Position;
    }
);

const char kQuadCopyFragment[] = SHADER(
    precision highp float;
    uniform sampler2D u_Texture;
    varying vec2 v_TexCoordinate;
    void main() {
      gl_FragColor = texture2D(u_Texture, v_TexCoordinate);
    }
);

const float kQuadVertices[] = {
    // x     y    u,   v
    -1.f,  1.f, 0.f, 1.f,
    -1.f, -1.f, 0.f, 0.f,
     1.f, -1.f, 1.f, 0.f,
     1.f,  1.f, 1.f, 1.f};
/* clang-format on */

static constexpr int kQuadVerticesSize = sizeof(kQuadVertices);

GLuint CompileShader(gpu::gles2::GLES2Interface* gl,
                     GLenum shader_type,
                     const GLchar* shader_source) {
  GLuint shader_handle = gl->CreateShader(shader_type);
  if (shader_handle != 0) {
    // Pass in the shader source.
    GLint len = strlen(shader_source);
    gl->ShaderSource(shader_handle, 1, &shader_source, &len);
    // Compile the shader.
    gl->CompileShader(shader_handle);
    // Get the compilation status.
    GLint status = 0;
    gl->GetShaderiv(shader_handle, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
      GLint info_log_length = 0;
      gl->GetShaderiv(shader_handle, GL_INFO_LOG_LENGTH, &info_log_length);
      auto str_info_log = std::make_unique<GLchar[]>(info_log_length + 1);
      gl->GetShaderInfoLog(shader_handle, info_log_length, nullptr,
                           str_info_log.get());
      DLOG(ERROR) << "Error compiling shader: " << str_info_log.get();
      gl->DeleteShader(shader_handle);
      shader_handle = 0;
    }
  }

  return shader_handle;
}

GLuint CreateAndLinkProgram(gpu::gles2::GLES2Interface* gl,
                            GLuint vertex_shader_handle,
                            GLuint fragment_shader_handle) {
  GLuint program_handle = gl->CreateProgram();

  if (program_handle != 0) {
    // Bind the vertex shader to the program.
    gl->AttachShader(program_handle, vertex_shader_handle);

    // Bind the fragment shader to the program.
    gl->AttachShader(program_handle, fragment_shader_handle);

    // Link the two shaders together into a program.
    gl->LinkProgram(program_handle);

    // Get the link status.
    GLint link_status = 0;
    gl->GetProgramiv(program_handle, GL_LINK_STATUS, &link_status);

    // If the link failed, delete the program.
    if (link_status == GL_FALSE) {
      GLint info_log_length;
      gl->GetProgramiv(program_handle, GL_INFO_LOG_LENGTH, &info_log_length);

      auto str_info_log = std::make_unique<GLchar[]>(info_log_length + 1);
      gl->GetProgramInfoLog(program_handle, info_log_length, nullptr,
                            str_info_log.get());
      DLOG(ERROR) << "Error compiling program: " << str_info_log.get();
      gl->DeleteProgram(program_handle);
      program_handle = 0;
    }
  }

  return program_handle;
}

GLuint ConsumeTexture(gpu::gles2::GLES2Interface* gl,
                      const gpu::MailboxHolder& mailbox) {
  TRACE_EVENT0("gpu", "MailboxToSurfaceBridgeImpl::ConsumeTexture");
  gl->WaitSyncTokenCHROMIUM(mailbox.sync_token.GetConstData());

  return gl->CreateAndTexStorage2DSharedImageCHROMIUM(mailbox.mailbox.name);
}

}  // namespace

namespace webxr {

MailboxToSurfaceBridgeImpl::MailboxToSurfaceBridgeImpl() {
  DVLOG(1) << __FUNCTION__;
}

MailboxToSurfaceBridgeImpl::~MailboxToSurfaceBridgeImpl() {
  if (surface_handle_) {
    // Unregister from the surface tracker to avoid a resource leak.
    gpu::GpuSurfaceTracker* tracker = gpu::GpuSurfaceTracker::Get();
    tracker->RemoveSurface(surface_handle_);
  }
  DestroyContext();
  DVLOG(1) << __FUNCTION__;
}

bool MailboxToSurfaceBridgeImpl::IsConnected() {
  return context_provider_ && gl_ && context_support_;
}

bool MailboxToSurfaceBridgeImpl::IsGpuWorkaroundEnabled(int32_t workaround) {
  DCHECK(IsConnected());

  return context_provider_->GetGpuFeatureInfo().IsWorkaroundEnabled(workaround);
}

void MailboxToSurfaceBridgeImpl::OnContextAvailableOnUiThread(
    scoped_refptr<viz::ContextProvider> provider) {
  DVLOG(1) << __FUNCTION__;
  // Must save a reference to the viz::ContextProvider to keep it alive,
  // otherwise the GL context created from it becomes invalid on its
  // destruction.
  context_provider_ = std::move(provider);

  DCHECK(on_context_bound_);
  gl_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MailboxToSurfaceBridgeImpl::BindContextProviderToCurrentThread,
          base::Unretained(this)));
}

void MailboxToSurfaceBridgeImpl::BindContextProviderToCurrentThread() {
  auto result = context_provider_->BindToCurrentSequence();
  if (result != gpu::ContextResult::kSuccess) {
    DLOG(ERROR) << "Failed to init viz::ContextProvider";
    return;
  }

  gl_ = context_provider_->ContextGL();
  context_support_ = context_provider_->ContextSupport();

  if (!gl_) {
    DLOG(ERROR) << "Did not get a GL context";
    return;
  }
  if (!context_support_) {
    DLOG(ERROR) << "Did not get a ContextSupport";
    return;
  }
  InitializeRenderer();

  DVLOG(1) << __FUNCTION__ << ": Context ready";
  if (on_context_bound_) {
    std::move(on_context_bound_).Run();
  }
}

void MailboxToSurfaceBridgeImpl::CreateSurface(
    gl::SurfaceTexture* surface_texture) {
  gpu::GpuSurfaceTracker* tracker = gpu::GpuSurfaceTracker::Get();
  surface_handle_ = tracker->AddSurfaceForNativeWidget(
      gpu::SurfaceRecord(gl::ScopedJavaSurface(surface_texture),
                         false /* can_be_used_with_surface_control */));
  // Unregistering happens in the destructor.
}

void MailboxToSurfaceBridgeImpl::CreateAndBindContextProvider(
    base::OnceClosure on_bound_callback) {
  gl_thread_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  on_context_bound_ = std::move(on_bound_callback);

  // The callback to run in this thread. It is necessary to keep |surface| alive
  // until the context becomes available. So pass it on to the callback, so that
  // it stays alive, and is destroyed on the same thread once done.
  auto callback =
      base::BindOnce(&MailboxToSurfaceBridgeImpl::OnContextAvailableOnUiThread,
                     weak_ptr_factory_.GetWeakPtr());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(
                     [](int surface_handle,
                        content::Compositor::ContextProviderCallback callback) {
                       content::Compositor::CreateContextProvider(
                           surface_handle,
                           gpu::SharedMemoryLimits::ForMailboxContext(),
                           std::move(callback));
                     },
                     surface_handle_, std::move(callback)));
}

void MailboxToSurfaceBridgeImpl::ResizeSurface(int width, int height) {
  // Make sure we have the surface.
  CHECK(surface_handle_);

  surface_width_ = width;
  surface_height_ = height;

  if (!IsConnected()) {
    // We're not initialized yet, save the requested size for later.
    needs_resize_ = true;
    return;
  }
  DVLOG(1) << __FUNCTION__ << ": resize Surface to " << surface_width_ << "x"
           << surface_height_;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  gl_->ResizeCHROMIUM(surface_width_, surface_height_, 1.f,
                      color_space.AsGLColorSpace(), false);
  gl_->Viewport(0, 0, surface_width_, surface_height_);
}

bool MailboxToSurfaceBridgeImpl::CopyMailboxToSurfaceAndSwap(
    const gpu::MailboxHolder& mailbox,
    const gfx::Transform& uv_transform) {
  if (!IsConnected()) {
    // We may not have a context yet, i.e. due to surface initialization
    // being incomplete. This is not an error, but we obviously can't draw
    // yet. This affects the non-shared-buffer path on Android N (or
    // if UseSharedBuffer was forced to false due to GPU bug workarounds),
    // and may result in a couple of discarded images while waiting for
    // initialization which is generally harmless.
    return false;
  }

  TRACE_EVENT0("gpu", __FUNCTION__);

  if (needs_resize_) {
    ResizeSurface(surface_width_, surface_height_);
    needs_resize_ = false;
  }

  // While it's not an error to use a zero-sized Surface, it's not going to
  // produce any visible output. Show a debug mode warning in that case to avoid
  // another annoying debugging session.
  DLOG_IF(WARNING, !surface_width_ || !surface_height_)
      << "Surface is zero-sized. Missing call to ResizeSurface?";

  GLuint sourceTexture = ConsumeTexture(gl_, mailbox);
  gl_->BeginSharedImageAccessDirectCHROMIUM(
      sourceTexture, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  DrawQuad(sourceTexture, uv_transform);
  gl_->EndSharedImageAccessDirectCHROMIUM(sourceTexture);
  gl_->DeleteTextures(1, &sourceTexture);
  gl_->SwapBuffers(swap_id_++);
  return true;
}

void MailboxToSurfaceBridgeImpl::GenSyncToken(gpu::SyncToken* out_sync_token) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(IsConnected());
  gl_->GenSyncTokenCHROMIUM(out_sync_token->GetData());
}

void MailboxToSurfaceBridgeImpl::WaitSyncToken(
    const gpu::SyncToken& sync_token) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(IsConnected());
  gl_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
}

void MailboxToSurfaceBridgeImpl::WaitForClientGpuFence(
    gfx::GpuFence& gpu_fence) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(IsConnected());
  GLuint id = gl_->CreateClientGpuFenceCHROMIUM(gpu_fence.AsClientGpuFence());
  gl_->WaitGpuFenceCHROMIUM(id);
  gl_->DestroyGpuFenceCHROMIUM(id);
}

void MailboxToSurfaceBridgeImpl::CreateGpuFence(
    const gpu::SyncToken& sync_token,
    base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(IsConnected());
  gl_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  GLuint id = gl_->CreateGpuFenceCHROMIUM();
  context_support_->GetGpuFence(id, std::move(callback));
  gl_->DestroyGpuFenceCHROMIUM(id);
}

scoped_refptr<gpu::ClientSharedImage>
MailboxToSurfaceBridgeImpl::CreateSharedImage(
    gfx::GpuMemoryBufferHandle buffer_handle,
    gfx::BufferFormat buffer_format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    gpu::SharedImageUsageSet usage,
    gpu::SyncToken& sync_token) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(IsConnected());

  auto* sii = context_provider_->SharedImageInterface();
  DCHECK(sii);

  CHECK_EQ(buffer_format, gfx::BufferFormat::RGBA_8888);
  auto client_shared_image = sii->CreateSharedImage(
      {viz::SinglePlaneFormat::kRGBA_8888, size, color_space, usage,
       "WebXrMailboxToSurfaceBridge"},
      std::move(buffer_handle));
  CHECK(client_shared_image);
  sync_token = sii->GenVerifiedSyncToken();
  DCHECK(client_shared_image->GetTextureTarget() == GL_TEXTURE_2D);
  return client_shared_image;
}

void MailboxToSurfaceBridgeImpl::DestroySharedImage(
    const gpu::SyncToken& sync_token,
    scoped_refptr<gpu::ClientSharedImage> shared_image) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(IsConnected());
  DCHECK(shared_image);

  auto* sii = context_provider_->SharedImageInterface();
  DCHECK(sii);
  sii->DestroySharedImage(sync_token, std::move(shared_image));
}

void MailboxToSurfaceBridgeImpl::DestroyContext() {
  gl_ = nullptr;
  context_provider_ = nullptr;
}

void MailboxToSurfaceBridgeImpl::InitializeRenderer() {
  GLuint vertex_shader_handle =
      CompileShader(gl_, GL_VERTEX_SHADER, kQuadCopyVertex);
  if (!vertex_shader_handle) {
    DestroyContext();
    return;
  }

  GLuint fragment_shader_handle =
      CompileShader(gl_, GL_FRAGMENT_SHADER, kQuadCopyFragment);
  if (!fragment_shader_handle) {
    DestroyContext();
    return;
  }

  GLuint program_handle =
      CreateAndLinkProgram(gl_, vertex_shader_handle, fragment_shader_handle);
  if (!program_handle) {
    DestroyContext();
    return;
  }

  // Once the program is linked the shader objects are no longer needed
  gl_->DeleteShader(vertex_shader_handle);
  gl_->DeleteShader(fragment_shader_handle);

  GLuint position_handle = gl_->GetAttribLocation(program_handle, "a_Position");
  GLuint texCoord_handle =
      gl_->GetAttribLocation(program_handle, "a_TexCoordinate");
  GLuint texUniform_handle =
      gl_->GetUniformLocation(program_handle, "u_Texture");
  uniform_uv_transform_handle_ =
      gl_->GetUniformLocation(program_handle, "u_UvTransform");

  GLuint vertexBuffer = 0;
  gl_->GenBuffers(1, &vertexBuffer);
  gl_->BindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  gl_->BufferData(GL_ARRAY_BUFFER, kQuadVerticesSize, kQuadVertices,
                  GL_STATIC_DRAW);

  // Set state once only, we assume that nobody else modifies GL state in a way
  // that would interfere with our operations.
  gl_->Disable(GL_CULL_FACE);
  gl_->DepthMask(GL_FALSE);
  gl_->Disable(GL_DEPTH_TEST);
  gl_->Disable(GL_SCISSOR_TEST);
  gl_->Disable(GL_BLEND);
  gl_->Disable(GL_POLYGON_OFFSET_FILL);

  // Not using gl_->Viewport, we assume that it defaults to the whole
  // surface and gets updated by ResizeSurface externally as
  // appropriate.

  gl_->UseProgram(program_handle);

  // Bind vertex attributes
  gl_->BindBuffer(GL_ARRAY_BUFFER, vertexBuffer);

  gl_->EnableVertexAttribArray(position_handle);
  gl_->EnableVertexAttribArray(texCoord_handle);

  static constexpr size_t VERTEX_STRIDE = sizeof(float) * 4;
  static constexpr size_t POSITION_ELEMENTS = 2;
  static constexpr size_t TEXCOORD_ELEMENTS = 2;
  static constexpr size_t POSITION_OFFSET = 0;
  static constexpr size_t TEXCOORD_OFFSET = sizeof(float) * 2;

  gl_->VertexAttribPointer(position_handle, POSITION_ELEMENTS, GL_FLOAT, false,
                           VERTEX_STRIDE, VOID_OFFSET(POSITION_OFFSET));
  gl_->VertexAttribPointer(texCoord_handle, TEXCOORD_ELEMENTS, GL_FLOAT, false,
                           VERTEX_STRIDE, VOID_OFFSET(TEXCOORD_OFFSET));

  gl_->ActiveTexture(GL_TEXTURE0);
  gl_->Uniform1i(texUniform_handle, 0);
}

void MailboxToSurfaceBridgeImpl::DrawQuad(unsigned int texture_handle,
                                          const gfx::Transform& uv_transform) {
  DCHECK(IsConnected());

  // We're redrawing over the entire viewport, but it's generally more
  // efficient on mobile tiling GPUs to clear anyway as a hint that
  // we're done with the old content.
  gl_->Clear(GL_COLOR_BUFFER_BIT);

  float uv_transform_floats[16];
  uv_transform.GetColMajorF(uv_transform_floats);
  gl_->UniformMatrix4fv(uniform_uv_transform_handle_, 1, GL_FALSE,
                        &uv_transform_floats[0]);

  // Configure texture. This is a 1:1 pixel copy since the surface
  // size is resized to match the source canvas, so we can use
  // GL_NEAREST.
  gl_->BindTexture(GL_TEXTURE_2D, texture_handle);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  if (uv_transform.IsIdentity()) {
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  } else {
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  }
  gl_->DrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

std::unique_ptr<device::MailboxToSurfaceBridge>
MailboxToSurfaceBridgeFactoryImpl::Create() const {
  return std::make_unique<MailboxToSurfaceBridgeImpl>();
}

}  // namespace webxr
