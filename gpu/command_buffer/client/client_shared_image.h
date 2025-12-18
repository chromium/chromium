// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CLIENT_SHARED_IMAGE_H_
#define GPU_COMMAND_BUFFER_CLIENT_CLIENT_SHARED_IMAGE_H_

#include <optional>

#include "base/byte_size.h"
#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_pool.h"
#include "gpu/command_buffer/client/gpu_command_buffer_client_export.h"
#include "gpu/command_buffer/client/internal/mappable_buffer.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/exported_shared_image.mojom-forward.h"
#include "gpu/ipc/common/gpu_memory_buffer_handle_info.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace base::trace_event {
class ProcessMemoryDump;
class MemoryAllocatorDumpGuid;
}  // namespace base::trace_event

namespace media {
class VideoFrame;
}  // namespace media

namespace viz {
class CopyOutputSharedImageResult;
struct ReturnedResource;
struct ReturnedResourceViz;
}  // namespace viz

namespace wgpu::dawn::wire::client {
class Device;
class Texture;
class Buffer;
struct TextureDescriptor;
struct BufferDescriptor;
}  // namespace wgpu::dawn::wire::client

namespace gpu {

namespace gles2 {
class GLES2Interface;
}  // namespace gles2

namespace webgpu {
class WebGPUInterface;
enum MailboxFlags : uint32_t;
}  // namespace webgpu

class SharedImageInterface;
class ClientSharedImageInterface;
class GpuChannelSharedImageInterface;
class MappableBuffer;
class InterfaceBase;
class RasterScopedAccess;
struct SharedImageInfo;
class SharedImageInterfaceHolder;
class SharedImageTexture;
class TestSharedImageInterface;
class WebGPUTextureScopedAccess;
class WebGPUBufferScopedAccess;

struct ExportedSharedImage;

struct SharedImageMetadata {
  viz::SharedImageFormat format;
  gfx::Size size;
  gfx::ColorSpace color_space;
  GrSurfaceOrigin surface_origin;
  SkAlphaType alpha_type;
  SharedImageUsageSet usage;
};

class GPU_COMMAND_BUFFER_CLIENT_EXPORT SharedImageExportResult {
 public:
  ~SharedImageExportResult() = default;

  // Used in FrameSinkResourceManager to facilitate creating a dummy
  // ReturnedResource for callback clearing.
  static SharedImageExportResult CreateEmptyResult() {
    return SharedImageExportResult(SyncToken());
  }

  static SharedImageExportResult CreateForTesting(const SyncToken& sync_token) {
    return SharedImageExportResult(sync_token);
  }

  // The two IsEqualForTesting methods allow easy SyncToken comparison without
  // unpacking SharedImageExportResult.
  bool IsEqualForTesting(const SyncToken& sync_token) const {
    return sync_token_ == sync_token;
  }

  bool IsEqualForTesting(const SharedImageExportResult& other_result) const {
    return IsEqualForTesting(other_result.sync_token_);
  }

 private:
  friend class ClientSharedImage;

  // Allows ReturnedResource to be default constructed.
  friend struct viz::ReturnedResource;

  // Allows ReturnedResourceViz to convert SyncToken to SharedImageExportResult.
  friend struct viz::ReturnedResourceViz;
  friend struct mojo::StructTraits<gpu::mojom::SharedImageExportResultDataView,
                                   SharedImageExportResult>;

  SharedImageExportResult() = default;
  explicit SharedImageExportResult(const SyncToken& sync_token);

