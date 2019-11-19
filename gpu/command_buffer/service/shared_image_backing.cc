// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing.h"

#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_representation.h"

namespace gpu {

SharedImageBacking::SharedImageBacking(const Mailbox& mailbox,
                                       viz::ResourceFormat format,
                                       const gfx::Size& size,
                                       const gfx::ColorSpace& color_space,
                                       uint32_t usage,
                                       size_t estimated_size,
                                       bool is_thread_safe)
    : mailbox_(mailbox),
      format_(format),
      size_(size),
      color_space_(color_space),
      usage_(usage),
      estimated_size_(estimated_size) {
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

  // Last ref deleted, clean up.
  Destroy();
}

bool SharedImageBacking::HasAnyRefs() const {
  AutoLock auto_lock(this);

  return !refs_.empty();
}

void SharedImageBacking::OnReadSucceeded() {
  if (scoped_write_uma_) {
    scoped_write_uma_->SetConsumed();
    scoped_write_uma_.reset();
  }
}

void SharedImageBacking::OnWriteSucceeded() {
  scoped_write_uma_.emplace();
}

size_t SharedImageBacking::EstimatedSizeForMemTracking() const {
  return estimated_size_;
}

bool SharedImageBacking::have_context() const {
  AssertLockedIfNecessary();

  DCHECK(refs_.empty());

  return have_context_;
}

void SharedImageBacking::AssertLockedIfNecessary() const {
  if (lock_)
    lock_->AssertAcquired();
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

}  // namespace gpu
