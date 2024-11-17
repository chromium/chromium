// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_MAILBOX_VIDEO_FRAME_CONVERTER_H_
#define MEDIA_GPU_CHROMEOS_MAILBOX_VIDEO_FRAME_CONVERTER_H_

#include <optional>

#include "base/containers/queue.h"
#include "base/containers/small_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/shared_image_stub.h"
#include "media/gpu/chromeos/frame_resource.h"
#include "media/gpu/chromeos/frame_resource_converter.h"
#include "media/gpu/media_gpu_export.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "ui/gfx/buffer_types.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace gpu {
class CommandBufferStub;
}  // namespace gpu

namespace media {

// This class is used for converting DMA buffer-backed FrameResources to
// SharedImage-backed VideoFrames. See ConvertFrameImpl() for more details.
// After conversion, the returned SharedImage VideoFrame will retain a reference
// to the FrameResource passed to ConvertFrameImpl().
class MEDIA_GPU_EXPORT MailboxVideoFrameConverter final
    : public FrameResourceConverter {
 public:
  using GetCommandBufferStubCB =
      base::RepeatingCallback<gpu::CommandBufferStub*()>;

  class GpuDelegate {
   public:
    GpuDelegate() = default;
    GpuDelegate(const GpuDelegate&) = delete;
    GpuDelegate& operator=(const GpuDelegate&) = delete;
    virtual ~GpuDelegate() = default;

    virtual bool Initialize() = 0;
    virtual std::optional<gpu::SharedImageCapabilities> GetCapabilities() = 0;
    virtual scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
        gfx::GpuMemoryBufferHandle handle,
        viz::SharedImageFormat format,
        const gfx::Size& size,
        const gfx::ColorSpace& color_space,
        GrSurfaceOrigin surface_origin,
        SkAlphaType alpha_type,
        gpu::SharedImageUsageSet usage) = 0;
    virtual std::optional<gpu::SyncToken> UpdateSharedImage(
        const gpu::Mailbox& mailbox) = 0;
    virtual bool WaitOnSyncTokenAndReleaseFrame(
        scoped_refptr<FrameResource> frame,
        const gpu::SyncToken& sync_token) = 0;
  };

  // Creates a MailboxVideoFrameConverter instance. |gpu_task_runner| is the
  // task runner of the GPU main thread. Returns nullptr if the
  // MailboxVideoFrameConverter can't be created.
  static std::unique_ptr<FrameResourceConverter> Create(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      GetCommandBufferStubCB get_stub_cb);

  MailboxVideoFrameConverter(const MailboxVideoFrameConverter&) = delete;
  MailboxVideoFrameConverter& operator=(const MailboxVideoFrameConverter&) =
      delete;

 private:
  friend class MailboxVideoFrameConverterTest;
  friend class MailboxVideoFrameConverterWithUnwrappedFramesTest;

  // Use FrameResource::unique_id() as internal frame indexing.
  using UniqueID = decltype(std::declval<FrameResource>().unique_id());

  // A self-cleaning SharedImage, with move-only semantics.
  class ScopedSharedImage;

  MailboxVideoFrameConverter(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      std::unique_ptr<GpuDelegate> gpu_delegate);
  // Destructor runs on the GPU main thread.
  ~MailboxVideoFrameConverter() override;

  void Destroy() override;
  void DestroyOnGPUThread();

  // FrameResourceConverter implementation.

  // Enqueues |frame| to be converted to a gpu::Mailbox-backed
  // VideoFrameResource. If set_get_original_frame_cb() was called with a
  // non-null callback, |frame| must wrap a FrameResource that is retrieved via
  // that callback. Otherwise, |frame| will be used directly. |frame| or the
  // FrameResource that it wraps must be able to be used to create a
  // GpuMemoryBufferHandle. This means that its storage type must be either
  // STORAGE_DMABUFS or STORAGE_GPU_MEMORY_BUFFER. The generated gpu::Mailbox is
  // kept alive until the underlying frame is destroyed. These methods must be
  // called on |parent_task_runner_|.
  void ConvertFrameImpl(scoped_refptr<FrameResource> frame) override;
  void AbortPendingFramesImpl() override;
  bool HasPendingFramesImpl() const override;
  bool UsesGetOriginalFrameCBImpl() const override;
  void OnError(const base::Location& location, const std::string& msg) override;

