// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_ANDROID_VIDEO_ENCODE_ACCELERATOR_H_
#define MEDIA_GPU_ANDROID_ANDROID_VIDEO_ENCODE_ACCELERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/base/android/media_codec_bridge_impl.h"
#include "media/base/bitrate.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

class BitstreamBuffer;

// Android-specific implementation of VideoEncodeAccelerator, enabling
// hardware-acceleration of video encoding, based on Android's MediaCodec class
// (http://developer.android.com/reference/android/media/MediaCodec.html).  This
// class expects to live and be called on a single thread (the GPU process'
// ChildThread).
class MEDIA_GPU_EXPORT AndroidVideoEncodeAccelerator
    : public VideoEncodeAccelerator {
 public:
  AndroidVideoEncodeAccelerator();

  AndroidVideoEncodeAccelerator(const AndroidVideoEncodeAccelerator&) = delete;
  AndroidVideoEncodeAccelerator& operator=(
      const AndroidVideoEncodeAccelerator&) = delete;

  ~AndroidVideoEncodeAccelerator() override;

  // VideoEncodeAccelerator implementation.
  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles() override;
  bool Initialize(const Config& config,
                  Client* client,
                  std::unique_ptr<MediaLog> media_log) override;
  void Encode(scoped_refptr<VideoFrame> frame, bool force_keyframe) override;
  void UseOutputBitstreamBuffer(BitstreamBuffer buffer) override;
  void RequestEncodingParametersChange(
      const Bitrate& bitrate,
      uint32_t framerate,
      const std::optional<gfx::Size>& size) override;
  void Destroy() override;

 private:
  enum {
    // Arbitrary choice.
    INITIAL_FRAMERATE = 30,
    // Default I-Frame interval in seconds.
    IFRAME_INTERVAL_H264 = 20,
    IFRAME_INTERVAL_VPX = 100,
    IFRAME_INTERVAL = INT32_MAX,
  };

  // Impedance-mismatch fixers: MediaCodec is a poll-based API but VEA is a
  // push-based API; these methods turn the crank to make the two work together.
  void DoIOTask();
  void QueueInput();
  void DequeueOutput();

  // Start & stop |io_timer_| if the time seems right.
  void MaybeStartIOTimer();
  void MaybeStopIOTimer();

  // Ask MediaCodec what input buffer layout it prefers and set values of
  // |input_buffer_stride_| and |input_buffer_yplane_height_|. If the codec
  // does not provide these values, sets up |aligned_size_| such that encoded
  // frames are cropped to the nearest 16x16 alignment.
  bool SetInputBufferLayout();

  void NotifyErrorStatus(EncoderStatus status);

  // Used to DCHECK that we are called on the correct sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // VideoDecodeAccelerator::Client callbacks go here.  Invalidated once any
  // error triggers.
  std::unique_ptr<base::WeakPtrFactory<Client>> client_ptr_factory_;

  std::unique_ptr<MediaLog> log_;

  std::unique_ptr<MediaCodecBridge> media_codec_;

  // Bitstream buffers waiting to be populated & returned to the client.
  std::vector<BitstreamBuffer> available_bitstream_buffers_;

  // Frames waiting to be passed to the codec, queued until an input buffer is
  // available.  Each element is a tuple of <Frame, key_frame, enqueue_time>.
  using PendingFrames =
      base::queue<std::tuple<scoped_refptr<VideoFrame>, bool, base::Time>>;
  PendingFrames pending_frames_;

  // Repeating timer responsible for draining pending IO to the codec.
  base::RepeatingTimer io_timer_;

  // The difference between number of buffers queued & dequeued at the codec.
  int32_t num_buffers_at_codec_ = 0;

  // A monotonically-growing value.
  base::TimeDelta presentation_timestamp_;

  std::map<base::TimeDelta /* presentation_timestamp */,
           base::TimeDelta /* frame_timestamp */>
      frame_timestamp_map_;

  // Resolution of input stream. Set once in initialization and not allowed to
  // change after.
  gfx::Size frame_size_;

  // Y and UV plane strides in the encoder's input buffer
  int input_buffer_stride_ = 0;

  // Y-plane height in the encoder's input
  int input_buffer_yplane_height_ = 0;

  uint32_t last_set_bitrate_ = 0;  // In bps.

  // True if there is encoder error.
  bool error_occurred_ = false;

  // Required for encoders which are missing stride information.
  std::optional<gfx::Size> aligned_size_;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_ANDROID_VIDEO_ENCODE_ACCELERATOR_H_
