// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_gl_representation.h"

#include <utility>
#include <vector>

#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"

#define GL_LAYOUT_GENERAL_EXT 0x958D
#define GL_LAYOUT_COLOR_ATTACHMENT_EXT 0x958E
#define GL_LAYOUT_DEPTH_STENCIL_ATTACHMENT_EXT 0x958F
#define GL_LAYOUT_DEPTH_STENCIL_READ_ONLY_EXT 0x9590
#define GL_LAYOUT_SHADER_READ_ONLY_EXT 0x9591
#define GL_LAYOUT_TRANSFER_SRC_EXT 0x9592
#define GL_LAYOUT_TRANSFER_DST_EXT 0x9593
#define GL_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_EXT 0x9530
#define GL_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_EXT 0x9531

#define GL_HANDLE_TYPE_OPAQUE_FD_EXT 0x9586

namespace gpu {

namespace {

GLenum ToGLImageLayout(VkImageLayout layout) {
  switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      return GL_NONE;
    case VK_IMAGE_LAYOUT_GENERAL:
      return GL_LAYOUT_GENERAL_EXT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return GL_LAYOUT_COLOR_ATTACHMENT_EXT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return GL_LAYOUT_DEPTH_STENCIL_ATTACHMENT_EXT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      return GL_LAYOUT_DEPTH_STENCIL_READ_ONLY_EXT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return GL_LAYOUT_SHADER_READ_ONLY_EXT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return GL_LAYOUT_TRANSFER_SRC_EXT;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return GL_LAYOUT_TRANSFER_DST_EXT;
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR:
      return GL_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_EXT;
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR:
      return GL_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_EXT;
    default:
      NOTREACHED() << "Invalid image layout " << layout;
      return GL_NONE;
  }
}

}  // namespace

ExternalVkImageGlRepresentation::ExternalVkImageGlRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    gles2::Texture* texture,
    GLuint texture_service_id)
    : SharedImageRepresentationGLTexture(manager, backing, tracker),
      texture_(texture),
      texture_service_id_(texture_service_id) {}

ExternalVkImageGlRepresentation::~ExternalVkImageGlRepresentation() {}

gles2::Texture* ExternalVkImageGlRepresentation::GetTexture() {
  return texture_;
}

bool ExternalVkImageGlRepresentation::BeginAccess(GLenum mode) {
  // There should not be multiple accesses in progress on the same
  // representation.
  if (current_access_mode_) {
    LOG(ERROR) << "BeginAccess called on ExternalVkImageGlRepresentation before"
               << " the previous access ended.";
    return false;
  }

  DCHECK(mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM ||
         mode == GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  const bool readonly = (mode == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);

  if (!readonly && backing()->format() == viz::ResourceFormat::BGRA_8888) {
    NOTIMPLEMENTED()
        << "BeginAccess write on a BGRA_8888 backing is not supported.";
    return false;
  }

  std::vector<SemaphoreHandle> handles;

  if (!backing_impl()->BeginAccess(readonly, &handles, true /* is_gl */))
    return false;

  for (auto& handle : handles) {
    GLuint gl_semaphore = ImportVkSemaphoreIntoGL(std::move(handle));
    if (gl_semaphore) {
      GrVkImageInfo info;
      auto result = backing_impl()->backend_texture().getVkImageInfo(&info);
      DCHECK(result);
      GLenum src_layout = ToGLImageLayout(info.fImageLayout);
      api()->glWaitSemaphoreEXTFn(gl_semaphore, 0, nullptr, 1,
                                  &texture_service_id_, &src_layout);
      api()->glDeleteSemaphoresEXTFn(1, &gl_semaphore);
    }
  }
  current_access_mode_ = mode;
  return true;
}

void ExternalVkImageGlRepresentation::EndAccess() {
  if (!current_access_mode_) {
    // TODO(crbug.com/933452): We should be able to handle this failure more
    // gracefully rather than shutting down the whole process.
    LOG(ERROR) << "EndAccess called on ExternalVkImageGlRepresentation before "
               << "BeginAccess";
    return;
  }

  DCHECK(current_access_mode_ == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM ||
         current_access_mode_ ==
             GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  const bool readonly =
      (current_access_mode_ == GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  current_access_mode_ = 0;

  VkSemaphore semaphore = VK_NULL_HANDLE;
  SemaphoreHandle semaphore_handle;
  GLuint gl_semaphore = 0;
  if (backing_impl()->need_sychronization()) {
    semaphore =
        vk_implementation()->CreateExternalSemaphore(backing_impl()->device());
    if (semaphore == VK_NULL_HANDLE) {
      // TODO(crbug.com/933452): We should be able to handle this failure more
      // gracefully rather than shutting down the whole process.
      LOG(FATAL) << "Unable to create a VkSemaphore in "
                 << "ExternalVkImageGlRepresentation for synchronization with "
                 << "Vulkan";
      return;
    }

    semaphore_handle =
        vk_implementation()->GetSemaphoreHandle(vk_device(), semaphore);
    vkDestroySemaphore(backing_impl()->device(), semaphore, nullptr);
    if (!semaphore_handle.is_valid()) {
      LOG(FATAL) << "Unable to export VkSemaphore into GL in "
                 << "ExternalVkImageGlRepresentation for synchronization with "
                 << "Vulkan";
      return;
    }

    SemaphoreHandle dup_semaphore_handle = semaphore_handle.Duplicate();
    gl_semaphore = ImportVkSemaphoreIntoGL(std::move(dup_semaphore_handle));

    if (!gl_semaphore) {
      // TODO(crbug.com/933452): We should be able to semaphore_handle this
      // failure more gracefully rather than shutting down the whole process.
      LOG(FATAL) << "Unable to export VkSemaphore into GL in "
                 << "ExternalVkImageGlRepresentation for synchronization with "
                 << "Vulkan";
      return;
    }
  }

  GrVkImageInfo info;
  auto result = backing_impl()->backend_texture().getVkImageInfo(&info);
  DCHECK(result);
  GLenum dst_layout = ToGLImageLayout(info.fImageLayout);
  if (backing_impl()->need_sychronization()) {
    api()->glSignalSemaphoreEXTFn(gl_semaphore, 0, nullptr, 1,
                                  &texture_service_id_, &dst_layout);
    api()->glDeleteSemaphoresEXTFn(1, &gl_semaphore);
  }

  backing_impl()->EndAccess(readonly, std::move(semaphore_handle),
                            true /* is_gl */);
}

GLuint ExternalVkImageGlRepresentation::ImportVkSemaphoreIntoGL(
    SemaphoreHandle handle) {
  if (!handle.is_valid())
    return 0;
#if defined(OS_FUCHSIA)
  NOTIMPLEMENTED_LOG_ONCE();
  return 0;
#elif defined(OS_LINUX)
  if (handle.vk_handle_type() !=
      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT) {
    DLOG(ERROR) << "Importing semaphore handle of unexpected type:"
                << handle.vk_handle_type();
    return 0;
  }
  base::ScopedFD fd = handle.TakeHandle();
  gl::GLApi* api = gl::g_current_gl_context;
  GLuint gl_semaphore;
  api->glGenSemaphoresEXTFn(1, &gl_semaphore);
  api->glImportSemaphoreFdEXTFn(gl_semaphore, GL_HANDLE_TYPE_OPAQUE_FD_EXT,
                                fd.release());

  return gl_semaphore;
#else  // !defined(OS_FUCHSIA) && !defined(OS_LINUX)
#error Unsupported OS
#endif
}

}  // namespace gpu
