// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_picker_cell.h"

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
    self.backgroundColor = [UIColor colorNamed:kGrey200Color];

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
    [self.contentView addSubview:addButton];

    [NSLayoutConstraint activateConstraints:@[
      [addButton.centerXAnchor
          constraintEqualToAnchor:self.contentView.centerXAnchor],
      [addButton.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
    ]];
  }
  return self;
}

@end
