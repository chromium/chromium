// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/video_stream_event_router.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"

namespace remoting::protocol {

namespace {
constexpr char kSingleStreamName[] = "screen_stream";
}  // namespace

VideoStreamEventRouter::VideoStreamEventRouter() = default;
VideoStreamEventRouter::~VideoStreamEventRouter() = default;

void VideoStreamEventRouter::OnEncodedFrameSent(
    webrtc::ScreenId screen_id,
    webrtc::EncodedImageCallback::Result result,
    const WebrtcVideoEncoder::EncodedFrame& frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto observer = GetObserver(screen_id);
  if (observer) {
    observer->OnEncodedFrameSent(result, frame);
  } else {
    LOG(WARNING) << "No registered VideoChannelStateObserver for " << screen_id;
  }
}

void VideoStreamEventRouter::SetVideoChannelStateObserver(
    const std::string& stream_name,
    base::WeakPtr<VideoChannelStateObserver> video_channel_state_observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (stream_name == kSingleStreamName) {
    single_stream_state_observer_ = video_channel_state_observer;
    // If switching modes, clear out all registered multi-stream observers.
    multi_stream_state_observers_.clear();
    return;
  }

  // Clear out the single stream observer in case the mode changed.
  single_stream_state_observer_ = nullptr;

  auto parts = base::RSplitStringOnce(stream_name, '_');
  if (!parts) {
    LOG(ERROR) << "Unexpected stream name format: " << stream_name;
    return;
  }

  int64_t screen_id;
  if (!base::StringToInt64(parts->second, &screen_id)) {
    LOG(ERROR) << "Failed to extract screen id from: " << stream_name;
    return;
  }

  multi_stream_state_observers_[screen_id] = video_channel_state_observer;
}

base::WeakPtr<VideoChannelStateObserver> VideoStreamEventRouter::GetObserver(
    webrtc::ScreenId screen_id) {
  if (single_stream_state_observer_) {
    return single_stream_state_observer_;
  }

  if (multi_stream_state_observers_.contains(screen_id)) {
    auto observer = multi_stream_state_observers_.at(screen_id);
    if (!observer) {
      LOG(WARNING) << "Removing invalid observer for screen_id: " << screen_id;
      multi_stream_state_observers_.erase(screen_id);
    }

    return observer;
  }

  return nullptr;
}

}  // namespace remoting::protocol
