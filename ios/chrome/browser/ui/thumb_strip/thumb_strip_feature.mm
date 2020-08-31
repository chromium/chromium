// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_feature.h"

#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool IsThumbStripEnabled() {
  return IsIPadIdiom() && base::FeatureList::IsEnabled(kExpandedTabStrip);
}