  SyncToken sync_token_;
};

// Wrapper around Mailbox and metadata for efficient sharing between threads
class GPU_COMMAND_BUFFER_CLIENT_EXPORT ClientSharedImage
    : public base::RefCountedThreadSafe<ClientSharedImage> {
 public:
  // Provides access to the CPU visible memory for the SharedImage if it is
  // being used for CPU READ/WRITE and underlying resource(native buffers/shared
  // memory) is CPU mappable. Memory and strides can be requested for each
  // plane.
  class GPU_COMMAND_BUFFER_CLIENT_EXPORT ScopedMapping {
   public:
    ScopedMapping(const gfx::Size& size, viz::SharedImageFormat format);
    ~ScopedMapping();

    base::span<uint8_t> GetMemoryForPlane(const uint32_t plane_index);

    SkPixmap GetSkPixmapForPlane(const uint32_t plane_index,
                                 SkImageInfo sk_image_info);

    // Returns plane stride.
    size_t Stride(const uint32_t plane_index);

    // Returns the size of the buffer.
    gfx::Size Size();

    // Returns whether the underlying resource is shared memory.
    bool IsSharedMemory();

   private:
    friend class ClientSharedImage;

    static std::unique_ptr<ScopedMapping> Create(
        SharedImageMetadata metadata_,
        MappableBuffer* mappable_buffer,
        bool is_already_mapped);
    static void StartCreateAsync(
        SharedImageMetadata metadata_,
        MappableBuffer* mappable_buffer,
        base::OnceCallback<void(std::unique_ptr<ScopedMapping>)> result_cb);
    static void FinishCreateAsync(
        SharedImageMetadata metadata_,
        MappableBuffer* mappable_buffer,
        base::OnceCallback<void(std::unique_ptr<ScopedMapping>)> result_cb,
        bool success);

    bool Init(MappableBuffer* mappable_buffer, bool is_already_mapped);

    // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of MotionMark).
    RAW_PTR_EXCLUSION MappableBuffer* buffer_ = nullptr;
    gfx::Size size_;
    viz::SharedImageFormat format_;
  };

  // `sii_holder` must not be null.
  ClientSharedImage(const Mailbox& mailbox,
                    const SharedImageInfo& info,
                    const SyncToken& sync_token,
                    scoped_refptr<SharedImageInterfaceHolder> sii_holder,
                    gfx::GpuMemoryBufferType gmb_type);

  // `sii_holder` must not be null.
  ClientSharedImage(const Mailbox& mailbox,
                    const SharedImageInfo& info,
                    const SyncToken& sync_token,
                    scoped_refptr<SharedImageInterfaceHolder> sii_holder,
                    base::WritableSharedMemoryMapping mapping);

  // `sii_holder` must not be null. |shared_memory_pool| can be null and is only
  // used on windows platform.
  ClientSharedImage(
      const Mailbox& mailbox,
      const SharedImageInfo& info,
      const SyncToken& sync_token,
      GpuMemoryBufferHandleInfo handle_info,
      scoped_refptr<SharedImageInterfaceHolder> sii_holder,
      scoped_refptr<base::UnsafeSharedMemoryPool> shared_memory_pool = nullptr);

  const Mailbox& mailbox() const { return mailbox_; }
  viz::SharedImageFormat format() const { return metadata_.format; }
  base::ByteSize EstimatedSizeInBytes() const {
    return base::ByteSize(format().EstimatedSizeInBytes(size()));
  }
  gfx::Size size() const { return metadata_.size; }
  const gfx::ColorSpace& color_space() const { return metadata_.color_space; }
  GrSurfaceOrigin surface_origin() const { return metadata_.surface_origin; }
  SkAlphaType alpha_type() const { return metadata_.alpha_type; }
  SharedImageUsageSet usage() const { return metadata_.usage; }
  std::optional<gfx::BufferUsage> buffer_usage() const { return buffer_usage_; }
  std::string debug_label() const { return debug_label_; }
  bool is_software() const { return is_software_; }

  bool HasHolder() { return sii_holder_ != nullptr; }

  // Returns a clone of the GpuMemoryBufferHandle associated with this ClientSI.
  // Valid to call only if this instance was created with a non-null
  // GpuMemoryBuffer.
  gfx::GpuMemoryBufferHandle CloneGpuMemoryBufferHandle() const;

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

  ExportedSharedImage Export(bool with_buffer_handle = false);

  // Returns an unowned reference for the underlying shared image backing. The
  // caller should ensure that the original shared image backing created in
  // client process outlives this reference. Note that it is preferable to use
  // SharedImageInterface::ImportSharedImage() instead, which returns an owning
  // reference, where the underlying shared image backing stays alive in gpu
  // process even if original ClientSharedImage goes away.
  static scoped_refptr<ClientSharedImage> ImportUnowned(
      ExportedSharedImage exported_shared_image);

  void UpdateDestructionSyncToken(const gpu::SyncToken& sync_token) {
    destruction_sync_token_ = sync_token;
  }

  // Signals the service-side that the backing of this SharedImage was modified
  // on the CPU or through external devices. `sync_token` can be passed to order
  // the processing of the signal. Returns a SyncToken that the caller can use
  // to ensure that any future service-side accesses to this SharedImage are
  // sequenced with respect to this call being processed.
  gpu::SyncToken BackingWasExternallyUpdated(const gpu::SyncToken& sync_token);

  // Creates a ClientSharedImage that is not associated with any
  // SharedImageInterface for testing.
  static scoped_refptr<ClientSharedImage> CreateForTesting();
  static scoped_refptr<ClientSharedImage> CreateSoftwareForTesting();
  static scoped_refptr<ClientSharedImage> CreateForTesting(
      viz::SharedImageFormat format,
      uint32_t texture_target);
  static scoped_refptr<ClientSharedImage> CreateForTesting(
      SharedImageUsageSet usage);
  static scoped_refptr<ClientSharedImage> CreateForTesting(
      const SharedImageMetadata& metadata);
  static scoped_refptr<ClientSharedImage> CreateForTesting(
      const SharedImageMetadata& metadata,
      uint32_t texture_target);

  using AsyncMapCompletionCallback = base::OnceCallback<void(bool)>;
  static scoped_refptr<ClientSharedImage> CreateForTesting(
      const Mailbox& mailbox,
      const SharedImageMetadata& metadata,
      const SyncToken& sync_token,
      uint32_t texture_target,
      bool is_software = false);

  // Used to control execution of `MapAsync()` completion callbacks. On a
  // `MapAsync()` invocation the completion callback will be passed to this
  // callback, which can execute it as the test requires.
  using AsyncMapInvokedCallback =
      base::RepeatingCallback<void(AsyncMapCompletionCallback)>;
  static scoped_refptr<ClientSharedImage> CreateForTesting(
      const Mailbox& mailbox,
      const SharedImageMetadata& metadata,
      const SyncToken& sync_token,
      bool premapped,
      const AsyncMapInvokedCallback& callback,
      gfx::BufferUsage buffer_usage,
      scoped_refptr<SharedImageInterfaceHolder> sii_holder);

  const SyncToken& creation_sync_token() const { return creation_sync_token_; }

  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      int importance);

