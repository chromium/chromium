// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/toolbar_utils.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

bool ShouldHaveFullHeightTopToolbar(id<UITraitEnvironment> trait_environment) {
  return !IsSplitToolbarMode(trait_environment) ||
         CanShowTabStrip(trait_environment);
}

bool ShouldHaveCompactLocationBar(UITraitCollection* trait_collection) {
  return !IsSplitToolbarMode(trait_collection) ||
         CanShowTabStrip(trait_collection);
}
