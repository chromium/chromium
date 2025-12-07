// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_SHARED_IMAGE_INTERFACE_H_
#define GPU_COMMAND_BUFFER_CLIENT_SHARED_IMAGE_INTERFACE_H_

#include <cstdint>
#include <optional>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/gpu_command_buffer_client_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_pool_id.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/shared_image_pool_client_interface.mojom.h"
#include "gpu/ipc/common/surface_handle.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/native_pixmap_handle.h"

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/channel.h>
#include <lib/zx/eventpair.h>
#endif  // BUILDFLAG(IS_FUCHSIA)

namespace gfx {
class GpuFence;
class Size;

#if BUILDFLAG(IS_WIN)
class D3DSharedFence;
#endif
}  // namespace gfx

namespace media {
class MockSharedImageInterface;
}

namespace gpu {
class ArcSharedImageInterface;
class ClientSharedImageInterface;
struct ExportedSharedImage;
class GpuChannelLostObserver;
struct SharedImageCapabilities;
class SharedImageInterfaceHolder;
class SharedImageInterfaceInProcessBase;
class TestSharedImageInterface;

struct SharedImageInfo {
  SharedImageInfo(const viz::SharedImageFormat& format,
                  gfx::Size size,
                  const gfx::ColorSpace& color_space,
                  GrSurfaceOrigin surface_origin,
                  SkAlphaType alpha_type,
                  SharedImageUsageSet usage,
                  std::string_view debug_label)
      : meta(format, size, color_space, surface_origin, alpha_type, usage),
        debug_label(debug_label) {}
  SharedImageInfo(const viz::SharedImageFormat& format,
                  gfx::Size size,
                  const gfx::ColorSpace& color_space,
                  SharedImageUsageSet usage,
                  std::string_view debug_label)
      : meta(format,
             size,
             color_space,
             kTopLeft_GrSurfaceOrigin,
             kPremul_SkAlphaType,
             usage),
        debug_label(debug_label) {}
  SharedImageInfo(const SharedImageMetadata& meta, std::string_view debug_label)
      : meta(meta), debug_label(debug_label) {}

  SharedImageMetadata meta;
  std::string debug_label;
};

// An interface to create shared images and swap chains that can be imported
// into other APIs. This interface is thread-safe and (essentially) stateless.
// It is asynchronous in the same sense as GLES2Interface or RasterInterface in
// that commands are executed asynchronously on the service side, but can be
// synchronized using SyncTokens. See //docs/design/gpu_synchronization.md.
class GPU_COMMAND_BUFFER_CLIENT_EXPORT SharedImageInterface
    : public base::RefCountedThreadSafe<SharedImageInterface> {
 public:
  // Creates a shared image of requested |format|, |size| and |color_space|.
  // |usage| is a combination of |SharedImageUsage| bits that describes which
  // API(s) the image will be used with.
  // Returns a non-null scoped_refptr to ClientSharedImage. The
  // ClientSharedImage struct contains a mailbox that can be imported into said
  // APIs using their corresponding shared image functions (e.g.
  // GLES2Interface::CreateAndTexStorage2DSharedImageCHROMIUM or
  // RasterInterface::CopySharedImage).
  // The |SharedImageInterface| keeps ownership of the image until
  // |DestroySharedImage| is called or the interface itself is destroyed (e.g.
  // the GPU channel is lost).
  // |debug_label| is retained for heap dumps and passed to graphics APIs for
  // tracing tools. Pick a name that is unique to the allocation site.
  virtual scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      gpu::SurfaceHandle surface_handle,
      std::optional<SharedImagePoolId> pool_id /*=std::nullopt*/) = 0;

  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      gpu::SurfaceHandle surface_handle) {
    return CreateSharedImage(si_info, surface_handle, std::nullopt);
  }

  // Same behavior as the above, except that this version takes |pixel_data|
  // which is used to populate the SharedImage.  |pixel_data| should have the
  // same format which would be passed to glTexImage2D to populate a similarly
  // specified texture.
  // May return null if |pixel_data| is too big for IPC.
  // TODO(crbug.com/40268891): Have the caller specify a row span for
  // |pixel_data| explicitly. Some backings have different row alignment
  // requirements which the caller has to match exactly or it won't work.
  virtual scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      base::span<const uint8_t> pixel_data) = 0;

  // Same behavior as above methods, except that this version is specifically
  // used by clients which intend to create a shared image back by either a
  // native buffer (if supported) or shared memory which are CPU mappable.
  // We are currently passing BufferUsage to this method for simplicity since
  // as of now we dont have a clear way to map BufferUsage to SharedImageUsage.
  // May return null if GPU memory buffer creation fails.
  // TODO(crbug.com/40276844): Merge this method to above existing methods once
  // we figure out mapping between BufferUsage and SharedImageUsage and
  // eliminate all usages of BufferUsage.
  virtual scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      gpu::SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage,
      std::optional<SharedImagePoolId> pool_id /*=std::nullopt*/);

  scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      gpu::SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage) {
    return CreateSharedImage(si_info, surface_handle, buffer_usage,
                             std::nullopt);
  }

  // Creates a shared image out an existing buffer. The buffer described by
  // `buffer_handle` must hold all planes based on `format` and `size`. This
  // version is specifically used by clients that need access to the buffer on
  // the client side. It ensures that
  // ClientSharedImage::CloneGpuMemoryBufferHandle() can be invoked on the
  // returned ClientSharedImage.
  // NOTE: We are currently passing BufferUsage to this method for simplicity
  // since as of now we dont have a clear way to map BufferUsage to
  // SharedImageUsage.
  // TODO(crbug.com/40276844): Merge this method to above existing methods once
  // we figure out mapping between BufferUsage and SharedImageUsage and
  // eliminate all usages of BufferUsage.
  virtual scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      gpu::SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage,
      gfx::GpuMemoryBufferHandle buffer_handle) = 0;

  // Creates a shared image out an existing buffer. The buffer described by
  // `buffer_handle` must hold all planes based `format` and `size. `usage` is a
  // combination of |SharedImageUsage| bits that describes which API(s) the
  // image will be used with.
  //
  // SharedImageInterface keeps ownership of the image until
  // `DestroySharedImage()` is called or the interface itself is destroyed (e.g.
  // the GPU channel is lost).
  virtual scoped_refptr<ClientSharedImage> CreateSharedImage(
      const SharedImageInfo& si_info,
      gfx::GpuMemoryBufferHandle buffer_handle) = 0;

  // Creates a shared image for an existing MLTensor.
  // Tensors store numeric values in multiple dimensions.
  // |size| is calculated from the tensor's shape: the product of all dimensions
  // except the last determines the height, and the last dimension becomes the
  // width. |usage| must include gpu::SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR.
  // |format| should be valid and correspond to the equivalent dataType.
  virtual scoped_refptr<ClientSharedImage> CreateSharedImageForMLTensor(
      std::string debug_label,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      gpu::SharedImageUsageSet usage) = 0;

  // Creates a shared image with the usage of
  // gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY only. A shared memory buffer is
  // created internally and a shared image is created out of this buffer. This
  // method is used by the software compositor only.
  virtual scoped_refptr<ClientSharedImage>
  CreateSharedImageForSoftwareCompositor(const SharedImageInfo& si_info) = 0;

  // Updates a shared image after its GpuMemoryBuffer (if any) was modified on
  // the CPU or through external devices, after |sync_token| has been released.
  virtual void UpdateSharedImage(const SyncToken& sync_token,
                                 const Mailbox& mailbox) = 0;

  // Updates a shared image after its GpuMemoryBuffer (if any) was modified on
  // the CPU or through external devices, after |sync_token| has been released.
  // If |acquire_fence| is not null, the fence is inserted in the GPU command
  // stream and a server side wait is issued before any GPU command referring
  // to this shared imaged is executed on the GPU.
  virtual void UpdateSharedImage(const SyncToken& sync_token,
                                 std::unique_ptr<gfx::GpuFence> acquire_fence,
                                 const Mailbox& mailbox) = 0;

  // Update the GpuMemoryBuffer associated with the shared image |mailbox| after
  // |sync_token| is released. This needed when the GpuMemoryBuffer is backed by
  // shared memory on platforms like Windows where the renderer cannot create
  // native GMBs.
  virtual void CopyToGpuMemoryBuffer(const SyncToken& sync_token,
                                     const Mailbox& mailbox);

  // Update the GpuMemoryBuffer associated with the shared image |mailbox| after
  // |sync_token| is released. The |callback| is run denoting if the copy was
  // successful and the GpuMemoryBuffer is ready to be mapped by the client.
  // This is needed when the GpuMemoryBuffer is backed by shared memory on
  // platforms like Windows where the renderer cannot create native GMBs.
  virtual void CopyToGpuMemoryBufferAsync(
      const SyncToken& sync_token,
      const Mailbox& mailbox,
      base::OnceCallback<void(bool)> callback);

  // On windows, native GMB can not be mapped in any process other than the GPU
  // process. So during GpuMemoryBuffer::Map() operation, an IPC to GPU process
  // is done to copy the content of |buffer_handle| into a shared memory
  // |memory_region| via below methods. This shared memory is mappable in any
  // process and is used internally during GpuMemoryBuffer::Map().
  // The |callback| will be run when the copy is done.
  virtual void CopyNativeGmbToSharedMemoryAsync(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion memory_region,
      base::OnceCallback<void(bool)> callback);

  // Destroys the shared image, unregistering its mailbox, after |sync_token|
  // has been released. After this call, the mailbox can't be used to reference
  // the image any more, however if the image was imported into other APIs,
  // those may keep a reference to the underlying data.
  virtual void DestroySharedImage(const SyncToken& sync_token,
                                  const Mailbox& mailbox) = 0;

  // Same behavior as the above, except that this version takes
  // a |client_shared_image| parameter (which holds a mailbox).
  virtual void DestroySharedImage(
      const SyncToken& sync_token,
      scoped_refptr<ClientSharedImage> client_shared_image) = 0;

  // Imports SharedImage to this interface and returns an owning reference. It
  // must be released via DestroySharedImage in the same way as for SharedImages
  // created via CreateSharedImage().
  virtual scoped_refptr<ClientSharedImage> ImportSharedImage(
      ExportedSharedImage exported_shared_image) = 0;

#if BUILDFLAG(IS_FUCHSIA)
  // Registers a sysmem buffer collection. `service_handle` contains a handle
  // for the eventpair that controls the lifetime of the collection. The
  // collection will be destroyed when all peer handles for that eventpair are
  // destroyed (i.e. when `ZX_EVENTPAIR_PEER_CLOSED` is signaled on that
  // handle). The caller can use CreateSharedImage() to create shared images
  // from the buffer in the collection by setting `buffer_collection_handle` and
  // `buffer_index` fields in NativePixmapHandle, wrapping it in
  // GpuMemoryBufferHandle and then creating a GpuMemoryBuffer from that handle.
  // If `register_with_image_pipe` field is set, the collection is shared with a
  // new ImagePipe, which allows it to display these images as overlays.
  virtual void RegisterSysmemBufferCollection(
      zx::eventpair service_handle,
      zx::channel sysmem_token,
      const viz::SharedImageFormat& format,
      gfx::BufferUsage usage,
      bool register_with_image_pipe) = 0;
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN)
  // Update fence between processes. Register D3DSharedFence in GPU process
  // first and then use DXGIHandleToken to identify the fence between processes
  // and pass signaled fence value from current process to GPU process.
  virtual void UpdateSharedImage(
      const SyncToken& sync_token,
      scoped_refptr<gfx::D3DSharedFence> d3d_shared_fence,
      const Mailbox& mailbox);
