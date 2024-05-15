// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_probe_result.h"

#include "base/notreached.h"

namespace content {

bool PrefetchProbeResultIsSuccess(PrefetchProbeResult result) {
  switch (result) {
    case PrefetchProbeResult::kNoProbing:
    case PrefetchProbeResult::kDNSProbeSuccess:
    case PrefetchProbeResult::kTLSProbeSuccess:
      return true;
    case PrefetchProbeResult::kTLSProbeFailure:
    case PrefetchProbeResult::kDNSProbeFailure:
      return false;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace content