// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/client_shared_image.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2extchromium.h>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "ui/gfx/buffer_types.h"

namespace gpu {

namespace {

static bool allow_external_sampling_without_native_buffers_for_testing = false;

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_OZONE)
bool GMBIsNative(gfx::GpuMemoryBufferType gmb_type) {
  return gmb_type != gfx::EMPTY_BUFFER && gmb_type != gfx::SHARED_MEMORY_BUFFER;
}
#endif

// Computes the texture target to use for a SharedImage that was created with
// `metadata` and the given type of GpuMemoryBuffer(Handle) supplied by the
// client (which will be gfx::EmptyBuffer if the client did not supply a
// GMB/GMBHandle). Conceptually:
// * On Mac the native buffer target is required if either (1) the client
//   gave a native buffer or (2) the usages require a native buffer.
// * On Ozone the native buffer target is required iff external sampling is
//   being used, which is dictated by the format of the SharedImage. Note
//   * Fuchsia does not support import of external images to GL for usage with
//     external sampling.  The ClientSharedImage's texture target must be 0 in
//     the case where external sampling would be used to signal this lack of
//     support to the //media code, which detects the lack of support *based on*
//     on the texture target being 0.
// * On all other platforms GL_TEXTURE_2D is always used (external sampling is
//   supported in Chromium only on Ozone).
uint32_t ComputeTextureTargetForSharedImage(
    SharedImageMetadata metadata,
    gfx::GpuMemoryBufferType client_gmb_type,
    scoped_refptr<SharedImageInterface> sii) {
  CHECK(sii);

#if !BUILDFLAG(IS_OZONE)
  // External sampling with GMBs is supported in Chromium only for Ozone.
  // Android uses a bespoke path for external sampling where the AHB doesn't get
  // put in a GMB and multiplanar formats aren't used, and Windows doesn't use
  // external sampling at all. It is not possible to set
  // PrefersExternalSampler() on a MP SIF outside of Ozone, but legacy MP
  // formats could theoretically be used on any platform. Such usage would be
  // incorrect outside of Ozone as legacy MP formats work only with external
  // sampling. This DUMP_WILL_BE_CHECK() is added in advance of adding the
  // invariant via a CHECK that legacy MP formats are *actually* used only on
  // Ozone.
  DUMP_WILL_BE_CHECK(!metadata.format.IsLegacyMultiplanar());
#endif

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_OZONE)
  return GL_TEXTURE_2D;
#elif BUILDFLAG(IS_MAC)
  // Check for IOSurfaces being used.
  // NOTE: WebGPU usage on Mac results in SharedImages being backed by
  // IOSurfaces.
  uint32_t usages_requiring_native_buffer = SHARED_IMAGE_USAGE_SCANOUT |
                                            SHARED_IMAGE_USAGE_WEBGPU_READ |
                                            SHARED_IMAGE_USAGE_WEBGPU_WRITE;

  bool uses_native_buffer = GMBIsNative(client_gmb_type) ||
                            (metadata.usage & usages_requiring_native_buffer);

  return uses_native_buffer
             ? sii->GetCapabilities().macos_specific_texture_target
             : GL_TEXTURE_2D;
#else  // Ozone
  // Check for external sampling being used.
  bool uses_external_sampler = metadata.format.PrefersExternalSampler() ||
                               metadata.format.IsLegacyMultiplanar();

  if (!uses_external_sampler) {
    return GL_TEXTURE_2D;
  }

  // Software video decode on ChromeOS for single P010 GpuMemoryBuffers with
  // legacy multiplanar SI codepath, always fallback to VideoResourceUpdater
  // instead of GMBVideoFramePool; this is due to mismatch in BufferUsage flags
  // which is not supported and causes this CHECK to surface. Accordingly, just
  // perform the CHECK for non-legacy multiplanar SI path that avoids preferring
  // external sampler when shared memory GMB is used and thus does not fallback.
  // TODO(crbug.com/40239769): Remove condition once multiplanar SI launches
  // everywhere.
  if (!metadata.format.IsLegacyMultiplanar()) {
    // The client should configure an SI to use external sampling only if they
    // have provided a native buffer to back that SI.
    // TODO(crbug.com/332069927): Figure out why this is going off on LaCrOS and
    // turn this into a CHECK.
    DUMP_WILL_BE_CHECK(
        GMBIsNative(client_gmb_type) ||
        allow_external_sampling_without_native_buffers_for_testing);
  }

  // See the note at the top of this function wrt Fuchsia.
#if BUILDFLAG(IS_FUCHSIA)
  return 0;
#else
  return GL_TEXTURE_EXTERNAL_OES;
#endif  // BUILDFLAG(IS_FUCHSIA)
#endif  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_OZONE)
}

}  // namespace

BASE_FEATURE(kUseUniversalGetTextureTargetFunction,
             "UseUniversalGetTextureTargetFunction",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDestroySharedImageAutomatically,
             "DestroySharedImageAutomatically",
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
  texture_target_ = ComputeTextureTargetForSharedImage(metadata_, gmb_type,
                                                       sii_holder_->Get());
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
              viz::SharedImageFormatToBufferFormatRestrictedUtils::
                  ToBufferFormat(handle_info.format),
              handle_info.buffer_usage,
              base::DoNothing())),
      sii_holder_(std::move(sii_holder)) {
  CHECK(!mailbox.IsZero());
  CHECK(sii_holder_);
  texture_target_ = ComputeTextureTargetForSharedImage(
      metadata_, gpu_memory_buffer_->GetType(), sii_holder_->Get());
}

ClientSharedImage::~ClientSharedImage() {
  if (!HasHolder()) {
    if (marked_for_destruction_) {
      CHECK_IS_TEST();
    }
    return;
  }

  if (base::FeatureList::IsEnabled(kDestroySharedImageAutomatically) ||
      marked_for_destruction_) {
    auto sii = sii_holder_->Get();
    if (sii) {
      sii->DestroySharedImage(destruction_sync_token_, mailbox_);
    }
  }
}

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
