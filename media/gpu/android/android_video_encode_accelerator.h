// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_ANDROID_VIDEO_ENCODE_ACCELERATOR_H_
#define MEDIA_GPU_ANDROID_ANDROID_VIDEO_ENCODE_ACCELERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "media/base/android/media_codec_bridge_impl.h"
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
  ~AndroidVideoEncodeAccelerator() override;

  // VideoEncodeAccelerator implementation.
  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles() override;
  bool Initialize(const Config& config, Client* client) override;
  void Encode(scoped_refptr<VideoFrame> frame, bool force_keyframe) override;
  void UseOutputBitstreamBuffer(BitstreamBuffer buffer) override;
  void RequestEncodingParametersChange(uint32_t bitrate,
                                       uint32_t framerate) override;
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

  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  // VideoDecodeAccelerator::Client callbacks go here.  Invalidated once any
  // error triggers.
  std::unique_ptr<base::WeakPtrFactory<Client>> client_ptr_factory_;

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
  int32_t num_buffers_at_codec_;

  // A monotonically-growing value.
  base::TimeDelta presentation_timestamp_;

  std::map<base::TimeDelta /* presentation_timestamp */,
           base::TimeDelta /* frame_timestamp */>
      frame_timestamp_map_;

  // Resolution of input stream. Set once in initialization and not allowed to
  // change after.
  gfx::Size frame_size_;

  uint32_t last_set_bitrate_;  // In bps.

  // True if there is encoder error.
  bool error_occurred_;

  DISALLOW_COPY_AND_ASSIGN(AndroidVideoEncodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_ANDROID_VIDEO_ENCODE_ACCELERATOR_H_
