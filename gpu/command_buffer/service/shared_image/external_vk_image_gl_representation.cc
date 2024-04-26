// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/external_vk_image_gl_representation.h"

#include <vector>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/vulkan/vulkan_util.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {
namespace {

template <typename T>
std::vector<GLuint> GetTextureIds(const std::vector<T>& textures) {
  std::vector<GLuint> texture_ids;
  texture_ids.reserve(textures.size());
  for (const T& texture : textures) {
    texture_ids.push_back(texture->service_id());
  }
  return texture_ids;
}

}  // namespace

// static
void ExternalVkImageGLRepresentationShared::AcquireTexture(
    ExternalSemaphore* semaphore,
    const std::vector<GLuint>& texture_ids,
    const std::vector<GLenum>& src_layouts) {
  DCHECK_EQ(texture_ids.size(), src_layouts.size());

  GLuint gl_semaphore = semaphore->GetGLSemaphore();
  if (gl_semaphore) {
    auto* api = gl::g_current_gl_context;
    api->glWaitSemaphoreEXTFn(gl_semaphore, 0, nullptr, texture_ids.size(),
                              texture_ids.data(), src_layouts.data());
  }
}

// static
ExternalSemaphore ExternalVkImageGLRepresentationShared::ReleaseTexture(
    ExternalSemaphorePool* pool,
    const std::vector<GLuint>& texture_ids,
    const std::vector<GLenum>& dst_layouts) {
  DCHECK_EQ(texture_ids.size(), dst_layouts.size());

  ExternalSemaphore semaphore = pool->GetOrCreateSemaphore();
  if (!semaphore) {
    // TODO(crbug.com/41442163): We should be able to handle this failure more
    // gracefully rather than shutting down the whole process.
    DLOG(ERROR) << "Unable to create an ExternalSemaphore in "
                << "ExternalVkImageGLRepresentation for synchronization with "
                << "Vulkan";
    return {};
  }

  GLuint gl_semaphore = semaphore.GetGLSemaphore();
  if (!gl_semaphore) {
    // TODO(crbug.com/41442163): We should be able to semaphore_handle this
    // failure more gracefully rather than shutting down the whole process.
    DLOG(ERROR) << "Unable to export VkSemaphore into GL in "
                << "ExternalVkImageGLRepresentation for synchronization with "
                << "Vulkan";
    return {};
  }

  auto* api = gl::g_current_gl_context;
  api->glSignalSemaphoreEXTFn(gl_semaphore, 0, nullptr, texture_ids.size(),
                              texture_ids.data(), dst_layouts.data());
  // Base on the spec, the glSignalSemaphoreEXT() call just inserts signal
  // semaphore command in the gl context. It may or may not flush the context
  // which depends on the implementation. So to make it safe, we always call
  // glFlush() here. If the implementation does flush in the
  // glSignalSemaphoreEXT() call, the glFlush() call should be a noop.
  api->glFlushFn();

  return semaphore;
}

ExternalVkImageGLRepresentationShared::ExternalVkImageGLRepresentationShared(
    SharedImageBacking* backing,
    std::vector<GLuint> texture_service_ids)
    : backing_(static_cast<ExternalVkImageBacking*>(backing)),
      texture_service_ids_(std::move(texture_service_ids)) {}

ExternalVkImageGLRepresentationShared::
    ~ExternalVkImageGLRepresentationShared() = default;

bool ExternalVkImageGLRepresentationShared::BeginAccess(GLenum mode) {
  // There should not be multiple accesses in progress on the same
  // representation.
  if (current_access_mode_) {
    DLOG(ERROR) << "BeginAccess called on ExternalVkImageGLRepresentation"
                << " before the previous access ended.";
    return false;
  }

  DCHECK(mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM ||
         mode == GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  const bool readonly =
      (mode != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);

  DCHECK(begin_access_semaphores_.empty());
  if (!backing_impl()->BeginAccess(readonly, &begin_access_semaphores_,
                                   /*is_gl=*/true)) {
    return false;
  }

  for (auto& external_semaphore : begin_access_semaphores_) {
    auto image_layouts = backing_impl()->GetVkImageLayoutsForGL();
    AcquireTexture(&external_semaphore, texture_service_ids_, image_layouts);
  }
  current_access_mode_ = mode;
  return true;
}

void ExternalVkImageGLRepresentationShared::EndAccess() {
  if (!current_access_mode_) {
    // TODO(crbug.com/41442163): We should be able to handle this failure more
    // gracefully rather than shutting down the whole process.
    DLOG(ERROR) << "EndAccess called on ExternalVkImageGLRepresentation before "
                << "BeginAccess";
    return;
  }

  DCHECK(current_access_mode_ == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM ||
         current_access_mode_ ==
             GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  const bool readonly =
      (current_access_mode_ != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  current_access_mode_ = 0;

  ExternalSemaphore external_semaphore;
  if (backing_impl()->need_synchronization() &&
      backing_impl()->gl_reads_in_progress() <= 1) {
    DCHECK(readonly == !!backing_impl()->gl_reads_in_progress());
    auto image_layouts = backing_impl()->GetVkImageLayoutsForGL();

    external_semaphore =
        ReleaseTexture(backing_impl()->external_semaphore_pool(),
                       texture_service_ids_, image_layouts);
    if (!external_semaphore) {
      backing_impl()->context_state()->MarkContextLost();
      return;
    }
  }
  backing_impl()->EndAccess(readonly, std::move(external_semaphore),
                            /*is_gl=*/true);

  // We have done with |begin_access_semaphores_|. They should have been waited.
  // So add them to pending semaphores for reusing or relaeasing.
  backing_impl()->AddSemaphoresToPendingListOrRelease(
      std::move(begin_access_semaphores_));
  begin_access_semaphores_.clear();
}

ExternalVkImageGLRepresentation::ExternalVkImageGLRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    std::vector<raw_ptr<gles2::Texture, VectorExperimental>> textures)
    : GLTextureImageRepresentation(manager, backing, tracker),
      textures_(std::move(textures)),
      representation_shared_(backing, GetTextureIds(textures_)) {
  DCHECK_EQ(textures_.size(), NumPlanesExpected());
}

ExternalVkImageGLRepresentation::~ExternalVkImageGLRepresentation() = default;

gles2::Texture* ExternalVkImageGLRepresentation::GetTexture(int plane_index) {
  return textures_[plane_index];
}

bool ExternalVkImageGLRepresentation::BeginAccess(GLenum mode) {
  return representation_shared_.BeginAccess(mode);
}
void ExternalVkImageGLRepresentation::EndAccess() {
  representation_shared_.EndAccess();
}

ExternalVkImageGLPassthroughRepresentation::
    ExternalVkImageGLPassthroughRepresentation(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        MemoryTypeTracker* tracker,
        std::vector<scoped_refptr<gles2::TexturePassthrough>> textures)
    : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
      textures_(std::move(textures)),
      representation_shared_(backing, GetTextureIds(textures_)) {
  DCHECK_EQ(textures_.size(), NumPlanesExpected());
}

ExternalVkImageGLPassthroughRepresentation::
    ~ExternalVkImageGLPassthroughRepresentation() = default;

const scoped_refptr<gles2::TexturePassthrough>&
ExternalVkImageGLPassthroughRepresentation::GetTexturePassthrough(
    int plane_index) {
  return textures_[plane_index];
}

bool ExternalVkImageGLPassthroughRepresentation::BeginAccess(GLenum mode) {
  return representation_shared_.BeginAccess(mode);
}
void ExternalVkImageGLPassthroughRepresentation::EndAccess() {
  representation_shared_.EndAccess();
}

}  // namespace gpu
