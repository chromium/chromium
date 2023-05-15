// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_VIDEO_STREAM_EVENT_ROUTER_H_
#define REMOTING_PROTOCOL_VIDEO_STREAM_EVENT_ROUTER_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/protocol/video_channel_state_observer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting::protocol {

class VideoStreamEventRouter {
 public:
  VideoStreamEventRouter();

  VideoStreamEventRouter(const VideoStreamEventRouter&) = delete;
  VideoStreamEventRouter& operator=(const VideoStreamEventRouter&) = delete;

  ~VideoStreamEventRouter();

  // Mirrors the VideoChannelStateObserver interface with an additional
  // |screen_id| param used to route the value to the appropriate video stream.
  void OnEncodedFrameSent(webrtc::ScreenId screen_id,
                          webrtc::EncodedImageCallback::Result result,
                          const WebrtcVideoEncoder::EncodedFrame& frame);

  void SetVideoChannelStateObserver(
      const std::string& stream_name,
      base::WeakPtr<VideoChannelStateObserver> video_channel_state_observer);

  base::WeakPtr<VideoStreamEventRouter> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtr<VideoChannelStateObserver> GetObserver(
      webrtc::ScreenId screen_id);

  base::WeakPtr<VideoChannelStateObserver> single_stream_state_observer_;
  base::flat_map<webrtc::ScreenId, base::WeakPtr<VideoChannelStateObserver>>
      multi_stream_state_observers_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<VideoStreamEventRouter> weak_factory_{this};
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_VIDEO_STREAM_EVENT_ROUTER_H_
