// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/features.h"

#include "ios/chrome/browser/crash_report/breadcrumbs/features.h"

const base::Feature kCrashpadIOS{"CrashpadIOS",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSyntheticCrashReportsForUte{
    "SyntheticCrashReportsForUte", base::FEATURE_DISABLED_BY_DEFAULT};

bool EnableSyntheticCrashReportsForUte() {
  return base::FeatureList::IsEnabled(kSyntheticCrashReportsForUte) &&
         base::FeatureList::IsEnabled(kLogBreadcrumbs);
}
