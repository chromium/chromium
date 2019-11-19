// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_DUMMY_VIDEO_ENCODER_H_
#define REMOTING_PROTOCOL_WEBRTC_DUMMY_VIDEO_ENCODER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "remoting/codec/webrtc_video_encoder.h"
#include "third_party/webrtc/api/video_codecs/video_encoder_factory.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"

namespace remoting {
namespace protocol {

class VideoChannelStateObserver;
class WebrtcDummyVideoEncoderFactory;

// WebrtcDummyVideoEncoder implements webrtc::VideoEncoder interface for WebRTC.
// It's responsible for getting  feedback on network bandwidth, latency & RTT
// and passing this information to the WebrtcVideoStream through the callbacks
// in WebrtcDummyVideoEncoderFactory. Video frames are captured and encoded
// outside of this dummy encoder (in WebrtcVideoEncoder called from
// WebrtcVideoStream). They are passed to SendEncodedFrame() to be delivered to
// the network layer.
class WebrtcDummyVideoEncoder : public webrtc::VideoEncoder {
 public:
  enum State { kUninitialized = 0, kInitialized };

  WebrtcDummyVideoEncoder(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      base::WeakPtr<VideoChannelStateObserver> video_channel_state_observer,
      WebrtcDummyVideoEncoderFactory* factory);
  ~WebrtcDummyVideoEncoder() override;

  // webrtc::VideoEncoder overrides.
  int32_t InitEncode(const webrtc::VideoCodec* codec_settings,
                     int32_t number_of_cores,
                     size_t max_payload_size) override;
  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) override;
  int32_t Release() override;
  int32_t Encode(
      const webrtc::VideoFrame& frame,
      const std::vector<webrtc::VideoFrameType>* frame_types) override;
  void SetRates(const RateControlParameters& parameters) override;
  webrtc::VideoEncoder::EncoderInfo GetEncoderInfo() const override;

  webrtc::EncodedImageCallback::Result SendEncodedFrame(
      const WebrtcVideoEncoder::EncodedFrame& frame,
      base::TimeTicks capture_time,
      base::TimeTicks encode_started_time,
      base::TimeTicks encode_finished_time);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Protects |encoded_callback_| and |state_|.
  base::Lock lock_;
  State state_;
  webrtc::EncodedImageCallback* encoded_callback_ = nullptr;

  base::WeakPtr<VideoChannelStateObserver> video_channel_state_observer_;

  // Holds a reference to the creating factory, if any. Will be notified
  // when this instance is released, so it can stop delivering frames.
  WebrtcDummyVideoEncoderFactory* factory_ = nullptr;
};

// This is the encoder factory implementation that is passed to
// WebRTC at the time of creation of peer connection. The encoder
// factory primarily manages creation of encoder.
class WebrtcDummyVideoEncoderFactory : public webrtc::VideoEncoderFactory {
 public:
  WebrtcDummyVideoEncoderFactory();
  ~WebrtcDummyVideoEncoderFactory() override;

  // webrtc::VideoEncoderFactory interface.
  std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
      const webrtc::SdpVideoFormat& format) override;
  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;
  CodecInfo QueryVideoEncoder(
      const webrtc::SdpVideoFormat& format) const override;

  webrtc::EncodedImageCallback::Result SendEncodedFrame(
      const WebrtcVideoEncoder::EncodedFrame& packet,
      base::TimeTicks capture_time,
      base::TimeTicks encode_started_time,
      base::TimeTicks encode_finished_time);

  // Callback will be called once the dummy encoder has been created on
  // |main_task_runner_|.
  void RegisterEncoderSelectedCallback(
      const base::Callback<void(webrtc::VideoCodecType)>& callback);

  void SetVideoChannelStateObserver(
      base::WeakPtr<VideoChannelStateObserver> video_channel_state_observer);
  base::WeakPtr<VideoChannelStateObserver>
  get_video_channel_state_observer_for_tests() {
    return video_channel_state_observer_;
  }

  void EncoderDestroyed(WebrtcDummyVideoEncoder* encoder);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  std::vector<webrtc::SdpVideoFormat> formats_;

  // Protects |video_channel_state_observer_| and |encoders_|.
  base::Lock lock_;
  base::WeakPtr<VideoChannelStateObserver> video_channel_state_observer_;
  std::vector<WebrtcDummyVideoEncoder*> encoders_;
  base::Callback<void(webrtc::VideoCodecType)> encoder_created_callback_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_DUMMY_VIDEO_ENCODER_H_
