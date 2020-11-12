// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SWAP_RESULT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SWAP_RESULT_H_

#include "base/callback.h"
#include "base/time/time.h"

namespace blink {

// SwapResult mirrors the values of cc::SwapPromise::DidNotSwapReason, and
// should be kept consistent with it. SwapResult additionally adds a success
// value (kDidSwap).
// These values are written to logs. New enum values can be added, but
// existing enums must never be renumbered, deleted or reused.
enum class WebSwapResult {
  kDidSwap = 0,
  kDidNotSwapSwapFails = 1,
  kDidNotSwapCommitFails = 2,
  kDidNotSwapCommitNoUpdate = 3,
  kDidNotSwapActivationFails = 4,
  kMaxValue = kDidNotSwapActivationFails,
};
using WebReportTimeCallback =
    base::OnceCallback<void(WebSwapResult, base::TimeTicks)>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SWAP_RESULT_H_
