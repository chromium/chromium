// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROBE_RESULT_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROBE_RESULT_H_

namespace content {

// The result of an origin probe. See PrefetchOriginProber.
enum class PrefetchProbeResult {
  kNoProbing = 0,
  kDNSProbeSuccess = 1,
  kDNSProbeFailure = 2,
  kTLSProbeSuccess = 3,
  kTLSProbeFailure = 4,
};

// Returns true if the probe result is not a failure.
bool PrefetchProbeResultIsSuccess(PrefetchProbeResult result);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROBE_RESULT_H_
