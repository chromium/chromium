// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CLIENT_SHARED_IMAGE_H_
#define GPU_COMMAND_BUFFER_CLIENT_CLIENT_SHARED_IMAGE_H_

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_pool.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/exported_shared_image.mojom-shared.h"
#include "gpu/ipc/common/gpu_memory_buffer_handle_info.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {
class VideoFrame;
}  // namespace media

namespace gpu {

namespace gles2 {
class GLES2Interface;
}  // namespace gles2

class ClientSharedImageInterface;
class GpuChannelSharedImageInterface;
class SharedImageTexture;
class TestSharedImageInterface;

struct ExportedSharedImage;

class GPU_EXPORT ClientSharedImage
    : public base::RefCountedThreadSafe<ClientSharedImage> {
 public:
  // Provides access to the CPU visible memory for the SharedImage if it is
  // being used for CPU READ/WRITE and underlying resource(native buffers/shared
  // memory) is CPU mappable. Memory and strides can be requested for each
  // plane.
  class GPU_EXPORT ScopedMapping {
   public:
    ~ScopedMapping();

    // Returns a pointer to the beginning of the plane.
    void* Memory(const uint32_t plane_index);

    base::span<uint8_t> GetMemoryForPlane(const uint32_t plane_index);

    SkPixmap GetSkPixmapForPlane(const uint32_t plane_index,
                                 SkImageInfo sk_image_info);

    // Returns plane stride.
    size_t Stride(const uint32_t plane_index);

    // Returns the size of the buffer.
    gfx::Size Size();

    // Returns BufferFormat.
    gfx::BufferFormat Format();

    // Returns whether the underlying resource is shared memory.
    bool IsSharedMemory();

    // Dumps information about the memory backing this instance to |pmd|.
    // The memory usage is attributed to |buffer_dump_guid|.
    // |tracing_process_id| uniquely identifies the process owning the memory.
    // |importance| is relevant only for the cases of co-ownership, the memory
    // gets attributed to the owner with the highest importance.
    void OnMemoryDump(
        base::trace_event::ProcessMemoryDump* pmd,
        const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
        uint64_t tracing_process_id,
        int importance);

   private:
    friend class ClientSharedImage;

    ScopedMapping();
    static std::unique_ptr<ScopedMapping> Create(
        gfx::GpuMemoryBuffer* gpu_memory_buffer,
        bool is_already_mapped);
    static void StartCreateAsync(
        gfx::GpuMemoryBuffer* gpu_memory_buffer,
        base::OnceCallback<void(std::unique_ptr<ScopedMapping>)> result_cb);
    static void FinishCreateAsync(
        gfx::GpuMemoryBuffer* gpu_memory_buffer,
        base::OnceCallback<void(std::unique_ptr<ScopedMapping>)> result_cb,
        bool success);

    bool Init(gfx::GpuMemoryBuffer* gpu_memory_buffer, bool is_already_mapped);

    // ScopedMapping is essentially a wrapper around GpuMemoryBuffer for now for
    // simplicity and will be removed later.
    // TODO(crbug.com/40279377): Refactor/Rename GpuMemoryBuffer and its
    // implementations  as the end goal after all clients using GMB are
    // converted to use the ScopedMapping and notion of GpuMemoryBuffer is being
    // removed.
    // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of MotionMark).
    RAW_PTR_EXCLUSION gfx::GpuMemoryBuffer* buffer_ = nullptr;
  };

  // `sii_holder` must not be null.
  ClientSharedImage(const Mailbox& mailbox,
                    const SharedImageMetadata& metadata,
                    const SyncToken& sync_token,
                    scoped_refptr<SharedImageInterfaceHolder> sii_holder,
                    gfx::GpuMemoryBufferType gmb_type);

  // `sii_holder` must not be null. |shared_memory_pool| can be null and is only
  // used on windows platform.
  ClientSharedImage(
      const Mailbox& mailbox,
      const SharedImageMetadata& metadata,
      const SyncToken& sync_token,
      GpuMemoryBufferHandleInfo handle_info,
      scoped_refptr<SharedImageInterfaceHolder> sii_holder,
      scoped_refptr<base::UnsafeSharedMemoryPool> shared_memory_pool = nullptr);

  const Mailbox& mailbox() { return mailbox_; }
  viz::SharedImageFormat format() const { return metadata_.format; }
  gfx::Size size() const { return metadata_.size; }
  gfx::ColorSpace color_space() const { return metadata_.color_space; }
  GrSurfaceOrigin surface_origin() const { return metadata_.surface_origin; }
  SkAlphaType alpha_type() const { return metadata_.alpha_type; }
  SharedImageUsageSet usage() { return metadata_.usage; }

  bool HasHolder() { return sii_holder_ != nullptr; }

  // Returns a clone of the GpuMemoryBufferHandle associated with this ClientSI.
  // Valid to call only if this instance was created with a non-null
  // GpuMemoryBuffer.
  gfx::GpuMemoryBufferHandle CloneGpuMemoryBufferHandle() const {
    CHECK(gpu_memory_buffer_);
    return gpu_memory_buffer_->CloneHandle();
  }

#if BUILDFLAG(IS_APPLE)
  // Sets the color space in which the native buffer backing this SharedImage
  // should be interpreted when used as an overlay. Note that this will not
  // impact texturing from the buffer. Used only for SharedImages backed by a
  // client-accessible IOSurface.
  void SetColorSpaceOnNativeBuffer(const gfx::ColorSpace& color_space);
#endif

  // Returns the GL texture target to use for this SharedImage.
  uint32_t GetTextureTarget();

  base::trace_event::MemoryAllocatorDumpGuid GetGUIDForTracing() {
    return gpu::GetSharedImageGUIDForTracing(mailbox_);
  }

  // Maps |mailbox| into CPU visible memory and returns a ScopedMapping object
  // which can be used to read/write to the CPU mapped memory. The SharedImage
  // backing this ClientSI must have been created with CPU_READ/CPU_WRITE usage.
  std::unique_ptr<ScopedMapping> Map();

  // Maps |mailbox| into CPU visible memory and returns a ScopedMapping object
  // which can be used to read/write to the CPU mapped memory. The SharedImage
  // backing this ClientSI must have been created with CPU_READ/CPU_WRITE usage.
  // Default implementation is blocking. However, on some platforms, where
  // possible, the implementation is non-blocking and may execute the callback
  // on the GpuMemoryThread. But if no GPU work is necessary, it still may
  // execute the callback immediately in the current sequence. Note: `this` must
  // be kept alive until the result callback is executed.
  void MapAsync(
      base::OnceCallback<void(std::unique_ptr<ScopedMapping>)> result_cb);

  // Returns an unowned copy of the current ClientSharedImage. This function
  // is a temporary workaround for the situation where a ClientSharedImage may
  // have more than one reference when being destroyed.
  // TODO(crbug.com/40286368): Remove this function once ClientSharedImage
  // can properly handle shared image destruction internally.
  scoped_refptr<ClientSharedImage> MakeUnowned();

  ExportedSharedImage Export();

  // Returns an unowned reference. The caller should ensure that the original
  // shared image outlives this reference. Note that it is preferable to use
  // SharedImageInterface::ImportSharedImage() instead, which returns an owning
  // reference.
  static scoped_refptr<ClientSharedImage> ImportUnowned(
      const ExportedSharedImage& exported_shared_image);

  void UpdateDestructionSyncToken(const gpu::SyncToken& sync_token) {
    destruction_sync_token_ = sync_token;
  }

  // Creates a ClientSharedImage that is not associated with any
  // SharedImageInterface for testing.
  static scoped_refptr<ClientSharedImage> CreateForTesting();
  static scoped_refptr<ClientSharedImage> CreateForTesting(
      uint32_t texture_target);

  static scoped_refptr<ClientSharedImage> CreateForTesting(
      const Mailbox& mailbox,
      const SharedImageMetadata& metadata,
      const SyncToken& sync_token,
      std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
      scoped_refptr<SharedImageInterfaceHolder> sii_holder) {
    auto client_si = base::MakeRefCounted<ClientSharedImage>(
        mailbox, metadata, sync_token, sii_holder,
        gpu_memory_buffer->GetType());
    client_si->gpu_memory_buffer_ = std::move(gpu_memory_buffer);
    return client_si;
  }

  const SyncToken& creation_sync_token() const { return creation_sync_token_; }

  // Note that this adds an ownership edge to mailbox using mailbox as id.
  // ScopedMapping::OnMemoryDump() uses underlying GpuMemoryBuffer's Id as
  // ownership edge which is broken since GMB inside mappableSI doesn't have
  // unique ids anymore. ScopedMapping::OnMemoryDump() should be removed and
  // replaced with this method.
  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      int importance);

