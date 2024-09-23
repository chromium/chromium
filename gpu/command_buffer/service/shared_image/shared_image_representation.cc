// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/MutableTextureState.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/GrYUVABackendTextures.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/graphite/Image.h"
#include "third_party/skia/include/gpu/graphite/YUVABackendTextures.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gl/gl_fence.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/vulkan_fence_helper.h"
#endif

namespace gpu {

///////////////////////////////////////////////////////////////////////////////
// SharedImageRepresentation

SharedImageRepresentation::SharedImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* owning_tracker)
    : manager_(manager), backing_(backing), tracker_(owning_tracker) {
  DCHECK(tracker_);
  // TODO(hitawala): Rewrite the reference counting so that
  // SharedImageRepresentation does not need manager and manager attaches to
  // backing in Register().
  if (manager_ && backing_->is_ref_counted()) {
    backing_->AddRef(this);
  }
}

SharedImageRepresentation::~SharedImageRepresentation() {
  // CHECK here as we'll crash later anyway, and this makes it clearer what the
  // error is.
  CHECK_EQ(access_mode_, AccessMode::kNone)
      << "Destroying a SharedImageRepresentation with "
         "outstanding Scoped*Access objects.";
  if (manager_ && backing_->is_ref_counted()) {
    manager_->OnRepresentationDestroyed(backing_->mailbox(), this);
  }
}

size_t SharedImageRepresentation::NumPlanesExpected() const {
  if (format().PrefersExternalSampler()) {
    return 1;
  }

  return static_cast<size_t>(format().NumberOfPlanes());
}

///////////////////////////////////////////////////////////////////////////////
// GLTextureImageRepresentationBase

std::unique_ptr<GLTextureImageRepresentation::ScopedAccess>
GLTextureImageRepresentationBase::BeginScopedAccess(
    GLenum mode,
    AllowUnclearedAccess allow_uncleared) {
  if (allow_uncleared != AllowUnclearedAccess::kYes && !IsCleared()) {
    LOG(ERROR) << "Attempt to access an uninitialized SharedImage";
    return nullptr;
  }

  if (!BeginAccess(mode)) {
    return nullptr;
  }

  UpdateClearedStateOnBeginAccess();

  AccessMode access_mode;
  if (mode == kReadAccessMode) {
    access_mode = AccessMode::kRead;
    backing()->OnReadSucceeded();
  } else {
    access_mode = AccessMode::kWrite;
    backing()->OnWriteSucceeded();
  }

  return std::make_unique<ScopedAccess>(
      base::PassKey<GLTextureImageRepresentationBase>(), this, access_mode);
}

gpu::TextureBase* GLTextureImageRepresentationBase::GetTextureBase() {
  CHECK_EQ(NumPlanesExpected(), 1u);
  return GetTextureBase(0);
}

