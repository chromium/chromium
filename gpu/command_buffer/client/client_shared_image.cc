// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/client_shared_image.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2extchromium.h>

#include <optional>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/process_memory_dump.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"

namespace gpu {

namespace {

class ScopedMappingSharedMemoryMapping
    : public ClientSharedImage::ScopedMapping {
 public:
  ScopedMappingSharedMemoryMapping(SharedImageMetadata metadata,
                                   base::WritableSharedMemoryMapping* mapping)
      : metadata_(metadata), mapping_(mapping) {}
  ~ScopedMappingSharedMemoryMapping() override = default;

  // ClientSharedImage::ScopedMapping:
  base::span<uint8_t> GetMemoryForPlane(const uint32_t plane_index) override {
    CHECK(mapping_->IsValid());
    CHECK_LT(plane_index, gfx::NumberOfPlanesForLinearBufferFormat(Format()));

    size_t height_in_pixels;
    CHECK(gfx::PlaneHeightForBufferFormatChecked(
        Size().height(), Format(), plane_index, &height_in_pixels));
    size_t span_length = Stride(plane_index) * height_in_pixels;

    // SAFETY: The validity of the mapping combined with the construction of
    // that mapping guarantee that it contains at least `span_length` bytes
    // beyond the start of the plane.
    return UNSAFE_BUFFERS(base::span<uint8_t>(
        static_cast<uint8_t*>(mapping_->memory()) +
            gfx::BufferOffsetForBufferFormat(Size(), Format(), plane_index),
        span_length));
  }
  size_t Stride(const uint32_t plane_index) override {
    CHECK_LT(plane_index, gfx::NumberOfPlanesForLinearBufferFormat(Format()));
    return gfx::RowSizeForBufferFormat(Size().width(), Format(), plane_index);
  }
  gfx::Size Size() override { return metadata_.size; }
  gfx::BufferFormat Format() override {
    return viz::SinglePlaneSharedImageFormatToBufferFormat(metadata_.format);
  }
  bool IsSharedMemory() override { return true; }

 private:
  SharedImageMetadata metadata_;
  raw_ptr<base::WritableSharedMemoryMapping> mapping_;
};

class ScopedMappingGpuMemoryBuffer : public ClientSharedImage::ScopedMapping {
 public:
  ScopedMappingGpuMemoryBuffer() = default;
  ~ScopedMappingGpuMemoryBuffer() override {
    if (buffer_) {
      buffer_->Unmap();
    }
  }

  // ClientSharedImage::ScopedMapping:
  base::span<uint8_t> GetMemoryForPlane(const uint32_t plane_index) override {
    CHECK(buffer_);

    size_t height_in_pixels;
    size_t row_size_in_bytes;

    CHECK(gfx::PlaneHeightForBufferFormatChecked(
        Size().height(), Format(), plane_index, &height_in_pixels));
    CHECK(gfx::RowSizeForBufferFormatChecked(Size().width(), Format(),
                                             plane_index, &row_size_in_bytes));

    // Note that the stride might be larger than the row size due to padding.
    // For all rows other than the last, this is legal data for the client to
    // access as it's part of the buffer.  However, the final row is not
    // guaranteed to have padding (it's a system-dependent internal detail).
    // Thus, the data that is legal for the client to access should *not*
    // include any bytes beyond the actual end of the final row.
    size_t span_length =
        Stride(plane_index) * (height_in_pixels - 1) + row_size_in_bytes;

    // SAFETY: The underlying platform-specific buffer generation mechanisms
    // guarantee that the buffer contains at least `span_length` bytes following
    // the start of the plane, as that region is by definition the memory
    // storing the data of the plane.
    return UNSAFE_BUFFERS(base::span<uint8_t>(
        reinterpret_cast<uint8_t*>(buffer_->memory(plane_index)), span_length));
  }
  size_t Stride(const uint32_t plane_index) override {
    CHECK(buffer_);
    return buffer_->stride(plane_index);
  }
  gfx::Size Size() override {
    CHECK(buffer_);
    return buffer_->GetSize();
  }
  gfx::BufferFormat Format() override {
    CHECK(buffer_);
    return buffer_->GetFormat();
  }
  bool IsSharedMemory() override {
    CHECK(buffer_);
    return buffer_->GetType() == gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER;
  }
  bool Init(gfx::GpuMemoryBuffer* gpu_memory_buffer, bool is_already_mapped) {
    if (!gpu_memory_buffer) {
      LOG(ERROR) << "No GpuMemoryBuffer.";
      return false;
    }

    if (!is_already_mapped && !gpu_memory_buffer->Map()) {
      LOG(ERROR) << "Failed to map the buffer.";
      return false;
    }
    buffer_ = gpu_memory_buffer;
    return true;
  }

