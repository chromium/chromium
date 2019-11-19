// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_MAILBOX_VIDEO_FRAME_CONVERTER_H_
#define MEDIA_GPU_CHROMEOS_MAILBOX_VIDEO_FRAME_CONVERTER_H_

#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "base/containers/small_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/gpu/chromeos/video_frame_converter.h"
#include "media/gpu/media_gpu_export.h"

namespace base {
class Location;
class SingleThreadTaskRunner;
}  // namespace base

namespace gpu {
class GpuChannel;
class CommandBufferStub;
}  // namespace gpu

namespace media {

class VideoFrame;

// This class is used for converting DMA-buf backed VideoFrames to mailbox-based
// VideoFrames. See ConvertFrame() for more details.
// After conversion, the mailbox VideoFrame will retain a reference of the
// VideoFrame passed to ConvertFrame().
class MEDIA_GPU_EXPORT MailboxVideoFrameConverter : public VideoFrameConverter {
 public:
  using UnwrapFrameCB =
      base::RepeatingCallback<VideoFrame*(const VideoFrame& wrapped_frame)>;
  using GetCommandBufferStubCB =
      base::RepeatingCallback<gpu::CommandBufferStub*()>;
  using GetGpuChannelCB =
      base::RepeatingCallback<base::WeakPtr<gpu::GpuChannel>()>;

  // Creates a MailboxVideoFrameConverter instance. The callers will send
  // wrapped VideoFrames to ConvertFrame(), |unwrap_frame_cb| is the callback
  // used to get the original, unwrapped, VideoFrame. |gpu_task_runner| is the
  // task runner of the GPU main thread. Returns nullptr if any argument is
  // invalid.
  static std::unique_ptr<VideoFrameConverter> Create(
      UnwrapFrameCB unwrap_frame_cb,
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      GetCommandBufferStubCB get_stub_cb);

  // Enqueues |frame| to be converted to a gpu::Mailbox backed VideoFrame.
  // |frame| must wrap a DMA-buf backed VideoFrame that is retrieved via
  // |unwrap_frame_cb_|. The generated gpu::Mailbox-based VideoFrame is kept
  // alive until the original (i.e. the unwrapped) DMA-Buf based VideoFrame one
  // goes out of scope.
  void ConvertFrame(scoped_refptr<VideoFrame> frame) override;
  void AbortPendingFrames() override;
  bool HasPendingFrames() const override;

 private:
  friend class MailboxVideoFrameConverterTest;

  // Use VideoFrame::unique_id() as internal VideoFrame indexing.
  using UniqueID = decltype(std::declval<VideoFrame>().unique_id());

  // A self-cleaning SharedImage, with move-only semantics.
  class ScopedSharedImage;

  MailboxVideoFrameConverter(
      UnwrapFrameCB unwrap_frame_cb,
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      GetGpuChannelCB get_gpu_channel_cb);
  // Destructor runs on the GPU main thread.
  ~MailboxVideoFrameConverter() override;
  void Destroy() override;
  void DestroyOnGPUThread();

  // TODO(crbug.com/998279): replace s/OnGPUThread/OnGPUTaskRunner/.
  bool InitializeOnGPUThread();

  // Wraps |mailbox| and |frame| into a new VideoFrame and sends it via
  // |output_cb_|.
  void WrapMailboxAndVideoFrameAndOutput(VideoFrame* origin_frame,
                                         scoped_refptr<VideoFrame> frame,
                                         const gpu::Mailbox& mailbox);

  // ConvertFrame() delegates to this method to GenerateSharedImageOnGPUThread()
  // or just UpdateSharedImageOnGPUThread(), then to jump back to
  // WrapMailboxAndVideoFrameAndOutput().
  void ConvertFrameOnGPUThread(VideoFrame* origin_frame,
                               scoped_refptr<VideoFrame> frame,
                               gpu::Mailbox mailbox);

  // Generates a ScopedSharedImage from a DMA-buf backed |video_frame|, and
  // returns it or nullptr if that could not be done. |video_frame| must be kept
  // alive for the duration of this method. This method runs on
  // |gpu_task_runner_|.
  std::unique_ptr<ScopedSharedImage> GenerateSharedImageOnGPUThread(
      VideoFrame* video_frame);

  // Registers the mapping between a DMA-buf VideoFrame and the SharedImage.
  // |origin_frame| must be kept alive for the duration of this method.
  void RegisterSharedImage(
      VideoFrame* origin_frame,
      std::unique_ptr<ScopedSharedImage> scoped_shared_image);
  // Unregisters the |origin_frame_id| and associated SharedImage.
  void UnregisterSharedImage(UniqueID origin_frame_id);

  // Updates the SharedImage associated to |mailbox|. Returns true if the update
  // could be carried out, false otherwise.
  bool UpdateSharedImageOnGPUThread(const gpu::Mailbox& mailbox);

  // Waits on |sync_token|, keeping |frame| alive until it is signalled. It
  // trampolines threads to |gpu_task_runner| if necessary.
  void WaitOnSyncTokenAndReleaseFrameOnGPUThread(
      scoped_refptr<VideoFrame> frame,
      const gpu::SyncToken& sync_token);

  // Invoked when any error occurs. |msg| is the error message.
  void OnError(const base::Location& location, const std::string& msg);

  // In DmabufVideoFramePool, we recycle the unused frames. To do that, each
  // time a frame is requested from the pool it is wrapped inside another frame.
  // A destruction callback is then added to this wrapped frame to automatically
  // return it to the pool upon destruction. Unfortunately this means that a new
  // frame is returned each time, and we need a way to uniquely identify the
  // underlying frame to avoid converting the same frame multiple times.
  // |unwrap_frame_cb_| is used to get the origin frame.
  UnwrapFrameCB unwrap_frame_cb_;

  const scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  const GetGpuChannelCB get_gpu_channel_cb_;

  // |gpu_channel_| will outlive CommandBufferStub, keep the former as a WeakPtr
  // to guarantee proper resource cleanup. To be dereferenced on
  // |gpu_task_runner_| only.
  base::WeakPtr<gpu::GpuChannel> gpu_channel_;

  // Mapping from the unique id of the frame to its corresponding SharedImage.
  // Accessed only on |parent_task_runner_|.
  base::small_map<std::map<UniqueID, std::unique_ptr<ScopedSharedImage>>>
      shared_images_;

  // The queue of input frames and the unique_id of their origin frame.
  // Accessed only on |parent_task_runner_|.
  // TODO(crbug.com/998279): remove this member entirely.
  base::queue<std::pair<scoped_refptr<VideoFrame>, UniqueID>>
      input_frame_queue_;

  // The weak pointer of this, bound to |parent_task_runner_|.
  // Used at the VideoFrame destruction callback.
  base::WeakPtr<MailboxVideoFrameConverter> parent_weak_this_;
  // The weak pointer of this, bound to |gpu_task_runner_|.
  // Used to generate SharedImages on the GPU main thread.
  base::WeakPtr<MailboxVideoFrameConverter> gpu_weak_this_;
  base::WeakPtrFactory<MailboxVideoFrameConverter> parent_weak_this_factory_{
      this};
  base::WeakPtrFactory<MailboxVideoFrameConverter> gpu_weak_this_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MailboxVideoFrameConverter);
};

}  // namespace media
#endif  // MEDIA_GPU_CHROMEOS_MAILBOX_VIDEO_FRAME_CONVERTER_H_
