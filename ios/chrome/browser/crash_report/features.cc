// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/features.h"

#include "components/breadcrumbs/core/features.h"

// Note the name here is "CrashpadIOSEnabler" and not "CrashpadIOS".  The former
// is the name of the feature that eventually enables the latter synthetic flag
// via NSUserDefault syncing.  This is needed to start CrashpadiOS immediately
// after startup.
const base::Feature kCrashpadIOS{"CrashpadIOSEnabler",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kMetrickitCrashReport{"MetrickitCrashReport",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kMetrickitNonCrashReport{"MetrickitNonCrashReport",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSyntheticCrashReportsForUte{
    "SyntheticCrashReportsForUte", base::FEATURE_DISABLED_BY_DEFAULT};

bool EnableSyntheticCrashReportsForUte() {
  return base::FeatureList::IsEnabled(kSyntheticCrashReportsForUte) &&
         base::FeatureList::IsEnabled(breadcrumbs::kLogBreadcrumbs);
}
