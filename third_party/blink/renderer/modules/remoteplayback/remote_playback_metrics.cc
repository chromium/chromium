// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/remoteplayback/remote_playback_metrics.h"

#include "base/metrics/histogram_macros.h"

namespace blink {
// static
void RemotePlaybackMetrics::RecordRemotePlaybackLocation(
    RemotePlaybackInitiationLocation location) {
  UMA_HISTOGRAM_ENUMERATION("Cast.Sender.RemotePlayback.InitiationLocation",
                            location,
                            RemotePlaybackInitiationLocation::kMaxValue);
}

}  // namespace blink
