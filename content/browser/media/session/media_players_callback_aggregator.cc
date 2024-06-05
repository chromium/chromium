// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_players_callback_aggregator.h"

namespace content {

MediaPlayersCallbackAggregator::MediaPlayersCallbackAggregator(
    ReportVisibilityCb report_visibility_cb)
    : report_visibility_cb_(std::move(report_visibility_cb)) {}

void MediaPlayersCallbackAggregator::OnGetVisibility(
    bool meets_visibility_threshold) {
  if (meets_visibility_threshold && report_visibility_cb_) {
    std::move(report_visibility_cb_).Run(true);
  }
}

MediaPlayersCallbackAggregator::VisibilityCb
MediaPlayersCallbackAggregator::CreateVisibilityCallback() {
  return base::BindOnce(&MediaPlayersCallbackAggregator::OnGetVisibility,
                        base::RetainedRef(this));
}

MediaPlayersCallbackAggregator::~MediaPlayersCallbackAggregator() {
  if (report_visibility_cb_) {
    std::move(report_visibility_cb_).Run(false);
  }
}

}  // namespace content
