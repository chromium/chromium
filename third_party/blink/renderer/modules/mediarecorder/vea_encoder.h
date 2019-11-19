// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_VEA_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_VEA_ENCODER_H_

#include "base/containers/queue.h"
#include "base/single_thread_task_runner.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"

#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace media {
class GpuVideoAcceleratorFactories;
}  // namespace media

namespace blink {

// Class encapsulating VideoEncodeAccelerator interactions.
// This class is created and destroyed on its owner thread. All other methods
// operate on the task runner pointed by GpuFactories.
class VEAEncoder final : public VideoTrackRecorder::Encoder,
                         public media::VideoEncodeAccelerator::Client {
 public:
  static scoped_refptr<VEAEncoder> Create(
      const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_callback,
      const VideoTrackRecorder::OnErrorCB& on_error_callback,
      int32_t bits_per_second,
      media::VideoCodecProfile codec,
      const gfx::Size& size,
      bool use_native_input,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // media::VideoEncodeAccelerator::Client implementation.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;
  void BitstreamBufferReady(
      int32_t bitstream_buffer_id,
      const media::BitstreamBufferMetadata& metadata) override;
  void NotifyError(media::VideoEncodeAccelerator::Error error) override;

 private:
  using VideoFrameAndTimestamp =
      std::pair<scoped_refptr<media::VideoFrame>, base::TimeTicks>;
  using VideoParamsAndTimestamp =
      std::pair<media::WebmMuxer::VideoParameters, base::TimeTicks>;

  struct InputBuffer {
    base::UnsafeSharedMemoryRegion region;
    base::WritableSharedMemoryMapping mapping;
  };

  struct OutputBuffer {
    base::UnsafeSharedMemoryRegion region;
    base::WritableSharedMemoryMapping mapping;

    bool IsValid();
  };

  VEAEncoder(
      const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_callback,
      const VideoTrackRecorder::OnErrorCB& on_error_callback,
      int32_t bits_per_second,
      media::VideoCodecProfile codec,
      const gfx::Size& size,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  void UseOutputBitstreamBufferId(int32_t bitstream_buffer_id);
  void FrameFinished(std::unique_ptr<InputBuffer> shm);

  // VideoTrackRecorder::Encoder implementation.
  ~VEAEncoder() override;
  void EncodeOnEncodingTaskRunner(scoped_refptr<media::VideoFrame> frame,
                                  base::TimeTicks capture_timestamp) override;

  void ConfigureEncoderOnEncodingTaskRunner(const gfx::Size& size,
                                            bool use_native_input);

  void DestroyOnEncodingTaskRunner(base::WaitableEvent* async_waiter = nullptr);

  media::GpuVideoAcceleratorFactories* const gpu_factories_;

  const media::VideoCodecProfile codec_;

  // The underlying VEA to perform encoding on.
  std::unique_ptr<media::VideoEncodeAccelerator> video_encoder_;

  // Shared memory buffers for output with the VEA.
  Vector<std::unique_ptr<OutputBuffer>> output_buffers_;

  // Shared memory buffers for output with the VEA as FIFO.
  // TODO(crbug.com/960665): Replace with a WTF equivalent.
  base::queue<std::unique_ptr<InputBuffer>> input_buffers_;

  // Tracks error status.
  bool error_notified_;

  // Tracks the last frame that we delay the encode.
  std::unique_ptr<VideoFrameAndTimestamp> last_frame_;

  // Size used to initialize encoder.
  gfx::Size input_visible_size_;

  // Coded size that encoder requests as input.
  gfx::Size vea_requested_input_coded_size_;

  // Frames and corresponding timestamps in encode as FIFO.
  // TODO(crbug.com/960665): Replace with a WTF equivalent.
  base::queue<VideoParamsAndTimestamp> frames_in_encode_;

  // Number of encoded frames produced consecutively without a keyframe.
  uint32_t num_frames_after_keyframe_;

  // Forces next frame to be a keyframe.
  bool force_next_frame_to_be_keyframe_;

  // This callback can be exercised on any thread.
  const VideoTrackRecorder::OnErrorCB on_error_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_VEA_ENCODER_H_