 private:
  // ScopedMappingGpuMemoryBuffer is essentially a wrapper around
  // GpuMemoryBuffer for now for simplicity and will be removed later.
  // TODO(crbug.com/40279377): Refactor/Rename GpuMemoryBuffer and its
  // implementations  as the end goal after all clients using GMB are
  // converted to use the ScopedMapping and notion of GpuMemoryBuffer is being
  // removed.
  // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of MotionMark).
  RAW_PTR_EXCLUSION gfx::GpuMemoryBuffer* buffer_ = nullptr;
};

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

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_OZONE)
  return GL_TEXTURE_2D;
#elif BUILDFLAG(IS_MAC)
  // Check for IOSurfaces being used.
  // NOTE: WebGPU usage on Mac results in SharedImages being backed by
  // IOSurfaces.
  gpu::SharedImageUsageSet usages_requiring_native_buffer =
      SHARED_IMAGE_USAGE_SCANOUT | SHARED_IMAGE_USAGE_WEBGPU_READ |
      SHARED_IMAGE_USAGE_WEBGPU_WRITE;

  bool uses_native_buffer =
      GMBIsNative(client_gmb_type) ||
      metadata.usage.HasAny(usages_requiring_native_buffer);

  return uses_native_buffer
             ? sii->GetCapabilities().texture_target_for_io_surfaces
             : GL_TEXTURE_2D;
#else  // Ozone
  // Check for external sampling being used.
  if (!metadata.format.PrefersExternalSampler()) {
    return GL_TEXTURE_2D;
  }

  // The client should configure an SI to use external sampling only if they
  // have provided a native buffer to back that SI.
  CHECK(GMBIsNative(client_gmb_type));

  // See the note at the top of this function wrt Fuchsia.
#if BUILDFLAG(IS_FUCHSIA)
  return 0;
#else
  return GL_TEXTURE_EXTERNAL_OES;
#endif  // BUILDFLAG(IS_FUCHSIA)
#endif  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_OZONE)
}

}  // namespace

// static
std::unique_ptr<ClientSharedImage::ScopedMapping>
ClientSharedImage::ScopedMapping::Create(
    SharedImageMetadata metadata,
    base::WritableSharedMemoryMapping* mapping) {
  return std::make_unique<ScopedMappingSharedMemoryMapping>(metadata, mapping);
}

// static
std::unique_ptr<ClientSharedImage::ScopedMapping>
ClientSharedImage::ScopedMapping::Create(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    bool is_already_mapped) {
  auto scoped_mapping = base::WrapUnique(new ScopedMappingGpuMemoryBuffer());
  if (!scoped_mapping->Init(gpu_memory_buffer, is_already_mapped)) {
    LOG(ERROR) << "ScopedMapping init failed.";
    return nullptr;
  }
  return scoped_mapping;
}

// static
void ClientSharedImage::ScopedMapping::StartCreateAsync(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    base::OnceCallback<void(std::unique_ptr<ScopedMapping>)> result_cb) {
  gpu_memory_buffer->MapAsync(
      base::BindOnce(&ClientSharedImage::ScopedMapping::FinishCreateAsync,
                     gpu_memory_buffer, std::move(result_cb)));
}

// static
void ClientSharedImage::ScopedMapping::FinishCreateAsync(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    base::OnceCallback<void(std::unique_ptr<ScopedMapping>)> result_cb,
    bool success) {
  std::unique_ptr<ClientSharedImage::ScopedMapping> mapping;
  if (success) {
    mapping = ClientSharedImage::ScopedMapping::Create(
        gpu_memory_buffer, /*is_already_mapped=*/true);
  }
  std::move(result_cb).Run(std::move(mapping));
}

