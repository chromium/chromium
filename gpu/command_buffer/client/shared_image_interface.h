// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_SHARED_IMAGE_INTERFACE_H_
#define GPU_COMMAND_BUFFER_CLIENT_SHARED_IMAGE_INTERFACE_H_

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if !BUILDFLAG(IS_NACL)
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/native_pixmap_handle.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/channel.h>
#include <lib/zx/eventpair.h>
#endif  // BUILDFLAG(IS_FUCHSIA)

namespace gfx {
class ColorSpace;
class GpuFence;
class Size;
}  // namespace gfx

namespace gpu {
class GpuMemoryBufferManager;

// An interface to create shared images and swap chains that can be imported
// into other APIs. This interface is thread-safe and (essentially) stateless.
// It is asynchronous in the same sense as GLES2Interface or RasterInterface in
// that commands are executed asynchronously on the service side, but can be
// synchronized using SyncTokens. See //docs/design/gpu_synchronization.md.
class GPU_EXPORT SharedImageInterface {
 public:
  virtual ~SharedImageInterface() = default;

  // Creates a shared image of requested |format|, |size| and |color_space|.
  // |usage| is a combination of |SharedImageUsage| bits that describes which
  // API(s) the image will be used with.
  // Returns a mailbox that can be imported into said APIs using their
  // corresponding shared image functions (e.g.
  // GLES2Interface::CreateAndTexStorage2DSharedImageCHROMIUM or
  // RasterInterface::CopySharedImage) or (deprecated) mailbox functions (e.g.
  // GLES2Interface::CreateAndConsumeTextureCHROMIUM).
  // The |SharedImageInterface| keeps ownership of the image until
  // |DestroySharedImage| is called or the interface itself is destroyed (e.g.
  // the GPU channel is lost).
  virtual Mailbox CreateSharedImage(viz::SharedImageFormat format,
                                    const gfx::Size& size,
                                    const gfx::ColorSpace& color_space,
                                    GrSurfaceOrigin surface_origin,
                                    SkAlphaType alpha_type,
                                    uint32_t usage,
                                    gpu::SurfaceHandle surface_handle) = 0;

  // Same behavior as the above, except that this version takes |pixel_data|
  // which is used to populate the SharedImage.  |pixel_data| should have the
  // same format which would be passed to glTexImage2D to populate a similarly
  // specified texture.
  virtual Mailbox CreateSharedImage(viz::SharedImageFormat format,
                                    const gfx::Size& size,
                                    const gfx::ColorSpace& color_space,
                                    GrSurfaceOrigin surface_origin,
                                    SkAlphaType alpha_type,
                                    uint32_t usage,
                                    base::span<const uint8_t> pixel_data) = 0;

  // Creates a shared image out an existing buffer. The buffer described by
  // `buffer_handle` must hold all planes based `format` and `size. `usage` is a
  // combination of |SharedImageUsage| bits that describes which API(s) the
  // image will be used with.
  //
  // SharedImageInterface keeps ownership of the image until
  // `DestroySharedImage()` is called or the interface itself is destroyed (e.g.
  // the GPU channel is lost).
  //
  // NOTE: `format` must be a multi-planar format. This is temporary until
  // support is added for single-planar formats here.
  virtual Mailbox CreateSharedImage(
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      gfx::GpuMemoryBufferHandle buffer_handle) = 0;

  // Creates a shared image out of a GpuMemoryBuffer, using |color_space|.
  // |usage| is a combination of |SharedImageUsage| bits that describes which
  // API(s) the image will be used with. Format and size are derived from the
  // GpuMemoryBuffer. |gpu_memory_buffer_manager| is the manager that created
  // |gpu_memory_buffer|. If the |gpu_memory_buffer| was created on the client
  // side (for NATIVE_PIXMAP or ANDROID_HARDWARE_BUFFER types only), without a
  // GpuMemoryBufferManager, |gpu_memory_buffer_manager| can be nullptr.
  // If valid, |color_space| will be applied to the shared
  // image (possibly overwriting the one set on the GpuMemoryBuffer).
  // Returns a mailbox that can be imported into said APIs using their
  // corresponding shared image functions (e.g.
  // GLES2Interface::CreateAndTexStorage2DSharedImageCHROMIUM or
  // RasterInterface::CopySharedImage) or (deprecated) mailbox functions (e.g.
  // GLES2Interface::CreateAndConsumeTextureCHROMIUM).
  // The |SharedImageInterface| keeps ownership of the image until
  // |DestroySharedImage| is called or the interface itself is destroyed (e.g.
  // the GPU channel is lost).
  virtual Mailbox CreateSharedImage(
      gfx::GpuMemoryBuffer* gpu_memory_buffer,
      GpuMemoryBufferManager* gpu_memory_buffer_manager,
      gfx::BufferPlane plane,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage) = 0;