  // Creates a GL Texture from the current SharedImage for the provided
  // GLES2Interface.
  std::unique_ptr<SharedImageTexture> CreateGLTexture(
      gles2::GLES2Interface* gl);

  // Creates a RasterScopedAccess object from the current SharedImage for the
  // provided raster interface.
  std::unique_ptr<RasterScopedAccess> BeginRasterAccess(
      InterfaceBase* raster_interface,
      const SyncToken& sync_token,
      bool readonly);

  // This is used for CopySharedImageToTextureINTERNAL, where we need GL access
  // but do not create a GL texture.
  std::unique_ptr<RasterScopedAccess> BeginGLAccessForCopySharedImage(
      InterfaceBase* gl_interface,
      const SyncToken& sync_token,
      bool readonly);

  std::unique_ptr<WebGPUTextureScopedAccess> BeginWebGPUTextureAccess(
      webgpu::WebGPUInterface* webgpu,
      const SyncToken& sync_token,
      const wgpu::dawn::wire::client::Device& device,
      const wgpu::dawn::wire::client::TextureDescriptor& desc,
      uint64_t usage,
      webgpu::MailboxFlags mailbox_flags);

  std::unique_ptr<WebGPUBufferScopedAccess> BeginWebGPUBufferAccess(
      webgpu::WebGPUInterface* webgpu,
      const SyncToken& sync_token,
      const wgpu::dawn::wire::client::Device& device,
      const wgpu::dawn::wire::client::BufferDescriptor& desc,
      uint64_t usage,
      webgpu::MailboxFlags mailbox_flags);

