// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/origin_trials/trial_token_result.h"

#include "third_party/blink/public/common/origin_trials/trial_token.h"

namespace blink {

TrialTokenResult::TrialTokenResult(OriginTrialTokenStatus status)
    : status_(status), parsed_token_(nullptr) {
  DCHECK(status_ != OriginTrialTokenStatus::kSuccess);
}
TrialTokenResult::TrialTokenResult(OriginTrialTokenStatus status,
                                   std::unique_ptr<TrialToken> parsed_token)
    : status_(status), parsed_token_(std::move(parsed_token)) {
  DCHECK(parsed_token_);
}

}  // namespace blink