#endif  // BUILDFLAG(IS_WIN)

  // Generates an unverified SyncToken that is released after all previous
  // commands on this interface have executed on the service side.
  virtual SyncToken GenUnverifiedSyncToken() = 0;

  // Generates a verified SyncToken that is released after all previous
  // commands on this interface have executed on the service side.
  virtual SyncToken GenVerifiedSyncToken() = 0;

  // Verifies the SyncToken.
  virtual void VerifySyncToken(gpu::SyncToken& sync_token) = 0;

  // Check if a token is able to be verified by this client
  virtual bool CanVerifySyncToken(const gpu::SyncToken& sync_token) = 0;

  // Runs a synchronous round trip mojo IPC to ensure everything up this point
  // is visible to the service
  virtual void VerifyFlush() = 0;

  // Verifies a range of SyncTokens. Will trigger a synchronous round trip IPC
  // if there is anything to sync.
  //
  // The `proj` parameter allows using this function with ranges containing
  // elements other than gpu::SyncToken references. For example to handle a
  // vector of unique_ptrs, use:
  // `[](const auto& p) { return *p.get(); }`
  template <std::ranges::input_range Range, typename Proj = std::identity>
    requires std::convertible_to<
        std::invoke_result_t<Proj&, std::ranges::range_reference_t<Range>>,
        gpu::SyncToken&>
  void VerifySyncTokens(Range&& sync_token_range, Proj proj = {}) {
    bool flush_required = false;
    for (auto const& element : sync_token_range) {
      gpu::SyncToken& sync_token = proj(element);
      if (sync_token.verified_flush()) {
        continue;
      }

      if (!sync_token.HasData()) {
        sync_token.SetVerifyFlush();
        continue;
      }

      if (CanVerifySyncToken(sync_token)) {
        flush_required = true;

        sync_token.SetVerifyFlush();
      }
    }

    if (flush_required) {
      VerifyFlush();
    }
  }

  // Wait on this SyncToken to be released before executing new commands on
  // this interface on the service side. This is an async wait for all the
  // previous commands which will be sent to server on the next flush().
  virtual void WaitSyncToken(const gpu::SyncToken& sync_token) = 0;

  // Informs that existing |mailbox| with the specified metadata can be passed
  // to DestroySharedImage().
  virtual scoped_refptr<ClientSharedImage> NotifyMailboxAdded(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      SharedImageUsageSet usage,
      uint32_t texture_target,
      std::string_view debug_label);

  // Returns true if the backing context has been lost.
  virtual bool IsLost() const;

  // Adds an observer that will be notified when the backing GPU channel is
  // lost. Returns true if the observer was added successfully.
  virtual bool AddGpuChannelLostObserver(GpuChannelLostObserver* observer);

  // Removes a GPU channel lost observer.
  virtual void RemoveGpuChannelLostObserver(GpuChannelLostObserver* observer);

  virtual const SharedImageCapabilities& GetCapabilities() = 0;

  void Release() const;

  // Used by client side shared image pool aka SharedImagePool to
  // create a service side pool. It also creates a new mojo IPC connection
  // between the client and the service side pool so that service side pool
  // can communicate with client side pool when needed.
  virtual void CreateSharedImagePool(
      const SharedImagePoolId& pool_id,
      mojo::PendingRemote<mojom::SharedImagePoolClientInterface> client_remote);

  // Called when client side SharedImagePool is destroyed. It will
  // in turn destroy the corresponding GPU service side SharedImagePool.
  virtual void DestroySharedImagePool(const SharedImagePoolId& pool_id);

 protected:
  friend class base::RefCountedThreadSafe<SharedImageInterface>;
  virtual ~SharedImageInterface();

  // Creates a WritableSharedMemoryRegion corresponding to the format/size
  // passed in `si_info` and populates `mapping` and `handle` from the created
  // shmem region. Fails if the shmem region cannot be created or mapped. For
  // usage in implementing APIs that create mappable SharedImages without
  // holding on to a handle on the client side for usage with the software
  // compositor. As such, verifies that `si_info`'s usage is
  // `SHARED_IMAGE_USAGE_CPU_WRITE_ONLY`.
  static void CreateSharedMemoryRegionFromSIInfo(
      const SharedImageInfo& si_info,
      base::WritableSharedMemoryMapping& mapping,
      gfx::GpuMemoryBufferHandle& handle);

  // Returns CPU read | write shared image usages based on BufferUsage passed
  // in.
  gpu::SharedImageUsageSet GetCpuSIUsage(gfx::BufferUsage buffer_usage);

  scoped_refptr<SharedImageInterfaceHolder> holder_;

 private:
  friend class ArcSharedImageInterface;
  friend class ClientSharedImageInterface;
  friend class SharedImageInterfaceInProcessBase;
  friend class TestSharedImageInterface;
  friend class media::MockSharedImageInterface;

  // Make the constructor private to ensure that any new subclassing of this
  // interface gets explicit approval from //gpu OWNERS (by adding to the list
  // of friends above). In particular, do not subclass this interface for
  // testing purposes - use (and extend if necessary) TestSharedImageInterface
  // instead.
  SharedImageInterface();
};

// |SharedImageInterfaceHolder| provides thread-safe access to
// |SharedImageInterface| via a weak reference.
class GPU_COMMAND_BUFFER_CLIENT_EXPORT SharedImageInterfaceHolder
    : public base::RefCountedThreadSafe<SharedImageInterfaceHolder> {
 public:
  SharedImageInterfaceHolder(SharedImageInterface* sii);

  scoped_refptr<SharedImageInterface> Get();

 private:
  friend base::RefCountedThreadSafe<SharedImageInterfaceHolder>;
  friend SharedImageInterface;
  ~SharedImageInterfaceHolder();

  void OnDestroy();

  mutable base::Lock lock_;
  raw_ptr<SharedImageInterface> sii_ GUARDED_BY(lock_);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_SHARED_IMAGE_INTERFACE_H_
