// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"

#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/whats_new/feature_flags.h"

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kOverflowMenuEntryKey = @"userHasInteractedWithWhatsNew";

}  // namespace

bool IsWhatsNewOverflowMenuUsed() {
  return [[NSUserDefaults standardUserDefaults]
      objectForKey:kOverflowMenuEntryKey];
}

void SetWhatsNewOverflowMenuUsed() {
  if (IsWhatsNewOverflowMenuUsed())
    return;

  [[NSUserDefaults standardUserDefaults] setBool:YES
                                          forKey:kOverflowMenuEntryKey];
}

bool IsWhatsNewEnabled() {
  return base::FeatureList::IsEnabled(kWhatsNewIOS);
}
