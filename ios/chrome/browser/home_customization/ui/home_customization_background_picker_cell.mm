// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_picker_cell.h"

#import "ios/chrome/browser/home_customization/ui/home_customization_background_cell+subclassing.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Size of the Add background icon.
const CGFloat kSymbolAddBackgroundPointSize = 12;

}  // namespace

@implementation HomeCustomizationBackgroundPickerCell

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  self.innerContentView.backgroundColor = [UIColor colorNamed:kGrey200Color];
  self.borderWrapperView.layer.borderColor = nil;
  self.borderWrapperView.layer.borderWidth = 0;

  UIImage* plusIcon = SymbolWithPalette(
      CustomSymbolWithPointSize(kPlusCircleFillSymbol,
                                kSymbolAddBackgroundPointSize),
      @[
        // The color of the 'plus'.
        [UIColor whiteColor],
        // The filling color of the circle.
        [UIColor colorNamed:kBlueColor]
      ]);

  UIButton* addButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [addButton
      setImage:[plusIcon
                   imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
      forState:UIControlStateNormal];

  addButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.innerContentView addSubview:addButton];

  [NSLayoutConstraint activateConstraints:@[
    [addButton.centerXAnchor
        constraintEqualToAnchor:self.innerContentView.centerXAnchor],
    [addButton.centerYAnchor
        constraintEqualToAnchor:self.innerContentView.centerYAnchor],
  ]];
}

@end
