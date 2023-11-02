// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SCOPED_OVERSCROLL_MODES_H_
#define CONTENT_PUBLIC_TEST_SCOPED_OVERSCROLL_MODES_H_

#include "content/public/browser/overscroll_configuration.h"

namespace content {

// Helper class to set the pull-to-refresh mode temporarily in tests.
class ScopedPullToRefreshMode {
 public:
  explicit ScopedPullToRefreshMode(OverscrollConfig::PullToRefreshMode mode);

  ScopedPullToRefreshMode(const ScopedPullToRefreshMode&) = delete;
  ScopedPullToRefreshMode& operator=(const ScopedPullToRefreshMode&) = delete;

  ~ScopedPullToRefreshMode();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SCOPED_OVERSCROLL_MODES_H_
