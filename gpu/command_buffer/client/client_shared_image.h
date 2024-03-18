// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CLIENT_SHARED_IMAGE_H_
#define GPU_COMMAND_BUFFER_CLIENT_CLIENT_SHARED_IMAGE_H_

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_memory_buffer_handle_info.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace viz {
class TestSharedImageInterface;
}

namespace gpu {

// Controls whether all ClientSharedImage::GetTextureTarget*(...) variants call
// through to ClientSharedImage::GetTextureTarget() under the hood.
GPU_EXPORT BASE_DECLARE_FEATURE(kUseUniversalGetTextureTargetFunction);

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
        gfx::GpuMemoryBuffer* gpu_memory_buffer);

    bool Init(gfx::GpuMemoryBuffer* gpu_memory_buffer);

    // ScopedMapping is essentially a wrapper around GpuMemoryBuffer for now for
    // simplicity and will be removed later.
    // TODO(crbug.com/1474697): Refactor/Rename GpuMemoryBuffer and its
    // implementations  as the end goal after all clients using GMB are
    // converted to use the ScopedMapping and notion of GpuMemoryBuffer is being
    // removed.
    raw_ptr<gfx::GpuMemoryBuffer> buffer_;
  };

  explicit ClientSharedImage(
      const Mailbox& mailbox,
      const SharedImageMetadata& metadata,
      const SyncToken& sync_token,
      scoped_refptr<SharedImageInterfaceHolder> sii_holder,
      gfx::GpuMemoryBufferType gmb_type = gfx::EMPTY_BUFFER);
  ClientSharedImage(const Mailbox& mailbox,
                    const SharedImageMetadata& metadata,
                    const SyncToken& sync_token,
                    GpuMemoryBufferHandleInfo handle_info,
                    scoped_refptr<SharedImageInterfaceHolder> sii_holder);

  const Mailbox& mailbox() { return mailbox_; }
  viz::SharedImageFormat format() const { return metadata_.format; }
  gfx::Size size() const { return metadata_.size; }
  uint32_t usage() { return metadata_.usage; }

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
  // TODO(crbug.com/41494843): Eliminate all the below variants in favor of all
  // clients using this function.
  uint32_t GetTextureTarget();

  // Returns the texture target to use for overlays:
  // * GL_TEXTURE_2D on platforms other than MacOS
  // * The platform-specific texture target for MacOS
  uint32_t GetTextureTargetForOverlays();

  // Returns the texture target to be used for the given |format|. For usage
  // when this SharedImage was created from a native buffer and the client knows
  // that the usages of this SI would result in needing the platform-specific
  // texture target for `format` if one exists on this platform. Returns
  // GL_TEXTURE_2D if |format| does not require a platform-specific target and
  // the relevant platform-specific target otherwise.
  uint32_t GetTextureTarget(gfx::BufferFormat format);

  // Returns the texture target to be used for the given |usage| and |format|
  // based on the underlying SharedImageCapabilities. Requires that
  // `HasHolder()` is true. For usage when this SharedImage was created from a
  // native buffer. Returns GL_TEXTURE_2D if the `usage`/`format` pair does not
  // require a platform-specific target and the relevant platform-specific
  // target otherwise.
  uint32_t GetTextureTarget(gfx::BufferUsage usage, gfx::BufferFormat format);

  // Similar to the above, but for usage if the client did not explicitly create
  // this SharedImage from a native buffer. Returns GL_TEXTURE_2D if the set of
  // usages that the client specified do not result in this SharedImage being
  // backed by a native buffer. Otherwise, uses this instance's
  // SharedImageFormat (which must be a single-planar format) to compute the
  // BufferFormat and returns the result of the above GetTextureTarget() call.
  uint32_t GetTextureTarget(gfx::BufferUsage usage);

  base::trace_event::MemoryAllocatorDumpGuid GetGUIDForTracing() {
    return gpu::GetSharedImageGUIDForTracing(mailbox_);
  }

  // Maps |mailbox| into CPU visible memory and returns a ScopedMapping object
  // which can be used to read/write to the CPU mapped memory. The SharedImage
  // backing this ClientSI must have been created with CPU_READ/CPU_WRITE usage.
  std::unique_ptr<ScopedMapping> Map();

  ExportedSharedImage Export();

  // Returns an unowned reference. The caller should ensure that the original
  // shared image outlives this reference. Note that it is preferable to use
  // SharedImageInterface::ImportSharedImage() instead, which returns an owning
  // reference.
  static scoped_refptr<ClientSharedImage> ImportUnowned(
      const ExportedSharedImage& exported_shared_image);

  static scoped_refptr<ClientSharedImage> CreateForTesting() {
    return base::MakeRefCounted<ClientSharedImage>(
        Mailbox::GenerateForSharedImage(), SharedImageMetadata(),
        gpu::SyncToken(), nullptr, gfx::EMPTY_BUFFER);
  }

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

 private:
  friend class base::RefCountedThreadSafe<ClientSharedImage>;
  ~ClientSharedImage();

  const Mailbox mailbox_;
  const SharedImageMetadata metadata_;
  SyncToken creation_sync_token_;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
  scoped_refptr<SharedImageInterfaceHolder> sii_holder_;

  // Whether a client-side native buffer was used in the creation of this
  // SharedImage.
  bool client_side_native_buffer_used_ = false;
};

struct GPU_EXPORT ExportedSharedImage {
 private:
  friend class ClientSharedImage;
  friend class SharedImageInterface;
  friend class ClientSharedImageInterface;
  friend class viz::TestSharedImageInterface;

  ExportedSharedImage(const Mailbox& mailbox,
                      const SharedImageMetadata& metadata,
                      const SyncToken& sync_token);

  const Mailbox mailbox_;
  const SharedImageMetadata metadata_;
  SyncToken sync_token_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_CLIENT_SHARED_IMAGE_H_