bool GLTextureImageRepresentationBase::SupportsMultipleConcurrentReadAccess() {
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// GLTextureImageRepresentation

gpu::TextureBase* GLTextureImageRepresentation::GetTextureBase(
    int plane_index) {
  return GetTexture(plane_index);
}

gles2::Texture* GLTextureImageRepresentation::GetTexture() {
  CHECK_EQ(NumPlanesExpected(), 1u);
  return GetTexture(0);
}

void GLTextureImageRepresentation::UpdateClearedStateOnEndAccess() {
  auto* texture = GetTexture();
  // Operations on the gles2::Texture may have cleared or uncleared it. Make
  // sure this state is reflected back in the SharedImage.
  gfx::Rect cleared_rect = texture->GetLevelClearedRect(texture->target(), 0);
  if (cleared_rect != ClearedRect()) {
    SetClearedRect(cleared_rect);
  }
}

void GLTextureImageRepresentation::UpdateClearedStateOnBeginAccess() {
  auto* texture = GetTexture();
  // Operations outside of the gles2::Texture may have cleared or uncleared it.
  // Make sure this state is reflected back in gles2::Texture.
  gfx::Rect cleared_rect = ClearedRect();
  if (cleared_rect != texture->GetLevelClearedRect(texture->target(), 0)) {
    texture->SetLevelClearedRect(texture->target(), 0, cleared_rect);
  }
}

///////////////////////////////////////////////////////////////////////////////
// GLTexturePassthroughImageRepresentation

gpu::TextureBase* GLTexturePassthroughImageRepresentation::GetTextureBase(
    int plane_index) {
  return GetTexturePassthrough(plane_index).get();
}

const scoped_refptr<gles2::TexturePassthrough>&
GLTexturePassthroughImageRepresentation::GetTexturePassthrough() {
  CHECK_EQ(NumPlanesExpected(), 1u);
  return GetTexturePassthrough(0);
}

bool GLTexturePassthroughImageRepresentation::
    NeedsSuspendAccessForDXGIKeyedMutex() const {
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// SkiaImageRepresentation

SkiaImageRepresentation::SkiaImageRepresentation(SharedImageManager* manager,
                                                 SharedImageBacking* backing,
                                                 MemoryTypeTracker* tracker)
    : SharedImageRepresentation(manager, backing, tracker) {}

SkiaImageRepresentation::~SkiaImageRepresentation() = default;

bool SkiaImageRepresentation::SupportsMultipleConcurrentReadAccess() {
  return false;
}

SkiaImageRepresentation::ScopedWriteAccess::ScopedWriteAccess(
    SkiaImageRepresentation* representation,
    std::vector<sk_sp<SkSurface>> surfaces)
    : ScopedAccessBase(representation, AccessMode::kWrite),
      surfaces_(std::move(surfaces)) {
  CHECK(!surfaces_.empty());
}

SkiaImageRepresentation::ScopedWriteAccess::ScopedWriteAccess(
    SkiaImageRepresentation* representation,
    std::vector<sk_sp<GrPromiseImageTexture>> promise_image_textures)
    : ScopedAccessBase(representation, AccessMode::kWrite),
      promise_image_textures_(std::move(promise_image_textures)) {
  CHECK(!promise_image_textures_.empty());
  CHECK(graphite_textures_.empty());
}

SkiaImageRepresentation::ScopedWriteAccess::ScopedWriteAccess(
    SkiaImageRepresentation* representation,
    std::vector<skgpu::graphite::BackendTexture> graphite_textures)
    : ScopedAccessBase(representation, AccessMode::kWrite),
      graphite_textures_(graphite_textures) {
  CHECK(!graphite_textures_.empty());
  CHECK(promise_image_textures_.empty());
}

SkiaImageRepresentation::ScopedWriteAccess::~ScopedWriteAccess() {
  // Ensure no one uses `surfaces_` by dropping the reference before calling
  // EndWriteAccess.
  surfaces_.clear();
  representation()->EndWriteAccess();
}

bool SkiaImageRepresentation::ScopedWriteAccess::NeedGraphiteContextSubmit() {
  return representation()->NeedGraphiteContextSubmitBeforeEndAccess();
}

SkiaImageRepresentation::ScopedReadAccess::ScopedReadAccess(
    SkiaImageRepresentation* representation,
    std::vector<sk_sp<GrPromiseImageTexture>> promise_image_textures)
    : ScopedAccessBase(representation, AccessMode::kRead),
      promise_image_textures_(std::move(promise_image_textures)) {
  CHECK(!promise_image_textures_.empty());
  CHECK(graphite_textures_.empty());
}

SkiaImageRepresentation::ScopedReadAccess::ScopedReadAccess(
    SkiaImageRepresentation* representation,
    std::vector<skgpu::graphite::BackendTexture> graphite_textures)
    : ScopedAccessBase(representation, AccessMode::kRead),
      graphite_textures_(graphite_textures) {
  CHECK(!graphite_textures_.empty());
  CHECK(promise_image_textures_.empty());
}

SkiaImageRepresentation::ScopedReadAccess::~ScopedReadAccess() {
  representation()->EndReadAccess();
}

bool SkiaImageRepresentation::ScopedReadAccess::NeedGraphiteContextSubmit() {
  return representation()->NeedGraphiteContextSubmitBeforeEndAccess();
}

///////////////////////////////////////////////////////////////////////////////
// SkiaGaneshImageRepresentation

SkiaGaneshImageRepresentation::SkiaGaneshImageRepresentation(
    GrDirectContext* gr_context,
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : SkiaImageRepresentation(manager, backing, tracker),
      gr_context_(gr_context) {}

bool SkiaGaneshImageRepresentation::NeedGraphiteContextSubmitBeforeEndAccess() {
  // Ganesh shouldn't need a Graphite context submit.
  return false;
}

SkiaGaneshImageRepresentation::ScopedGaneshWriteAccess::ScopedGaneshWriteAccess(
    base::PassKey<SkiaGaneshImageRepresentation> /* pass_key */,
    SkiaImageRepresentation* representation,
    std::vector<sk_sp<SkSurface>> surfaces,
    std::unique_ptr<skgpu::MutableTextureState> end_state)
    : ScopedWriteAccess(representation, std::move(surfaces)),
      end_state_(std::move(end_state)) {
  DCHECK(!surfaces_.empty());
}

SkiaGaneshImageRepresentation::ScopedGaneshWriteAccess::ScopedGaneshWriteAccess(
    base::PassKey<SkiaGaneshImageRepresentation> /* pass_key */,
    SkiaImageRepresentation* representation,
    std::vector<sk_sp<GrPromiseImageTexture>> promise_image_textures,
    std::unique_ptr<skgpu::MutableTextureState> end_state)
    : ScopedWriteAccess(representation, std::move(promise_image_textures)),
      end_state_(std::move(end_state)) {
  DCHECK(!promise_image_textures_.empty());
}

SkiaGaneshImageRepresentation::ScopedGaneshWriteAccess::
    ~ScopedGaneshWriteAccess() {
  if (end_state_) {
    NOTREACHED_IN_MIGRATION()
        << "Before ending write access TakeEndState() must be called "
           "and the result passed to skia to make sure all layout and "
           "ownership transitions are done.";
  }
}

bool SkiaGaneshImageRepresentation::ScopedGaneshWriteAccess::
    HasBackendSurfaceEndState() {
  return !!end_state_;
}

void SkiaGaneshImageRepresentation::ScopedGaneshWriteAccess::
    ApplyBackendSurfaceEndState() {
  if (!end_state_) {
    return;
  }
  DCHECK(promise_image_textures_.empty() || surfaces_.empty());

  int num_planes = representation()->format().NumberOfPlanes();
  GrDirectContext* direct_context = ganesh_representation()->gr_context();
  CHECK(direct_context);
  if (!surfaces_.empty()) {
    for (int plane = 0; plane < num_planes; plane++) {
      direct_context->flush(surface(plane), /*info=*/{}, end_state_.get());
    }
  }
  if (!promise_image_textures_.empty()) {
    for (int plane = 0; plane < num_planes; plane++) {
      if (!direct_context->setBackendTextureState(
              promise_image_texture(plane)->backendTexture(), *end_state_)) {
        LOG(ERROR) << "setBackendTextureState() failed for plane: " << plane;
      }
    }
  }
  end_state_ = nullptr;
}

std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
SkiaGaneshImageRepresentation::BeginScopedWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    AllowUnclearedAccess allow_uncleared,
    bool use_sk_surface) {
  if (allow_uncleared != AllowUnclearedAccess::kYes && !IsCleared()) {
    LOG(ERROR) << "Attempt to write to an uninitialized SharedImage";
    return nullptr;
  }

  if (surface_origin() != kTopLeft_GrSurfaceOrigin) {
    LOG(ERROR)
        << "Skia write access is only allowed for top left origin surfaces.";
    return nullptr;
  }

  std::unique_ptr<skgpu::MutableTextureState> end_state;
  if (use_sk_surface) {
    std::vector<sk_sp<SkSurface>> surfaces =
        BeginWriteAccess(final_msaa_count, surface_props, update_rect,
                         begin_semaphores, end_semaphores, &end_state);
    if (surfaces.empty()) {
      LOG(ERROR) << "Unable to initialize SkSurface";
      return nullptr;
    }

    backing()->OnWriteSucceeded();

    return std::make_unique<ScopedGaneshWriteAccess>(
        base::PassKey<SkiaGaneshImageRepresentation>(), this,
        std::move(surfaces), std::move(end_state));
  }
  std::vector<sk_sp<GrPromiseImageTexture>> promise_image_textures =
      BeginWriteAccess(begin_semaphores, end_semaphores, &end_state);
  if (promise_image_textures.empty()) {
    LOG(ERROR) << "Unable to initialize GrPromiseImageTexture";
    return nullptr;
  }

  backing()->OnWriteSucceeded();

  return std::make_unique<ScopedGaneshWriteAccess>(
      base::PassKey<SkiaGaneshImageRepresentation>(), this,
      std::move(promise_image_textures), std::move(end_state));
}

std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
SkiaGaneshImageRepresentation::BeginScopedWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    AllowUnclearedAccess allow_uncleared,
    bool use_sk_surface) {
  return BeginScopedWriteAccess(
      final_msaa_count, surface_props, gfx::Rect(size()), begin_semaphores,
      end_semaphores, allow_uncleared, use_sk_surface);
}

std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
SkiaGaneshImageRepresentation::BeginScopedWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    AllowUnclearedAccess allow_uncleared,
    bool use_sk_surface) {
  return BeginScopedWriteAccess(
      /*final_msaa_count=*/1, SkSurfaceProps(), begin_semaphores,
      end_semaphores, allow_uncleared, use_sk_surface);
}

