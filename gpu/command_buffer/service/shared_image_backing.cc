// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing.h"

#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#endif

namespace gpu {

SharedImageBacking::SharedImageBacking(const Mailbox& mailbox,
                                       viz::ResourceFormat format,
                                       const gfx::Size& size,
                                       const gfx::ColorSpace& color_space,
                                       GrSurfaceOrigin surface_origin,
                                       SkAlphaType alpha_type,
                                       uint32_t usage,
                                       size_t estimated_size,
                                       bool is_thread_safe)
    : mailbox_(mailbox),
      format_(format),
      size_(size),
      color_space_(color_space),
      surface_origin_(surface_origin),
      alpha_type_(alpha_type),
      usage_(usage),
      estimated_size_(estimated_size) {
  DCHECK_CALLED_ON_VALID_THREAD(factory_thread_checker_);

  if (is_thread_safe)
    lock_.emplace();
}

SharedImageBacking::~SharedImageBacking() = default;

void SharedImageBacking::OnContextLost() {
  AutoLock auto_lock(this);

  have_context_ = false;
}

bool SharedImageBacking::PresentSwapChain() {
  return false;
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                     MemoryTypeTracker* tracker) {
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageBacking::ProduceRGBEmulationGLTexture(SharedImageManager* manager,
                                                 MemoryTypeTracker* tracker) {
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                                MemoryTypeTracker* tracker) {
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationSkia> SharedImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationDawn> SharedImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    WGPUDevice device) {
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationOverlay>
SharedImageBacking::ProduceOverlay(SharedImageManager* manager,
                                   MemoryTypeTracker* tracker) {
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationVaapi>
SharedImageBacking::ProduceVASurface(SharedImageManager* manager,
                                     MemoryTypeTracker* tracker,
                                     VaapiDependenciesFactory* dep_factory) {
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationMemory>
SharedImageBacking::ProduceMemory(SharedImageManager* manager,
                                  MemoryTypeTracker* tracker) {
  return nullptr;
}

void SharedImageBacking::AddRef(SharedImageRepresentation* representation) {
  AutoLock auto_lock(this);

  bool first_ref = refs_.empty();
  refs_.push_back(representation);

  if (first_ref) {
    refs_[0]->tracker()->TrackMemAlloc(estimated_size_);
  }
}

void SharedImageBacking::ReleaseRef(SharedImageRepresentation* representation) {
  AutoLock auto_lock(this);

  auto found = std::find(refs_.begin(), refs_.end(), representation);
  DCHECK(found != refs_.end());

  // If the found representation is the first (owning) ref, free the attributed
  // memory.
  bool released_owning_ref = found == refs_.begin();
  if (released_owning_ref)
    (*found)->tracker()->TrackMemFree(estimated_size_);

  refs_.erase(found);

  if (!released_owning_ref)
    return;

  if (!refs_.empty()) {
    refs_[0]->tracker()->TrackMemAlloc(estimated_size_);
    return;
  }
}

void SharedImageBacking::RegisterImageFactory(SharedImageFactory* factory) {
  DCHECK_CALLED_ON_VALID_THREAD(factory_thread_checker_);
  DCHECK(!factory_);

  factory_ = factory;
}

void SharedImageBacking::UnregisterImageFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(factory_thread_checker_);

  factory_ = nullptr;
}

bool SharedImageBacking::HasAnyRefs() const {
  AutoLock auto_lock(this);

  return !refs_.empty();
}

void SharedImageBacking::OnReadSucceeded() {
  AutoLock auto_lock(this);
  if (scoped_write_uma_) {
    scoped_write_uma_->SetConsumed();
    scoped_write_uma_.reset();
  }
}

void SharedImageBacking::OnWriteSucceeded() {
  AutoLock auto_lock(this);
  scoped_write_uma_.emplace();
}

size_t SharedImageBacking::EstimatedSizeForMemTracking() const {
  return estimated_size_;
}

bool SharedImageBacking::have_context() const {
  return have_context_;
}

SharedImageBacking::AutoLock::AutoLock(
    const SharedImageBacking* shared_image_backing)
    : auto_lock_(InitializeLock(shared_image_backing)) {}

SharedImageBacking::AutoLock::~AutoLock() = default;

base::Lock* SharedImageBacking::AutoLock::InitializeLock(
    const SharedImageBacking* shared_image_backing) {
  if (!shared_image_backing->lock_)
    return nullptr;

  return &shared_image_backing->lock_.value();
}

ClearTrackingSharedImageBacking::ClearTrackingSharedImageBacking(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    size_t estimated_size,
    bool is_thread_safe)
    : SharedImageBacking(mailbox,
                         format,
                         size,
                         color_space,
                         surface_origin,
                         alpha_type,
                         usage,
                         estimated_size,
                         is_thread_safe) {}

gfx::Rect ClearTrackingSharedImageBacking::ClearedRect() const {
  AutoLock auto_lock(this);
  return ClearedRectInternal();
}

void ClearTrackingSharedImageBacking::SetClearedRect(
    const gfx::Rect& cleared_rect) {
  AutoLock auto_lock(this);
  SetClearedRectInternal(cleared_rect);
}

gfx::Rect ClearTrackingSharedImageBacking::ClearedRectInternal() const {
  return cleared_rect_;
}

void ClearTrackingSharedImageBacking::SetClearedRectInternal(
    const gfx::Rect& cleared_rect) {
  cleared_rect_ = cleared_rect;
}

scoped_refptr<gfx::NativePixmap> SharedImageBacking::GetNativePixmap() {
  return nullptr;
}

#if defined(OS_ANDROID)
std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
SharedImageBacking::GetAHardwareBuffer() {
  return nullptr;
}
#endif

}  // namespace gpu
