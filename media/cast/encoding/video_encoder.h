// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_ENCODING_VIDEO_ENCODER_H_
#define MEDIA_CAST_ENCODING_VIDEO_ENCODER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_callbacks.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"

namespace media {

class VideoEncoderMetricsProvider;
class GpuVideoAcceleratorFactories;

namespace cast {

struct SenderEncodedFrame;

// This interface encapsulates logic for encoding video frames, and abstracts
// out complexities related to specific encoder implementations.
//
// NOTE: All public methods must be called from the MAIN CastEnvironment thread.
class VideoEncoder {
 public:
  // Callback used to deliver an encoded frame on the MAIN thread.
  using FrameEncodedCallback =
      base::OnceCallback<void(std::unique_ptr<SenderEncodedFrame>)>;

  // Creates a VideoEncoder instance from the given `video_config` and based on
  // the current platform's hardware/library support; or null if no
  // implementation will suffice.  The instance will run `status_change_cb` at
  // some point in the future to indicate initialization success/failure.
  //
  // All VideoEncoder instances returned by this function support encoding
  // sequences of differently-size VideoFrames.
  static std::unique_ptr<VideoEncoder> Create(
      const scoped_refptr<CastEnvironment>& cast_environment,
      const FrameSenderConfig& video_config,
      std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider,
      StatusChangeCallback status_change_cb,
      const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
      media::GpuVideoAcceleratorFactories* gpu_factories);

  virtual ~VideoEncoder() {}

  // If true is returned, the Encoder has accepted the request and will process
  // it asynchronously, running `frame_encoded_callback` on the MAIN
  // CastEnvironment thread with the result--either a valid SenderEncodedFrame
  // or nullptr if an encoding error occurred.  If false is returned, nothing
  // happens and the callback will not be run.
  virtual bool EncodeVideoFrame(
      scoped_refptr<media::VideoFrame> video_frame,
      base::TimeTicks reference_time,
      FrameEncodedCallback frame_encoded_callback) = 0;

  // Inform the encoder about the new target bit rate.
  virtual void SetBitRate(int new_bit_rate) = 0;

  // Inform the encoder to encode the next frame as a key frame.
  virtual void GenerateKeyFrame() = 0;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_ENCODING_VIDEO_ENCODER_H_
