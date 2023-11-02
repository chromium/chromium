// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/scoped_overscroll_modes.h"

namespace content {

ScopedPullToRefreshMode::ScopedPullToRefreshMode(
    OverscrollConfig::PullToRefreshMode mode) {
  OverscrollConfig::SetPullToRefreshMode(mode);
}

ScopedPullToRefreshMode::~ScopedPullToRefreshMode() {
  OverscrollConfig::ResetPullToRefreshMode();
}

}  // namespace content
