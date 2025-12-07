// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/ui/reader_mode_chip_view_controller.h"

#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_constants.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/ui/constants.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_options_commands.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

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

#pragma mark - Private

- (UIButton*)configuredButton {
  __weak __typeof(self) weakSelf = self;
  UIAction* buttonAction = [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf showReaderModeOptions];
  }];

  UIButtonConfiguration* configuration =
      [UIButtonConfiguration filledButtonConfiguration];
  configuration.baseBackgroundColor = [UIColor colorNamed:kBlue600Color];
  configuration.baseForegroundColor =
      [UIColor colorNamed:kInvertedTextPrimaryColor];
  configuration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;

  UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
      configurationWithPointSize:kChipSymbolPointSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
  configuration.image =
      DefaultSymbolWithConfiguration(GetReaderModeSymbolName(), symbolConfig);

  UIButton* button =
      [ExtendedTouchTargetButton buttonWithConfiguration:configuration
                                           primaryAction:buttonAction];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider = CreateLiftEffectCirclePointerStyleProvider();
  button.accessibilityIdentifier = kReaderModeChipViewAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_READER_MODE_CHIP_ACCESSIBILITY_LABEL);

  return button;
}

#pragma mark - UI actions

- (void)showReaderModeOptions {
  [self.readerModeOptionsHandler showReaderModeOptions];
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  // Fade in/out the entrypoint badge.
  CGFloat alphaValue = fmax((progress - kFullscreenProgressThreshold) /
                                (1 - kFullscreenProgressThreshold),
                            0);
  self.view.alpha = alphaValue;
}

@end
