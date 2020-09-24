// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_VD_VIDEO_DECODE_ACCELERATOR_H_
#define MEDIA_GPU_CHROMEOS_VD_VIDEO_DECODE_ACCELERATOR_H_

#include <map>
#include <memory>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/status.h"
#include "media/base/video_decoder.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/vda_video_frame_pool.h"
#include "media/gpu/chromeos/video_frame_converter.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_decode_accelerator.h"

namespace media {

class MediaLog;
class VideoFrame;

// Implements the VideoDecodeAccelerator backed by a VideoDecoder.
// Currently GpuArcVideoDecodeAccelerator bridges the video codec from
// ARC++ to a VDA instance. We plan to deprecate to VDA implementation at
// chromium by new VD implementation. So we need the adapter between
// these two interface.
//
// Important note: This adaptor is only used temporary as an intermediate step
// while the GpuArcVideoDecodeAccelerator is being ported to the new VD
// interface. This Adaptor will be deprecated soon and should not be used
// anywhere else.
class MEDIA_GPU_EXPORT VdVideoDecodeAccelerator
    : public VideoDecodeAccelerator,
      public VdaVideoFramePool::VdaDelegate {
 public:
  // Callback for creating VideoDecoder instance.
  using CreateVideoDecoderCb =
      base::RepeatingCallback<std::unique_ptr<VideoDecoder>(
          scoped_refptr<base::SequencedTaskRunner>,
          std::unique_ptr<DmabufVideoFramePool>,
          std::unique_ptr<VideoFrameConverter>,
          std::unique_ptr<MediaLog>)>;

  // Create VdVideoDecodeAccelerator instance, and call Initialize().
  // Return nullptr if Initialize() failed.
  static std::unique_ptr<VideoDecodeAccelerator> Create(
      CreateVideoDecoderCb create_vd_cb,
      Client* client,
      const Config& config,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  VdVideoDecodeAccelerator(const VdVideoDecodeAccelerator&) = delete;
  VdVideoDecodeAccelerator& operator=(const VdVideoDecodeAccelerator&) = delete;
  ~VdVideoDecodeAccelerator() override;

  // Implementation of VideoDecodeAccelerator.
  bool Initialize(const Config& config, Client* client) override;
  void AssignPictureBuffers(const std::vector<PictureBuffer>& buffers) override;
  void ImportBufferForPicture(
      int32_t picture_buffer_id,
      VideoPixelFormat pixel_format,
      gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) override;
  void ReusePictureBuffer(int32_t picture_buffer_id) override;
  void Decode(BitstreamBuffer bitstream_buffer) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer,
              int32_t bitstream_id) override;
  void Flush() override;
  void Reset() override;
  void Destroy() override;

  // Implementation of VdaVideoFramePool::VdaDelegate.
  void RequestFrames(const Fourcc& fourcc,
                     const gfx::Size& coded_size,
                     const gfx::Rect& visible_rect,
                     size_t max_num_frames,
                     NotifyLayoutChangedCb notify_layout_changed_cb,
                     ImportFrameCb import_frame_cb) override;

 private:
  using DmabufId = DmabufVideoFramePool::DmabufId;

  VdVideoDecodeAccelerator(
      CreateVideoDecoderCb create_vd_cb,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Callback methods of |vd_|.
  void OnInitializeDone(Status status);
  void OnDecodeDone(int32_t bitstream_buffer_id, Status status);
  void OnFrameReady(scoped_refptr<VideoFrame> frame);
  void OnFlushDone(Status status);
  void OnResetDone();

  // Get Picture instance that represents the same buffer as |frame|. Return
  // base::nullopt if the buffer is already dismissed.
  base::Optional<Picture> GetPicture(const VideoFrame& frame);

  // Thunk to post OnFrameReleased() to |task_runner|.
  // Because this thunk may be called in any thread, We don't want to
  // dereference WeakPtr. Therefore we wrap the WeakPtr by base::Optional to
  // avoid the task runner defererencing the WeakPtr.
  static void OnFrameReleasedThunk(
      base::Optional<base::WeakPtr<VdVideoDecodeAccelerator>> weak_this,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      scoped_refptr<VideoFrame> origin_frame);
  // Called when a frame gets destroyed.
  void OnFrameReleased(scoped_refptr<VideoFrame> origin_frame);

  // Called when any error occurs. Notify |client_| an error occurred.
  void OnError(base::Location location, Error error);

  // Callback to generate VideoDecoder.
  CreateVideoDecoderCb create_vd_cb_;
  // The client of this VDA.
  VideoDecodeAccelerator::Client* client_ = nullptr;
  // The delegated VideoDecoder instance.
  std::unique_ptr<VideoDecoder> vd_;
  // Callback for returning the result after this instance is asked to request
  // new frames. The VdaVideoFramePool is blocked until this callback is called.
  NotifyLayoutChangedCb notify_layout_changed_cb_;
  // Callback for passing the available frames to the pool.
  ImportFrameCb import_frame_cb_;

  // The size requested from VdaVideoFramePool.
  gfx::Size pending_coded_size_;
  // The formats of the current buffers.
  gfx::Size coded_size_;
  base::Optional<VideoFrameLayout> layout_;

  // Mapping from VideoFrame's DmabufId to picture buffer id.
  std::map<DmabufId, int32_t /* picture_buffer_id */> frame_id_to_picture_id_;
  // Record how many times the picture is sent to the client, and keep a refptr
  // of corresponding VideoFrame when the client owns the buffers.
  std::map<int32_t /* picture_buffer_id */,
           std::pair<scoped_refptr<VideoFrame>, size_t /* num_sent */>>
      picture_at_client_;

  // Main task runner and its sequence checker. All methods should be called
  // on it.
  scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  SEQUENCE_CHECKER(client_sequence_checker_);

  // The weak pointer of this class instance, bound to |client_task_runner_|.
  base::WeakPtr<VdVideoDecodeAccelerator> weak_this_;
  base::WeakPtrFactory<VdVideoDecodeAccelerator> weak_this_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_VD_VIDEO_DECODE_ACCELERATOR_H_
