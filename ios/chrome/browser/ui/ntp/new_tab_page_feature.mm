// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Feature disabled by default to keep the legacy NTP until the refactored one
// covers all existing functionality.
const base::Feature kRefactoredNTP{"RefactoredNTP",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

bool IsRefactoredNTP() {
  return base::FeatureList::IsEnabled(kRefactoredNTP);
}