SkPixmap ClientSharedImage::ScopedMapping::GetSkPixmapForPlane(
    const uint32_t plane_index,
    SkImageInfo sk_image_info) {
  return SkPixmap(sk_image_info, GetMemoryForPlane(plane_index).data(),
                  Stride(plane_index));
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
    base::WritableSharedMemoryMapping mapping)
    : ClientSharedImage(mailbox,
                        metadata,
                        sync_token,
                        sii_holder,
                        gfx::SHARED_MEMORY_BUFFER) {
  shared_memory_mapping_ = std::move(mapping);
  is_software_ = true;
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
  // TODO(crbug.com/391788839): Create GpuMemoryBuffer from handle.
  CHECK(!mailbox.IsZero());
  CHECK(sii_holder_);
#if !BUILDFLAG(IS_FUCHSIA)
  CHECK(texture_target);
#endif
}

ClientSharedImage::ClientSharedImage(
    ExportedSharedImage exported_si,
    scoped_refptr<SharedImageInterfaceHolder> sii_holder)
    : mailbox_(exported_si.mailbox_),
      metadata_(exported_si.metadata_),
      creation_sync_token_(exported_si.creation_sync_token_),
      buffer_usage_(exported_si.buffer_usage_),
      sii_holder_(std::move(sii_holder)),
      texture_target_(exported_si.texture_target_) {
  if (exported_si.buffer_handle_) {
#if BUILDFLAG(IS_WIN)
    gpu_memory_buffer_manager_ =
        std::make_unique<HelperGpuMemoryBufferManager>(this);
#else
    gpu_memory_buffer_manager_ = nullptr;
#endif
    gpu_memory_buffer_ =
        GpuMemoryBufferSupport().CreateGpuMemoryBufferImplFromHandle(
            std::move(exported_si.buffer_handle_.value()), metadata_.size,
            viz::SharedImageFormatToBufferFormatRestrictedUtils::ToBufferFormat(
                metadata_.format),
            exported_si.buffer_usage_.value(), base::DoNothing(),
            gpu_memory_buffer_manager_.get());
  }
  CHECK(!mailbox_.IsZero());
  CHECK(sii_holder_);
#if !BUILDFLAG(IS_FUCHSIA)
  CHECK(texture_target_);
#endif
}

ClientSharedImage::ClientSharedImage(ExportedSharedImage exported_si)
    : mailbox_(exported_si.mailbox_),
      metadata_(exported_si.metadata_),
      creation_sync_token_(exported_si.creation_sync_token_),
      buffer_usage_(exported_si.buffer_usage_),
      texture_target_(exported_si.texture_target_) {
  if (exported_si.buffer_handle_) {
#if BUILDFLAG(IS_WIN)
    gpu_memory_buffer_manager_ =
        std::make_unique<HelperGpuMemoryBufferManager>(this);
#else
    gpu_memory_buffer_manager_ = nullptr;
#endif
    gpu_memory_buffer_ =
        GpuMemoryBufferSupport().CreateGpuMemoryBufferImplFromHandle(
            std::move(exported_si.buffer_handle_.value()), metadata_.size,
            viz::SharedImageFormatToBufferFormatRestrictedUtils::ToBufferFormat(
                metadata_.format),
            exported_si.buffer_usage_.value(), base::DoNothing(),
            gpu_memory_buffer_manager_.get());
  }
  CHECK(!mailbox_.IsZero());
#if !BUILDFLAG(IS_FUCHSIA)
  CHECK(texture_target_);
#endif
}

ClientSharedImage::ClientSharedImage(
    const Mailbox& mailbox,
    const SharedImageMetadata& metadata,
    const SyncToken& sync_token,
    GpuMemoryBufferHandleInfo handle_info,
    scoped_refptr<SharedImageInterfaceHolder> sii_holder,
    scoped_refptr<base::UnsafeSharedMemoryPool> shared_memory_pool)
    : mailbox_(mailbox),
      metadata_(metadata),
      creation_sync_token_(sync_token),
      gpu_memory_buffer_manager_(
#if BUILDFLAG(IS_WIN)
          std::make_unique<HelperGpuMemoryBufferManager>(this)
#else
          nullptr
#endif
              ),
      gpu_memory_buffer_(
          GpuMemoryBufferSupport().CreateGpuMemoryBufferImplFromHandle(
              std::move(handle_info.handle),
              handle_info.size,
              viz::SharedImageFormatToBufferFormatRestrictedUtils::
                  ToBufferFormat(handle_info.format),
              handle_info.buffer_usage,
              base::DoNothing(),
              gpu_memory_buffer_manager_.get(),
              std::move(shared_memory_pool))),
      buffer_usage_(handle_info.buffer_usage),
      sii_holder_(std::move(sii_holder)) {
  CHECK(!mailbox.IsZero());
  CHECK(sii_holder_);
  CHECK(gpu_memory_buffer_);
  texture_target_ = ComputeTextureTargetForSharedImage(
      metadata_, gpu_memory_buffer_->GetType(), sii_holder_->Get());
}

