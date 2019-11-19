// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_histograms.h"

#include "base/metrics/histogram_macros.h"

namespace content {

void AppCacheHistograms::CountReinitAttempt(bool repeated_attempt) {
  UMA_HISTOGRAM_BOOLEAN("appcache.ReinitAttempt", repeated_attempt);
}

}  // namespace content
