// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides IsTrialPersistentToNextResponse which is declared in
// origin_trials.h. IsTrialPersistentToNextResponse is defined in this file
// since changes to it require review from security reviewers, listed in the
// SECURITY_OWNERS file.

#include <string_view>

#include "base/containers/contains.h"
#include "third_party/blink/public/common/origin_trials/origin_trials.h"

namespace blink::origin_trials {

bool IsTrialPersistentToNextResponse(std::string_view trial_name) {
  static std::string_view const kPersistentTrials[] = {
      // Enable the FrobulatePersistent* trials as a persistent trials for
      // tests.
      "FrobulatePersistent",
      "FrobulatePersistentExpiryGracePeriod",
      "FrobulatePersistentInvalidOS",
      "FrobulatePersistentThirdPartyDeprecation",
      // Production persistent origin trials follow below:
      "MediaPreviewsOptOutPersistent",
      "WebViewXRequestedWithDeprecation",
      "Tpcd",
      "TopLevelTpcd",
      "LimitThirdPartyCookies",
      "DisableReduceAcceptLanguage",
      "StorageAccessHeader",
  };
  return base::Contains(kPersistentTrials, trial_name);
}

}  // namespace blink::origin_trials