SkiaGaneshImageRepresentation::ScopedGaneshReadAccess::ScopedGaneshReadAccess(
    base::PassKey<SkiaGaneshImageRepresentation> /* pass_key */,
    SkiaImageRepresentation* representation,
    std::vector<sk_sp<GrPromiseImageTexture>> promise_image_textures,
    std::unique_ptr<skgpu::MutableTextureState> end_state)
    : ScopedReadAccess(representation, std::move(promise_image_textures)),
      end_state_(std::move(end_state)) {
  DCHECK(!promise_image_textures_.empty());
}

SkiaGaneshImageRepresentation::ScopedGaneshReadAccess::
    ~ScopedGaneshReadAccess() {
  if (end_state_) {
    NOTREACHED_IN_MIGRATION()
        << "Before ending read access TakeEndState() must be called "
           "and the result passed to skia to make sure all layout and "
           "ownership transitions are done.";
  }
}

sk_sp<SkImage>
SkiaGaneshImageRepresentation::ScopedGaneshReadAccess::CreateSkImage(
    SharedContextState* context_state,
    SkImages::TextureReleaseProc texture_release_proc,
    SkImages::ReleaseContext release_context) {
  auto format = representation()->format();
  auto surface_origin = representation()->surface_origin();
  auto sk_color_space =
      representation()->color_space().GetAsFullRangeRGB().ToSkColorSpace();
  if (format.is_single_plane() || format.PrefersExternalSampler()) {
    DCHECK_EQ(static_cast<int>(promise_image_textures_.size()), 1);
    auto alpha_type = representation()->alpha_type();
    auto color_type =
        format.PrefersExternalSampler()
            ? ToClosestSkColorTypeExternalSampler(format)
            : viz::ToClosestSkColorType(/*gpu_compositing=*/true, format);
    return SkImages::BorrowTextureFrom(
        context_state->gr_context(), promise_image_texture()->backendTexture(),
        surface_origin, color_type, alpha_type, sk_color_space,
        texture_release_proc, release_context);
  } else {
    DCHECK_EQ(static_cast<int>(promise_image_textures_.size()),
              format.NumberOfPlanes());
    std::array<GrBackendTexture, SkYUVAInfo::kMaxPlanes> yuva_textures;
    // Get the texture per plane.
    for (int plane_index = 0; plane_index < format.NumberOfPlanes();
         plane_index++) {
      yuva_textures[plane_index] =
          promise_image_texture(plane_index)->backendTexture();
    }

    SkISize sk_size = gfx::SizeToSkISize(representation()->size());
    // TODO(crbug.com/41380578): This should really default to rec709.
    SkYUVColorSpace yuv_color_space = kRec601_SkYUVColorSpace;
    representation()->color_space().ToSkYUVColorSpace(
        format.MultiplanarBitDepth(), &yuv_color_space);
    SkYUVAInfo yuva_info(sk_size, ToSkYUVAPlaneConfig(format),
                         ToSkYUVASubsampling(format), yuv_color_space);
    GrYUVABackendTextures yuva_backend_textures(yuva_info, yuva_textures.data(),
                                                surface_origin);
    return SkImages::TextureFromYUVATextures(
        context_state->gr_context(), yuva_backend_textures, sk_color_space,
        texture_release_proc, release_context);
  }
}