  // Pack/unpack the SharedImageExportResult.
  SharedImageExportResult EndImport(const SyncToken& sync_token) {
    return SharedImageExportResult{sync_token};
  }

  SyncToken EndExport(SharedImageExportResult&& result) {
    return result.sync_token_;
  }

#if BUILDFLAG(IS_WIN)
  // Allows client to indicate the |gpu_memory_buffer_| to pre map its shared
  // memory region internally for performance optimization purposes. It is only
  // used on windows.
  void SetUsePreMappedMemory(bool use_premapped_memory);
#endif

 private:
  friend class base::RefCountedThreadSafe<ClientSharedImage>;
  friend class SharedImageTexture;
  ~ClientSharedImage();

  // static
  std::unique_ptr<MappableBuffer> CreateMappableBufferFromHandle(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      viz::SharedImageFormat format,
      gfx::BufferUsage usage,
      gpu::SharedImageUsageSet si_usage,
      MappableBuffer::CopyNativeBufferToShMemCallback
          copy_native_buffer_to_shmem_callback =
              MappableBuffer::CopyNativeBufferToShMemCallback(),
      scoped_refptr<base::UnsafeSharedMemoryPool> pool = nullptr);

  // This constructor is used only when importing an owned ClientSharedImage,
  // which should only be done via implementations of
  // SharedImageInterface::ImportSharedImage().
  // `sii_holder` must not be null.
  friend class ClientSharedImageInterface;
  friend class GpuChannelSharedImageInterface;
  friend class RasterScopedAccess;
  friend class TestSharedImageInterface;
  friend class media::VideoFrame;
  friend class WebGPUTextureScopedAccess;
  friend class WebGPUBufferScopedAccess;

  ClientSharedImage(const Mailbox& mailbox,
                    const SharedImageInfo& info,
                    const SyncToken& sync_token,
                    scoped_refptr<SharedImageInterfaceHolder> sii_holder,
                    uint32_t texture_target);

  ClientSharedImage(ExportedSharedImage exported_si,
                    scoped_refptr<SharedImageInterfaceHolder> sii_holder);

  // This constructor is used only when importing an unowned ClientSharedImage,
  // in which case this ClientSharedImage is not associated with a
  // SharedImageInterface.
  explicit ClientSharedImage(ExportedSharedImage exported_si);

  friend class ::viz::CopyOutputSharedImageResult;
  // Creates unowned (no `sii_holder`) `ClientSharedImage`
  explicit ClientSharedImage(const Mailbox& mailbox,
                             const SharedImageInfo& info);

  // VideoFrame needs this info currently for MappableSI.
  // TODO(crbug.com/40263579): Once MappableSI is fully launched for VideoFrame,
  // VF can be refactored to behave like OPAQUE storage which does not need
  // layout info and hence stride. This method will then no longer needed and
  // can be removed.
  size_t GetStrideForVideoFrame(uint32_t plane_index) const;

  // Returns whether the underlying resource is shared memory without needing to
  // Map() the shared image. This method is supposed to be used by VideoFrame
  // temporarily as mentioned above in ::GetStrideForVideoFrame().
  bool IsSharedMemoryForVideoFrame() const;

  bool AsyncMappingIsNonBlocking() const;

