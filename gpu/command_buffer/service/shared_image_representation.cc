// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_representation.h"

#include "third_party/skia/include/core/SkPromiseImageTexture.h"

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
  manager_->OnRepresentationDestroyed(backing_->mailbox(), this);
}

bool SharedImageRepresentationGLTexture::BeginAccess(GLenum mode) {
  return true;
}

bool SharedImageRepresentationGLTexturePassthrough::BeginAccess(GLenum mode) {
  return true;
}

SharedImageRepresentationSkia::ScopedWriteAccess::ScopedWriteAccess(
    SharedImageRepresentationSkia* representation,
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores)
    : representation_(representation),
      surface_(representation_->BeginWriteAccess(final_msaa_count,
                                                 surface_props,
                                                 begin_semaphores,
                                                 end_semaphores)) {
  if (success())
    representation->backing()->OnWriteSucceeded();
}

SharedImageRepresentationSkia::ScopedWriteAccess::ScopedWriteAccess(
    SharedImageRepresentationSkia* representation,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores)
    : ScopedWriteAccess(representation,
                        0 /* final_msaa_count */,
                        SkSurfaceProps(0 /* flags */, kUnknown_SkPixelGeometry),
                        begin_semaphores,
                        end_semaphores) {}

SharedImageRepresentationSkia::ScopedWriteAccess::~ScopedWriteAccess() {
  if (success())
    representation_->EndWriteAccess(std::move(surface_));
}

SharedImageRepresentationSkia::ScopedReadAccess::ScopedReadAccess(
    SharedImageRepresentationSkia* representation,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores)
    : representation_(representation),
      promise_image_texture_(
          representation_->BeginReadAccess(begin_semaphores, end_semaphores)) {
  if (success())
    representation->backing()->OnReadSucceeded();
}

SharedImageRepresentationSkia::ScopedReadAccess::~ScopedReadAccess() {
  if (success())
    representation_->EndReadAccess();
}

}  // namespace gpu