sk_sp<SkImage>
SkiaGaneshImageRepresentation::ScopedGaneshReadAccess::CreateSkImageForPlane(
    int plane_index,
    SharedContextState* context_state,
    SkImages::TextureReleaseProc texture_release_proc,
    SkImages::ReleaseContext release_context) {
  auto format = representation()->format();
  DCHECK(format.is_multi_plane());
  DCHECK_EQ(static_cast<int>(promise_image_textures_.size()),
            format.NumberOfPlanes());

  auto surface_origin = representation()->surface_origin();
  auto alpha_type = SkAlphaType::kOpaque_SkAlphaType;
  auto color_type =
      viz::ToClosestSkColorType(/*gpu_compositing=*/true, format, plane_index);
  return SkImages::BorrowTextureFrom(
      context_state->gr_context(),
      promise_image_texture(plane_index)->backendTexture(), surface_origin,
      color_type, alpha_type, /*sk_color_space=*/nullptr, texture_release_proc,
      release_context);
}

bool SkiaGaneshImageRepresentation::ScopedGaneshReadAccess::
    HasBackendSurfaceEndState() {
  return !!end_state_;
}

void SkiaGaneshImageRepresentation::ScopedGaneshReadAccess::
    ApplyBackendSurfaceEndState() {
  if (!end_state_) {
    return;
  }
  for (size_t plane = 0; plane < representation()->NumPlanesExpected();
       plane++) {
    if (!ganesh_representation()->gr_context()->setBackendTextureState(
            promise_image_texture(plane)->backendTexture(), *end_state_)) {
      LOG(ERROR) << "setBackendTextureState() failed for plane: " << plane;
    }
  }
  end_state_ = nullptr;
}

std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
SkiaGaneshImageRepresentation::BeginScopedReadAccess(
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

  std::unique_ptr<skgpu::MutableTextureState> end_state;
  std::vector<sk_sp<GrPromiseImageTexture>> promise_image_textures =
      BeginReadAccess(begin_semaphores, end_semaphores, &end_state);
  if (promise_image_textures.empty()) {
    LOG(ERROR) << "Unable to initialize GrPromiseImageTexture";
    return nullptr;
  }

  backing()->OnReadSucceeded();

  return std::make_unique<ScopedGaneshReadAccess>(
      base::PassKey<SkiaGaneshImageRepresentation>(), this,
      std::move(promise_image_textures), std::move(end_state));
}

///////////////////////////////////////////////////////////////////////////////
// SkiaGraphiteImageRepresentation

SkiaGraphiteImageRepresentation::SkiaGraphiteImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : SkiaImageRepresentation(manager, backing, tracker) {}

bool SkiaGraphiteImageRepresentation::
    NeedGraphiteContextSubmitBeforeEndAccess() {
  // As default, assume Graphite context submit is needed.
  return true;
}

SkiaGraphiteImageRepresentation::ScopedGraphiteWriteAccess::
    ScopedGraphiteWriteAccess(
        base::PassKey<SkiaGraphiteImageRepresentation> /* pass_key */,
        SkiaImageRepresentation* representation,
        std::vector<sk_sp<SkSurface>> surfaces)
    : ScopedWriteAccess(representation, std::move(surfaces)) {
  CHECK(!surfaces_.empty());
}

SkiaGraphiteImageRepresentation::ScopedGraphiteWriteAccess::
    ScopedGraphiteWriteAccess(
        base::PassKey<SkiaGraphiteImageRepresentation> /* pass_key */,
        SkiaImageRepresentation* representation,
        std::vector<skgpu::graphite::BackendTexture> backend_textures)
    : ScopedWriteAccess(representation, backend_textures) {
  CHECK(!graphite_textures_.empty());
}

SkiaGraphiteImageRepresentation::ScopedGraphiteWriteAccess::
    ~ScopedGraphiteWriteAccess() = default;

bool SkiaGraphiteImageRepresentation::ScopedGraphiteWriteAccess::
    HasBackendSurfaceEndState() {
  return false;
}

// Graphite-Dawn backend handles Vulkan transitions by itself, so nothing to do
// here.
void SkiaGraphiteImageRepresentation::ScopedGraphiteWriteAccess::
    ApplyBackendSurfaceEndState() {}

