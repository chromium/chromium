// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_picker_cell.h"

#import "ios/chrome/browser/home_customization/ui/home_customization_background_cell+subclassing.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_picker_presentation_delegate.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_mutator.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Size of the Add background icon.
const CGFloat kSymbolAddBackgroundPointSize = 12;

}  // namespace

@implementation HomeCustomizationBackgroundPickerCell

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
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

    UIImageView* plusIconView = [[UIImageView alloc] initWithImage:plusIcon];
    plusIconView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.innerContentView addSubview:plusIconView];

    [NSLayoutConstraint activateConstraints:@[
      [plusIconView.centerXAnchor
          constraintEqualToAnchor:self.innerContentView.centerXAnchor],
      [plusIconView.centerYAnchor
          constraintEqualToAnchor:self.innerContentView.centerYAnchor],
    ]];

    UITapGestureRecognizer* tapGesture =
        [[UITapGestureRecognizer alloc] initWithTarget:self
                                                action:@selector(handleTap)];
    [self.contentView addGestureRecognizer:tapGesture];
  }
  return self;
}

#pragma mark - Private

// Handles tap gesture by notifying the delegate to display background picker
// options.
- (void)handleTap {
  [self.delegate showBackgroundPickerOptions];
}

@end
