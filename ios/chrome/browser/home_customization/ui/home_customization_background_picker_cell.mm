// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_picker_cell.h"

#import "ios/chrome/browser/home_customization/ui/home_customization_background_picker_presentation_delegate.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_mutator.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Size of the Add background icon.
const CGFloat kSymbolAddBackgroundPointSize = 12;

}  // namespace

@implementation HomeCustomizationBackgroundPickerCell

#pragma mark - HomeCustomizationBackgroundCell

- (void)setupContentView:(UIView*)contentView {
  contentView.backgroundColor = [UIColor colorNamed:kGrey200Color];

  UIImage* plusIcon = SymbolWithPalette(
      CustomSymbolWithPointSize(kPlusCircleFillSymbol,
                                kSymbolAddBackgroundPointSize),
      @[
        // The color of the 'plus'.
        [UIColor whiteColor],
        // The filling color of the circle.
        [UIColor colorNamed:kBlueColor]
      ]);

  UIImageView* plusIconView = [[UIImageView alloc] initWithImage:plusIcon];
  plusIconView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:plusIconView];

  [NSLayoutConstraint activateConstraints:@[
    [plusIconView.centerXAnchor
        constraintEqualToAnchor:contentView.centerXAnchor],
    [plusIconView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
  ]];

  UITapGestureRecognizer* tapGesture =
      [[UITapGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handleTap)];
  [self.contentView addGestureRecognizer:tapGesture];
}

#pragma mark - Private

// Handles tap gesture by notifying the delegate to display background picker
// options.
- (void)handleTap {
  [self.delegate showBackgroundPickerOptions];
}

@end