std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
SkiaGraphiteImageRepresentation::BeginScopedWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    AllowUnclearedAccess allow_uncleared,
    bool use_sk_surface) {
  if (allow_uncleared != AllowUnclearedAccess::kYes && !IsCleared()) {
    LOG(ERROR) << "Attempt to write to an uninitialized SharedImage";
    return nullptr;
  }

  if (surface_origin() != kTopLeft_GrSurfaceOrigin) {
    LOG(ERROR)
        << "Skia write access is only allowed for top left origin surfaces.";
    return nullptr;
  }

  if (use_sk_surface) {
    std::vector<sk_sp<SkSurface>> surfaces =
        BeginWriteAccess(surface_props, update_rect);
    if (surfaces.empty()) {
      LOG(ERROR) << "Unable to initialize SkSurface";
      return nullptr;
    }

    backing()->OnWriteSucceeded();

    return std::make_unique<ScopedGraphiteWriteAccess>(
        base::PassKey<SkiaGraphiteImageRepresentation>(), this,
        std::move(surfaces));
  }
  std::vector<skgpu::graphite::BackendTexture> graphite_textures =
      BeginWriteAccess();
  if (graphite_textures.empty()) {
    LOG(ERROR) << "Unable to initialize graphite::BackendTextures";
    return nullptr;
  }

  backing()->OnWriteSucceeded();

  return std::make_unique<ScopedGraphiteWriteAccess>(
      base::PassKey<SkiaGraphiteImageRepresentation>(), this,
      graphite_textures);
}

std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
SkiaGraphiteImageRepresentation::BeginScopedWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    AllowUnclearedAccess allow_uncleared,
    bool use_sk_surface) {
  return BeginScopedWriteAccess(
      final_msaa_count, surface_props, gfx::Rect(size()), begin_semaphores,
      end_semaphores, allow_uncleared, use_sk_surface);
}

std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
SkiaGraphiteImageRepresentation::BeginScopedWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    AllowUnclearedAccess allow_uncleared,
    bool use_sk_surface) {
  return BeginScopedWriteAccess(
      /*final_msaa_count=*/1, SkSurfaceProps(), begin_semaphores,
      end_semaphores, allow_uncleared, use_sk_surface);
}

SkiaGraphiteImageRepresentation::ScopedGraphiteReadAccess::
    ScopedGraphiteReadAccess(
        base::PassKey<SkiaGraphiteImageRepresentation> /* pass_key */,
        SkiaImageRepresentation* representation,
        std::vector<skgpu::graphite::BackendTexture> graphite_textures)
    : ScopedReadAccess(representation, graphite_textures) {
  CHECK(!graphite_textures_.empty());
}

SkiaGraphiteImageRepresentation::ScopedGraphiteReadAccess::
    ~ScopedGraphiteReadAccess() = default;

sk_sp<SkImage>
SkiaGraphiteImageRepresentation::ScopedGraphiteReadAccess::CreateSkImage(
    SharedContextState* context_state,
    SkImages::TextureReleaseProc texture_release_proc,
    SkImages::ReleaseContext release_context) {
  auto format = representation()->format();
  auto sk_color_space =
      representation()->color_space().GetAsFullRangeRGB().ToSkColorSpace();
  auto* recorder = context_state->gpu_main_graphite_recorder();
  if (format.is_single_plane() || format.PrefersExternalSampler()) {
    CHECK_EQ(static_cast<int>(graphite_textures_.size()), 1);
    auto alpha_type = representation()->alpha_type();
    auto color_type =
        format.PrefersExternalSampler()
            ? ToClosestSkColorTypeExternalSampler(format)
            : viz::ToClosestSkColorType(/*gpu_compositing=*/true, format);
    auto origin = representation()->surface_origin() == kTopLeft_GrSurfaceOrigin
                      ? skgpu::Origin::kTopLeft
                      : skgpu::Origin::kBottomLeft;
    return SkImages::WrapTexture(recorder, graphite_texture(), color_type,
                                 alpha_type, sk_color_space, origin);
  } else {
    CHECK_EQ(static_cast<int>(graphite_textures_.size()),
             format.NumberOfPlanes());
    SkISize sk_size = gfx::SizeToSkISize(representation()->size());
    // TODO(crbug.com/41380578): This should really default to rec709.
    SkYUVColorSpace yuv_color_space = kRec601_SkYUVColorSpace;
    representation()->color_space().ToSkYUVColorSpace(
        format.MultiplanarBitDepth(), &yuv_color_space);
    SkYUVAInfo yuva_info(sk_size, ToSkYUVAPlaneConfig(format),
                         ToSkYUVASubsampling(format), yuv_color_space);
    skgpu::graphite::YUVABackendTextures yuva_backend_textures(
        recorder, yuva_info, graphite_textures_);
    return SkImages::TextureFromYUVATextures(
        recorder, yuva_backend_textures, sk_color_space, texture_release_proc,
        release_context);
  }
}

