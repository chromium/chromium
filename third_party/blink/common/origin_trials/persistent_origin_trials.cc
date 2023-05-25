// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides IsTrialPersistentToNextResponse which is declared in
// origin_trials.h. IsTrialPersistentToNextResponse is defined in this file
// since changes to it require review from security reviewers, listed in the
// SECURITY_OWNERS file.

#include "third_party/blink/public/common/origin_trials/origin_trials.h"

#include "base/containers/contains.h"

namespace blink::origin_trials {

bool IsTrialPersistentToNextResponse(base::StringPiece trial_name) {
  static base::StringPiece const kPersistentTrials[] = {
      // Enable the FrobulatePersistent* trials as a persistent trials for
      // tests.
      "FrobulatePersistent",
      "FrobulatePersistentExpiryGracePeriod",
      "FrobulatePersistentInvalidOS",
      "FrobulatePersistentThirdPartyDeprecation",
      // Production persistent origin trials follow below:
      "WebViewXRequestedWithDeprecation",
  };
  return base::Contains(kPersistentTrials, trial_name);
}

}  // namespace blink::origin_trials
