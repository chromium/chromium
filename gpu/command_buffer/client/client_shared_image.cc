// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/client_shared_image.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"

namespace gpu {

ClientSharedImage::ScopedMapping::ScopedMapping() = default;
ClientSharedImage::ScopedMapping::~ScopedMapping() {
  if (buffer_) {
    buffer_->Unmap();
  }
}

// static
std::unique_ptr<ClientSharedImage::ScopedMapping>
ClientSharedImage::ScopedMapping::Create(
    gfx::GpuMemoryBuffer* gpu_memory_buffer) {
  auto scoped_mapping = base::WrapUnique(new ScopedMapping());
  if (!scoped_mapping->Init(gpu_memory_buffer)) {
    LOG(ERROR) << "ScopedMapping init failed.";
    return nullptr;
  }
  return scoped_mapping;
}

bool ClientSharedImage::ScopedMapping::Init(
    gfx::GpuMemoryBuffer* gpu_memory_buffer) {
  if (!gpu_memory_buffer) {
    LOG(ERROR) << "No GpuMemoryBuffer.";
    return false;
  }

  if (!gpu_memory_buffer->Map()) {
    LOG(ERROR) << "Failed to map the buffer.";
    return false;
  }
  buffer_ = gpu_memory_buffer;
  return true;
}

void* ClientSharedImage::ScopedMapping::Memory(const uint32_t plane_index) {
  CHECK(buffer_);
  return buffer_->memory(plane_index);
}

size_t ClientSharedImage::ScopedMapping::Stride(const uint32_t plane_index) {
  CHECK(buffer_);
  return buffer_->stride(plane_index);
}

gfx::Size ClientSharedImage::ScopedMapping::Size() {
  CHECK(buffer_);
  return buffer_->GetSize();
}

gfx::BufferFormat ClientSharedImage::ScopedMapping::Format() {
  CHECK(buffer_);
  return buffer_->GetFormat();
}

bool ClientSharedImage::ScopedMapping::IsSharedMemory() {
  CHECK(buffer_);
  return buffer_->GetType() == gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER;
}

void ClientSharedImage::ScopedMapping::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
    uint64_t tracing_process_id,
    int importance) {
  buffer_->OnMemoryDump(pmd, buffer_dump_guid, tracing_process_id, importance);
}

ClientSharedImage::ClientSharedImage(const Mailbox& mailbox)
    : mailbox_(mailbox) {}

ClientSharedImage::ClientSharedImage(const Mailbox& mailbox,
                                     GpuMemoryBufferHandleInfo handle_info)
    : mailbox_(mailbox),
      gpu_memory_buffer_(
          GpuMemoryBufferSupport().CreateGpuMemoryBufferImplFromHandle(
              std::move(handle_info.handle),
              handle_info.size,
              // Only single planar buffer formats are supported currently.
              // Multiplanar will be supported when Multiplanar SharedImages are
              // fully implemented.
              viz::SinglePlaneSharedImageFormatToBufferFormat(
                  handle_info.format),
              handle_info.buffer_usage,
              base::DoNothing())) {}

ClientSharedImage::~ClientSharedImage() = default;

std::unique_ptr<ClientSharedImage::ScopedMapping> ClientSharedImage::Map() {
  auto scoped_mapping = ScopedMapping::Create(gpu_memory_buffer_.get());
  if (!scoped_mapping) {
    LOG(ERROR) << "Unable to create ScopedMapping";
  }
  return scoped_mapping;
}

}  // namespace gpu
