// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_REMOTEPLAYBACK_REMOTE_PLAYBACK_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_REMOTEPLAYBACK_REMOTE_PLAYBACK_METRICS_H_

namespace blink {

// Do not remove or renumber enums as this is used for metrics. When making
// changes, also update the enum list in tools/metrics/histograms/enums.xml to
// keep it in sync.
enum RemotePlaybackInitiationLocation {
  REMOTE_PLAYBACK_API = 0,
  HTML_MEDIA_ELEMENT = 1,
  kMaxValue = HTML_MEDIA_ELEMENT,
};

class RemotePlaybackMetrics {
 public:
  static void RecordRemotePlaybackLocation(
      RemotePlaybackInitiationLocation location);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_REMOTEPLAYBACK_REMOTE_PLAYBACK_METRICS_H_