  // Creates a GL Texture from the current SharedImage for the provided
  // GLES2Interface.
  std::unique_ptr<SharedImageTexture> CreateGLTexture(
      gles2::GLES2Interface* gl);

 private:
  friend class base::RefCountedThreadSafe<ClientSharedImage>;
  friend class SharedImageTexture;
  ~ClientSharedImage();

  // Helper class that implements the GpuMemoryBufferManager interface.
  // Note that this is primarily needed for transition to MappableSI where some
  // clients will be using GpuMemoryBufferManager and some will want to use SII
  // instead.
  // TODO(crbug.com/368562234): Once all the clients and tests using
  // GpuMemoryBufferManager are converted to use MappableSI,
  // GpuMemoryBufferManager and all  its implementations might be removed
  // including this.
  class HelperGpuMemoryBufferManager : public gpu::GpuMemoryBufferManager {
   public:
    explicit HelperGpuMemoryBufferManager(
        ClientSharedImage* client_shared_image);

    // GpuMemoryBufferManager interface implementation.
    // This method should not be used via this interface. Hence marking it as
    // NOTREACHED.
    std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
        const gfx::Size& size,
        gfx::BufferFormat format,
        gfx::BufferUsage usage,
        gpu::SurfaceHandle surface_handle,
        base::WaitableEvent* shutdown_event) final;