ClientSharedImage::~ClientSharedImage() {
  if (!HasHolder()) {
    return;
  }

  auto sii = sii_holder_->Get();
  if (sii) {
    sii->DestroySharedImage(destruction_sync_token_, mailbox_);
  }
}

std::unique_ptr<ClientSharedImage::ScopedMapping> ClientSharedImage::Map() {
  std::unique_ptr<ClientSharedImage::ScopedMapping> scoped_mapping;
  if (shared_memory_mapping_.IsValid()) {
    scoped_mapping = ScopedMapping::Create(metadata_, &shared_memory_mapping_);
  } else {
    scoped_mapping = ScopedMapping::Create(gpu_memory_buffer_.get(),
                                           /*is_already_mapped=*/false);
  }

  if (!scoped_mapping) {
    LOG(ERROR) << "Unable to create ScopedMapping";
  }
  return scoped_mapping;
}

void ClientSharedImage::MapAsync(
    base::OnceCallback<void(std::unique_ptr<ScopedMapping>)> result_cb) {
  ScopedMapping::StartCreateAsync(gpu_memory_buffer_.get(),
                                  std::move(result_cb));
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

scoped_refptr<ClientSharedImage> ClientSharedImage::MakeUnowned() {
  return ClientSharedImage::ImportUnowned(Export());
}

ExportedSharedImage ClientSharedImage::Export(bool with_buffer_handle) {
  if (creation_sync_token_.HasData() &&
      !creation_sync_token_.verified_flush()) {
    sii_holder_->Get()->VerifySyncToken(creation_sync_token_);
  }
  std::optional<gfx::GpuMemoryBufferHandle> buffer_handle;
  std::optional<gfx::BufferUsage> buffer_usage;
  if (with_buffer_handle && gpu_memory_buffer_) {
    buffer_handle = gpu_memory_buffer_->CloneHandle();
    buffer_usage = buffer_usage_.value();
  }
  return ExportedSharedImage(mailbox_, metadata_, creation_sync_token_,
                             std::move(buffer_handle), buffer_usage,
                             texture_target_);
}

scoped_refptr<ClientSharedImage> ClientSharedImage::ImportUnowned(
    ExportedSharedImage exported_shared_image) {
  return base::WrapRefCounted<ClientSharedImage>(
      new ClientSharedImage(std::move(exported_shared_image)));
}

gpu::SyncToken ClientSharedImage::BackingWasExternallyUpdated(
    const gpu::SyncToken& sync_token) {
  CHECK(sii_holder_);
  auto sii = sii_holder_->Get();
  if (!sii) {
    return gpu::SyncToken();
  }

  sii->UpdateSharedImage(sync_token, mailbox());
  return sii->GenUnverifiedSyncToken();
}

void ClientSharedImage::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
    int importance) {
  auto tracing_guid = GetGUIDForTracing();
  pmd->CreateSharedGlobalAllocatorDump(tracing_guid);
  pmd->AddOwnershipEdge(buffer_dump_guid, tracing_guid, importance);
}

void ClientSharedImage::BeginAccess(bool readonly) {
  if (readonly) {
    CHECK(!has_writer_ ||
          usage().Has(SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE));
    num_readers_++;
  } else {
    CHECK(!has_writer_);
    CHECK(num_readers_ == 0 ||
          usage().Has(SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE));
    has_writer_ = true;
  }
}

void ClientSharedImage::EndAccess(bool readonly) {
  if (readonly) {
    CHECK(num_readers_ > 0);
    num_readers_--;
  } else {
    CHECK(has_writer_);
    has_writer_ = false;
  }
}

std::unique_ptr<SharedImageTexture> ClientSharedImage::CreateGLTexture(
    gles2::GLES2Interface* gl) {
  return base::WrapUnique(new SharedImageTexture(gl, this));
}

std::unique_ptr<RasterScopedAccess> ClientSharedImage::BeginRasterAccess(
    InterfaceBase* raster_interface,
    const SyncToken& sync_token,
    bool readonly) {
  return base::WrapUnique(
      new RasterScopedAccess(raster_interface, this, sync_token, readonly));
}

