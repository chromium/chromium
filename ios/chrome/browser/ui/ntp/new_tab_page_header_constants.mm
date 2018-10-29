// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"

#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ntp_header {

const CGFloat kMinHeaderHeight = 62;
const CGFloat kAnimationDistance = 42;
const CGFloat kToolbarHeight = 48;
const CGFloat kScrolledToTopOmniboxBottomMargin = 4;
const CGFloat kHintLabelSidePadding = 37;
const CGFloat kHintLabelSidePaddingLegacy = 12;
const CGFloat kMaxHorizontalMarginDiff = 5;
const CGFloat kMaxTopMarginDiff = 4;

CGFloat ToolbarHeight() {
  return kToolbarHeight;
}

}  // ntp_header
