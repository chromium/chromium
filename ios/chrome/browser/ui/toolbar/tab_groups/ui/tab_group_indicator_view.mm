// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_view.h"

#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_constants.h"

@implementation TabGroupIndicatorView {
  const tab_groups::TabGroupVisualData* _visualData;
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

- (void)setTabGroupVisuaData:(const tab_groups::TabGroupVisualData*)visualData {
  if (visualData == _visualData) {
    return;
  }
  _visualData = visualData;
  // TODO(crbug.com/361499394): Update the view.
}

@end