sk_sp<SkImage> SkiaGraphiteImageRepresentation::ScopedGraphiteReadAccess::
    CreateSkImageForPlane(int plane_index,
                          SharedContextState* context_state,
                          SkImages::TextureReleaseProc texture_release_proc,
                          SkImages::ReleaseContext release_context) {
  auto format = representation()->format();
  CHECK(format.is_multi_plane());
  CHECK_EQ(static_cast<int>(graphite_textures_.size()),
           format.NumberOfPlanes());
  auto alpha_type = SkAlphaType::kOpaque_SkAlphaType;
  auto color_type =
      viz::ToClosestSkColorType(/*gpu_compositing=*/true, format, plane_index);
  return SkImages::WrapTexture(context_state->gpu_main_graphite_recorder(),
                               graphite_texture(plane_index), color_type,
                               alpha_type, /*colorSpace=*/nullptr,
                               texture_release_proc, release_context);
}

bool SkiaGraphiteImageRepresentation::ScopedGraphiteReadAccess::
    HasBackendSurfaceEndState() {
  return false;
}

// Graphite-Dawn backend handles Vulkan transitions by itself, so nothing to do
// here.
void SkiaGraphiteImageRepresentation::ScopedGraphiteReadAccess::
    ApplyBackendSurfaceEndState() {}

std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
SkiaGraphiteImageRepresentation::BeginScopedReadAccess(
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

  std::vector<skgpu::graphite::BackendTexture> graphite_textures =
      BeginReadAccess();
  if (graphite_textures.empty()) {
    LOG(ERROR) << "Unable to initialize graphite::BackendTextures";
    return nullptr;
  }

  backing()->OnReadSucceeded();

  return std::make_unique<ScopedGraphiteReadAccess>(
      base::PassKey<SkiaGraphiteImageRepresentation>(), this,
      graphite_textures);
}

std::string SkiaGraphiteImageRepresentation::WrappedTextureDebugLabel(
    int plane) const {
  std::string debug_label;
  if (format().is_single_plane()) {
    debug_label = base::StringPrintf("%s_%s", backing()->GetName(),
                                     backing()->debug_label().c_str());
  } else {
    debug_label = base::StringPrintf("%s_%s_Plane%d", backing()->GetName(),
                                     backing()->debug_label().c_str(), plane);
  }
  return debug_label;
}

///////////////////////////////////////////////////////////////////////////////
// OverlayImageRepresentation

#if BUILDFLAG(IS_ANDROID)
AHardwareBuffer* OverlayImageRepresentation::GetAHardwareBuffer() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}
std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
OverlayImageRepresentation::GetAHardwareBufferFenceSync() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}
#elif BUILDFLAG(IS_OZONE)
scoped_refptr<gfx::NativePixmap> OverlayImageRepresentation::GetNativePixmap() {
  return backing()->GetNativePixmap();
}
#elif BUILDFLAG(IS_WIN)
std::optional<gl::DCLayerOverlayImage>
OverlayImageRepresentation::GetDCLayerOverlayImage() {
  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}
#elif BUILDFLAG(IS_APPLE)
gfx::ScopedIOSurface OverlayImageRepresentation::GetIOSurface() const {
  return gfx::ScopedIOSurface();
}
bool OverlayImageRepresentation::IsInUseByWindowServer() const {
  return false;
}
#endif

OverlayImageRepresentation::ScopedReadAccess::ScopedReadAccess(
    base::PassKey<OverlayImageRepresentation> pass_key,
    OverlayImageRepresentation* representation,
    gfx::GpuFenceHandle acquire_fence)
    : ScopedAccessBase(representation, AccessMode::kRead),
      acquire_fence_(std::move(acquire_fence)) {}

OverlayImageRepresentation::ScopedReadAccess::~ScopedReadAccess() {
  representation()->EndReadAccess(std::move(release_fence_));
}

std::unique_ptr<OverlayImageRepresentation::ScopedReadAccess>
OverlayImageRepresentation::BeginScopedReadAccess() {
  if (!IsCleared()) {
    LOG(ERROR) << "Attempt to read from an uninitialized SharedImage";
    return nullptr;
  }

  gfx::GpuFenceHandle acquire_fence;
  if (!BeginReadAccess(acquire_fence)) {
    return nullptr;
  }

  backing()->OnReadSucceeded();

  return std::make_unique<ScopedReadAccess>(
      base::PassKey<OverlayImageRepresentation>(), this,
      std::move(acquire_fence));
}

///////////////////////////////////////////////////////////////////////////////
// DawnImageRepresentation

DawnImageRepresentation::ScopedAccess::ScopedAccess(
    base::PassKey<DawnImageRepresentation> /* pass_key */,
    DawnImageRepresentation* representation,
    wgpu::Texture texture,
    AccessMode access_mode)
    : ScopedAccessBase(representation, access_mode),
      texture_(std::move(texture)) {}

DawnImageRepresentation::ScopedAccess::~ScopedAccess() {
  representation()->EndAccess();
}

std::unique_ptr<DawnImageRepresentation::ScopedAccess>
DawnImageRepresentation::BeginScopedAccess(
    wgpu::TextureUsage usage,
    AllowUnclearedAccess allow_uncleared) {
  return BeginScopedAccess(usage, wgpu::TextureUsage::None, allow_uncleared,
                           gfx::Rect(size()));
}

std::unique_ptr<DawnImageRepresentation::ScopedAccess>
DawnImageRepresentation::BeginScopedAccess(
    wgpu::TextureUsage usage,
    wgpu::TextureUsage internal_usage,
    AllowUnclearedAccess allow_uncleared) {
  return BeginScopedAccess(usage, internal_usage, allow_uncleared,
                           gfx::Rect(size()));
}