    void CopyGpuMemoryBufferAsync(
        gfx::GpuMemoryBufferHandle buffer_handle,
        base::UnsafeSharedMemoryRegion memory_region,
        base::OnceCallback<void(bool)> callback) final;

    bool CopyGpuMemoryBufferSync(
        gfx::GpuMemoryBufferHandle buffer_handle,
        base::UnsafeSharedMemoryRegion memory_region) final;

   private:
    // Points to the parent ClientSharedImage. It will be used to access SII via
    // SII holder.
    raw_ptr<ClientSharedImage> client_shared_image_;

    // Allows accessing SharedImageInterface from ClientSharedImage.
    scoped_refptr<SharedImageInterface> GetSharedImageInterface();
  };

  // This constructor is used only when importing an owned ClientSharedImage,
  // which should only be done via implementations of
  // SharedImageInterface::ImportSharedImage().
  // `sii_holder` must not be null.
  friend class ClientSharedImageInterface;
  friend class GpuChannelSharedImageInterface;
  friend class TestSharedImageInterface;
  friend class media::VideoFrame;
  ClientSharedImage(const Mailbox& mailbox,
                    const SharedImageMetadata& metadata,
                    const SyncToken& sync_token,
                    scoped_refptr<SharedImageInterfaceHolder> sii_holder,
                    uint32_t texture_target);

  // This constructor is used only when importing an unowned ClientSharedImage,
  // in which case this ClientSharedImage is not associated with a
  // SharedImageInterface.
  ClientSharedImage(const Mailbox& mailbox,
                    const SharedImageMetadata& metadata,
                    const SyncToken& sync_token,
                    uint32_t texture_target);

  // VideoFrame needs this info currently for MappableSI.
  // TODO(crbug.com/40263579): Once MappableSI is fully launched for VideoFrame,
  // VF can be refactored to behave like OPAQUE storage which does not need
  // layout info and hence stride. This method will then no longer needed and
  // can be removed.
  size_t GetStrideForVideoFrame(uint32_t plane_index) const {
    CHECK(gpu_memory_buffer_);
    return gpu_memory_buffer_->stride(plane_index);
  }

  // Returns whether the underlying resource is shared memory without needing to
  // Map() the shared image. This method is supposed to be used by VideoFrame
  // temporarily as mentioned above in ::GetStrideForVideoFrame().
  bool IsSharedMemoryForVideoFrame() const {
    CHECK(gpu_memory_buffer_);
    return gpu_memory_buffer_->GetType() ==
           gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER;
  }

  bool AsyncMappingIsNonBlocking() const {
    CHECK(gpu_memory_buffer_);
    return gpu_memory_buffer_->AsyncMappingIsNonBlocking();
  }

  // This pair of functions are used by SharedImageTexture to notify
  // ClientSharedImage of the beginning and the end of a scoped access.
  void BeginAccess(bool readonly);
  void EndAccess(bool readonly);

  const Mailbox mailbox_;
  const SharedImageMetadata metadata_;
  SyncToken creation_sync_token_;
  SyncToken destruction_sync_token_;
  // Helper to hold the instance of GpuMemoryBufferManager.
  std::unique_ptr<HelperGpuMemoryBufferManager> gpu_memory_buffer_manager_;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
  scoped_refptr<SharedImageInterfaceHolder> sii_holder_;

  // The texture target returned by `GetTextureTarget()`.
  uint32_t texture_target_ = 0;

  // The number of active scoped read accesses.
  unsigned int num_readers_ = 0;

  // Whether there exists an active scoped write access.
  bool has_writer_ = false;
};

