// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_SHARED_IMAGE_INTERFACE_H_
#define GPU_COMMAND_BUFFER_CLIENT_SHARED_IMAGE_INTERFACE_H_

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/native_pixmap_handle.h"

#if defined(OS_FUCHSIA)
#include <lib/zx/channel.h>
#endif  // defined(OS_FUCHSIA)

namespace gfx {
class ColorSpace;
class GpuFence;
class GpuMemoryBuffer;
class Size;
}  // namespace gfx

namespace gpu {
class GpuMemoryBufferManager;

// An interface to create shared images and swap chains that can be imported
// into other APIs. This interface is thread-safe and (essentially) stateless.
// It is asynchronous in the same sense as GLES2Interface or RasterInterface in
// that commands are executed asynchronously on the service side, but can be
// synchronized using SyncTokens. See //docs/design/gpu_synchronization.md.
class SharedImageInterface {
 public:
  virtual ~SharedImageInterface() {}

  // Creates a shared image of requested |format|, |size| and |color_space|.
  // |usage| is a combination of |SharedImageUsage| bits that describes which
  // API(s) the image will be used with.
  // Returns a mailbox that can be imported into said APIs using their
  // corresponding shared image functions (e.g.
  // GLES2Interface::CreateAndTexStorage2DSharedImageCHROMIUM or
  // RasterInterface::CopySubTexture) or (deprecated) mailbox functions (e.g.
  // GLES2Interface::CreateAndConsumeTextureCHROMIUM).
  // The |SharedImageInterface| keeps ownership of the image until
  // |DestroySharedImage| is called or the interface itself is destroyed (e.g.
  // the GPU channel is lost).
  virtual Mailbox CreateSharedImage(viz::ResourceFormat format,
                                    const gfx::Size& size,
                                    const gfx::ColorSpace& color_space,
                                    uint32_t usage) = 0;

  // Same behavior as the above, except that this version takes |pixel_data|
  // which is used to populate the SharedImage.  |pixel_data| should have the
  // same format which would be passed to glTexImage2D to populate a similarly
  // specified texture.
  virtual Mailbox CreateSharedImage(viz::ResourceFormat format,
                                    const gfx::Size& size,
                                    const gfx::ColorSpace& color_space,
                                    uint32_t usage,
                                    base::span<const uint8_t> pixel_data) = 0;

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
  // RasterInterface::CopySubTexture) or (deprecated) mailbox functions (e.g.
  // GLES2Interface::CreateAndConsumeTextureCHROMIUM).
  // The |SharedImageInterface| keeps ownership of the image until
  // |DestroySharedImage| is called or the interface itself is destroyed (e.g.
  // the GPU channel is lost).
  virtual Mailbox CreateSharedImage(
      gfx::GpuMemoryBuffer* gpu_memory_buffer,
      GpuMemoryBufferManager* gpu_memory_buffer_manager,
      const gfx::ColorSpace& color_space,
      uint32_t usage) = 0;

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
                                             uint32_t usage) = 0;

  // Swaps front and back buffer of a swap chain. Back buffer mailbox still
  // refers to the back buffer of the swap chain after calling PresentSwapChain.
  // The mailbox argument should be back buffer mailbox. Sync token is required
  // for synchronization between shared image stream and command buffer stream,
  // to ensure that all the rendering commands to a frame are executed before
  // presenting the swap chain.
  virtual void PresentSwapChain(const SyncToken& sync_token,
                                const Mailbox& mailbox) = 0;

#if defined(OS_FUCHSIA)
  // Registers a sysmem buffer collection. While the collection exists (i.e.
  // between RegisterSysmemBufferCollection() and
  // ReleaseSysmemBufferCollection()) the caller can use CreateSharedImage() to
  // create shared images from the buffer in the collection by setting
  // |buffer_collection_id| and |buffer_index| fields in NativePixmapHandle,
  // wrapping it in GpuMemoryBufferHandle and then creating GpuMemoryBuffer from
  // that handle.
  virtual void RegisterSysmemBufferCollection(gfx::SysmemBufferCollectionId id,
                                              zx::channel token) = 0;

  virtual void ReleaseSysmemBufferCollection(
      gfx::SysmemBufferCollectionId id) = 0;
#endif  // defined(OS_FUCHSIA)

  // Generates an unverified SyncToken that is released after all previous
  // commands on this interface have executed on the service side.
  virtual SyncToken GenUnverifiedSyncToken() = 0;

  // Generates a verified SyncToken that is released after all previous
  // commands on this interface have executed on the service side.
  virtual SyncToken GenVerifiedSyncToken() = 0;

  // Flush the SharedImageInterface, issuing any deferred IPCs.
  virtual void Flush() = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_SHARED_IMAGE_INTERFACE_H_
