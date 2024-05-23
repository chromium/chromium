// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/omnibox_util.h"

#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"

bool IsCurrentLayoutBottomOmnibox(Browser* browser) {
  LayoutGuideCenter* layout_guide_center = LayoutGuideCenterForBrowser(browser);
  return IsCurrentLayoutBottomOmnibox(layout_guide_center);
}

bool IsCurrentLayoutBottomOmnibox(LayoutGuideCenter* layout_guide_center) {
  if (!IsBottomOmniboxAvailable()) {
    return false;
  }
  UIView* top_omnibox =
      [layout_guide_center referencedViewUnderName:kTopOmniboxGuide];
  // The top omnibox guide is only hidden when the bottom omnibox is shown.
  return top_omnibox.hidden;
}