std::unique_ptr<DawnImageRepresentation::ScopedAccess>
DawnImageRepresentation::BeginScopedAccess(wgpu::TextureUsage usage,
                                           AllowUnclearedAccess allow_uncleared,
                                           const gfx::Rect& update_rect) {
  return BeginScopedAccess(usage, wgpu::TextureUsage::None, allow_uncleared,
                           update_rect);
}

std::unique_ptr<DawnImageRepresentation::ScopedAccess>
DawnImageRepresentation::BeginScopedAccess(wgpu::TextureUsage usage,
                                           wgpu::TextureUsage internal_usage,
                                           AllowUnclearedAccess allow_uncleared,
                                           const gfx::Rect& update_rect) {
  if (allow_uncleared != AllowUnclearedAccess::kYes && !IsCleared()) {
    LOG(ERROR) << "Attempt to access an uninitialized SharedImage";
    return nullptr;
  }

  wgpu::Texture texture = BeginAccess(usage, internal_usage, update_rect);
  if (!texture) {
    LOG(ERROR) << "Error creating wgpu::Texture";
    return nullptr;
  }

  AccessMode access_mode;
  if (usage & kWriteUsage) {
    access_mode = AccessMode::kWrite;
    backing()->OnWriteSucceeded();
  } else {
    access_mode = AccessMode::kRead;
    backing()->OnReadSucceeded();
  }

  return std::make_unique<ScopedAccess>(
      base::PassKey<DawnImageRepresentation>(), this, std::move(texture),
      access_mode);
}

wgpu::Texture DawnImageRepresentation::BeginAccess(
    wgpu::TextureUsage usage,
    wgpu::TextureUsage internal_usage,
    const gfx::Rect& update_rect) {
  return this->BeginAccess(usage, internal_usage);
}

