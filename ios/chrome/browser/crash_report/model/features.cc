// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/model/features.h"

BASE_FEATURE(kMetrickitNonCrashReport, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMetrickitDeferRegistration, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsMetrickitDeferRegistrationEnabled() {
  return base::FeatureList::IsEnabled(kMetrickitDeferRegistration);
}
