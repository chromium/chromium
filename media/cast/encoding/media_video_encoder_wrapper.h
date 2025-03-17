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

class GpuVideoAcceleratorFactories;
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
      GpuVideoAcceleratorFactories* gpu_factories);

  MediaVideoEncoderWrapper(const MediaVideoEncoderWrapper&) = delete;
  MediaVideoEncoderWrapper& operator=(const MediaVideoEncoderWrapper&) = delete;

  ~MediaVideoEncoderWrapper() final;

  // media::cast::VideoEncoder implementation.
  bool EncodeVideoFrame(scoped_refptr<VideoFrame> video_frame,
                        base::TimeTicks reference_time,
                        FrameEncodedCallback frame_encoded_callback) final;
  void SetBitRate(int new_bit_rate) final;
  void GenerateKeyFrame() final;

  // media::VideoEncoder callbacks.
  void OnEncodedFrame(
      VideoEncoderOutput output,
      std::optional<media::VideoEncoder::CodecDescription> description);
  void OnEncoderStatus(EncoderStatus error);
  void OnEncoderInfo(const VideoEncoderInfo& encoder_info);

  // Test-only API to override the backing video encoder implementation.
  void SetEncoderForTesting(std::unique_ptr<media::VideoEncoder> encoder);

 private:
  // Metadata associated with a given video frame, that we want to store between
  // beginning and ending encoding. Note that this includes fields NOT in
  // VideoFrameMetadata, such as the timestamp, and does not include all fields
  // of VideoFrameMetadata.
  struct CachedMetadata {
    CachedMetadata(std::optional<base::TimeTicks> capture_begin_time,
                   std::optional<base::TimeTicks> capture_end_time,
                   base::TimeTicks encode_start_time,
                   RtpTimeTicks rtp_timestamp,
                   base::TimeTicks reference_time,
                   base::TimeDelta frame_duration,
                   FrameEncodedCallback frame_encoded_callback);
    CachedMetadata();
    // This type is move-only due to `frame_encoded_callback`.
    CachedMetadata(const CachedMetadata& other) = delete;
    CachedMetadata& operator=(const CachedMetadata& other) = delete;
    CachedMetadata(CachedMetadata&& other);
    CachedMetadata& operator=(CachedMetadata&& other);
    ~CachedMetadata();

    std::optional<base::TimeTicks> capture_begin_time;
    std::optional<base::TimeTicks> capture_end_time;
    base::TimeTicks encode_start_time;
    RtpTimeTicks rtp_timestamp;
    base::TimeTicks reference_time;
    base::TimeDelta frame_duration;
    FrameEncodedCallback frame_encoded_callback;
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
  void OnOptionsUpdated(EncoderStatus status);

  // We currently manage the threads used for interacting with the encoder
  // manually. Hardware encoding demands posting to the "accelerator thread"
  // which is the same as the dedicated VIDEO thread. Software encoding makes no
  // such demands, but should not be ran on the MAIN thread in order to avoid
  // blocking.
  //
  // In order to avoid creating a third thread purely for the accelerator class,
  // and then trampolining calls from MAIN -> VIDEO -> ACCELERATOR, this private
  // method is used to ensure we invoke the encoder method on the correct
  // thread. Any usage of `encoder_` should be wrapped in this method.
  void CallEncoderOnCorrectThread(base::OnceClosure closure);

  // Called every time a frame encode request is completed. The encoder only
  // calls OnEncodedFrame() if the frame encoding was completed, however
  // OnFrameEncodeDone() is always called, regardless of success.
  //
  // NOTE: the media::VideoEncoder API makes no guarantees on what order the two
  // callbacks get called in.
  void OnFrameEncodeDone(base::TimeTicks reference_time, EncoderStatus status);

  // Callback generator. Returned callbacks are intended to be called on the
  // VIDEO thread and post back to the MAIN thread.
  template <typename Method, typename... Args>
  auto CreateCallback(Method&& method, Args&&... args) {
    CHECK(cast_environment_->CurrentlyOn(CastEnvironment::ThreadId::kMain));
    return base::BindPostTask(
        cast_environment_->GetTaskRunner(CastEnvironment::ThreadId::kMain),
        base::BindRepeating(std::forward<Method>(method),
                            weak_factory_.GetWeakPtr(), args...));
  }

  // Properties set directly from arguments passed at construction.
  scoped_refptr<CastEnvironment> cast_environment_;
  const std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider_;
  StatusChangeCallback status_change_cb_;
  raw_ptr<GpuVideoAcceleratorFactories> gpu_factories_;
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

  // If true, we are currently updating options, and any enqueued frames will
  // be rejected.
  bool is_updating_options_ = false;

  // These options are intended to be per frame.
  media::VideoEncoder::EncodeOptions encode_options_;

  // This member belongs to the video encoder thread. It must not be
  // dereferenced on the main thread. We manage the lifetime of this member
  // manually because it needs to be initialize, used and destroyed on the
  // video encoder thread and video encoder thread can out-live the main thread.
  std::unique_ptr<media::VideoEncoder> encoder_;

  // If true, we use the overridden encoder and do not update it when the frame
  // size changes.
  bool encoder_is_overridden_for_testing_ = false;

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