  void CopyNativeGmbToSharedMemoryAsync(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion memory_region,
      base::OnceCallback<void(bool)> callback);

  // This pair of functions are used by SharedImageTexture to notify
  // ClientSharedImage of the beginning and the end of a scoped access.
  void BeginAccess(bool readonly);
  void EndAccess(bool readonly);

  void FinishMapAsyncForTests(
      base::OnceCallback<void(std::unique_ptr<ScopedMapping>)> result_cb,
      bool success);

  const Mailbox mailbox_;
  const SharedImageMetadata metadata_;
  const std::string debug_label_;
  SyncToken creation_sync_token_;
  SyncToken destruction_sync_token_;

  std::unique_ptr<MappableBuffer> mappable_buffer_;
  std::optional<gfx::BufferUsage> buffer_usage_;
  scoped_refptr<SharedImageInterfaceHolder> sii_holder_;

  // CopyNativeGmbToSharedMemoryAsync uses this task runner for
  // operations to prevent deadlocks.
  //
  // Deadlock Scenario:
  // 1. Client thread calls CopyGpuMemoryBufferAsync() with a completion
  // callback.
  // 2. Client thread blocks, waiting for an event which is often signaled by
  // the callback.
  // 3. If the copy ran on the client thread, the callback would also need to
  // run on the *same*, now-blocked thread.
  // 4. The callback can't run, the event isn't signaled, and a deadlock
  // occurs.
  //
  // Solution:
  // This dedicated task runner ensures the copy and callback execute
  // independently of the client thread, allowing the callback to signal the
  // event and prevent the deadlock.
  scoped_refptr<base::SingleThreadTaskRunner>
      copy_native_buffer_to_shmem_task_runner_;

  // The texture target returned by `GetTextureTarget()`.
  uint32_t texture_target_ = 0;

  bool is_software_ = false;

  AsyncMapInvokedCallback async_map_invoked_callback_for_testing_;
  bool premapped_for_testing_;

  // The number of active scoped read accesses.
  unsigned int num_readers_ GUARDED_BY(lock_) = 0;

  // Whether there exists an active scoped write access.
  bool has_writer_ GUARDED_BY(lock_) = false;

  base::Lock lock_;
};

struct GPU_COMMAND_BUFFER_CLIENT_EXPORT ExportedSharedImage {
 public:
  ExportedSharedImage();
  ~ExportedSharedImage();

  ExportedSharedImage(const ExportedSharedImage& other) = delete;
  ExportedSharedImage& operator=(const ExportedSharedImage& other) = delete;
  ExportedSharedImage(ExportedSharedImage&& other);
  ExportedSharedImage& operator=(ExportedSharedImage&& other);

  ExportedSharedImage Clone() const;

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
                      std::string debug_label,
                      std::optional<gfx::GpuMemoryBufferHandle> buffer_handle,
                      std::optional<gfx::BufferUsage> buffer_usage,
                      uint32_t texture_target,
                      bool is_software);

  Mailbox mailbox_;
  SharedImageMetadata metadata_;
  SyncToken creation_sync_token_;
  std::string debug_label_;
  std::optional<gfx::GpuMemoryBufferHandle> buffer_handle_;
  std::optional<gfx::BufferUsage> buffer_usage_;
  uint32_t texture_target_ = 0;
  bool is_software_ = false;
};

class GPU_COMMAND_BUFFER_CLIENT_EXPORT SharedImageTexture {
 public:
  class GPU_COMMAND_BUFFER_CLIENT_EXPORT ScopedAccess {
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

class GPU_COMMAND_BUFFER_CLIENT_EXPORT RasterScopedAccess {
 public:
  RasterScopedAccess(const RasterScopedAccess&) = delete;
  RasterScopedAccess& operator=(const RasterScopedAccess&) = delete;
  RasterScopedAccess(RasterScopedAccess&&) = delete;
  RasterScopedAccess& operator=(RasterScopedAccess&&) = delete;

