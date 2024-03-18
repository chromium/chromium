// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2extchromium.h>

#include "base/containers/contains.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "ui/gfx/buffer_types.h"

namespace gpu {

namespace {

bool GMBIsNative(gfx::GpuMemoryBufferType gmb_type) {
  return gmb_type != gfx::EMPTY_BUFFER && gmb_type != gfx::SHARED_MEMORY_BUFFER;
}

}  // namespace

BASE_FEATURE(kUseUniversalGetTextureTargetFunction,
             "UseUniversalGetTextureTargetFunction",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

ClientSharedImage::ClientSharedImage(
    const Mailbox& mailbox,
    const SharedImageMetadata& metadata,
    const SyncToken& sync_token,
    scoped_refptr<SharedImageInterfaceHolder> sii_holder,
    gfx::GpuMemoryBufferType gmb_type /*= gfx::EMPTY_BUFFER*/)
    : mailbox_(mailbox),
      metadata_(metadata),
      creation_sync_token_(sync_token),
      sii_holder_(std::move(sii_holder)),
      client_side_native_buffer_used_(GMBIsNative(gmb_type)) {
  CHECK(!mailbox.IsZero());
}

ClientSharedImage::ClientSharedImage(
    const Mailbox& mailbox,
    const SharedImageMetadata& metadata,
    const SyncToken& sync_token,
    GpuMemoryBufferHandleInfo handle_info,
    scoped_refptr<SharedImageInterfaceHolder> sii_holder)
    : mailbox_(mailbox),
      metadata_(metadata),
      creation_sync_token_(sync_token),
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
              base::DoNothing())),
      sii_holder_(std::move(sii_holder)) {
  CHECK(!mailbox.IsZero());
  client_side_native_buffer_used_ = GMBIsNative(gpu_memory_buffer_->GetType());
}

ClientSharedImage::~ClientSharedImage() = default;

std::unique_ptr<ClientSharedImage::ScopedMapping> ClientSharedImage::Map() {
  auto scoped_mapping = ScopedMapping::Create(gpu_memory_buffer_.get());
  if (!scoped_mapping) {
    LOG(ERROR) << "Unable to create ScopedMapping";
  }
  return scoped_mapping;
}

#if BUILDFLAG(IS_APPLE)
void ClientSharedImage::SetColorSpaceOnNativeBuffer(
    const gfx::ColorSpace& color_space) {
  CHECK(gpu_memory_buffer_);
  gpu_memory_buffer_->SetColorSpace(color_space);
}
#endif

uint32_t ClientSharedImage::GetTextureTarget() {
  // On Mac, the platform-specific texture target is required if this
  // SharedImage is backed by a native buffer. On other platforms, the
  // platform-specific target is required if external sampling is used.
#if BUILDFLAG(IS_MAC)
  // NOTE: WebGPU usage on Mac results in SharedImages being backed by
  // IOSurfaces.
  uint32_t usages_requiring_native_buffer = SHARED_IMAGE_USAGE_SCANOUT |
                                            SHARED_IMAGE_USAGE_WEBGPU_READ |
                                            SHARED_IMAGE_USAGE_WEBGPU_WRITE;

  bool uses_native_buffer = client_side_native_buffer_used_ ||
                            (usage() & usages_requiring_native_buffer);

  return uses_native_buffer ? GetPlatformSpecificTextureTarget()
                            : GL_TEXTURE_2D;
#else
  bool uses_external_sampler =
      format().PrefersExternalSampler() || format().IsLegacyMultiplanar();

  // The client should configure an SI to use external sampling only if they
  // have provided a native buffer to back that SI.
  CHECK(!uses_external_sampler || client_side_native_buffer_used_);

  return uses_external_sampler ? GetPlatformSpecificTextureTarget()
                               : GL_TEXTURE_2D;
#endif
}

uint32_t ClientSharedImage::GetTextureTargetForOverlays() {
  if (base::FeatureList::IsEnabled(kUseUniversalGetTextureTargetFunction)) {
    return GetTextureTarget();
  }

#if BUILDFLAG(IS_MAC)
  return GetPlatformSpecificTextureTarget();
#else
  return GL_TEXTURE_2D;
#endif
}

uint32_t ClientSharedImage::GetTextureTarget(gfx::BufferFormat format) {
  if (base::FeatureList::IsEnabled(kUseUniversalGetTextureTargetFunction)) {
    return GetTextureTarget();
  }

  return NativeBufferNeedsPlatformSpecificTextureTarget(format)
             ? GetPlatformSpecificTextureTarget()
             : GL_TEXTURE_2D;
}

uint32_t ClientSharedImage::GetTextureTarget(gfx::BufferUsage usage,
                                             gfx::BufferFormat format) {
  if (base::FeatureList::IsEnabled(kUseUniversalGetTextureTargetFunction)) {
    return GetTextureTarget();
  }

  CHECK(HasHolder());

  auto capabilities = sii_holder_->Get()->GetCapabilities();
  bool found = base::Contains(capabilities.texture_target_exception_list,
                              gfx::BufferUsageAndFormat(usage, format));
  return found ? gpu::GetPlatformSpecificTextureTarget() : GL_TEXTURE_2D;
}

uint32_t ClientSharedImage::GetTextureTarget(gfx::BufferUsage usage) {
  if (base::FeatureList::IsEnabled(kUseUniversalGetTextureTargetFunction)) {
    return GetTextureTarget();
  }

  uint32_t usages_forcing_native_buffer = SHARED_IMAGE_USAGE_SCANOUT;
#if BUILDFLAG(IS_MAC)
  // On Mac, WebGPU usage results in SharedImages being backed by IOSurfaces.
  usages_forcing_native_buffer = usages_forcing_native_buffer |
                                 SHARED_IMAGE_USAGE_WEBGPU_READ |
                                 SHARED_IMAGE_USAGE_WEBGPU_WRITE;
#endif

  bool uses_native_buffer = this->usage() & usages_forcing_native_buffer;
  return uses_native_buffer
             ? GetTextureTarget(usage,
                                viz::SinglePlaneSharedImageFormatToBufferFormat(
                                    metadata_.format))
             : GL_TEXTURE_2D;
}

ExportedSharedImage ClientSharedImage::Export() {
  if (creation_sync_token_.HasData() &&
      !creation_sync_token_.verified_flush()) {
    sii_holder_->Get()->VerifySyncToken(creation_sync_token_);
  }
  return ExportedSharedImage(mailbox_, metadata_, creation_sync_token_);
}

scoped_refptr<ClientSharedImage> ClientSharedImage::ImportUnowned(
    const ExportedSharedImage& exported_shared_image) {
  // TODO(crbug.com/41494843): Plumb information through ExportedSharedImage to
  // ensure that the ClientSharedImage created here computes the same texture
  // target via GetTextureTarget() as the source ClientSharedImage from which
  // the ExportedSharedImage was created.
  return base::MakeRefCounted<ClientSharedImage>(
      exported_shared_image.mailbox_, exported_shared_image.metadata_,
      exported_shared_image.sync_token_, nullptr, gfx::EMPTY_BUFFER);
}

ExportedSharedImage::ExportedSharedImage(const Mailbox& mailbox,
                                         const SharedImageMetadata& metadata,
                                         const SyncToken& sync_token)
    : mailbox_(mailbox), metadata_(metadata), sync_token_(sync_token) {}

}  // namespace gpu
