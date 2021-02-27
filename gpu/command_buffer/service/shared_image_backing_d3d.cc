// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_d3d.h"

#include "base/trace_event/memory_dump_manager.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/service/shared_image_representation_d3d.h"
#include "gpu/command_buffer/service/shared_image_representation_skia_gl.h"
#include "ui/gl/trace_util.h"

namespace gpu {

namespace {

class ScopedRestoreTexture2D {
 public:
  explicit ScopedRestoreTexture2D(gl::GLApi* api) : api_(api) {
    GLint binding = 0;
    api->glGetIntegervFn(GL_TEXTURE_BINDING_2D, &binding);
    prev_binding_ = binding;
  }

  ~ScopedRestoreTexture2D() {
    api_->glBindTextureFn(GL_TEXTURE_2D, prev_binding_);
  }

 private:
  gl::GLApi* const api_;
  GLuint prev_binding_ = 0;
  DISALLOW_COPY_AND_ASSIGN(ScopedRestoreTexture2D);
};

}  // anonymous namespace

SharedImageBackingD3D::SharedImageBackingD3D(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain,
    scoped_refptr<gles2::TexturePassthrough> texture,
    scoped_refptr<gl::GLImage> image,
    size_t buffer_index,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture,
    base::win::ScopedHandle shared_handle,
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dxgi_keyed_mutex)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      texture->estimated_size(),
                                      false /* is_thread_safe */),
      swap_chain_(std::move(swap_chain)),
      texture_(std::move(texture)),
      image_(std::move(image)),
      buffer_index_(buffer_index),
      d3d11_texture_(std::move(d3d11_texture)),
      shared_handle_(std::move(shared_handle)),
      dxgi_keyed_mutex_(std::move(dxgi_keyed_mutex)) {
  DCHECK(texture_);
}

SharedImageBackingD3D::~SharedImageBackingD3D() {
  if (!have_context())
    texture_->MarkContextLost();
  texture_ = nullptr;
  swap_chain_ = nullptr;
  d3d11_texture_.Reset();
  dxgi_keyed_mutex_.Reset();
  keyed_mutex_acquire_key_ = 0;
  keyed_mutex_acquired_ = false;
  shared_handle_.Close();
}

void SharedImageBackingD3D::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  DLOG(ERROR) << "SharedImageBackingD3D::Update : Trying to update "
                 "Shared Images associated with swap chain.";
}

bool SharedImageBackingD3D::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  mailbox_manager->ProduceTexture(mailbox(), texture_.get());
  return true;
}

std::unique_ptr<SharedImageRepresentationDawn>
SharedImageBackingD3D::ProduceDawn(SharedImageManager* manager,
                                   MemoryTypeTracker* tracker,
                                   WGPUDevice device) {
#if BUILDFLAG(USE_DAWN)
  return std::make_unique<SharedImageRepresentationDawnD3D>(manager, this,
                                                            tracker, device);
#else
  return nullptr;
#endif  // BUILDFLAG(USE_DAWN)
}

void SharedImageBackingD3D::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDump* dump,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
  // Add a |service_guid| which expresses shared ownership between the
  // various GPU dumps.
  auto client_guid = GetSharedImageGUIDForTracing(mailbox());
  base::trace_event::MemoryAllocatorDumpGuid service_guid =
      gl::GetGLTextureServiceGUIDForTracing(texture_->service_id());
  pmd->CreateSharedGlobalAllocatorDump(service_guid);

  int importance = 2;  // This client always owns the ref.
  pmd->AddOwnershipEdge(client_guid, service_guid, importance);

  // Swap chain textures only have one level backed by an image.
  image_->OnMemoryDump(pmd, client_tracing_id, dump_name);
}

bool SharedImageBackingD3D::BeginAccessD3D12(uint64_t* acquire_key) {
  if (keyed_mutex_acquired_) {
    DLOG(ERROR) << "Recursive BeginAccess not supported";
    return false;
  }
  *acquire_key = keyed_mutex_acquire_key_;
  keyed_mutex_acquire_key_++;
  keyed_mutex_acquired_ = true;
  return true;
}

void SharedImageBackingD3D::EndAccessD3D12() {
  keyed_mutex_acquired_ = false;
}

bool SharedImageBackingD3D::BeginAccessD3D11() {
  if (dxgi_keyed_mutex_) {
    if (keyed_mutex_acquired_) {
      DLOG(ERROR) << "Recursive BeginAccess not supported";
      return false;
    }
    const HRESULT hr =
        dxgi_keyed_mutex_->AcquireSync(keyed_mutex_acquire_key_, INFINITE);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Unable to acquire the keyed mutex " << std::hex << hr;
      return false;
    }
    keyed_mutex_acquire_key_++;
    keyed_mutex_acquired_ = true;
  }
  return true;
}
void SharedImageBackingD3D::EndAccessD3D11() {
  if (dxgi_keyed_mutex_) {
    const HRESULT hr = dxgi_keyed_mutex_->ReleaseSync(keyed_mutex_acquire_key_);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Unable to release the keyed mutex " << std::hex << hr;
      return;
    }
    keyed_mutex_acquired_ = false;
  }
}

HANDLE SharedImageBackingD3D::GetSharedHandle() const {
  return shared_handle_.Get();
}

gl::GLImage* SharedImageBackingD3D::GetGLImage() const {
  return image_.get();
}

bool SharedImageBackingD3D::PresentSwapChain() {
  TRACE_EVENT0("gpu", "SharedImageBackingD3D::PresentSwapChain");
  if (buffer_index_ != 0) {
    DLOG(ERROR) << "Swap chain backing does not correspond to back buffer";
    return false;
  }

  DXGI_PRESENT_PARAMETERS params = {};
  params.DirtyRectsCount = 0;
  params.pDirtyRects = nullptr;

  UINT flags = DXGI_PRESENT_ALLOW_TEARING;

  HRESULT hr = swap_chain_->Present1(0 /* interval */, flags, &params);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Present1 failed with error " << std::hex << hr;
    return false;
  }

  gl::GLApi* const api = gl::g_current_gl_context;
  ScopedRestoreTexture2D scoped_restore(api);

  api->glBindTextureFn(GL_TEXTURE_2D, texture_->service_id());
  if (!image_->BindTexImage(GL_TEXTURE_2D)) {
    DLOG(ERROR) << "GLImage::BindTexImage failed";
    return false;
  }

  TRACE_EVENT0("gpu", "SharedImageBackingD3D::PresentSwapChain::Flush");
  // Flush device context through ANGLE otherwise present could be deferred.
  api->glFlushFn();
  return true;
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageBackingD3D::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                                   MemoryTypeTracker* tracker) {
  TRACE_EVENT0("gpu", "SharedImageBackingD3D::ProduceGLTexturePassthrough");
  return std::make_unique<SharedImageRepresentationGLTexturePassthroughD3D>(
      manager, this, tracker, texture_);
}

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageBackingD3D::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  return SharedImageRepresentationSkiaGL::Create(
      ProduceGLTexturePassthrough(manager, tracker), std::move(context_state),
      manager, this, tracker);
}

std::unique_ptr<SharedImageRepresentationOverlay>
SharedImageBackingD3D::ProduceOverlay(SharedImageManager* manager,
                                      MemoryTypeTracker* tracker) {
  TRACE_EVENT0("gpu", "SharedImageBackingD3D::ProduceOverlay");
  return std::make_unique<SharedImageRepresentationOverlayD3D>(manager, this,
                                                               tracker);
}

}  // namespace gpu
