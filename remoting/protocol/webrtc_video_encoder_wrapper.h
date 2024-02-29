// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_VIDEO_ENCODER_WRAPPER_H_
#define REMOTING_PROTOCOL_WEBRTC_VIDEO_ENCODER_WRAPPER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "remoting/base/constants.h"
#include "remoting/base/running_samples.h"
#include "remoting/base/session_options.h"
#include "remoting/codec/webrtc_video_encoder.h"
#include "third_party/webrtc/api/video/video_codec_type.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/api/video_codecs/video_encoder.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting::protocol {

class VideoStreamEventRouter;

// WebrtcVideoEncoderWrapper is a wrapper around the remoting codecs which
// implement the webrtc::VideoEncoder interface. This class is instantiated by
// WebRTC via the webrtc::VideoEncoderFactory, and all methods (including the
// c'tor) are called on WebRTC's foreground worker thread.
class WebrtcVideoEncoderWrapper : public webrtc::VideoEncoder {
 public:
  // Called by the VideoEncoderFactory. |video_channel_state_observer| is
  // notified of important events on the |main_task_runner| thread.
  WebrtcVideoEncoderWrapper(
      const webrtc::SdpVideoFormat& format,
      const SessionOptions& session_options,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner,
      base::WeakPtr<VideoStreamEventRouter> video_stream_event_router);
  ~WebrtcVideoEncoderWrapper() override;

  void SetEncoderForTest(std::unique_ptr<WebrtcVideoEncoder> encoder);

  // webrtc::VideoEncoder interface.
  int32_t InitEncode(const webrtc::VideoCodec* codec_settings,
                     const webrtc::VideoEncoder::Settings& settings) override;
  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) override;
  int32_t Release() override;
  int32_t Encode(
      const webrtc::VideoFrame& frame,
      const std::vector<webrtc::VideoFrameType>* frame_types) override;
  void SetRates(const RateControlParameters& parameters) override;
  void OnRttUpdate(int64_t rtt_ms) override;
  webrtc::VideoEncoder::EncoderInfo GetEncoderInfo() const override;

 private:
  static constexpr int kStatsWindow = 5;

  // Returns an encoded frame to WebRTC's registered callback.
  webrtc::EncodedImageCallback::Result ReturnEncodedFrame(
      const WebrtcVideoEncoder::EncodedFrame& frame);

  // Called when |encoder_| has finished encoding a frame.
  void OnFrameEncoded(WebrtcVideoEncoder::EncodeResult encode_result,
                      std::unique_ptr<WebrtcVideoEncoder::EncodedFrame> frame);

  // Notifies WebRTC that this encoder has dropped a frame.
  void NotifyFrameDropped();

  // Returns whether the frame should be encoded at low quality, to reduce
  // latency for large frame updates. This is only done here for VP8, as VP9
  // automatically detects target-overshoot and re-encodes the frame at
  // lower quality. This calculation is based on |frame|'s update-region
  // (compared with recent history) and the current bandwidth-estimation.
  bool ShouldDropQualityForLargeFrame(const webrtc::DesktopFrame& frame);

  // Begins encoding |pending_frame_| if it contains valid frame data.
  void SchedulePendingFrame();

  // Clears |pending_frame_| and notifies WebRTC of the dropped frame when
  // |pending_frame_| contains valid frame data.
  void DropPendingFrame();

  std::unique_ptr<WebrtcVideoEncoder> encoder_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Callback registered by WebRTC to receive encoded frames.
  raw_ptr<webrtc::EncodedImageCallback> encoded_callback_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  // Timestamp to be added to the EncodedImage when sending it to
  // |encoded_callback_|. This value comes from the frame that WebRTC
  // passes to Encode().
  uint32_t rtp_timestamp_ GUARDED_BY_CONTEXT(sequence_checker_);

  // FrameStats taken from the input VideoFrameAdapter, then added to the
  // EncodedFrame when encoding is complete.
  std::unique_ptr<WebrtcVideoEncoder::FrameStats> frame_stats_;

  // Bandwidth estimate from SetRates(), which is expected to be called before
  // Encode().
  int bitrate_kbps_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // Latest RTT estimate provided by OnRttUpdate().
  base::TimeDelta rtt_estimate_ GUARDED_BY_CONTEXT(sequence_checker_){
      base::TimeDelta::Max()};

  // True when encoding unchanged frames for top-off.
  bool top_off_active_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  webrtc::VideoCodecType codec_type_ GUARDED_BY_CONTEXT(sequence_checker_);

  // True when a frame is being encoded. This guards against encoding multiple
  // frames in parallel, which the encoders are not prepared to handle.
  bool encode_pending_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  std::unique_ptr<webrtc::VideoFrame> pending_frame_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Stores the expected id of the next incoming frame to be encoded. If this
  // does not match, it means that WebRTC dropped a frame, and the original
  // DesktopFrame's updated-region should not be passed to the encoder.
  // Consecutive frames have incrementing IDs, wrapping around to 0 (which can
  // happen many times during a connection - the unsigned type guarantees that
  // the '++' operator will wrap to 0 after overflow).
  uint16_t next_frame_id_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // Keeps track of any update-rectangles from dropped frames. When WebRTC
  // requests to encode a frame, this class will either:
  // * Send it to be encoded - if any prior frames were dropped, this
  //   accumulated update-rect will be added to the incoming frame, then it will
  //   be reset to empty.
  // * Drop the frame - the frame's update-rect will be stored and combined with
  //   this accumulated update-rect.
  // This tracking is similar to what WebRTC does whenever it drops frames
  // internally.  WebRTC will also detect resolution-changes and set the
  // frame's update-rect to the full area, so no special logic is needed here
  // for changes in resolution (except to make sure that any frame's update-rect
  // always lies within the frame's bounding rect).
  webrtc::VideoFrame::UpdateRect accumulated_update_rect_
      GUARDED_BY_CONTEXT(sequence_checker_){};

  // Used by ShouldDropQualityForLargeFrame(). This stores the most recent
  // update-region areas of previously-encoded frames, in order to detect an
  // unusually-large update.
  RunningSamples updated_region_area_ GUARDED_BY_CONTEXT(sequence_checker_){
      kStatsWindow};

  // Stores the time when the most recent frame was sent to the encoder. This is
  // used to rate-limit the encoding and sending of empty frames.
  base::TimeTicks latest_frame_encode_start_time_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // If a key-frame is requested, but this class needs to drop the frame, this
  // flag remembers the request so it can be applied to the next frame.
  bool pending_key_frame_request_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // TaskRunner used for notifying |video_channel_state_observer_|.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // TaskRunner used for scheduling encoding tasks.
  scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner_;

  // Stores the current frame interval, updated for each frame received.
  base::TimeDelta current_frame_interval_ = base::Hertz(kTargetFrameRate);

  // Stores the timestamp of the last frame that was sent for encoding.
  base::Time last_frame_received_timestamp_;

  // Represents the screen which is being encoded by this instance. Initialized
  // after the first captured frame has been received.
  std::optional<webrtc::ScreenId> screen_id_;

  base::WeakPtr<VideoStreamEventRouter> video_stream_event_router_;

  // This class lives on WebRTC's encoding thread. All methods (including ctor
  // and dtor) are expected to be called on the same thread.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebrtcVideoEncoderWrapper> weak_factory_{this};
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_WEBRTC_VIDEO_ENCODER_WRAPPER_H_
