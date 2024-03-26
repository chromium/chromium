// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_mutator.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_constants.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

namespace {

// The relative height of the entrypoint badge button compared to the location
// bar's height.
const CGFloat kEntrypointHeightMultiplier = 0.8;

// Damping ratio of animating a change to the entrypoint's badge button.
const CGFloat kUpdateDisplayedBadgeAnimationDamping = 0.85;

// Accessibility identifier for the entrypoint's badge button.
NSString* const kContextualPanelEntrypointBadgeButtonIdentifier =
    @"ContextualPanelEntrypointBadgeButtonAXID";

}  // namespace

@interface ContextualPanelEntrypointViewController () {
  // The stack view that encompasses all the entrypoint's views.
  UIStackView* _stackView;
  // The badge button view for the entrypoint (circular button with image).
  UIButton* _badgeButton;
  // Whether the entrypoint should currently be shown or not (transcends
  // fullscreen events).
  BOOL _entrypointDisplayed;
  // Whether the entrypoint should currently collapse for fullscreen.
  BOOL _shouldCollapseForFullscreen;
}
@end

@implementation ContextualPanelEntrypointViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // Set the view as hidden when created as it should only appear when the
  // entrypoint should be shown.
  self.view.hidden = YES;
  _entrypointDisplayed = NO;

  [self createAndConfigureStackView];
  [self createAndConfigureBadgeButton];
}

- (void)displayEntrypointView:(BOOL)display {
  self.view.hidden = !display || !_entrypointDisplayed;
}

#pragma mark - private

// Creates and configures the entrypoint's stackview.
- (void)createAndConfigureStackView {
  UILayoutGuide* leadingSpace = [[UILayoutGuide alloc] init];
  [self.view addLayoutGuide:leadingSpace];

  _stackView = [[UIStackView alloc] init];
  _stackView.translatesAutoresizingMaskIntoConstraints = NO;
  _stackView.axis = UILayoutConstraintAxisHorizontal;
  _stackView.alignment = UIStackViewAlignmentCenter;
  [self.view addSubview:_stackView];

  [NSLayoutConstraint activateConstraints:@[
    // The badge button doesn't fully fill the height of the location bar, so to
    // make it exactly follow the curvature of the location bar's corner radius,
    // it must be placed with the same amount of margin space horizontally that
    // exists vertically between the badge button and the location bar itself.
    [leadingSpace.widthAnchor
        constraintEqualToAnchor:self.view.heightAnchor
                     multiplier:((1 - kEntrypointHeightMultiplier) / 2)],
    [leadingSpace.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [leadingSpace.trailingAnchor
        constraintEqualToAnchor:_stackView.leadingAnchor],
    [_stackView.leadingAnchor
        constraintEqualToAnchor:leadingSpace.trailingAnchor],
    [_stackView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [_stackView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [self.view.leadingAnchor
        constraintEqualToAnchor:leadingSpace.leadingAnchor],
    [self.view.trailingAnchor
        constraintEqualToAnchor:_stackView.trailingAnchor],
  ]];
}

// Creates and configures the entrypoint's badge button.
- (void)createAndConfigureBadgeButton {
  _badgeButton = [[UIButton alloc] init];
  _badgeButton.translatesAutoresizingMaskIntoConstraints = NO;
  _badgeButton.pointerInteractionEnabled = YES;
  _badgeButton.pointerStyleProvider =
      CreateDefaultEffectCirclePointerStyleProvider();

  UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
      configurationWithPointSize:kInfobarSymbolPointSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
  [_badgeButton setPreferredSymbolConfiguration:symbolConfig
                                forImageInState:UIControlStateNormal];

  _badgeButton.imageView.contentMode = UIViewContentModeScaleAspectFit;

  [_stackView addArrangedSubview:_badgeButton];

  [NSLayoutConstraint activateConstraints:@[
    [_badgeButton.heightAnchor
        constraintEqualToAnchor:_stackView.heightAnchor
                     multiplier:kEntrypointHeightMultiplier],
    [_badgeButton.widthAnchor constraintEqualToAnchor:_badgeButton.heightAnchor]
  ]];

  _badgeButton.clipsToBounds = YES;

  [_badgeButton addTarget:self
                   action:@selector(userTappedEntrypoint)
         forControlEvents:UIControlEventTouchUpInside];
  _badgeButton.accessibilityIdentifier =
      kContextualPanelEntrypointBadgeButtonIdentifier;
}

#pragma mark - ContextualPanelEntrypointConsumer

- (void)setEntrypointConfig:(ContextualPanelItemConfiguration)config {
  _badgeButton.accessibilityLabel =
      base::SysUTF8ToNSString(config.accessibility_label);

  UIImage* image = CustomSymbolWithPointSize(
      base::SysUTF8ToNSString(config.entrypoint_image_name),
      kInfobarSymbolPointSize);
  [_badgeButton setImage:image forState:UIControlStateNormal];
}

- (void)showEntrypoint {
  if (_entrypointDisplayed) {
    return;
  }

  _entrypointDisplayed = YES;

  if (_shouldCollapseForFullscreen) {
    return;
  }

  // Animate the entrypoint appearance.
  self.view.alpha = 0;
  self.view.transform = CGAffineTransformMakeScale(0.1, 0.1);

  self.view.hidden = !_entrypointDisplayed;

  [UIView animateWithDuration:kMaterialDuration2
                        delay:0
       usingSpringWithDamping:kUpdateDisplayedBadgeAnimationDamping
        initialSpringVelocity:0
                      options:UIViewAnimationOptionBeginFromCurrentState
                   animations:^{
                     self.view.alpha = 1;
                     self.view.transform = CGAffineTransformIdentity;
                   }
                   completion:nil];
}

- (void)hideEntrypoint {
  _entrypointDisplayed = NO;
  self.view.hidden = YES;
}

#pragma mark - ContextualPanelEntrypointMutator

- (void)userTappedEntrypoint {
  [self.mutator entrypointTapped];
}

#pragma mark FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  _shouldCollapseForFullscreen = progress <= kFullscreenProgressThreshold;
  if (_shouldCollapseForFullscreen) {
    self.view.hidden = YES;
  } else {
    self.view.hidden = !_entrypointDisplayed;

    // Fade in/out the entrypoint badge.
    CGFloat alphaValue = fmax((progress - kFullscreenProgressThreshold) /
                                  (1 - kFullscreenProgressThreshold),
                              0);
    self.view.alpha = alphaValue;
  }
}

@end
