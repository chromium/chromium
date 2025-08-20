// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/tabs/model/features.h"

BASE_FEATURE(kCreateTabHelperOnlyForRealizedWebStates,
             "CreateTabHelperOnlyForRealizedWebStates",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool CreateTabHelperOnlyForRealizedWebStates() {
  return base::FeatureList::IsEnabled(kCreateTabHelperOnlyForRealizedWebStates);
}
