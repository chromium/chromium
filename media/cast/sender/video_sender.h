// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_SENDER_VIDEO_SENDER_H_
#define MEDIA_CAST_SENDER_VIDEO_SENDER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/capture/video/video_capture_feedback.h"
#include "media/cast/cast_callbacks.h"
#include "media/cast/cast_config.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/sender/frame_sender.h"

namespace openscreen::cast {
class Sender;
}

namespace media {
class VideoEncoderMetricsProvider;
class VideoFrame;
}

namespace media::cast {

class VideoEncoder;
class VideoFrameFactory;

using PlayoutDelayChangeCB = base::RepeatingCallback<void(base::TimeDelta)>;

// Not thread safe. Only called from the main cast thread.
// This class owns all objects related to sending video, objects that create RTP
// packets, congestion control, video encoder, parsing and sending of
// RTCP packets.
// Additionally it posts a bunch of delayed tasks to the main thread for various
// timeouts.
class VideoSender : public FrameSender::Client {
 public:
  // New way of instantiating using an openscreen::cast::Sender. Since the
  // |Sender| instance is destroyed when renegotiation is complete, |this|
  // is also invalid and should be immediately torn down.
  VideoSender(scoped_refptr<CastEnvironment> cast_environment,
              const FrameSenderConfig& video_config,
              StatusChangeCallback status_change_cb,
              const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
              std::unique_ptr<openscreen::cast::Sender> sender,
              std::unique_ptr<media::VideoEncoderMetricsProvider>
                  encoder_metrics_provider,
              PlayoutDelayChangeCB playout_delay_change_cb,
              media::VideoCaptureFeedbackCB feedback_cb,
              FrameSender::GetSuggestedVideoBitrateCB get_bitrate_cb);

  VideoSender(const VideoSender&) = delete;
  VideoSender& operator=(const VideoSender&) = delete;

  ~VideoSender() override;

  // Note: It is not guaranteed that |video_frame| will actually be encoded and
  // sent, if VideoSender detects too many frames in flight.  Therefore, clients
  // should be careful about the rate at which this method is called.
  virtual void InsertRawVideoFrame(scoped_refptr<media::VideoFrame> video_frame,
                                   base::TimeTicks reference_time);

  // Creates a |VideoFrameFactory| object to vend |VideoFrame| object with
  // encoder affinity (defined as offering some sort of performance benefit). If
  // the encoder does not have any such capability, returns null.
  std::unique_ptr<VideoFrameFactory> CreateVideoFrameFactory();

  void SetTargetPlayoutDelay(base::TimeDelta new_target_playout_delay);
  base::TimeDelta GetTargetPlayoutDelay() const;

  base::WeakPtr<VideoSender> AsWeakPtr();

 protected:
  // For mocking in unit tests.
  VideoSender();

  // FrameSender::Client overrides.
  int GetNumberOfFramesInEncoder() const final;
  base::TimeDelta GetEncoderBacklogDuration() const final;

 private:
  // Called by the |video_encoder_| with the next EncodedFrame to send.
  void OnEncodedVideoFrame(scoped_refptr<media::VideoFrame> video_frame,
                           const base::TimeTicks reference_time,
                           std::unique_ptr<SenderEncodedFrame> encoded_frame);

  // The backing frame sender implementation.
  std::unique_ptr<FrameSender> frame_sender_;

  // Encodes media::VideoFrame images into EncodedFrames.  Per configuration,
  // this will point to either the internal software-based encoder or a proxy to
  // a hardware-based encoder.
  std::unique_ptr<VideoEncoder> video_encoder_;

  scoped_refptr<CastEnvironment> cast_environment_;

  // The number of frames queued for encoding, but not yet sent.
  int frames_in_encoder_ = 0;

  // The duration of video queued for encoding, but not yet sent.
  base::TimeDelta duration_in_encoder_;

  // The timestamp of the frame that was last enqueued in |video_encoder_|.
  RtpTimeTicks last_enqueued_frame_rtp_timestamp_;
  base::TimeTicks last_enqueued_frame_reference_time_;

  // Remember what we set the bitrate to before, no need to set it again if
  // we get the same value.
  int last_bitrate_ = 0;

  // The total amount of time between a frame's capture/recording on the sender
  // and its playback on the receiver (i.e., shown to a user).
  base::TimeDelta min_playout_delay_;
  base::TimeDelta max_playout_delay_;

  PlayoutDelayChangeCB playout_delay_change_cb_;

  media::VideoCaptureFeedbackCB feedback_cb_;

  // Indicates we are operating in a mode where the target playout latency is
  // low for best user experience. When operating in low latency mode, we
  // prefer dropping frames over increasing target playout time.
  bool low_latency_mode_ = false;

  // The video encoder's performance metrics as of the last call to
  // OnEncodedVideoFrame().  See header file comments for SenderEncodedFrame for
  // an explanation of these values.
  double last_reported_encoder_utilization_ = -1.0;
  double last_reported_lossiness_ = -1.0;

  // Used to calculate the percentage of lost frames. We currently report this
  // metric as the number of frames dropped in the entire session.
  int number_of_frames_inserted_ = 0;
  int number_of_frames_dropped_ = 0;

  // Used to throttle metrics reporting of the bitrate.
  int frames_since_bitrate_reported_ = 0;

  // This tracks the time when the request was sent to encoder to encode a key
  // frame on receiving a Pli message. It is used to limit the sender not
  // to duplicately respond to multiple Pli messages in a short period.
  base::TimeTicks last_time_attempted_to_resolve_pli_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<VideoSender> weak_factory_{this};
};

}  // namespace media::cast

#endif  // MEDIA_CAST_SENDER_VIDEO_SENDER_H_