#if BUILDFLAG(IS_WIN)
void ClientSharedImage::SetUsePreMappedMemory(bool use_premapped_memory) {
  CHECK(gpu_memory_buffer_);
  gpu_memory_buffer_->SetUsePreMappedMemory(use_premapped_memory);
}
#endif

// static
scoped_refptr<ClientSharedImage> ClientSharedImage::CreateForTesting() {
  return CreateForTesting(viz::SinglePlaneFormat::kRGBA_8888, GL_TEXTURE_2D);
}

// static
scoped_refptr<ClientSharedImage> ClientSharedImage::CreateForTesting(
    viz::SharedImageFormat format,
    uint32_t texture_target) {
  SharedImageMetadata metadata;
  metadata.format = format;
  metadata.color_space = gfx::ColorSpace::CreateSRGB();
  metadata.surface_origin = kTopLeft_GrSurfaceOrigin;
  metadata.alpha_type = kOpaque_SkAlphaType;
  metadata.usage = gpu::SharedImageUsageSet();

  return CreateForTesting(metadata, texture_target);
}

// static
scoped_refptr<ClientSharedImage> ClientSharedImage::CreateForTesting(
    SharedImageUsageSet usage) {
  SharedImageMetadata metadata;
  metadata.format = viz::SinglePlaneFormat::kRGBA_8888;
  metadata.color_space = gfx::ColorSpace::CreateSRGB();
  metadata.surface_origin = kTopLeft_GrSurfaceOrigin;
  metadata.alpha_type = kOpaque_SkAlphaType;
  metadata.usage = usage;

  return CreateForTesting(metadata, GL_TEXTURE_2D);
}

// static
scoped_refptr<ClientSharedImage> ClientSharedImage::CreateForTesting(
    const SharedImageMetadata& metadata,
    uint32_t texture_target) {
  return ImportUnowned(ExportedSharedImage(Mailbox::Generate(), metadata,
                                           SyncToken(), std::nullopt,
                                           std::nullopt, texture_target));
}

ClientSharedImage::HelperGpuMemoryBufferManager::HelperGpuMemoryBufferManager(
    ClientSharedImage* client_shared_image)
    : client_shared_image_(client_shared_image) {
  CHECK(client_shared_image_);
}

ClientSharedImage::HelperGpuMemoryBufferManager::
    ~HelperGpuMemoryBufferManager() = default;

std::unique_ptr<gfx::GpuMemoryBuffer>
ClientSharedImage::HelperGpuMemoryBufferManager::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle,
    base::WaitableEvent* shutdown_event) {
  NOTREACHED();
}

void ClientSharedImage::HelperGpuMemoryBufferManager::CopyGpuMemoryBufferAsync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion memory_region,
    base::OnceCallback<void(bool)> callback) {
  // Lazily create the |task_runner_|.
  if (!task_runner_) {
    task_runner_ =
        base::ThreadPool::CreateSingleThreadTaskRunner({base::MayBlock()});
    CHECK(*task_runner_);
  }

  if (!(*task_runner_)->BelongsToCurrentThread()) {
    (*task_runner_)
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&ClientSharedImage::HelperGpuMemoryBufferManager::
                               CopyGpuMemoryBufferAsync,
                           base::Unretained(this), std::move(buffer_handle),
                           std::move(memory_region), std::move(callback)));
    return;
  }

  auto sii = GetSharedImageInterface();
  if (!sii) {
    DLOG(WARNING) << "No SharedImageInterface.";
    std::move(callback).Run(false);
    return;
  }
  sii->CopyNativeGmbToSharedMemoryAsync(
      std::move(buffer_handle), std::move(memory_region),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                  /*result=*/false));
}

bool ClientSharedImage::HelperGpuMemoryBufferManager::IsConnected() {
  auto sii = GetSharedImageInterface();
  if (!sii) {
    DLOG(WARNING) << "No SharedImageInterface.";
    return false;
  }
  return sii->IsConnected();
}

// Access the SharedImageInterface via the SharedImageInterfaceHolder.
scoped_refptr<SharedImageInterface>
ClientSharedImage::HelperGpuMemoryBufferManager::GetSharedImageInterface() {
  return client_shared_image_->sii_holder_->Get();
}

