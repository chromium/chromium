// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRERENDER_PRERENDER_METRICS_H_
#define CONTENT_BROWSER_PRERENDER_PRERENDER_METRICS_H_

#include <string>

namespace content {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Note: Please update GetCancelledInterfaceType() in the corresponding .cc file
// if you add a new item.
enum class PrerenderCancelledInterface {
  kUnknown = 0,  // For kCancel interfaces added by embedders or tests.
  kGamepadHapticsManager = 1,
  kGamepadMonitor = 2,
  kMaxValue = kGamepadMonitor
};

void RecordPrerenderCancelledInterface(const std::string& interface_name);

}  // namespace content

#endif  // CONTENT_BROWSER_PRERENDER_PRERENDER_METRICS_H_
