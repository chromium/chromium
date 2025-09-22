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
class ClientSharedImage;
class CommandBufferStub;
class SharedImageInterface;
}  // namespace gpu

namespace media {

// This class is used for converting DMA buffer-backed FrameResources to
// SharedImage-backed VideoFrames. See ConvertFrameImpl() for more details.
// After conversion, the returned SharedImage VideoFrame will retain a reference
// to the FrameResource passed to ConvertFrameImpl() in its `done_callbacks_`.
class MEDIA_GPU_EXPORT MailboxVideoFrameConverter final
    : public FrameResourceConverter {
 public:
  using GetCommandBufferStubCB =
      base::RepeatingCallback<gpu::CommandBufferStub*()>;

  // |gpu_task_runner| is the task runner of the GPU main thread. Returns
  // Creates a MailboxVideoFrameConverter instance in gpu process.
  static std::unique_ptr<FrameResourceConverter> Create(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      GetCommandBufferStubCB get_stub_cb);
  static std::unique_ptr<FrameResourceConverter> Create(
      scoped_refptr<gpu::SharedImageInterface> sii);

  MailboxVideoFrameConverter(const MailboxVideoFrameConverter&) = delete;
  MailboxVideoFrameConverter& operator=(const MailboxVideoFrameConverter&) =
      delete;

 private:
  friend class MailboxVideoFrameConverterTest;
  friend class MailboxVideoFrameConverterWithUnwrappedFramesTest;

  // Use FrameResource::unique_id() as internal frame indexing.
  using UniqueID = decltype(std::declval<FrameResource>().unique_id());

  explicit MailboxVideoFrameConverter(
      scoped_refptr<gpu::SharedImageInterface> sii);
  ~MailboxVideoFrameConverter() override;

  // FrameResourceConverter implementation.
  void Destroy() override;
  // Converts |frame| to a gpu::Mailbox-backed VideoFrameResource. If
  // set_get_original_frame_cb() was called with a non-null callback, |frame|
  // must wrap a FrameResource that is retrieved via that callback. Otherwise,
  // |frame| will be used directly. |frame| or the FrameResource that it wraps
  // must be able to be used to create a GpuMemoryBufferHandle. This means that
  // its storage type must be either STORAGE_DMABUFS or
  // STORAGE_GPU_MEMORY_BUFFER. The generated gpu::Mailbox is kept alive until
  // the underlying frame is destroyed.
  void ConvertFrameImpl(scoped_refptr<FrameResource> frame) override;
  bool UsesGetOriginalFrameCBImpl() const override;
  void OnError(const base::Location& location, const std::string& msg) override;

  // Wraps |shared_image| and |frame| into a new VideoFrameResource and sends it
  // via |output_cb_|.
  void WrapSharedImageAndVideoFrameAndOutput(
      FrameResource* origin_frame,
      scoped_refptr<FrameResource> frame,
      scoped_refptr<gpu::ClientSharedImage> shared_image,
      const gpu::SyncToken& sync_token);

  // Returns a ClientSharedImage from a FrameResource. |origin_frame| must be
  // kept alive for the duration of this method. Returns nullptr if the
  // SharedImage could not created successfully (and calls OnError()).
  scoped_refptr<gpu::ClientSharedImage> GenerateSharedImage(
      FrameResource* origin_frame,
      scoped_refptr<FrameResource> frame);

  // Registers the mapping between a FrameResource and the SharedImage.
  // |origin_frame| must be kept alive for the duration of this method. After
  // this method returns, |origin_frame| holds a reference to
  // |client_shared_image|. This guarantees that the SharedImage lives as long
  // as the associated buffer even if MailboxVideoFrameConverter dies.
  void RegisterSharedImage(
      FrameResource* origin_frame,
      scoped_refptr<gpu::ClientSharedImage> client_shared_image);
  // Unregisters the |origin_frame_id| and associated SharedImage, also destroys
  // the SI. |client_shared_image| is passed to check that the
  // |client_shared_image| match what is stored in |shared_images_|.
  void UnregisterSharedImage(
      UniqueID origin_frame_id,
      scoped_refptr<gpu::ClientSharedImage> client_shared_image);

  const scoped_refptr<gpu::SharedImageInterface> shared_image_interface_;

  // Mapping from the unique id of the frame to its corresponding SharedImage.
  // The ClientSharedImage are also referenced the unwrapped FrameResources so
  // that they can be used even after MailboxVideoFrameConverter dies (e.g.,
  // there may still be compositing commands that need the shared images).
  base::small_map<std::map<UniqueID, scoped_refptr<gpu::ClientSharedImage>>>
      shared_images_;

  base::WeakPtrFactory<MailboxVideoFrameConverter> weak_this_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_MAILBOX_VIDEO_FRAME_CONVERTER_H_
