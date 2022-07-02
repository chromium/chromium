// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_representation.h"

#include "base/debug/dump_without_crashing.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSurfaceMutableState.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "ui/gl/gl_fence.h"

namespace gpu {

SharedImageRepresentation::SharedImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* owning_tracker)
    : manager_(manager), backing_(backing), tracker_(owning_tracker) {
  DCHECK(tracker_);
  // TODO(hitawala): Rewrite the reference counting so that
  // SharedImageRepresentation does not need manager and manager attaches to
  // backing in Register().
  if (manager_) {
    backing_->AddRef(this);
  }
}

SharedImageRepresentation::~SharedImageRepresentation() {
  // CHECK here as we'll crash later anyway, and this makes it clearer what the
  // error is.
  CHECK(!has_scoped_access_) << "Destroying a SharedImageRepresentation with "
                                "outstanding Scoped*Access objects.";
  if (manager_) {
    manager_->OnRepresentationDestroyed(backing_->mailbox(), this);
  }
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
      base::PassKey<SharedImageRepresentationGLTextureBase>(), this);
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
    base::PassKey<SharedImageRepresentationSkia> /* pass_key */,
    SharedImageRepresentationSkia* representation,
    sk_sp<SkSurface> surface,
    std::unique_ptr<GrBackendSurfaceMutableState> end_state)
    : ScopedAccessBase(representation),
      surface_(std::move(surface)),
      end_state_(std::move(end_state)) {}

SharedImageRepresentationSkia::ScopedWriteAccess::ScopedWriteAccess(
    base::PassKey<SharedImageRepresentationSkia> /* pass_key */,
    SharedImageRepresentationSkia* representation,
    sk_sp<SkPromiseImageTexture> promise_image_texture,
    std::unique_ptr<GrBackendSurfaceMutableState> end_state)
    : ScopedAccessBase(representation),
      promise_image_texture_(std::move(promise_image_texture)),
      end_state_(std::move(end_state)) {}

SharedImageRepresentationSkia::ScopedWriteAccess::~ScopedWriteAccess() {
  if (end_state_) {
    NOTREACHED() << "Before ending write access TakeEndState() must be called "
                    "and the result passed to skia to make sure all layout and "
                    "ownership transitions are done.";

    static std::atomic_int count = 0;
    if (count++ < 3)
      base::debug::DumpWithoutCrashing();
  }

  representation()->EndWriteAccess(std::move(surface_));
}

std::unique_ptr<GrBackendSurfaceMutableState>
SharedImageRepresentationSkia::ScopedWriteAccess::TakeEndState() {
  return std::move(end_state_);
}

std::unique_ptr<SharedImageRepresentationSkia::ScopedWriteAccess>
SharedImageRepresentationSkia::BeginScopedWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    AllowUnclearedAccess allow_uncleared,
    bool use_sk_surface) {
  if (allow_uncleared != AllowUnclearedAccess::kYes && !IsCleared()) {
    LOG(ERROR) << "Attempt to write to an uninitialized SharedImage";
    return nullptr;
  }

  std::unique_ptr<GrBackendSurfaceMutableState> end_state;
  if (use_sk_surface) {
    sk_sp<SkSurface> surface =
        BeginWriteAccess(final_msaa_count, surface_props, begin_semaphores,
                         end_semaphores, &end_state);
    if (!surface) {
      LOG(ERROR) << "Unable to initialize SkSurface";
      return nullptr;
    }

    backing()->OnWriteSucceeded();

    return std::make_unique<ScopedWriteAccess>(
        base::PassKey<SharedImageRepresentationSkia>(), this,
        std::move(surface), std::move(end_state));
  }
  sk_sp<SkPromiseImageTexture> promise_image_texture =
      BeginWriteAccess(begin_semaphores, end_semaphores, &end_state);
  if (!promise_image_texture) {
    LOG(ERROR) << "Unable to initialize SkPromiseImageTexture";
    return nullptr;
  }

  backing()->OnWriteSucceeded();

  return std::make_unique<ScopedWriteAccess>(
      base::PassKey<SharedImageRepresentationSkia>(), this,
      std::move(promise_image_texture), std::move(end_state));
}

