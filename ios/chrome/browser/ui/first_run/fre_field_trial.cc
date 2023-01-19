// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/first_run/fre_field_trial.h"

#include "components/signin/ios/browser/features.h"

namespace fre_field_trial {

NewMobileIdentityConsistencyFRE GetNewMobileIdentityConsistencyFRE() {
  return base::FeatureList::IsEnabled(signin::kNewMobileIdentityConsistencyFRE)
             ? NewMobileIdentityConsistencyFRE::kTangibleSyncA
             : NewMobileIdentityConsistencyFRE::kOld;
}

}  // namespace fre_field_trial
