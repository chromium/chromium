// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_skeleton_cell.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation HomeCustomizationBackgroundSkeletonCell

#pragma mark - HomeCustomizationBackgroundCell

- (void)setupContentView:(UIView*)contentView {
  [self applyTheme];
}

- (void)applyTheme {
  self.innerContentView.backgroundColor = [UIColor colorNamed:kGrey100Color];
}

@end
