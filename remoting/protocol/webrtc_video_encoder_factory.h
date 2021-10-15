// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_VIDEO_ENCODER_FACTORY_H_
#define REMOTING_PROTOCOL_WEBRTC_VIDEO_ENCODER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/webrtc/api/video_codecs/video_encoder_factory.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"

namespace remoting {
namespace protocol {

class VideoChannelStateObserver;

// This is the encoder factory that is passed to WebRTC when the peer connection
// is created. This factory creates the video encoder, which is an instance of
// WebrtcVideoEncoderWrapper which wraps a video codec from remoting/codec.
class WebrtcVideoEncoderFactory : public webrtc::VideoEncoderFactory {
 public:
  WebrtcVideoEncoderFactory();
  ~WebrtcVideoEncoderFactory() override;

  // webrtc::VideoEncoderFactory interface.
  std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
      const webrtc::SdpVideoFormat& format) override;
  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;

  // TODO(crbug.com/1192865): Remove these 2 methods, and just pass the
  // callbacks in the ctor. Then the |lock_| can also be removed.
  void RegisterEncoderSelectedCallback(
      const base::RepeatingCallback<
          void(webrtc::VideoCodecType,
               const webrtc::SdpVideoFormat::Parameters&)>& callback);

  void SetVideoChannelStateObserver(
      base::WeakPtr<VideoChannelStateObserver> video_channel_state_observer);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  std::vector<webrtc::SdpVideoFormat> formats_;

  // Protects |video_channel_state_observer_|.
  base::Lock lock_;
  base::WeakPtr<VideoChannelStateObserver> video_channel_state_observer_;
  base::RepeatingCallback<void(webrtc::VideoCodecType,
                               const webrtc::SdpVideoFormat::Parameters&)>
      encoder_created_callback_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_VIDEO_ENCODER_FACTORY_H_
