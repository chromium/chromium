// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_STATS_REPORTER_H_
#define MEDIA_FILTERS_HLS_STATS_REPORTER_H_

#include "media/formats/hls/rendition_manager.h"

namespace media::hls {

// Records histograms for built-in HLS playback.
class MEDIA_EXPORT HlsStatsReporter {
 public:
  ~HlsStatsReporter();

  // Updates the origin tainting flag, which gets recorded when it becomes set
  // true.
  void SetWouldTaintOrigin(bool would_taint_origin);

  // Notifies that an adaptation has occurred.
  void OnAdaptation(AdaptationReason reason);

  // When a rendition is added, it's liveness should be recorded.
  void SetIsLiveContent(bool is_live);

  // Log multivariant playlist vs media playlist. Live content will re-parse
  // the main playlist on a cadence, and so this is only logged during
  // initialization.
  void SetIsMultivariantPlaylist(bool is_multivariant);

  // TODO(crbug/40057824): Log codec type. We need an enum of all codecs that
  // the HLS spec says it _should_ support, not just the ones we currently
  // support.
 private:
  std::optional<bool> would_taint_origin_;
  std::optional<bool> is_multivariant_;
  std::optional<bool> is_live_content_;
};

}  // namespace media::hls

#endif  // MEDIA_FILTERS_HLS_STATS_REPORTER_H_