  // Same as the above, but specifies gfx::BufferPlane::DEFAULT for |plane|.
  Mailbox CreateSharedImage(gfx::GpuMemoryBuffer* gpu_memory_buffer,
                            GpuMemoryBufferManager* gpu_memory_buffer_manager,
                            const gfx::ColorSpace& color_space,
                            GrSurfaceOrigin surface_origin,
                            SkAlphaType alpha_type,
                            uint32_t usage);

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

  // Destroys the shared image, unregistering its mailbox, after |sync_token|
  // has been released. After this call, the mailbox can't be used to reference
  // the image any more, however if the image was imported into other APIs,
  // those may keep a reference to the underlying data.
  virtual void DestroySharedImage(const SyncToken& sync_token,
                                  const Mailbox& mailbox) = 0;

  struct SwapChainMailboxes {
    Mailbox front_buffer;
    Mailbox back_buffer;
  };

  // Creates a swap chain.
  // Returns mailboxes for front and back buffers of a DXGI Swap Chain that can
  // be imported into GL command buffer using shared image functions (e.g.
  // GLES2Interface::CreateAndTexStorage2DSharedImageCHROMIUM) or (deprecated)
  // mailbox functions (e.g. GLES2Interface::CreateAndConsumeTextureCHROMIUM).
  virtual SwapChainMailboxes CreateSwapChain(viz::ResourceFormat format,
                                             const gfx::Size& size,
                                             const gfx::ColorSpace& color_space,
                                             GrSurfaceOrigin surface_origin,
                                             SkAlphaType alpha_type,
                                             uint32_t usage) = 0;

  // Swaps front and back buffer of a swap chain. Back buffer mailbox still
  // refers to the back buffer of the swap chain after calling PresentSwapChain.
  // The mailbox argument should be back buffer mailbox. Sync token is required
  // for synchronization between shared image stream and command buffer stream,
  // to ensure that all the rendering commands to a frame are executed before
  // presenting the swap chain.
  virtual void PresentSwapChain(const SyncToken& sync_token,
                                const Mailbox& mailbox) = 0;

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
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      bool register_with_image_pipe) = 0;
#endif  // BUILDFLAG(IS_FUCHSIA)

  // Generates an unverified SyncToken that is released after all previous
  // commands on this interface have executed on the service side.
  virtual SyncToken GenUnverifiedSyncToken() = 0;

  // Generates a verified SyncToken that is released after all previous
  // commands on this interface have executed on the service side.
  virtual SyncToken GenVerifiedSyncToken() = 0;

  // Wait on this SyncToken to be released before executing new commands on
  // this interface on the service side. This is an async wait for all the
  // previous commands which will be sent to server on the next flush().
  virtual void WaitSyncToken(const gpu::SyncToken& sync_token) = 0;

  // Flush the SharedImageInterface, issuing any deferred IPCs.
  virtual void Flush() = 0;

#if !BUILDFLAG(IS_NACL)
  // Returns the NativePixmap backing |mailbox|. This is a privileged API. Only
  // the callers living inside the GPU process are able to retrieve the
  // NativePixmap; otherwise null is returned. Also returns null if the
  // SharedImage doesn't exist or is not backed by a NativePixmap. The caller is
  // not expected to read from or write into the provided NativePixmap because
  // it can be modified at any time. The primary purpose of this method is to
  // facilitate pageflip testing on the viz thread.
  virtual scoped_refptr<gfx::NativePixmap> GetNativePixmap(
      const gpu::Mailbox& mailbox) = 0;
#endif

  // Provides the usage flags supported by the given |mailbox|. This must have
  // been created using a SharedImageInterface on the same channel.
  virtual uint32_t UsageForMailbox(const Mailbox& mailbox);

  // Informs that existing |mailbox| with |usage| can be passed to
  // DestroySharedImage().
  virtual void NotifyMailboxAdded(const Mailbox& mailbox, uint32_t usage);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_SHARED_IMAGE_INTERFACE_H_
