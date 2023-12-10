// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/popup_table_view.h"

#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"

namespace {

// In Popout omnibox, to make the popup visually balanced, some extra vertical
// padding should be removed.
const CGFloat kHeightAdjustmentPopoutOmnibox = -20;

}  // namespace

@implementation PopupTableView

- (CGSize)intrinsicContentSize {
  CGSize size = [super intrinsicContentSize];

  if (IsIpadPopoutOmniboxEnabled()) {
    size.height += kHeightAdjustmentPopoutOmnibox;
  }

  return size;
}

@end