std::unique_ptr<SharedImageRepresentationSkia::ScopedWriteAccess>
SharedImageRepresentationSkia::BeginScopedWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    AllowUnclearedAccess allow_uncleared,
    bool use_sk_surface) {
  return BeginScopedWriteAccess(
      /*final_msaa_count=*/1,
      SkSurfaceProps(0 /* flags */, kUnknown_SkPixelGeometry), begin_semaphores,
      end_semaphores, allow_uncleared, use_sk_surface);
}

SharedImageRepresentationSkia::ScopedReadAccess::ScopedReadAccess(
    base::PassKey<SharedImageRepresentationSkia> /* pass_key */,
    SharedImageRepresentationSkia* representation,
    sk_sp<SkPromiseImageTexture> promise_image_texture,
    std::unique_ptr<GrBackendSurfaceMutableState> end_state)
    : ScopedAccessBase(representation),
      promise_image_texture_(std::move(promise_image_texture)),
      end_state_(std::move(end_state)) {}

SharedImageRepresentationSkia::ScopedReadAccess::~ScopedReadAccess() {
  if (end_state_) {
    NOTREACHED() << "Before ending read access TakeEndState() must be called "
                    "and the result passed to skia to make sure all layout and "
                    "ownership transitions are done.";
    static std::atomic_int count = 0;
    if (count++ < 3)
      base::debug::DumpWithoutCrashing();
  }

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

std::unique_ptr<GrBackendSurfaceMutableState>
SharedImageRepresentationSkia::ScopedReadAccess::TakeEndState() {
  return std::move(end_state_);
}

std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
SharedImageRepresentationSkia::BeginScopedReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  if (!IsCleared()) {
    auto cr = ClearedRect();
    LOG(ERROR) << base::StringPrintf(
        "Attempt to read from an uninitialized SharedImage. "
        "Initialized region: (%d, %d, %d, %d) Size: (%d, %d)",
        cr.x(), cr.y(), cr.width(), cr.height(), size().width(),
        size().height());
    return nullptr;
  }

  std::unique_ptr<GrBackendSurfaceMutableState> end_state;
  sk_sp<SkPromiseImageTexture> promise_image_texture =
      BeginReadAccess(begin_semaphores, end_semaphores, &end_state);
  if (!promise_image_texture)
    return nullptr;

  backing()->OnReadSucceeded();

  return std::make_unique<ScopedReadAccess>(
      base::PassKey<SharedImageRepresentationSkia>(), this,
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

#if BUILDFLAG(IS_ANDROID)
AHardwareBuffer* SharedImageRepresentationOverlay::GetAHardwareBuffer() {
  NOTREACHED();
  return nullptr;
}
#elif defined(USE_OZONE)
scoped_refptr<gfx::NativePixmap>
SharedImageRepresentationOverlay::GetNativePixmap() {
  return backing()->GetNativePixmap();
}
#endif

SharedImageRepresentationOverlay::ScopedReadAccess::ScopedReadAccess(
    base::PassKey<SharedImageRepresentationOverlay> pass_key,
    SharedImageRepresentationOverlay* representation,
    gl::GLImage* gl_image,
    std::vector<gfx::GpuFence> acquire_fences)
    : ScopedAccessBase(representation),
      gl_image_(gl_image),
      acquire_fences_(std::move(acquire_fences)) {}

SharedImageRepresentationOverlay::ScopedReadAccess::~ScopedReadAccess() {
  representation()->EndReadAccess(std::move(release_fence_));
}

std::unique_ptr<SharedImageRepresentationOverlay::ScopedReadAccess>
SharedImageRepresentationOverlay::BeginScopedReadAccess(bool needs_gl_image) {
  if (!IsCleared()) {
    LOG(ERROR) << "Attempt to read from an uninitialized SharedImage";
    return nullptr;
  }

  std::vector<gfx::GpuFence> acquire_fences;
  if (!BeginReadAccess(&acquire_fences))
    return nullptr;

  backing()->OnReadSucceeded();

  return std::make_unique<ScopedReadAccess>(
      base::PassKey<SharedImageRepresentationOverlay>(), this,
      needs_gl_image ? GetGLImage() : nullptr, std::move(acquire_fences));
}

SharedImageRepresentationDawn::ScopedAccess::ScopedAccess(
    base::PassKey<SharedImageRepresentationDawn> /* pass_key */,
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
      WGPUTextureUsage_CopyDst | WGPUTextureUsage_RenderAttachment;

  if (usage & kWriteUsage) {
    backing()->OnWriteSucceeded();
  } else {
    backing()->OnReadSucceeded();
  }

  return std::make_unique<ScopedAccess>(
      base::PassKey<SharedImageRepresentationDawn>(), this, texture);
}

SharedImageRepresentationFactoryRef::~SharedImageRepresentationFactoryRef() {
  backing()->UnregisterImageFactory();
  backing()->MarkForDestruction();
}

SharedImageRepresentationVaapi::SharedImageRepresentationVaapi(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    VaapiDependencies* vaapi_deps)
    : SharedImageRepresentation(manager, backing, tracker),
      vaapi_deps_(vaapi_deps) {}

SharedImageRepresentationVaapi::~SharedImageRepresentationVaapi() = default;

SharedImageRepresentationVaapi::ScopedWriteAccess::ScopedWriteAccess(
    base::PassKey<SharedImageRepresentationVaapi> /* pass_key */,
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
      base::PassKey<SharedImageRepresentationVaapi>(), this);
}

SharedImageRepresentationMemory::ScopedReadAccess::ScopedReadAccess(
    base::PassKey<SharedImageRepresentationMemory> pass_key,
    SharedImageRepresentationMemory* representation,
    SkPixmap pixmap)
    : ScopedAccessBase(representation), pixmap_(pixmap) {}

SharedImageRepresentationMemory::ScopedReadAccess::~ScopedReadAccess() =
    default;

std::unique_ptr<SharedImageRepresentationMemory::ScopedReadAccess>
SharedImageRepresentationMemory::BeginScopedReadAccess() {
  return std::make_unique<ScopedReadAccess>(
      base::PassKey<SharedImageRepresentationMemory>(), this,
      BeginReadAccess());
}

SharedImageRepresentationRaster::ScopedReadAccess::ScopedReadAccess(
    base::PassKey<SharedImageRepresentationRaster> pass_key,
    SharedImageRepresentationRaster* representation,
    const cc::PaintOpBuffer* paint_op_buffer,
    const absl::optional<SkColor4f>& clear_color)
    : ScopedAccessBase(representation),
      paint_op_buffer_(paint_op_buffer),
      clear_color_(clear_color) {}

SharedImageRepresentationRaster::ScopedReadAccess::~ScopedReadAccess() {
  representation()->EndReadAccess();
}

SharedImageRepresentationRaster::ScopedWriteAccess::ScopedWriteAccess(
    base::PassKey<SharedImageRepresentationRaster> pass_key,
    SharedImageRepresentationRaster* representation,
    cc::PaintOpBuffer* paint_op_buffer)
    : ScopedAccessBase(representation), paint_op_buffer_(paint_op_buffer) {}

SharedImageRepresentationRaster::ScopedWriteAccess::~ScopedWriteAccess() {
  representation()->EndWriteAccess(std::move(callback_));
}

std::unique_ptr<SharedImageRepresentationRaster::ScopedReadAccess>
SharedImageRepresentationRaster::BeginScopedReadAccess() {
  absl::optional<SkColor4f> clear_color;
  auto* paint_op_buffer = BeginReadAccess(clear_color);
  if (!paint_op_buffer)
    return nullptr;
  return std::make_unique<ScopedReadAccess>(
      base::PassKey<SharedImageRepresentationRaster>(), this, paint_op_buffer,
      clear_color);
}

std::unique_ptr<SharedImageRepresentationRaster::ScopedWriteAccess>
SharedImageRepresentationRaster::BeginScopedWriteAccess(
    scoped_refptr<SharedContextState> context_state,
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const absl::optional<SkColor4f>& clear_color,
    bool visible) {
  return std::make_unique<ScopedWriteAccess>(
      base::PassKey<SharedImageRepresentationRaster>(), this,
      BeginWriteAccess(std::move(context_state), final_msaa_count,
                       surface_props, clear_color, visible));
}

}  // namespace gpu
