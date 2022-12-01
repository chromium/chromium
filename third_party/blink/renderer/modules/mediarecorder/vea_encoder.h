// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_VEA_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_VEA_ENCODER_H_

#include "base/containers/queue.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/time/time.h"
#include "media/base/bitrate.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class WaitableEvent;
class SequencedTaskRunner;
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
      const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_cb,
      const VideoTrackRecorder::OnErrorCB& on_error_cb,
      media::Bitrate::Mode bitrate_mode,
      uint32_t bits_per_second,
      media::VideoCodecProfile codec,
      absl::optional<uint8_t> level,
      const gfx::Size& size,
      bool use_native_input,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

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
      std::pair<media::Muxer::VideoParameters, base::TimeTicks>;

  struct OutputBuffer {
    base::UnsafeSharedMemoryRegion region;
    base::WritableSharedMemoryMapping mapping;

    bool IsValid();
  };

  VEAEncoder(const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_cb,
             const VideoTrackRecorder::OnErrorCB& on_error_cb,
             media::Bitrate::Mode bitrate_mode,
             uint32_t bits_per_second,
             media::VideoCodecProfile codec,
             absl::optional<uint8_t> level,
             const gfx::Size& size,
             scoped_refptr<base::SequencedTaskRunner> task_runner);

  void UseOutputBitstreamBufferId(int32_t bitstream_buffer_id);
  void FrameFinished(std::unique_ptr<base::MappedReadOnlyRegion> shm);

  // VideoTrackRecorder::Encoder implementation.
  ~VEAEncoder() override;
  void EncodeOnEncodingTaskRunner(scoped_refptr<media::VideoFrame> frame,
                                  base::TimeTicks capture_timestamp) override;

  void ConfigureEncoderOnEncodingTaskRunner(const gfx::Size& size,
                                            bool use_native_input);

  void DestroyOnEncodingTaskRunner(base::WaitableEvent* async_waiter = nullptr);

  media::GpuVideoAcceleratorFactories* const gpu_factories_;

  const media::VideoCodecProfile codec_;

  const absl::optional<uint8_t> level_;

  const media::Bitrate::Mode bitrate_mode_;

  // The underlying VEA to perform encoding on.
  std::unique_ptr<media::VideoEncodeAccelerator> video_encoder_;

  // Shared memory buffers for output with the VEA.
  Vector<std::unique_ptr<OutputBuffer>> output_buffers_;

  Vector<std::unique_ptr<base::MappedReadOnlyRegion>> input_buffers_;

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
  const VideoTrackRecorder::OnErrorCB on_error_cb_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_VEA_ENCODER_H_
