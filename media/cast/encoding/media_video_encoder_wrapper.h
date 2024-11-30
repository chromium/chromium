// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_ENCODING_MEDIA_VIDEO_ENCODER_WRAPPER_H_
#define MEDIA_CAST_ENCODING_MEDIA_VIDEO_ENCODER_WRAPPER_H_

#include <memory>

#include "base/containers/queue.h"
#include "media/base/video_encoder.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/encoding/video_encoder.h"

namespace media {

class VideoEncoderMetricsProvider;
class VideoFrame;

namespace cast {

// This class is written to be called exclusively from the MAIN cast thread.
// All public and private methods CHECK this. Encoding is performed on the VIDEO
// thread through anonymous namespaced free functions.
class MediaVideoEncoderWrapper final : public media::cast::VideoEncoder {
 public:
  MediaVideoEncoderWrapper(
      scoped_refptr<CastEnvironment> cast_environment,
      const FrameSenderConfig& video_config,
      std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider,
      StatusChangeCallback status_change_cb,
      FrameEncodedCallback output_cb,
      const CreateVideoEncodeAcceleratorCallback& create_vea_cb);

  MediaVideoEncoderWrapper(const MediaVideoEncoderWrapper&) = delete;
  MediaVideoEncoderWrapper& operator=(const MediaVideoEncoderWrapper&) = delete;

  ~MediaVideoEncoderWrapper() final;

  // media::cast::VideoEncoder implementation.
  bool EncodeVideoFrame(scoped_refptr<media::VideoFrame> video_frame,
                        base::TimeTicks reference_time) final;
  void SetBitRate(int new_bit_rate) final;
  void GenerateKeyFrame() final;

  // media::VideoEncoder callbacks.
  void OnEncodedFrame(
      VideoEncoderOutput output,
      std::optional<media::VideoEncoder::CodecDescription> description);
  void OnEncoderStatus(EncoderStatus error);
  void OnEncoderInfo(const VideoEncoderInfo& encoder_info);

 private:
  // Metadata associated with a given video frame, that we want to store between
  // beginning and ending encoding. Note that this includes fields NOT in
  // VideoFrameMetadata, such as the timestamp, and does not include all fields
  // of VideoFrameMetadata.
  struct CachedMetadata {
    std::optional<base::TimeTicks> capture_begin_time;
    std::optional<base::TimeTicks> capture_end_time;
    base::TimeTicks encode_start_time;
    RtpTimeTicks rtp_timestamp;
    base::TimeTicks reference_time;
    base::TimeDelta frame_duration;
  };

  // Once we know the frame size on the first call to `EncodeVideoFrame`, we
  // can then construct the encoder.
  void ConstructEncoder();

  // Calculates the predicated frame duration for `frame`. Used to provide
  // metrics on encoder utilization.
  // TODO(crbug.com/282984511): this method is written, in some form, in several
  // places, including the VPX and AV1 encoders both in media/base and in
  // media/cast/encoding. Unify at least some of these as appropriate.
  base::TimeDelta GetFrameDuration(const VideoFrame& frame);

  // Posts a task to update the encoder options, such as whether a key frame
  // is requested.
  void UpdateEncoderOptions();

  // Callback generators. Intended to be called on the VIDEO thread and post
  // back to the MAIN thread.
  media::VideoEncoder::EncoderInfoCB GetInfoCB();
  media::VideoEncoder::EncoderStatusCB GetDoneCB();
  media::VideoEncoder::OutputCB GetOutputCB();

  // Properties set directly from arguments passed at construction.
  scoped_refptr<CastEnvironment> cast_environment_;
  const std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider_;
  StatusChangeCallback status_change_cb_;
  FrameEncodedCallback output_cb_;
  const bool is_hardware_encoder_;
  const VideoCodec codec_;

  // Last recorded encoder status. Used to ensure we do not call
  // `status_change_cb_` repeatedly with the same value, since the
  // media::VideoEncoder implementation frequently updates its status even when
  // everything is OK.
  std::optional<EncoderStatus> last_recorded_status_;

  // The |VideoFrame::timestamp()| of the last encoded frame.  This is used to
  // predict the duration of the next frame.
  std::optional<base::TimeDelta> last_frame_timestamp_;

  // These options are for the entire encoder.
  media::VideoEncoder::Options options_;

  // These options are intended to be per frame.
  media::VideoEncoder::EncodeOptions encode_options_;

  // This member belongs to the video encoder thread. It must not be
  // dereferenced on the main thread. We manage the lifetime of this member
  // manually because it needs to be initialize, used and destroyed on the
  // video encoder thread and video encoder thread can out-live the main thread.
  std::unique_ptr<media::VideoEncoder> encoder_;

  // The ID for the next frame to be emitted.
  FrameId next_frame_id_ = FrameId::first();

  // Metadata associated with recently queued frames.
  base::queue<CachedMetadata> recent_metadata_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaVideoEncoderWrapper> weak_factory_{this};
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_ENCODING_MEDIA_VIDEO_ENCODER_WRAPPER_H_
