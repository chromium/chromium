// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_view.h"

#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_constants.h"

@implementation TabGroupIndicatorView

- (instancetype)init {
  self = [super init];
  if (self) {
    self.accessibilityIdentifier = kTabGroupIndicatorViewIdentifier;
    // TODO(crbug.com/361499394): Implement this.
  }
  return self;
}

@end
