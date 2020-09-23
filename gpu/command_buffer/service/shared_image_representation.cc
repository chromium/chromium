// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_representation.h"

#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "ui/gl/gl_fence.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#endif

namespace gpu {

SharedImageRepresentation::SharedImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* owning_tracker)
    : manager_(manager), backing_(backing), tracker_(owning_tracker) {
  DCHECK(tracker_);
  backing_->AddRef(this);
}

SharedImageRepresentation::~SharedImageRepresentation() {
  // CHECK here as we'll crash later anyway, and this makes it clearer what the
  // error is.
  CHECK(!has_scoped_access_) << "Destroying a SharedImageRepresentation with "
                                "outstanding Scoped*Access objects.";
  manager_->OnRepresentationDestroyed(backing_->mailbox(), this);
}

std::unique_ptr<SharedImageRepresentationGLTexture::ScopedAccess>
SharedImageRepresentationGLTextureBase::BeginScopedAccess(
    GLenum mode,
    AllowUnclearedAccess allow_uncleared) {
  if (allow_uncleared != AllowUnclearedAccess::kYes && !IsCleared()) {
    LOG(ERROR) << "Attempt to access an uninitialized SharedImage";
    return nullptr;
  }

  if (!BeginAccess(mode))
    return nullptr;

  UpdateClearedStateOnBeginAccess();

  constexpr GLenum kReadAccess = 0x8AF6;
  if (mode == kReadAccess)
    backing()->OnReadSucceeded();
  else
    backing()->OnWriteSucceeded();

  return std::make_unique<ScopedAccess>(
      util::PassKey<SharedImageRepresentationGLTextureBase>(), this);
}

bool SharedImageRepresentationGLTextureBase::BeginAccess(GLenum mode) {
  return true;
}

bool SharedImageRepresentationGLTextureBase::
    SupportsMultipleConcurrentReadAccess() {
  return false;
}

gpu::TextureBase* SharedImageRepresentationGLTexture::GetTextureBase() {
  return GetTexture();
}

void SharedImageRepresentationGLTexture::UpdateClearedStateOnEndAccess() {
  auto* texture = GetTexture();
  // Operations on the gles2::Texture may have cleared or uncleared it. Make
  // sure this state is reflected back in the SharedImage.
  gfx::Rect cleared_rect = texture->GetLevelClearedRect(texture->target(), 0);
  if (cleared_rect != ClearedRect())
    SetClearedRect(cleared_rect);
}

void SharedImageRepresentationGLTexture::UpdateClearedStateOnBeginAccess() {
  auto* texture = GetTexture();
  // Operations outside of the gles2::Texture may have cleared or uncleared it.
  // Make sure this state is reflected back in gles2::Texture.
  gfx::Rect cleared_rect = ClearedRect();
  if (cleared_rect != texture->GetLevelClearedRect(texture->target(), 0))
    texture->SetLevelClearedRect(texture->target(), 0, cleared_rect);
}

gpu::TextureBase*
SharedImageRepresentationGLTexturePassthrough::GetTextureBase() {
  return GetTexturePassthrough().get();
}

bool SharedImageRepresentationSkia::SupportsMultipleConcurrentReadAccess() {
  return false;
}

SharedImageRepresentationSkia::ScopedWriteAccess::ScopedWriteAccess(
    util::PassKey<SharedImageRepresentationSkia> /* pass_key */,
    SharedImageRepresentationSkia* representation,
    sk_sp<SkSurface> surface,
    std::unique_ptr<GrBackendSurfaceMutableState> end_state)
    : ScopedAccessBase(representation),
      surface_(std::move(surface)),
      end_state_(std::move(end_state)) {}

SharedImageRepresentationSkia::ScopedWriteAccess::~ScopedWriteAccess() {
  representation()->EndWriteAccess(std::move(surface_));
}

