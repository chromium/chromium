// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_view_controller.h"

#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_mutator.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_constants.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

namespace {

// The relative height of the entrypoint badge button compared to the location
// bar's height.
const CGFloat kEntrypointHeightMultiplier = 0.8;

// Amount of time animating the entrypoint into the location bar should take.
const NSTimeInterval kEntrypointDisplayingAnimationTime = 0.8;

// Accessibility identifier for the entrypoint's image view.
NSString* const kContextualPanelEntrypointImageViewIdentifier =
    @"ContextualPanelEntrypointImageViewAXID";

// Accessibility identifier for the entrypoint's label.
NSString* const kContextualPanelEntrypointLabelIdentifier =
    @"ContextualPanelEntrypointLabelAXID";

}  // namespace

@interface ContextualPanelEntrypointViewController () {
  // The UIButton view containing the image and label of the entrypoint. The
  // button acts as the container for the separate UIImageView and UILabel below
  // to enable proper positioning, animations and button-like behavior of the
  // entire entrypoint package.
  UIButton* _entrypointContainer;
  UIImageView* _imageView;
  UILabel* _label;

  // UILayoutGuide to add a trailing space after the label (when it is shown)
  // but before the end of the button container.
  UILayoutGuide* _labelTrailingSpace;
  // Constraints for the two states of the trailing edge of the entrypoint
  // container. They are activated/deactivated as needed when the label is
  // shown/hidden.
  NSLayoutConstraint* _largeTrailingConstraint;
  NSLayoutConstraint* _smallTrailingConstraint;

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

  _entrypointContainer = [self configuredEntrypointContainer];
  _imageView = [self configuredImageView];
  _label = [self configuredLabel];

  [self.view addSubview:_entrypointContainer];
  [_entrypointContainer addSubview:_imageView];
  [_entrypointContainer addSubview:_label];

  [self activateInitialConstraints];