struct GPU_EXPORT ExportedSharedImage {
 public:
  ExportedSharedImage();

 private:
  friend class ClientSharedImage;
  friend class SharedImageInterface;
  friend class ClientSharedImageInterface;
  friend class TestSharedImageInterface;
  friend struct mojo::StructTraits<gpu::mojom::ExportedSharedImageDataView,
                                   ExportedSharedImage>;
  FRIEND_TEST_ALL_PREFIXES(ClientSharedImageTest, ImportUnowned);

  ExportedSharedImage(const Mailbox& mailbox,
                      const SharedImageMetadata& metadata,
                      const SyncToken& sync_token,
                      uint32_t texture_target);

  Mailbox mailbox_;
  SharedImageMetadata metadata_;
  SyncToken creation_sync_token_;
  uint32_t texture_target_ = 0;
};

class GPU_EXPORT SharedImageTexture {
 public:
  class GPU_EXPORT ScopedAccess {
   public:
    ScopedAccess(const ScopedAccess&) = delete;
    ScopedAccess& operator=(const ScopedAccess&) = delete;
    ScopedAccess(ScopedAccess&&) = delete;
    ScopedAccess& operator=(ScopedAccess&&) = delete;

    ~ScopedAccess();

    unsigned int texture_id() { return texture_->id(); }

    static SyncToken EndAccess(
        std::unique_ptr<ScopedAccess> scoped_shared_image);

   private:
    friend class SharedImageTexture;
    ScopedAccess(SharedImageTexture* texture,
                 const SyncToken& sync_token,
                 bool readonly);
    void DidEndAccess();

    const raw_ptr<SharedImageTexture> texture_;
    const bool readonly_;
    bool is_access_ended_ = false;
  };

  SharedImageTexture(const SharedImageTexture&) = delete;
  SharedImageTexture& operator=(const SharedImageTexture&) = delete;
  SharedImageTexture(SharedImageTexture&&) = delete;
  SharedImageTexture& operator=(SharedImageTexture&&) = delete;

  ~SharedImageTexture();

  std::unique_ptr<ScopedAccess> BeginAccess(const SyncToken& sync_token,
                                            bool readonly);

  void DidEndAccess(bool readonly);
  unsigned int id() { return id_; }

 private:
  friend class ClientSharedImage;
  SharedImageTexture(gles2::GLES2Interface* gl,
                     ClientSharedImage* shared_image);

  const raw_ptr<gles2::GLES2Interface> gl_;
  const raw_ptr<gpu::ClientSharedImage> shared_image_;
  unsigned int id_ = 0;
  bool has_active_access_ = false;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_CLIENT_SHARED_IMAGE_H_
