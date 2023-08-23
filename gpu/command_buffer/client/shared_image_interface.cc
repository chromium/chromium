// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/shared_image_interface.h"

#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"

namespace gpu {

Mailbox SharedImageInterface::CreateSharedImage(
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::StringPiece debug_label,
    gpu::SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage) {
  NOTREACHED();
  return Mailbox();
}

SharedImageInterface::ScopedMapping::ScopedMapping() = default;
SharedImageInterface::ScopedMapping::~ScopedMapping() {
  if (buffer_) {
    buffer_->Unmap();
  }
}

// static
std::unique_ptr<SharedImageInterface::ScopedMapping>
SharedImageInterface::ScopedMapping::Create(gfx::GpuMemoryBufferHandle handle,
                                            viz::SharedImageFormat format,
                                            gfx::Size size,
                                            gfx::BufferUsage buffer_usage) {
  auto scoped_mapping = base::WrapUnique(new ScopedMapping());
  if (!scoped_mapping->Init(std::move(handle), format, size, buffer_usage)) {
    LOG(ERROR) << "ScopedMapping init failed.";
    return nullptr;
  }
  return scoped_mapping;
}

bool SharedImageInterface::ScopedMapping::Init(
    gfx::GpuMemoryBufferHandle handle,
    viz::SharedImageFormat format,
    gfx::Size size,
    gfx::BufferUsage buffer_usage) {
  GpuMemoryBufferSupport support;

  // Only single planar buffer formats are supported currently. Multiplanar will
  // be supported when Multiplanar SharedImages are fully implemented.
  CHECK(format.is_single_plane());
  buffer_ = support.CreateGpuMemoryBufferImplFromHandle(
      std::move(handle), size,
      viz::SinglePlaneSharedImageFormatToBufferFormat(format), buffer_usage,
      base::DoNothing());
  if (!buffer_) {
    LOG(ERROR) << "Unable to create GpuMemoruBuffer.";
    return false;
  }
  if (!buffer_->Map()) {
    LOG(ERROR) << "Failed to map the buffer.";
    buffer_.reset();
    return false;
  }
  return true;
}

void* SharedImageInterface::ScopedMapping::Memory(const uint32_t plane_index) {
  CHECK(buffer_);
  return buffer_->memory(plane_index);
}

size_t SharedImageInterface::ScopedMapping::Stride(const uint32_t plane_index) {
  CHECK(buffer_);
  return buffer_->stride(plane_index);
}

gfx::BufferFormat SharedImageInterface::ScopedMapping::Format() {
  CHECK(buffer_);
  return buffer_->GetFormat();
}

uint32_t SharedImageInterface::UsageForMailbox(const Mailbox& mailbox) {
  return 0u;
}

void SharedImageInterface::NotifyMailboxAdded(const Mailbox& /*mailbox*/,
                                              uint32_t /*usage*/) {}

Mailbox SharedImageInterface::CreateSharedImage(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::StringPiece debug_label) {
  return CreateSharedImage(gpu_memory_buffer, gpu_memory_buffer_manager,
                           gfx::BufferPlane::DEFAULT, color_space,
                           surface_origin, alpha_type, usage, debug_label);
}

void SharedImageInterface::CopyToGpuMemoryBuffer(const SyncToken& sync_token,
                                                 const Mailbox& mailbox) {
  NOTREACHED();
}

std::unique_ptr<SharedImageInterface::ScopedMapping>
SharedImageInterface::MapSharedImage(const Mailbox& mailbox) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace gpu
