// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_SENDER_OPENSCREEN_FRAME_SENDER_H_
#define MEDIA_CAST_SENDER_OPENSCREEN_FRAME_SENDER_H_

#include <stdint.h>

#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/sender/frame_sender.h"
#include "media/cast/sender/video_bitrate_suggester.h"
#include "third_party/openscreen/src/cast/streaming/public/sender.h"

namespace media::cast {

struct SenderEncodedFrame;

// This is the Open Screen implementation of the FrameSender API. It wraps
// an openscreen::cast::Sender object and provides some basic functionality
// that is shared between the AudioSender, VideoSender, and RemotingSender
// classes.
//
// For more information, see the Cast Streaming README.md located at:
// https://source.chromium.org/chromium/chromium/src/+/main:third_party/openscreen/src/cast/streaming/README.md
//
// NOTE: This class mostly exists to wrap an openscreen::cast::Sender, implement
// frame dropping logic, and support type translation between Chrome and Open
// Screen.  See if it can be removed by migrating functionality into
// openscreen::cast::Sender.
//
// TODO(issues.chromium.org/329781397): Remove unnecessary wrapper objects in
// Chrome's implementation of the Cast sender.
class OpenscreenFrameSender : public FrameSender,
                              openscreen::cast::Sender::Observer {
 public:
  OpenscreenFrameSender(scoped_refptr<CastEnvironment> cast_environment,
                        const FrameSenderConfig& config,
                        std::unique_ptr<openscreen::cast::Sender> sender,
                        Client& client,
                        FrameSender::GetSuggestedVideoBitrateCB get_bitrate_cb);
  OpenscreenFrameSender(OpenscreenFrameSender&& other) = delete;
  OpenscreenFrameSender& operator=(OpenscreenFrameSender&& other) = delete;
  OpenscreenFrameSender(const OpenscreenFrameSender&) = delete;
  OpenscreenFrameSender& operator=(const OpenscreenFrameSender&) = delete;
  ~OpenscreenFrameSender() override;

  // FrameSender overrides.
  void SetTargetPlayoutDelay(base::TimeDelta new_target_playout_delay) override;
  base::TimeDelta GetTargetPlayoutDelay() const override;
  bool NeedsKeyFrame() const override;
  CastStreamingFrameDropReason EnqueueFrame(
      std::unique_ptr<SenderEncodedFrame> encoded_frame) override;
  CastStreamingFrameDropReason ShouldDropNextFrame(
      base::TimeDelta frame_duration) override;
  RtpTimeTicks GetRecordedRtpTimestamp(FrameId frame_id) const override;
  int GetUnacknowledgedFrameCount() const override;
  int GetSuggestedBitrate(base::TimeTicks playout_time,
                          base::TimeDelta playout_delay) override;
  double MaxFrameRate() const override;
  void SetMaxFrameRate(double max_frame_rate) override;
  base::TimeDelta TargetPlayoutDelay() const override;
  base::TimeDelta CurrentRoundTripTime() const override;
  base::TimeTicks LastSendTime() const override;
  FrameId LastAckedFrameId() const override;

 private:
  // openscreen::cast::Sender::Observer overrides.
  void OnFrameCanceled(openscreen::cast::FrameId frame_id) override;
  // NOTE: this is a no-op since the encoder checks if it should generate a key
  // frame when the next raw frame is inserted.
  void OnPictureLost() override;

  // Helper for getting the reference time recorded on the frame associated
  // with |frame_id|.
  base::TimeTicks GetRecordedReferenceTime(FrameId frame_id) const;

  // Record timestamps for later retrieval by GetRecordedRtpTimestamp.
  void RecordLatestFrameTimestamps(FrameId frame_id,
                                   base::TimeTicks reference_time,
                                   RtpTimeTicks rtp_timestamp);

  base::TimeDelta GetInFlightMediaDuration() const;

 private:
  friend class OpenscreenFrameSenderTest;

  // Returns the maximum media duration currently allowed in-flight.  This
  // fluctuates in response to the currently-measured network latency.
  base::TimeDelta GetAllowedInFlightMediaDuration() const;

  void RecordShouldDropNextFrame(bool should_drop);

  // The cast environment.
  const scoped_refptr<CastEnvironment> cast_environment_;

  // The backing Open Screen sender implementation.
  std::unique_ptr<openscreen::cast::Sender> const sender_;

  // The frame sender client.
  const raw_ref<Client> client_;

  // The method for getting the recommended bitrate.
  GetSuggestedVideoBitrateCB get_bitrate_cb_;

  // Max encoded frames generated per second.
  double max_frame_rate_;

  // Whether this is an audio or video frame sender.
  const bool is_audio_;

  // The target playout delay, may fluctuate between the min and max delays.
  base::TimeDelta target_playout_delay_;
  base::TimeDelta min_playout_delay_;
  base::TimeDelta max_playout_delay_;

  // This is "null" until the first frame is sent.  Thereafter, this tracks the
  // last time any frame was sent or re-sent.
  base::TimeTicks last_send_time_;

  // The ID of the last enqueued frame. This member is invalid until
  // |!last_send_time_.is_null()|.
  FrameId last_enqueued_frame_id_;

  // The ID of the last acknowledged/"cancelled" frame.
  FrameId last_acked_frame_id_;

  // The ID of the frame that was the first one to have a different identifier
  // used inside of Open Screen. This only occurs if a frame is dropped.
  std::optional<FrameId> diverged_frame_id_;

  // Since the encoder emits frames that depend on each other, and the Open
  // Screen sender demands that we use its FrameIDs for enqueued frames, we
  // have to keep a map of the encoder's frame id to the Open Screen
  // sender's frame id. This map is cleared on each keyframe.
  base::flat_map<FrameId, FrameId> frame_id_map_;

  // This is the maximum delay that the sender should get ack from receiver.
  // Counts how many RTCP reports are being "aggressively" sent (i.e., one per
  // frame) at the start of the session.  Once a threshold is reached, RTCP
  // reports are instead sent at the configured interval + random drift.
  int num_aggressive_rtcp_reports_sent_ = 0;

  // Should send the target playout delay with the next frame. Behind the
  // scenes, the openscreen::cast::Sender checks the frame's playout delay and
  // notifies the receiver if it has changed.
  bool send_target_playout_delay_ = false;

  // Ring buffer to keep track of recent frame timestamps. These should only be
  // accessed through the Record/GetXXX() methods.  The index into this ring
  // buffer is the lower 8 bits of the FrameId.
  RtpTimeTicks frame_rtp_timestamps_[256];

  // TODO(https://crbug.com/1316434): move this property to VideoSender once
  // the legacy implementation has been removed.
  std::unique_ptr<VideoBitrateSuggester> bitrate_suggester_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<OpenscreenFrameSender> weak_factory_{this};
};

}  // namespace media::cast

#endif  // MEDIA_CAST_SENDER_OPENSCREEN_FRAME_SENDER_H_