  // TODO(crbug.com/998279): replace s/OnGPUThread/OnGPUTaskRunner/.
  bool InitializeOnGPUThread();

  // Wraps |shared_image| and |frame| into a new VideoFrameResource and sends it
  // via |output_cb_|.
  void WrapSharedImageAndVideoFrameAndOutput(
      FrameResource* origin_frame,
      scoped_refptr<FrameResource> frame,
      scoped_refptr<gpu::ClientSharedImage> shared_image,
      const gpu::SyncToken& sync_token);

  // ConvertFrameImpl() delegates to this method to
  // GenerateSharedImageOnGPUThread() or just UpdateSharedImageOnGPUThread(),
  // then to jump back to WrapMailboxAndVideoFrameAndOutput().
  void ConvertFrameOnGPUThread(FrameResource* origin_frame,
                               scoped_refptr<FrameResource> frame,
                               ScopedSharedImage* stored_shared_image);

  // Populates a ScopedSharedImage from a FrameResource. |origin_frame| must be
  // kept alive for the duration of this method. This method runs on
  // |gpu_task_runner_|. Returns true if the SharedImage could be created
  // successfully; false otherwise (and OnError() is called).
  bool GenerateSharedImageOnGPUThread(FrameResource* origin_frame,
                                      const gfx::ColorSpace& src_color_space,
                                      const gfx::Rect& destination_visible_rect,
                                      ScopedSharedImage* shared_image);

  // Registers the mapping between a FrameResource and the SharedImage.
  // |origin_frame| must be kept alive for the duration of this method. After
  // this method returns, |scoped_shared_image| will be owned by |origin_frame|.
  // This guarantees that the SharedImage lives as long as the associated buffer
  // even if MailboxVideoFrameConverter dies.
  void RegisterSharedImage(
      FrameResource* origin_frame,
      std::unique_ptr<ScopedSharedImage> scoped_shared_image);
  // Unregisters the |origin_frame_id| and associated SharedImage.
  // |scoped_shared_image| is passed to guarantee that the SharedImage is alive
  // until after we delete the pointer from |shared_images_|.
  void UnregisterSharedImage(
      UniqueID origin_frame_id,
      std::unique_ptr<ScopedSharedImage> scoped_shared_image);

  // Updates the SharedImage associated to |mailbox|. Returns a sync token if
  // the update could be carried out, or nullopt otherwise.
  std::optional<gpu::SyncToken> UpdateSharedImageOnGPUThread(
      const gpu::Mailbox& mailbox);

  // Waits on |sync_token|, keeping |frame| alive until it is signalled. It
  // trampolines threads to |gpu_task_runner| if necessary.
  void WaitOnSyncTokenAndReleaseFrameOnGPUThread(
      scoped_refptr<FrameResource> frame,
      const gpu::SyncToken& sync_token);

  const scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  const std::unique_ptr<GpuDelegate> gpu_delegate_;

  // Mapping from the unique id of the frame to its corresponding SharedImage.
  // Accessed only on |parent_task_runner_|. The ScopedSharedImages are owned by
  // the unwrapped FrameResources so that they can be used even after
  // MailboxVideoFrameConverter dies (e.g., there may still be compositing
  // commands that need the shared images).
  base::small_map<
      std::map<UniqueID, raw_ptr<ScopedSharedImage, CtnExperimental>>>
      shared_images_;

  // The queue of input frames and the unique_id of their origin frame.
  // Accessed only on |parent_task_runner_|.
  // TODO(crbug.com/998279): remove this member entirely.
  base::queue<std::pair<scoped_refptr<FrameResource>, UniqueID>>
      input_frame_queue_;

  // The weak pointer of this, bound to |parent_task_runner_|. Used at the
  // in the original frame's destruction callback.
  base::WeakPtr<MailboxVideoFrameConverter> parent_weak_this_;
  // The weak pointer of this, bound to |gpu_task_runner_|.
  // Used to generate SharedImages on the GPU main thread.
  base::WeakPtr<MailboxVideoFrameConverter> gpu_weak_this_;
  base::WeakPtrFactory<MailboxVideoFrameConverter> parent_weak_this_factory_{
      this};
  base::WeakPtrFactory<MailboxVideoFrameConverter> gpu_weak_this_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_MAILBOX_VIDEO_FRAME_CONVERTER_H_