std::unique_ptr<SharedImageRepresentationSkia::ScopedWriteAccess>
SharedImageRepresentationSkia::BeginScopedWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    AllowUnclearedAccess allow_uncleared) {
  if (allow_uncleared != AllowUnclearedAccess::kYes && !IsCleared()) {
    LOG(ERROR) << "Attempt to write to an uninitialized SharedImage";
    return nullptr;
  }

  std::unique_ptr<GrBackendSurfaceMutableState> end_state;
  sk_sp<SkSurface> surface =
      BeginWriteAccess(final_msaa_count, surface_props, begin_semaphores,
                       end_semaphores, &end_state);
  if (!surface)
    return nullptr;

  backing()->OnWriteSucceeded();

  return std::make_unique<ScopedWriteAccess>(
      util::PassKey<SharedImageRepresentationSkia>(), this, std::move(surface),
      std::move(end_state));
}

std::unique_ptr<SharedImageRepresentationSkia::ScopedWriteAccess>
SharedImageRepresentationSkia::BeginScopedWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    AllowUnclearedAccess allow_uncleared) {
  return BeginScopedWriteAccess(
      0 /* final_msaa_count */,
      SkSurfaceProps(0 /* flags */, kUnknown_SkPixelGeometry), begin_semaphores,
      end_semaphores, allow_uncleared);
}

SharedImageRepresentationSkia::ScopedReadAccess::ScopedReadAccess(
    util::PassKey<SharedImageRepresentationSkia> /* pass_key */,
    SharedImageRepresentationSkia* representation,
    sk_sp<SkPromiseImageTexture> promise_image_texture,
    std::unique_ptr<GrBackendSurfaceMutableState> end_state)
    : ScopedAccessBase(representation),
      promise_image_texture_(std::move(promise_image_texture)),
      end_state_(std::move(end_state)) {}

SharedImageRepresentationSkia::ScopedReadAccess::~ScopedReadAccess() {
  representation()->EndReadAccess();
}

sk_sp<SkImage> SharedImageRepresentationSkia::ScopedReadAccess::CreateSkImage(
    GrDirectContext* context) const {
  auto surface_origin = representation()->surface_origin();
  auto color_type =
      viz::ResourceFormatToClosestSkColorType(true, representation()->format());
  auto alpha_type = representation()->alpha_type();
  auto sk_color_space = representation()->color_space().ToSkColorSpace();
  return SkImage::MakeFromTexture(
      context, promise_image_texture_->backendTexture(), surface_origin,
      color_type, alpha_type, sk_color_space);
}

std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
SharedImageRepresentationSkia::BeginScopedReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  if (!IsCleared()) {
    LOG(ERROR) << "Attempt to read from an uninitialized SharedImage";
    return nullptr;
  }

  std::unique_ptr<GrBackendSurfaceMutableState> end_state;
  sk_sp<SkPromiseImageTexture> promise_image_texture =
      BeginReadAccess(begin_semaphores, end_semaphores, &end_state);
  if (!promise_image_texture)
    return nullptr;

  backing()->OnReadSucceeded();

  return std::make_unique<ScopedReadAccess>(
      util::PassKey<SharedImageRepresentationSkia>(), this,
      std::move(promise_image_texture), std::move(end_state));
}

sk_sp<SkSurface> SharedImageRepresentationSkia::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  return BeginWriteAccess(final_msaa_count, surface_props, begin_semaphores,
                          end_semaphores);
}

sk_sp<SkSurface> SharedImageRepresentationSkia::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  return nullptr;
}

sk_sp<SkPromiseImageTexture> SharedImageRepresentationSkia::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState>* end_state) {
  return BeginReadAccess(begin_semaphores, end_semaphores);
}

sk_sp<SkPromiseImageTexture> SharedImageRepresentationSkia::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  return nullptr;
}

SharedImageRepresentationOverlay::ScopedReadAccess::ScopedReadAccess(
    util::PassKey<SharedImageRepresentationOverlay> pass_key,
    SharedImageRepresentationOverlay* representation,
    gl::GLImage* gl_image,
    std::unique_ptr<gfx::GpuFence> fence)
    : ScopedAccessBase(representation),
      gl_image_(gl_image),
      fence_(std::move(fence)) {}

