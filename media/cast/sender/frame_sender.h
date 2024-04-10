// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_SENDER_FRAME_SENDER_H_
#define MEDIA_CAST_SENDER_FRAME_SENDER_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/constants.h"

namespace openscreen::cast {
class Sender;
}

namespace media::cast {

struct SenderEncodedFrame;
class CastEnvironment;

// This is the pure virtual interface for an object that sends encoded frames
// to a receiver.
class FrameSender {
 public:
  // The client is responsible for implementing some encoder-specific methods
  // as well as having the option to subscribe to frame cancellation events.
  class Client {
   public:
    virtual ~Client();

    // Returns the number of frames in the encoder's backlog.
    virtual int GetNumberOfFramesInEncoder() const = 0;

    // Should return the amount of playback time that is in the encoder's
    // backlog. Assuming that the encoder emits frames consecutively, this is
    // the same as the difference between the smallest and largest presentation
    // timestamps in the backlog.
    virtual base::TimeDelta GetEncoderBacklogDuration() const = 0;

    // The frame associated with |frame_id| was canceled and not sent.
    virtual void OnFrameCanceled(FrameId frame_id) {}
  };

  // NOTE: currently only used by the VideoSender.
  // TODO(https://crbug.com/1316434): cleanup bitrate calculations when libcast
  // has successfully launched.
  using GetSuggestedVideoBitrateCB = base::RepeatingCallback<int()>;

  // Method of creating a frame sender using an openscreen::cast::Sender.
  static std::unique_ptr<FrameSender> Create(
      scoped_refptr<CastEnvironment> cast_environment,
      const FrameSenderConfig& config,
      std::unique_ptr<openscreen::cast::Sender> sender,
      Client& client,
      GetSuggestedVideoBitrateCB get_bitrate_cb = GetSuggestedVideoBitrateCB());

  FrameSender();
  FrameSender(FrameSender&&) = delete;
  FrameSender(const FrameSender&) = delete;
  FrameSender& operator=(const FrameSender&) = delete;
  FrameSender& operator=(FrameSender&&) = delete;
  virtual ~FrameSender();

  // Setting of the target playout delay. It should be communicated to the
  // receiver on the next encoded frame.
  // NOTE: Calling this function is only valid if the receiver supports the
  // "extra_playout_delay", rtp extension.
  virtual void SetTargetPlayoutDelay(
      base::TimeDelta new_target_playout_delay) = 0;
  virtual base::TimeDelta GetTargetPlayoutDelay() const = 0;

  // Whether a key frame is needed, typically caused by a picture loss
  // indication event.
  virtual bool NeedsKeyFrame() const = 0;

  // Called by the encoder with the next encoded frame to send. Returns
  // kNotDropped if successfully enqueued.
  virtual CastStreamingFrameDropReason EnqueueFrame(
      std::unique_ptr<SenderEncodedFrame> encoded_frame) = 0;

  // Returns the reason the frame should be dropped, or kNotDropped if it should
  // not be dropped. This method should be called exactly once for each frame,
  // as its result may be used to calculate updates to the suggested bitrate.
  //
  // Callers are recommended to compute the frame duration based on the
  // difference between the next and last frames' reference times, or the period
  // between frames of the configured max frame rate if the reference times are
  // unavailable.
  virtual CastStreamingFrameDropReason ShouldDropNextFrame(
      base::TimeDelta frame_duration) = 0;

  // Returns the RTP timestamp on the frame associated with |frame_id|.
  // In practice this should be implemented as a ring buffer using the lower
  // eight bits of the FrameId, so timestamps older than 256 frames will be
  // incorrect.
  virtual RtpTimeTicks GetRecordedRtpTimestamp(FrameId frame_id) const = 0;

  // Returns the number of frames that were sent but not yet acknowledged.
  virtual int GetUnacknowledgedFrameCount() const = 0;

  // Returns the suggested bitrate the next frame should be encoded at.
  virtual int GetSuggestedBitrate(base::TimeTicks playout_time,
                                  base::TimeDelta playout_delay) = 0;

  // Configuration specific methods.

  // The maximum frame rate.
  virtual double MaxFrameRate() const = 0;
  virtual void SetMaxFrameRate(double max_frame_rate) = 0;

  // The current target playout delay.
  virtual base::TimeDelta TargetPlayoutDelay() const = 0;

  // The current, estimated round trip time.
  virtual base::TimeDelta CurrentRoundTripTime() const = 0;

  // When the last frame was sent.
  virtual base::TimeTicks LastSendTime() const = 0;

  // The last acknowledged frame ID.
  virtual FrameId LastAckedFrameId() const = 0;
};

}  // namespace media::cast

#endif  // MEDIA_CAST_SENDER_FRAME_SENDER_H_
