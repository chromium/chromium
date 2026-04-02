// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_ENCODING_MEDIA_VIDEO_ENCODER_WRAPPER_H_
#define MEDIA_CAST_ENCODING_MEDIA_VIDEO_ENCODER_WRAPPER_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/task/sequenced_task_runner.h"
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

  // Setter method for the video encoder.
  //
  // Takes an `encoder` with a default deleter, unwraps its contents, and takes
  // ownership of the underlying instance by setting `encoder_`, which has a
  // custom task runner deleter. This is done to ensure that the encoder is
  // always deleted on the VIDEO thread, to avoid lifetime issues.
  void SetEncoder(std::unique_ptr<media::VideoEncoder> encoder);

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

  // If we have any pending updates, any enqueued frames will be rejected. In
  // some cases we may have multiple pending updates in flight.
  int num_pending_updates_ = 0;

  // These options are intended to be per frame.
  media::VideoEncoder::EncodeOptions encode_options_;

  // The backing `encoder_` instance is constructed on the main thread, but
  // should only be dereferenced and destructed on the video thread. Proper
  // destruction is ensured using a custom deleter.
  std::unique_ptr<media::VideoEncoder, base::OnTaskRunnerDeleter> encoder_;

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