bool DawnImageRepresentation::SupportsMultipleConcurrentReadAccess() {
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// DawnBufferRepresentation

DawnBufferRepresentation::ScopedAccess::ScopedAccess(
    base::PassKey<DawnBufferRepresentation> /* pass_key */,
    DawnBufferRepresentation* representation,
    wgpu::Buffer buffer,
    AccessMode access_mode)
    : ScopedAccessBase(representation, access_mode),
      buffer_(std::move(buffer)) {}

DawnBufferRepresentation::ScopedAccess::~ScopedAccess() {
  representation()->EndAccess();
}

std::unique_ptr<DawnBufferRepresentation::ScopedAccess>
DawnBufferRepresentation::BeginScopedAccess(wgpu::BufferUsage usage) {
  wgpu::Buffer buffer = BeginAccess(usage);
  if (!buffer) {
    LOG(ERROR) << "Error creating wgpu::Buffer";
    return nullptr;
  }

  return std::make_unique<ScopedAccess>(
      base::PassKey<DawnBufferRepresentation>(), this, std::move(buffer),
      AccessMode::kWrite);
}

///////////////////////////////////////////////////////////////////////////////
// SharedImageRepresentationFactoryRef

SharedImageRepresentationFactoryRef::SharedImageRepresentationFactoryRef(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    bool is_primary)
    : SharedImageRepresentation(manager, backing, tracker),
      is_primary_(is_primary) {
  // If this is secondary reference, we need to notify SharedImageBacking that
  // it can significantly outlive its owning SharedImageFactory and can't rely
  // on it for operation.
  if (!is_primary) {
    backing->OnAddSecondaryReference();
  }
}

SharedImageRepresentationFactoryRef::~SharedImageRepresentationFactoryRef() {
  // Only primary refs provide link to the owning SharedImageFactory.
  if (is_primary_) {
    backing()->UnregisterImageFactory();
    backing()->MarkForDestruction();
  }
}

///////////////////////////////////////////////////////////////////////////////
// MemoryImageRepresentation

MemoryImageRepresentation::ScopedReadAccess::ScopedReadAccess(
    base::PassKey<MemoryImageRepresentation> pass_key,
    MemoryImageRepresentation* representation,
    SkPixmap pixmap)
    : ScopedAccessBase(representation, AccessMode::kRead), pixmap_(pixmap) {}

MemoryImageRepresentation::ScopedReadAccess::~ScopedReadAccess() = default;

std::unique_ptr<MemoryImageRepresentation::ScopedReadAccess>
MemoryImageRepresentation::BeginScopedReadAccess() {
  return std::make_unique<ScopedReadAccess>(
      base::PassKey<MemoryImageRepresentation>(), this, BeginReadAccess());
}

///////////////////////////////////////////////////////////////////////////////
// RasterImageRepresentation

RasterImageRepresentation::ScopedReadAccess::ScopedReadAccess(
    base::PassKey<RasterImageRepresentation> pass_key,
    RasterImageRepresentation* representation,
    const cc::PaintOpBuffer* paint_op_buffer,
    const std::optional<SkColor4f>& clear_color)
    : ScopedAccessBase(representation, AccessMode::kRead),
      paint_op_buffer_(paint_op_buffer),
      clear_color_(clear_color) {}

RasterImageRepresentation::ScopedReadAccess::~ScopedReadAccess() {
  representation()->EndReadAccess();
}

RasterImageRepresentation::ScopedWriteAccess::ScopedWriteAccess(
    base::PassKey<RasterImageRepresentation> pass_key,
    RasterImageRepresentation* representation,
    cc::PaintOpBuffer* paint_op_buffer)
    : ScopedAccessBase(representation, AccessMode::kWrite),
      paint_op_buffer_(paint_op_buffer) {}

RasterImageRepresentation::ScopedWriteAccess::~ScopedWriteAccess() {
  representation()->EndWriteAccess(std::move(callback_));
}

std::unique_ptr<RasterImageRepresentation::ScopedReadAccess>
RasterImageRepresentation::BeginScopedReadAccess() {
  std::optional<SkColor4f> clear_color;
  auto* paint_op_buffer = BeginReadAccess(clear_color);
  if (!paint_op_buffer) {
    return nullptr;
  }
  return std::make_unique<ScopedReadAccess>(
      base::PassKey<RasterImageRepresentation>(), this, paint_op_buffer,
      clear_color);
}

std::unique_ptr<RasterImageRepresentation::ScopedWriteAccess>
RasterImageRepresentation::BeginScopedWriteAccess(
    scoped_refptr<SharedContextState> context_state,
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const std::optional<SkColor4f>& clear_color,
    bool visible) {
  return std::make_unique<ScopedWriteAccess>(
      base::PassKey<RasterImageRepresentation>(), this,
      BeginWriteAccess(std::move(context_state), final_msaa_count,
                       surface_props, clear_color, visible));
}

///////////////////////////////////////////////////////////////////////////////
// VideoImageRepresentation

VideoImageRepresentation::VideoImageRepresentation(SharedImageManager* manager,
                                                   SharedImageBacking* backing,
                                                   MemoryTypeTracker* tracker)
    : SharedImageRepresentation(manager, backing, tracker) {}

VideoImageRepresentation::~VideoImageRepresentation() = default;

VideoImageRepresentation::ScopedWriteAccess::ScopedWriteAccess(
    base::PassKey<VideoImageRepresentation> /* pass_key */,
    VideoImageRepresentation* representation)
    : ScopedAccessBase(representation, AccessMode::kWrite) {}

VideoImageRepresentation::ScopedWriteAccess::~ScopedWriteAccess() {
  representation()->EndWriteAccess();
}

std::unique_ptr<VideoImageRepresentation::ScopedWriteAccess>
VideoImageRepresentation::BeginScopedWriteAccess() {
  if (!BeginWriteAccess()) {
    return nullptr;
  }

  return std::make_unique<ScopedWriteAccess>(
      base::PassKey<VideoImageRepresentation>(), this);
}
VideoImageRepresentation::ScopedReadAccess::ScopedReadAccess(
    base::PassKey<VideoImageRepresentation> /* pass_key */,
    VideoImageRepresentation* representation)
    : ScopedAccessBase(representation, AccessMode::kWrite) {}

VideoImageRepresentation::ScopedReadAccess::~ScopedReadAccess() {
  representation()->EndReadAccess();
}

std::unique_ptr<VideoImageRepresentation::ScopedReadAccess>
VideoImageRepresentation::BeginScopedReadAccess() {
  if (!BeginReadAccess()) {
    return nullptr;
  }

  return std::make_unique<ScopedReadAccess>(
      base::PassKey<VideoImageRepresentation>(), this);
}

///////////////////////////////////////////////////////////////////////////////
// VulkanImageRepresentation

#if BUILDFLAG(ENABLE_VULKAN)
VulkanImageRepresentation::VulkanImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    std::unique_ptr<gpu::VulkanImage> vulkan_image,
    gpu::VulkanDeviceQueue* vulkan_device_queue,
    gpu::VulkanImplementation& vulkan_impl)
    : SharedImageRepresentation(manager, backing, tracker),
      vulkan_image_(std::move(vulkan_image)),
      vulkan_device_queue_(vulkan_device_queue),
      vulkan_impl_(vulkan_impl) {}

VulkanImageRepresentation::~VulkanImageRepresentation() {
  vulkan_device_queue_->GetFenceHelper()
      ->EnqueueVulkanObjectCleanupForSubmittedWork<gpu::VulkanImage>(
          std::move(vulkan_image_));
}

VulkanImageRepresentation::ScopedAccess::ScopedAccess(
    VulkanImageRepresentation* representation,
    AccessMode access_mode,
    std::vector<VkSemaphore> begin_semaphores,
    VkSemaphore end_semaphore)
    : ScopedAccessBase(representation, access_mode),
      is_read_only_(access_mode == AccessMode::kRead),
      begin_semaphores_(begin_semaphores),
      end_semaphore_(end_semaphore) {}

VulkanImageRepresentation::ScopedAccess::~ScopedAccess() {
  representation()->EndScopedAccess(is_read_only_, end_semaphore_);

  auto* fence_helper = representation()->vulkan_device_queue_->GetFenceHelper();
  fence_helper->EnqueueSemaphoresCleanupForSubmittedWork(
      std::move(begin_semaphores_));
  fence_helper->EnqueueSemaphoreCleanupForSubmittedWork(end_semaphore_);
}

gpu::VulkanImage& VulkanImageRepresentation::ScopedAccess::GetVulkanImage() {
  return *representation()->vulkan_image_;
}
#endif

}  // namespace gpu