  ~RasterScopedAccess() = default;

  static SyncToken EndAccess(
      std::unique_ptr<RasterScopedAccess> scoped_shared_image);

 private:
  friend class ClientSharedImage;
  RasterScopedAccess(InterfaceBase* raster_interface,
                     ClientSharedImage* shared_image,
                     const SyncToken& sync_token,
                     bool readonly);

  const raw_ptr<InterfaceBase> raster_interface_;
  const raw_ptr<ClientSharedImage> shared_image_;
  bool readonly_;
};

class GPU_COMMAND_BUFFER_CLIENT_EXPORT WebGPUTextureScopedAccess {
 public:
  WebGPUTextureScopedAccess(const WebGPUTextureScopedAccess&) = delete;
  WebGPUTextureScopedAccess& operator=(const WebGPUTextureScopedAccess&) =
      delete;
  WebGPUTextureScopedAccess(WebGPUTextureScopedAccess&&) = delete;
  WebGPUTextureScopedAccess& operator=(WebGPUTextureScopedAccess&&) = delete;

  ~WebGPUTextureScopedAccess();

  static SyncToken EndAccess(
      std::unique_ptr<WebGPUTextureScopedAccess> scoped_access);

  void SetNeedsPresent(bool needs_present);
  const wgpu::dawn::wire::client::Texture& texture();

  // This method is used to clear the context before the object is destroyed.
  // This is necessary to avoid a dangling pointer crash when the context is
  // lost.
  void ClearContext();

 private:
  friend class ClientSharedImage;
  WebGPUTextureScopedAccess(
      webgpu::WebGPUInterface* webgpu,
      ClientSharedImage* shared_image,
      const SyncToken& sync_token,
      const wgpu::dawn::wire::client::Device& device,
      const wgpu::dawn::wire::client::TextureDescriptor& desc,
      uint64_t usage,
      webgpu::MailboxFlags mailbox_flags);

  raw_ptr<webgpu::WebGPUInterface> webgpu_;
  std::unique_ptr<wgpu::dawn::wire::client::Texture> texture_;
  raw_ptr<gpu::ClientSharedImage> shared_image_;
  uint32_t device_id_ = 0;
  uint32_t device_generation_ = 0;
  uint32_t texture_id_ = 0;
  uint32_t texture_generation_ = 0;
  bool needs_present_ = false;
  bool readonly_;
};

class GPU_COMMAND_BUFFER_CLIENT_EXPORT WebGPUBufferScopedAccess {
 public:
  WebGPUBufferScopedAccess(const WebGPUBufferScopedAccess&) = delete;
  WebGPUBufferScopedAccess& operator=(const WebGPUBufferScopedAccess&) = delete;
  WebGPUBufferScopedAccess(WebGPUBufferScopedAccess&&) = delete;
  WebGPUBufferScopedAccess& operator=(WebGPUBufferScopedAccess&&) = delete;

  ~WebGPUBufferScopedAccess();

  static SyncToken EndAccess(
      std::unique_ptr<WebGPUBufferScopedAccess> scoped_access);

  const wgpu::dawn::wire::client::Buffer& buffer();

 private:
  friend class ClientSharedImage;
  WebGPUBufferScopedAccess(
      webgpu::WebGPUInterface* webgpu,
      ClientSharedImage* shared_image,
      const SyncToken& sync_token,
      const wgpu::dawn::wire::client::Device& device,
      const wgpu::dawn::wire::client::BufferDescriptor& desc,
      uint64_t usage,
      webgpu::MailboxFlags mailbox_flags);

  const raw_ptr<webgpu::WebGPUInterface> webgpu_;
  std::unique_ptr<wgpu::dawn::wire::client::Buffer> buffer_;
  raw_ptr<gpu::ClientSharedImage> shared_image_;
  uint32_t buffer_id_ = 0;
  uint32_t buffer_generation_ = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_CLIENT_SHARED_IMAGE_H_