  if (@available(iOS 17, *)) {
    [self registerForTraitChanges:@[ UITraitPreferredContentSizeCategory.self ]
                       withAction:@selector(updateLabelFont)];
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  _entrypointContainer.layer.cornerRadius =
      _entrypointContainer.bounds.size.height / 2.0;
}

- (void)displayEntrypointView:(BOOL)display {
  self.view.hidden = !display || !_entrypointDisplayed;
}

#pragma mark - private

// Creates and configures the entrypoint's button container view.
- (UIButton*)configuredEntrypointContainer {
  UIButton* button = [[UIButton alloc] init];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  [button addTarget:self
                action:@selector(userTappedEntrypoint)
      forControlEvents:UIControlEventTouchUpInside];

  return button;
}

// Creates and configures the entrypoint's image view.
- (UIImageView*)configuredImageView {
  UIImageView* imageView = [[UIImageView alloc] init];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  imageView.isAccessibilityElement = NO;
  imageView.contentMode = UIViewContentModeCenter;
  imageView.accessibilityIdentifier =
      kContextualPanelEntrypointImageViewIdentifier;

  UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
      configurationWithPointSize:kInfobarSymbolPointSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
  imageView.preferredSymbolConfiguration = symbolConfig;

  return imageView;
}

// Creates and configures the entrypoint's label for louder moments. Starts off
// as hidden.
- (UILabel*)configuredLabel {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [self entrypointLabelFont];
  label.numberOfLines = 1;
  label.accessibilityIdentifier = kContextualPanelEntrypointLabelIdentifier;
  label.hidden = YES;

  return label;
}

- (void)activateInitialConstraints {
  // Leading space before the start of the button container view.
  UILayoutGuide* leadingSpace = [[UILayoutGuide alloc] init];
  [self.view addLayoutGuide:leadingSpace];

  _labelTrailingSpace = [[UILayoutGuide alloc] init];
  [_entrypointContainer addLayoutGuide:_labelTrailingSpace];

  _smallTrailingConstraint = [_entrypointContainer.trailingAnchor
      constraintEqualToAnchor:_imageView.trailingAnchor];
  _largeTrailingConstraint = [_entrypointContainer.trailingAnchor
      constraintEqualToAnchor:_labelTrailingSpace.trailingAnchor];

  [NSLayoutConstraint activateConstraints:@[
    _smallTrailingConstraint,
    // The entrypoint doesn't fully fill the height of the location bar, so to
    // make it exactly follow the curvature of the location bar's corner radius,
    // it must be placed with the same amount of margin space horizontally that
    // exists vertically between the entrypoint and the location bar itself.
    [leadingSpace.widthAnchor
        constraintEqualToAnchor:self.view.heightAnchor
                     multiplier:((1 - kEntrypointHeightMultiplier) / 2)],
    [leadingSpace.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [leadingSpace.trailingAnchor
        constraintEqualToAnchor:_entrypointContainer.leadingAnchor],
    [_entrypointContainer.leadingAnchor
        constraintEqualToAnchor:leadingSpace.trailingAnchor],
    [_entrypointContainer.heightAnchor
        constraintEqualToAnchor:self.view.heightAnchor
                     multiplier:kEntrypointHeightMultiplier],
    [_entrypointContainer.centerYAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
    [self.view.leadingAnchor
        constraintEqualToAnchor:leadingSpace.leadingAnchor],
    [self.view.trailingAnchor
        constraintEqualToAnchor:_entrypointContainer.trailingAnchor],
    [_imageView.heightAnchor
        constraintEqualToAnchor:_entrypointContainer.heightAnchor],
    [_imageView.widthAnchor constraintEqualToAnchor:_imageView.heightAnchor],
    [_imageView.leadingAnchor
        constraintEqualToAnchor:_entrypointContainer.leadingAnchor],
    [_imageView.centerYAnchor
        constraintEqualToAnchor:_entrypointContainer.centerYAnchor],
    [_labelTrailingSpace.widthAnchor
        constraintEqualToAnchor:self.view.heightAnchor
                     multiplier:((1 - kEntrypointHeightMultiplier))],
    [_labelTrailingSpace.leadingAnchor
        constraintEqualToAnchor:_label.trailingAnchor],
    [_label.heightAnchor
        constraintEqualToAnchor:_entrypointContainer.heightAnchor],
    [_label.centerYAnchor
        constraintEqualToAnchor:_entrypointContainer.centerYAnchor],
    [_label.leadingAnchor constraintEqualToAnchor:_imageView.trailingAnchor],
  ]];
}

- (void)updateLabelFont {
  _label.font = [self entrypointLabelFont];
}

// Returns the preferred font and size given the current ContentSizeCategory.
- (UIFont*)entrypointLabelFont {
  return PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleFootnote,
      self.traitCollection.preferredContentSizeCategory,
      UIContentSizeCategoryAccessibilityLarge);
}

#pragma mark - ContextualPanelEntrypointConsumer

- (void)setEntrypointConfig:
    (base::WeakPtr<ContextualPanelItemConfiguration>)config {
  if (!config) {
    return;
  }

  _entrypointContainer.accessibilityLabel =
      base::SysUTF8ToNSString(config->accessibility_label);

  _label.text = base::SysUTF8ToNSString(config->entrypoint_message);

  UIImage* image = CustomSymbolWithPointSize(
      base::SysUTF8ToNSString(config->entrypoint_image_name),
      kInfobarSymbolPointSize);

  _imageView.image = image;
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
  self.view.transform = CGAffineTransformMakeScale(0.85, 0.85);

  self.view.hidden = !_entrypointDisplayed;

  [UIView animateWithDuration:kEntrypointDisplayingAnimationTime
                        delay:0
       usingSpringWithDamping:1
        initialSpringVelocity:0
                      options:UIViewAnimationOptionCurveEaseOut
                   animations:^{
                     self.view.alpha = 1;
                     self.view.transform = CGAffineTransformIdentity;
                   }
                   completion:nil];
}

- (void)hideEntrypoint {
  [self transitionToSmallEntrypoint];

  _entrypointDisplayed = NO;
  self.view.hidden = YES;

  [self.mutator setLocationBarLabelCenteredBetweenContent:NO];

  [self.view layoutIfNeeded];
}

- (void)transitionToLargeEntrypoint {
  // TODO(crbug.com/332911172): Animate the following changes.

  _smallTrailingConstraint.active = NO;
  _largeTrailingConstraint.active = YES;
  _label.hidden = NO;

  [self.mutator setLocationBarLabelCenteredBetweenContent:YES];

  [self.view layoutIfNeeded];
}

- (void)transitionToSmallEntrypoint {
  // TODO(crbug.com/332911172): Animate the following changes.

  _largeTrailingConstraint.active = NO;
  _smallTrailingConstraint.active = YES;
  _label.hidden = YES;

  [self.mutator setLocationBarLabelCenteredBetweenContent:NO];

  [self.view layoutIfNeeded];
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

#pragma mark - UIView

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  if (@available(iOS 17, *)) {
    return;
  }

  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    [self updateLabelFont];
  }
}

@end
