// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_stats_reporter.h"

#include "base/metrics/histogram_functions.h"

namespace media::hls {

HlsStatsReporter::~HlsStatsReporter() = default;

void HlsStatsReporter::SetWouldTaintOrigin(bool tainted) {
  // The first time the tainted origin flag is set, report it.
  if (!would_taint_origin_.value_or(true) && tainted) {
    base::UmaHistogramBoolean("Media.HLS.CrossOriginContent", true);
  }
  would_taint_origin_ = would_taint_origin_.value_or(false) | tainted;
}

void HlsStatsReporter::OnAdaptation(AdaptationReason reason) {
  base::UmaHistogramEnumeration("Media.HLS.Adaptation", reason);
}

void HlsStatsReporter::SetIsLiveContent(bool live_content) {
  // Log only once, after it has been set. Multiple renditions may not know it
  // has already been logged.
  if (!is_live_content_.has_value()) {
    is_live_content_ = live_content;
    base::UmaHistogramBoolean("Media.HLS.LiveContent", *is_live_content_);
  }
}

void HlsStatsReporter::SetIsMultivariantPlaylist(bool is_multivariant) {
  // Log only once, after it has been set. Multiple renditions may not know it
  // has already been logged.
  if (!is_multivariant_.has_value()) {
    is_multivariant_ = is_multivariant;
    base::UmaHistogramBoolean("Media.HLS.MultivariantPlaylist",
                              *is_multivariant_);
  }
}

}  // namespace media::hls