ExportedSharedImage::ExportedSharedImage() = default;
ExportedSharedImage::~ExportedSharedImage() = default;

ExportedSharedImage::ExportedSharedImage(ExportedSharedImage&& other) = default;
ExportedSharedImage& ExportedSharedImage::operator=(
    ExportedSharedImage&& other) = default;

ExportedSharedImage::ExportedSharedImage(
    const Mailbox& mailbox,
    const SharedImageMetadata& metadata,
    const SyncToken& sync_token,
    std::optional<gfx::GpuMemoryBufferHandle> buffer_handle,
    std::optional<gfx::BufferUsage> buffer_usage,
    uint32_t texture_target)
    : mailbox_(mailbox),
      metadata_(metadata),
      creation_sync_token_(sync_token),
      buffer_handle_(std::move(buffer_handle)),
      buffer_usage_(buffer_usage),
      texture_target_(texture_target) {}

ExportedSharedImage ExportedSharedImage::Clone() const {
  std::optional<gfx::GpuMemoryBufferHandle> handle = std::nullopt;
  if (buffer_handle_.has_value()) {
    handle = buffer_handle_->Clone();
  }
  return ExportedSharedImage(mailbox_, metadata_, creation_sync_token_,
                             std::move(handle), buffer_usage_, texture_target_);
}

SharedImageTexture::ScopedAccess::ScopedAccess(SharedImageTexture* texture,
                                               const SyncToken& sync_token,
                                               bool readonly)
    : texture_(texture), readonly_(readonly) {
  texture_->gl_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  texture_->gl_->BeginSharedImageAccessDirectCHROMIUM(
      texture->id(), (readonly_)
                         ? GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM
                         : GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
}

SharedImageTexture::ScopedAccess::~ScopedAccess() {
  CHECK(is_access_ended_);
}

void SharedImageTexture::ScopedAccess::DidEndAccess() {
  is_access_ended_ = true;
  texture_->DidEndAccess(readonly_);
}

// static
SyncToken SharedImageTexture::ScopedAccess::EndAccess(
    std::unique_ptr<SharedImageTexture::ScopedAccess> scoped_shared_image) {
  gles2::GLES2Interface* gl = scoped_shared_image->texture_->gl_;
  gl->EndSharedImageAccessDirectCHROMIUM(scoped_shared_image->texture_->id());
  scoped_shared_image->DidEndAccess();
  SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  return sync_token;
}

SharedImageTexture::SharedImageTexture(gles2::GLES2Interface* gl,
                                       ClientSharedImage* shared_image)
    : gl_(gl), shared_image_(shared_image) {
  CHECK(gl_);
  CHECK(shared_image_);
  gl_->WaitSyncTokenCHROMIUM(
      shared_image_->creation_sync_token().GetConstData());
  id_ = gl_->CreateAndTexStorage2DSharedImageCHROMIUM(
      shared_image_->mailbox().name);
}

SharedImageTexture::~SharedImageTexture() {
  CHECK(!has_active_access_);
  gl_->DeleteTextures(1, &id_);
}

std::unique_ptr<SharedImageTexture::ScopedAccess>
SharedImageTexture::BeginAccess(const SyncToken& sync_token, bool readonly) {
  CHECK(!has_active_access_);
  has_active_access_ = true;
  shared_image_->BeginAccess(readonly);
  return base::WrapUnique(
      new SharedImageTexture::ScopedAccess(this, sync_token, readonly));
}

void SharedImageTexture::DidEndAccess(bool readonly) {
  has_active_access_ = false;
  shared_image_->EndAccess(readonly);
}

RasterScopedAccess::RasterScopedAccess(InterfaceBase* raster_interface,
                                       ClientSharedImage* shared_image,
                                       const SyncToken& sync_token,
                                       bool readonly)
    : raster_interface_(raster_interface),
      shared_image_(shared_image),
      readonly_(readonly) {
  CHECK(raster_interface_);
  shared_image_->BeginAccess(readonly);
  raster_interface_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
}

// static
SyncToken RasterScopedAccess::EndAccess(
    std::unique_ptr<RasterScopedAccess> scoped_access) {
  InterfaceBase* raster_interface = scoped_access->raster_interface_;
  SyncToken sync_token;
  scoped_access->shared_image_->EndAccess(scoped_access->readonly_);
  raster_interface->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  return sync_token;
}

}  // namespace gpu
