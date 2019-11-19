// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_track_metrics_host.h"

#include "base/metrics/histogram_macros.h"

// We use a histogram with a maximum bucket of 16 hours to infinity
// for track durations.
#define UMA_HISTOGRAM_TIMES_16H(name, sample)                        \
  UMA_HISTOGRAM_CUSTOM_TIMES(name, sample,                           \
                             base::TimeDelta::FromMilliseconds(100), \
                             base::TimeDelta::FromHours(16),         \
                             50);

namespace content {

MediaStreamTrackMetricsHost::MediaStreamTrackMetricsHost() {}

MediaStreamTrackMetricsHost::~MediaStreamTrackMetricsHost() {
  // Our render process has exited. We won't receive any more IPC
  // messages from it. Assume all tracks ended now.
  for (auto it = tracks_.begin(); it != tracks_.end(); ++it) {
    TrackInfo& info = it->second;
    ReportDuration(info);
  }
  tracks_.clear();
}

void MediaStreamTrackMetricsHost::BindReceiver(
    mojo::PendingReceiver<blink::mojom::MediaStreamTrackMetricsHost> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MediaStreamTrackMetricsHost::AddTrack(uint64_t id,
                                           bool is_audio,
                                           bool is_remote) {
  if (tracks_.find(id) != tracks_.end())
    return;

  TrackInfo info = {is_audio, is_remote, base::TimeTicks::Now()};
  tracks_[id] = info;
}

void MediaStreamTrackMetricsHost::RemoveTrack(uint64_t id) {
  if (tracks_.find(id) == tracks_.end())
    return;

  TrackInfo& info = tracks_[id];
  ReportDuration(info);
  tracks_.erase(id);
}

void MediaStreamTrackMetricsHost::ReportDuration(const TrackInfo& info) {
  base::TimeDelta duration = base::TimeTicks::Now() - info.timestamp;
  if (info.is_remote) {
    if (info.is_audio) {
      DVLOG(3) << "WebRTC.ReceivedAudioTrackDuration: " << duration.InSeconds();
      UMA_HISTOGRAM_TIMES_16H("WebRTC.ReceivedAudioTrackDuration", duration);
    } else {
      DVLOG(3) << "WebRTC.ReceivedVideoTrackDuration: " << duration.InSeconds();
      UMA_HISTOGRAM_TIMES_16H("WebRTC.ReceivedVideoTrackDuration", duration);
    }
  } else {
    if (info.is_audio) {
      DVLOG(3) << "WebRTC.SentAudioTrackDuration: " << duration.InSeconds();
      UMA_HISTOGRAM_TIMES_16H("WebRTC.SentAudioTrackDuration", duration);
    } else {
      DVLOG(3) << "WebRTC.SentVideoTrackDuration: " << duration.InSeconds();
      UMA_HISTOGRAM_TIMES_16H("WebRTC.SentVideoTrackDuration", duration);
    }
  }
}

}  // namespace content
