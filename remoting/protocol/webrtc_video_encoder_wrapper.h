// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_VIDEO_ENCODER_WRAPPER_H_
#define REMOTING_PROTOCOL_WEBRTC_VIDEO_ENCODER_WRAPPER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "remoting/codec/webrtc_video_encoder.h"
#include "third_party/webrtc/api/video/video_codec_type.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/api/video_codecs/video_encoder.h"

namespace remoting {
namespace protocol {

class VideoChannelStateObserver;

// WebrtcVideoEncoderWrapper is a wrapper around the remoting codecs, which
// implements the webrtc::VideoEncoder interface. This class is instantiated
// by WebRTC via the webrtc::VideoEncoderFactory, and all methods (including
// the ctor) are called on WebRTC's foreground worker thread.
class WebrtcVideoEncoderWrapper : public webrtc::VideoEncoder {
 public:
  // Called by the VideoEncoderFactory. |video_channel_state_observer| is
  // notified of important events on the |main_task_runner| thread.
  WebrtcVideoEncoderWrapper(
      const webrtc::SdpVideoFormat& format,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      base::WeakPtr<VideoChannelStateObserver> video_channel_state_observer);
  ~WebrtcVideoEncoderWrapper() override;

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
  // Returns an encoded frame to WebRTC's registered callback.
  webrtc::EncodedImageCallback::Result ReturnEncodedFrame(
      const WebrtcVideoEncoder::EncodedFrame& frame);

  // Called when |encoder_| has finished encoding a frame.
  void OnFrameEncoded(WebrtcVideoEncoder::EncodeResult encode_result,
                      std::unique_ptr<WebrtcVideoEncoder::EncodedFrame> frame);

  // Sets whether top-off is active, and fires a notification if the setting
  // changes.
  void SetTopOffActive(bool active);

  std::unique_ptr<WebrtcVideoEncoder> encoder_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Callback registered by WebRTC to receive encoded frames.
  webrtc::EncodedImageCallback* encoded_callback_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  // Timestamp to be added to the EncodedImage when sending it to
  // |encoded_callback_|. This value comes from the frame that WebRTC
  // passes to Encode().
  uint32_t rtp_timestamp_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Bandwidth estimate from SetRates(), which is expected to be called before
  // Encode().
  int bitrate_kbps_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // True when encoding unchanged frames for top-off.
  bool top_off_active_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  webrtc::VideoCodecType codec_type_ GUARDED_BY_CONTEXT(sequence_checker_);

  // TaskRunner used for notifying |video_channel_state_observer_|.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  base::WeakPtr<VideoChannelStateObserver> video_channel_state_observer_;

  // This class lives on WebRTC's encoding thread. All methods (including ctor
  // and dtor) are expected to be called on the same thread.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebrtcVideoEncoderWrapper> weak_factory_{this};
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_VIDEO_ENCODER_WRAPPER_H_
