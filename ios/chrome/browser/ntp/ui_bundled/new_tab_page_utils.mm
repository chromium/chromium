// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_utils.h"

#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"

bool ShouldShowTopOfFeedSyncPromo() {
  // Checks the flag and ensures that the user is not in first run.
  return IsDiscoverFeedTopSyncPromoEnabled() &&
         !ShouldPresentFirstRunExperience();
}
