// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_SENDER_VIDEO_SENDER_H_
#define MEDIA_CAST_SENDER_VIDEO_SENDER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_sender.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/sender/congestion_control.h"
#include "media/cast/sender/frame_sender.h"

namespace media {

class VideoFrame;

namespace cast {

class CastTransport;
class VideoEncoder;
class VideoFrameFactory;

typedef base::Callback<void(base::TimeDelta)> PlayoutDelayChangeCB;

// Not thread safe. Only called from the main cast thread.
// This class owns all objects related to sending video, objects that create RTP
// packets, congestion control, video encoder, parsing and sending of
// RTCP packets.
// Additionally it posts a bunch of delayed tasks to the main thread for various
// timeouts.
class VideoSender : public FrameSender {
 public:
  VideoSender(scoped_refptr<CastEnvironment> cast_environment,
              const FrameSenderConfig& video_config,
              const StatusChangeCallback& status_change_cb,
              const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
              const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb,
              CastTransport* const transport_sender,
              const PlayoutDelayChangeCB& playout_delay_change_cb);

  ~VideoSender() override;

  // Note: It is not guaranteed that |video_frame| will actually be encoded and
  // sent, if VideoSender detects too many frames in flight.  Therefore, clients
  // should be careful about the rate at which this method is called.
  void InsertRawVideoFrame(scoped_refptr<media::VideoFrame> video_frame,
                           const base::TimeTicks& reference_time);

  // Creates a |VideoFrameFactory| object to vend |VideoFrame| object with
  // encoder affinity (defined as offering some sort of performance benefit). If
  // the encoder does not have any such capability, returns null.
  std::unique_ptr<VideoFrameFactory> CreateVideoFrameFactory();

  base::WeakPtr<VideoSender> AsWeakPtr();

 protected:
  int GetNumberOfFramesInEncoder() const final;
  base::TimeDelta GetInFlightMediaDuration() const final;

 private:
  // Called by the |video_encoder_| with the next EncodedFrame to send.
  void OnEncodedVideoFrame(scoped_refptr<media::VideoFrame> video_frame,
                           int encoder_bitrate,
                           std::unique_ptr<SenderEncodedFrame> encoded_frame);

  // Encodes media::VideoFrame images into EncodedFrames.  Per configuration,
  // this will point to either the internal software-based encoder or a proxy to
  // a hardware-based encoder.
  std::unique_ptr<VideoEncoder> video_encoder_;

  // The number of frames queued for encoding, but not yet sent.
  int frames_in_encoder_;

  // The duration of video queued for encoding, but not yet sent.
  base::TimeDelta duration_in_encoder_;

  // The timestamp of the frame that was last enqueued in |video_encoder_|.
  RtpTimeTicks last_enqueued_frame_rtp_timestamp_;
  base::TimeTicks last_enqueued_frame_reference_time_;

  // Remember what we set the bitrate to before, no need to set it again if
  // we get the same value.
  int last_bitrate_;

  PlayoutDelayChangeCB playout_delay_change_cb_;

  // Indicates we are operating in a mode where the target playout latency is
  // low for best user experience. When operating in low latency mode, we
  // prefer dropping frames over increasing target playout time.
  bool low_latency_mode_;

  // The video encoder's performance metrics as of the last call to
  // OnEncodedVideoFrame().  See header file comments for SenderEncodedFrame for
  // an explanation of these values.
  double last_reported_encoder_utilization_;
  double last_reported_lossy_utilization_;

  // This tracks the time when the request was sent to encoder to encode a key
  // frame on receiving a Pli message. It is used to limit the sender not
  // to duplicately respond to multiple Pli messages in a short period.
  base::TimeTicks last_time_attempted_to_resolve_pli_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<VideoSender> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VideoSender);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_SENDER_VIDEO_SENDER_H_
