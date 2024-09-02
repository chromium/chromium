// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_view.h"

#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_constants.h"

@implementation TabGroupIndicatorView {
  // Stores the tab group informations.
  NSString* _groupTitle;
  UIColor* _groupColor;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    self.accessibilityIdentifier = kTabGroupIndicatorViewIdentifier;
    // TODO(crbug.com/361499394): Implement this.
  }
  return self;
}

#pragma mark - TabGroupIndicatorConsumer

- (void)setTabGroupTitle:(NSString*)groupTitle groupColor:(UIColor*)groupColor {
  if (groupTitle == _groupTitle && groupColor == _groupColor) {
    return;
  }
  // TODO(crbug.com/361499394): Update the view.
}

@end
