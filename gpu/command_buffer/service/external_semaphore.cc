// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_semaphore.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_util.h"
#include "ui/gl/gl_bindings.h"

#define GL_HANDLE_TYPE_OPAQUE_FD_EXT 0x9586
#define GL_HANDLE_TYPE_OPAQUE_WIN32_EXT 0x9587
#define GL_HANDLE_TYPE_ZIRCON_VMO_ANGLE 0x93AE
#define GL_HANDLE_TYPE_ZIRCON_EVENT_ANGLE 0x93AF

namespace gpu {

namespace {

GLuint ImportSemaphoreHandleToGLSemaphore(SemaphoreHandle handle) {
  if (!handle.is_valid())
    return 0;

#if BUILDFLAG(IS_POSIX)
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
#elif BUILDFLAG(IS_WIN)
  if (handle.vk_handle_type() !=
      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT) {
    DLOG(ERROR) << "Importing semaphore handle of unexpected type:"
                << handle.vk_handle_type();
    return 0;
  }
  auto win32_handle = handle.TakeHandle();
  gl::GLApi* api = gl::g_current_gl_context;
  GLuint gl_semaphore;
  api->glGenSemaphoresEXTFn(1, &gl_semaphore);
  api->glImportSemaphoreWin32HandleEXTFn(
      gl_semaphore, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, win32_handle.Take());

  return gl_semaphore;
#elif BUILDFLAG(IS_FUCHSIA)
  if (handle.vk_handle_type() !=
      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA) {
    DLOG(ERROR) << "Importing semaphore handle of unexpected type:"
                << handle.vk_handle_type();
    return 0;
  }
  zx::event event = handle.TakeHandle();
  gl::GLApi* api = gl::g_current_gl_context;
  GLuint gl_semaphore;
  api->glGenSemaphoresEXTFn(1, &gl_semaphore);
  api->glImportSemaphoreZirconHandleANGLEFn(
      gl_semaphore, GL_HANDLE_TYPE_ZIRCON_EVENT_ANGLE, event.release());
  return gl_semaphore;
#else
#error Unsupported OS
#endif
}

}  // namespace

// static
ExternalSemaphore ExternalSemaphore::Create(
    viz::VulkanContextProvider* context_provider) {
  VkDevice device = context_provider->GetDeviceQueue()->GetVulkanDevice();

  VkSemaphore semaphore = CreateVkOpaqueExternalSemaphore(device);
  if (semaphore == VK_NULL_HANDLE)
    return {};

  auto handle = ExportVkOpaqueExternalSemaphore(device, semaphore);
  if (!handle.is_valid()) {
    vkDestroySemaphore(device, semaphore, /*pAllocator=*/nullptr);
    return {};
  }

  return ExternalSemaphore(base::PassKey<ExternalSemaphore>(), context_provider,
                           semaphore, std::move(handle));
}

// static
ExternalSemaphore ExternalSemaphore::CreateFromHandle(
    viz::VulkanContextProvider* context_provider,
    SemaphoreHandle handle) {
  if (!handle.is_valid())
    return {};

  auto* implementation = context_provider->GetVulkanImplementation();
  VkDevice device = context_provider->GetDeviceQueue()->GetVulkanDevice();

  VkSemaphore semaphore =
      implementation->ImportSemaphoreHandle(device, handle.Duplicate());
  if (semaphore == VK_NULL_HANDLE)
    return {};

  return ExternalSemaphore(base::PassKey<ExternalSemaphore>(), context_provider,
                           semaphore, std::move(handle));
}

ExternalSemaphore::ExternalSemaphore() = default;

ExternalSemaphore::ExternalSemaphore(ExternalSemaphore&& other) {
  *this = std::move(other);
}

ExternalSemaphore::ExternalSemaphore(
    base::PassKey<ExternalSemaphore>,
    viz::VulkanContextProvider* context_provider,
    VkSemaphore semaphore,
    SemaphoreHandle handle)
    : context_provider_(context_provider),
      semaphore_(semaphore),
      handle_(std::move(handle)) {}

ExternalSemaphore::~ExternalSemaphore() {
  Reset();
}

ExternalSemaphore& ExternalSemaphore::operator=(ExternalSemaphore&& other) {
  Reset();
  std::swap(context_provider_, other.context_provider_);
  std::swap(semaphore_, other.semaphore_);
  std::swap(handle_, other.handle_);
  std::swap(gl_semaphore_, other.gl_semaphore_);
  return *this;
}

void ExternalSemaphore::Reset() {
  if (semaphore_ != VK_NULL_HANDLE) {
    DCHECK(context_provider_);
    VkDevice device = context_provider_->GetDeviceQueue()->GetVulkanDevice();
    vkDestroySemaphore(device, semaphore_, /*pAllocator=*/nullptr);
  }

  if (gl_semaphore_ != 0) {
    // We assume there is always one GL context current. If there isn't a
    // GL context current, we assume the last GL context is destroyed, in that
    // case, we will skip glDeleteSemaphoresEXT().
    if (gl::g_current_gl_driver) {
      gl::GLApi* const api = gl::g_current_gl_context;
      api->glDeleteSemaphoresEXTFn(1, &gl_semaphore_);
    }
  }

  context_provider_ = nullptr;
  semaphore_ = VK_NULL_HANDLE;
  gl_semaphore_ = 0;
  handle_ = {};
}

unsigned int ExternalSemaphore::GetGLSemaphore() {
  DCHECK(handle_.is_valid());
  if (gl_semaphore_ == 0) {
    gl_semaphore_ = ImportSemaphoreHandleToGLSemaphore(handle_.Duplicate());
  }
  return gl_semaphore_;
}

VkSemaphore ExternalSemaphore::GetVkSemaphore() {
  DCHECK(handle_.is_valid());
  if (semaphore_ == VK_NULL_HANDLE) {
    auto* implementation = context_provider_->GetVulkanImplementation();
    VkDevice device = context_provider_->GetDeviceQueue()->GetVulkanDevice();
    semaphore_ =
        implementation->ImportSemaphoreHandle(device, handle_.Duplicate());
  }
  return semaphore_;
}

SemaphoreHandle ExternalSemaphore::TakeSemaphoreHandle() {
  SemaphoreHandle handle = std::move(handle_);
  DCHECK(!handle_.is_valid());
  Reset();
  return handle;
}

}  // namespace gpu
