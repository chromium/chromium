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

static bool allow_external_sampling_without_native_buffers_for_testing = false;

// Computes the texture target to use for a SharedImage that was created with
// `metadata` and the given type of GpuMemoryBuffer(Handle) supplied by the
// client (which will be gfx::EmptyBuffer if the client did not supply a
// GMB/GMBHandle). Conceptually:
// * On Mac the native buffer target is required if either (1) the client
//   gave a native buffer or (2) the usages require a native buffer.
// * On all other platforms the native buffer target is required iff external
//   sampling is being used, which is dictated by the format of the SharedImage.
uint32_t ComputeTextureTargetForSharedImage(
    SharedImageMetadata metadata,
    gfx::GpuMemoryBufferType client_gmb_type) {
  bool client_side_native_buffer_used =
      client_gmb_type != gfx::EMPTY_BUFFER &&
      client_gmb_type != gfx::SHARED_MEMORY_BUFFER;

#if BUILDFLAG(IS_MAC)
  // NOTE: WebGPU usage on Mac results in SharedImages being backed by
  // IOSurfaces.
  uint32_t usages_requiring_native_buffer = SHARED_IMAGE_USAGE_SCANOUT |
                                            SHARED_IMAGE_USAGE_WEBGPU_READ |
                                            SHARED_IMAGE_USAGE_WEBGPU_WRITE;

  bool uses_native_buffer = client_side_native_buffer_used ||
                            (metadata.usage & usages_requiring_native_buffer);

  return uses_native_buffer ? GetPlatformSpecificTextureTarget()
                            : GL_TEXTURE_2D;
#else
  bool uses_external_sampler = metadata.format.PrefersExternalSampler() ||
                               metadata.format.IsLegacyMultiplanar();

  // The client should configure an SI to use external sampling only if they
  // have provided a native buffer to back that SI.
  CHECK(!uses_external_sampler || client_side_native_buffer_used ||
        allow_external_sampling_without_native_buffers_for_testing);

  return uses_external_sampler ? GetPlatformSpecificTextureTarget()
                               : GL_TEXTURE_2D;
#endif
}

}  // namespace

BASE_FEATURE(kUseUniversalGetTextureTargetFunction,
             "UseUniversalGetTextureTargetFunction",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

// static
void ClientSharedImage::AllowExternalSamplingWithoutNativeBuffersForTesting(
    bool allow) {
  allow_external_sampling_without_native_buffers_for_testing = allow;
}

ClientSharedImage::ClientSharedImage(
    const Mailbox& mailbox,
    const SharedImageMetadata& metadata,
    const SyncToken& sync_token,
    scoped_refptr<SharedImageInterfaceHolder> sii_holder,
    gfx::GpuMemoryBufferType gmb_type)
    : mailbox_(mailbox),
      metadata_(metadata),
      creation_sync_token_(sync_token),
      sii_holder_(std::move(sii_holder)) {
  CHECK(!mailbox.IsZero());
  CHECK(sii_holder_);
  texture_target_ = ComputeTextureTargetForSharedImage(metadata_, gmb_type);
}

ClientSharedImage::ClientSharedImage(
    const Mailbox& mailbox,
    const SharedImageMetadata& metadata,
    const SyncToken& sync_token,
    scoped_refptr<SharedImageInterfaceHolder> sii_holder,
    uint32_t texture_target)
    : mailbox_(mailbox),
      metadata_(metadata),
      creation_sync_token_(sync_token),
      sii_holder_(std::move(sii_holder)),
      texture_target_(texture_target) {
  CHECK(!mailbox.IsZero());
  CHECK(sii_holder_);
#if !BUILDFLAG(IS_FUCHSIA)
  CHECK(texture_target);
#endif
}

ClientSharedImage::ClientSharedImage(const Mailbox& mailbox,
                                     const SharedImageMetadata& metadata,
                                     const SyncToken& sync_token,
                                     uint32_t texture_target)
    : mailbox_(mailbox),
      metadata_(metadata),
      creation_sync_token_(sync_token),
      texture_target_(texture_target) {
  CHECK(!mailbox.IsZero());
#if !BUILDFLAG(IS_FUCHSIA)
  CHECK(texture_target);
#endif
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
  CHECK(sii_holder_);
  texture_target_ = ComputeTextureTargetForSharedImage(
      metadata_, gpu_memory_buffer_->GetType());
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
#if !BUILDFLAG(IS_FUCHSIA)
  // Check that `texture_target_` has been initialized (note that on Fuchsia it
  // is possible for `texture_target_` to be initialized to 0: Fuchsia does not
  // support import of external images to GL for usage with external sampling.
  // SetTextureTarget() sets the texture target to 0 in the case where external
  // sampling would be used to signal this lack of support to the //media code,
  // which detects the lack of support *based on* on the texture target being
  // 0).
  CHECK(texture_target_);
#endif
  return texture_target_;
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

scoped_refptr<ClientSharedImage> ClientSharedImage::MakeUnowned() {
  return ClientSharedImage::ImportUnowned(Export());
}

ExportedSharedImage ClientSharedImage::Export() {
  if (creation_sync_token_.HasData() &&
      !creation_sync_token_.verified_flush()) {
    sii_holder_->Get()->VerifySyncToken(creation_sync_token_);
  }
  return ExportedSharedImage(mailbox_, metadata_, creation_sync_token_,
                             texture_target_);
}

scoped_refptr<ClientSharedImage> ClientSharedImage::ImportUnowned(
    const ExportedSharedImage& exported_shared_image) {
  return base::WrapRefCounted<ClientSharedImage>(new ClientSharedImage(
      exported_shared_image.mailbox_, exported_shared_image.metadata_,
      exported_shared_image.creation_sync_token_,
      exported_shared_image.texture_target_));
}

// static
scoped_refptr<ClientSharedImage> ClientSharedImage::CreateForTesting() {
  SharedImageMetadata metadata;
  metadata.format = viz::SinglePlaneFormat::kRGBA_8888;
  metadata.color_space = gfx::ColorSpace::CreateSRGB();
  metadata.surface_origin = kTopLeft_GrSurfaceOrigin;
  metadata.alpha_type = kOpaque_SkAlphaType;
  metadata.usage = 0;

  return ImportUnowned(ExportedSharedImage(
      Mailbox::GenerateForSharedImage(), metadata, SyncToken(), GL_TEXTURE_2D));
}

ExportedSharedImage::ExportedSharedImage() = default;
ExportedSharedImage::ExportedSharedImage(const Mailbox& mailbox,
                                         const SharedImageMetadata& metadata,
                                         const SyncToken& sync_token,
                                         uint32_t texture_target)
    : mailbox_(mailbox),
      metadata_(metadata),
      creation_sync_token_(sync_token),
      texture_target_(texture_target) {}

}  // namespace gpu