SharedImageRepresentationOverlay::ScopedReadAccess::~ScopedReadAccess() {
  representation()->EndReadAccess();
}

std::unique_ptr<SharedImageRepresentationOverlay::ScopedReadAccess>
SharedImageRepresentationOverlay::BeginScopedReadAccess(bool needs_gl_image) {
  if (!IsCleared()) {
    LOG(ERROR) << "Attempt to read from an uninitialized SharedImage";
    return nullptr;
  }

  if (!BeginReadAccess())
    return nullptr;

  backing()->OnReadSucceeded();

  return std::make_unique<ScopedReadAccess>(
      util::PassKey<SharedImageRepresentationOverlay>(), this,
      needs_gl_image ? GetGLImage() : nullptr, GetReadFence());
}

SharedImageRepresentationDawn::ScopedAccess::ScopedAccess(
    util::PassKey<SharedImageRepresentationDawn> /* pass_key */,
    SharedImageRepresentationDawn* representation,
    WGPUTexture texture)
    : ScopedAccessBase(representation), texture_(texture) {}

SharedImageRepresentationDawn::ScopedAccess::~ScopedAccess() {
  representation()->EndAccess();
}

std::unique_ptr<SharedImageRepresentationDawn::ScopedAccess>
SharedImageRepresentationDawn::BeginScopedAccess(
    WGPUTextureUsage usage,
    AllowUnclearedAccess allow_uncleared) {
  if (allow_uncleared != AllowUnclearedAccess::kYes && !IsCleared()) {
    LOG(ERROR) << "Attempt to access an uninitialized SharedImage";
    return nullptr;
  }

  WGPUTexture texture = BeginAccess(usage);
  if (!texture)
    return nullptr;

  constexpr auto kWriteUsage =
      WGPUTextureUsage_CopyDst | WGPUTextureUsage_OutputAttachment;

  if (usage & kWriteUsage) {
    backing()->OnWriteSucceeded();
  } else {
    backing()->OnReadSucceeded();
  }

  return std::make_unique<ScopedAccess>(
      util::PassKey<SharedImageRepresentationDawn>(), this, texture);
}

SharedImageRepresentationFactoryRef::~SharedImageRepresentationFactoryRef() {
  backing()->UnregisterImageFactory();
  backing()->MarkForDestruction();
}

#if defined(OS_ANDROID)
std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
SharedImageRepresentationFactoryRef::GetAHardwareBuffer() {
  return backing()->GetAHardwareBuffer();
}
#endif

SharedImageRepresentationVaapi::SharedImageRepresentationVaapi(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    VaapiDependencies* vaapi_deps)
    : SharedImageRepresentation(manager, backing, tracker),
      vaapi_deps_(vaapi_deps) {}

SharedImageRepresentationVaapi::~SharedImageRepresentationVaapi() = default;

SharedImageRepresentationVaapi::ScopedWriteAccess::ScopedWriteAccess(
    util::PassKey<SharedImageRepresentationVaapi> /* pass_key */,
    SharedImageRepresentationVaapi* representation)
    : ScopedAccessBase(representation) {}

SharedImageRepresentationVaapi::ScopedWriteAccess::~ScopedWriteAccess() {
  representation()->EndAccess();
}

const media::VASurface*
SharedImageRepresentationVaapi::ScopedWriteAccess::va_surface() {
  return representation()->vaapi_deps_->GetVaSurface();
}

std::unique_ptr<SharedImageRepresentationVaapi::ScopedWriteAccess>
SharedImageRepresentationVaapi::BeginScopedWriteAccess() {
  return std::make_unique<ScopedWriteAccess>(
      util::PassKey<SharedImageRepresentationVaapi>(), this);
}

}  // namespace gpu
