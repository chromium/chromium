// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/ui/reader_mode_chip_view_controller.h"

#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/ui/constants.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

namespace {

// The relative height of the entrypoint badge button compared to the location
// bar's height.
const CGFloat kChipHeightMultiplier = 0.72;

// The point size of the entrypoint's symbol.
const CGFloat kChipSymbolPointSize = 15;

}  // namespace

@interface ReaderModeChipViewController ()

@property(nonatomic, strong) UIButton* button;

@end

@implementation ReaderModeChipViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier =
      kReaderModeChipViewAccessibilityIdentifier;
  self.button = [self configuredButton];
  [self.view addSubview:self.button];
  [NSLayoutConstraint activateConstraints:@[
    [self.view.widthAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.heightAnchor],
    [self.button.heightAnchor constraintEqualToAnchor:self.view.heightAnchor
                                           multiplier:kChipHeightMultiplier],
    [self.button.widthAnchor constraintEqualToAnchor:self.button.heightAnchor],
    [self.button.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
    [self.button.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor],
  ]];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  self.button.layer.cornerRadius = self.button.bounds.size.height / 2.0;
}

#pragma mark - Private

- (UIButton*)configuredButton {
  UIButton* button = [[ExtendedTouchTargetButton alloc] init];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.backgroundColor = [UIColor colorNamed:kBlue600Color];
  button.clipsToBounds = YES;
  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider = CreateLiftEffectCirclePointerStyleProvider();

  UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
      configurationWithPointSize:kChipSymbolPointSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
  UIImage* image =
      DefaultSymbolWithConfiguration(GetReaderModeSymbolName(), symbolConfig);
  [button setImage:image forState:UIControlStateNormal];
  button.tintColor = [UIColor colorNamed:kInvertedTextPrimaryColor];

  return button;
}

@end
