// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PREFETCH_PRIORITY_H_
#define CONTENT_PUBLIC_BROWSER_PREFETCH_PRIORITY_H_

namespace content {

// An optimization hint that indicates the relative priority of a prefetch
// request. A higher priority suggests that the prefetch caller expects prefetch
// resources to be available sooner.
// TODO(crbug.com/426404355): Consider revisitting the name.
enum class PrefetchPriority {
  kLow = 0,
  kMedium = 1,
  kHigh = 2,
  kHighest = 3,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PREFETCH_PRIORITY_H_
