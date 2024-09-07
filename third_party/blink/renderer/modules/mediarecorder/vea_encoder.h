// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_VEA_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_VEA_ENCODER_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/base/bitrate.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/sequence_bound.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/size.h"

namespace media {
class GpuVideoAcceleratorFactories;
}  // namespace media

namespace blink {

// Class encapsulating VideoEncodeAccelerator interactions.
// This class needs to be created on the main thread. All other methods
// operate on Platform::Current()->GetGpuFactories()->GetTaskRunner().
class VEAEncoder final : public VideoTrackRecorder::Encoder,
                         public media::VideoEncodeAccelerator::Client {
 public:
  VEAEncoder(scoped_refptr<base::SequencedTaskRunner> encoding_task_runner,
             const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_cb,
             const VideoTrackRecorder::OnErrorCB& on_error_cb,
             media::Bitrate::Mode bitrate_mode,
             uint32_t bits_per_second,
             media::VideoCodecProfile codec,
             std::optional<uint8_t> level,
             const gfx::Size& size,
             bool use_native_input,
             bool is_screencast);
  ~VEAEncoder() override;

  // media::VideoEncodeAccelerator::Client implementation.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;
  void BitstreamBufferReady(
      int32_t bitstream_buffer_id,
      const media::BitstreamBufferMetadata& metadata) override;
  void NotifyErrorStatus(const media::EncoderStatus& status) override;

 private:
  struct VideoFrameAndMetadata {
    VideoFrameAndMetadata(scoped_refptr<media::VideoFrame> frame,
                          base::TimeTicks timestamp,
                          bool request_keyframe)
        : frame(frame),
          timestamp(timestamp),
          request_keyframe(request_keyframe) {}
    scoped_refptr<media::VideoFrame> frame;
    base::TimeTicks timestamp;
    bool request_keyframe;
  };
  using VideoParamsAndTimestamp =
      std::pair<media::Muxer::VideoParameters, base::TimeTicks>;
  friend class base::SequenceBound<blink::VEAEncoder,
                                   WTF::internal::SequenceBoundBindTraits>;

  struct OutputBuffer {
    base::UnsafeSharedMemoryRegion region;
    base::WritableSharedMemoryMapping mapping;

    bool IsValid();
  };

  void UseOutputBitstreamBufferId(int32_t bitstream_buffer_id);
  void FrameFinished(std::unique_ptr<base::MappedReadOnlyRegion> shm);

  // VideoTrackRecorder::Encoder implementation.
  void Initialize() override;
  void EncodeFrame(scoped_refptr<media::VideoFrame> frame,
                   base::TimeTicks capture_timestamp,
                   bool request_keyframe) override;

  void ConfigureEncoder(const gfx::Size& size, bool use_native_input);

  const raw_ptr<media::GpuVideoAcceleratorFactories> gpu_factories_;
  const media::VideoCodecProfile codec_;
  const std::optional<uint8_t> level_;
  const media::Bitrate::Mode bitrate_mode_;

  // Attributes for initialization.
  const gfx::Size size_;
  const bool use_native_input_;
  const bool is_screencast_;

  // The underlying VEA to perform encoding on.
  std::unique_ptr<media::VideoEncodeAccelerator> video_encoder_;

  // Shared memory buffers for output with the VEA.
  Vector<std::unique_ptr<OutputBuffer>> output_buffers_;

  Vector<std::unique_ptr<base::MappedReadOnlyRegion>> input_buffers_;

  // Tracks error status.
  bool error_notified_;

  // Tracks the last frame that we delay the encode.
  std::unique_ptr<VideoFrameAndMetadata> last_frame_;

  // Size used to initialize encoder.
  gfx::Size input_visible_size_;

  // Coded size that encoder requests as input.
  gfx::Size vea_requested_input_coded_size_;

  // Frames and corresponding timestamps in encode as FIFO.
  // TODO(crbug.com/960665): Replace with a WTF equivalent.
  base::queue<VideoParamsAndTimestamp> frames_in_encode_;

  const VideoTrackRecorder::OnErrorCB on_error_cb_;
  base::WeakPtrFactory<VEAEncoder> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_VEA_ENCODER_H_
