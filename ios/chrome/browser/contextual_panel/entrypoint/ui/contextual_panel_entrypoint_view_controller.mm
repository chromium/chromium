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
const CGFloat kEntrypointHeightMultiplier = 0.72;

// The margins before and after the entrypoint's label used as multipliers of
// the entrypoint container's height.
const CGFloat kLabelTrailingSpaceMultiplier = 0.375;
const CGFloat kLabelLeadingSpaceMultiplier = 0.095;

// Amount of time animating the entrypoint into the location bar should take.
const NSTimeInterval kEntrypointDisplayingAnimationTime = 0.8;

// Amount of time animating the large entrypoint (label) appearance.
const NSTimeInterval kLargeEntrypointDisplayingAnimationTime = 0.3;

// Entrypoint container shadow constants.
const float kEntrypointContainerShadowOpacity = 0.09f;
const float kEntrypointContainerShadowRadius = 5.0f;
const CGSize kEntrypointContainerShadowOffset = {0, 3};

// The point size of the entrypoint's symbol.
const CGFloat kEntrypointSymbolPointSize = 15;

// The colorset used for the Contextual Panel's entrypoint background.
NSString* const kContextualPanelEntrypointBackgroundColor =
    @"contextual_panel_entrypoint_background_color";

// Accessibility identifier for the entrypoint's image view.
NSString* const kContextualPanelEntrypointImageViewIdentifier =
    @"ContextualPanelEntrypointImageViewAXID";

// Accessibility identifier for the entrypoint's label.
NSString* const kContextualPanelEntrypointLabelIdentifier =
    @"ContextualPanelEntrypointLabelAXID";

}  // namespace

@interface ContextualPanelEntrypointViewController () {
  // The UIButton contains the wrapper UIView, which itself contains the
  // entrypoint items (image and label). The container (UIButton) is needed for
  // button-like behavior and to create the shadow around the entire entrypoint
  // package. The wrapper (UIView) is needed for clipping the label to the
  // entrypoint's bounds for proper animations and sizing.
  UIButton* _entrypointContainer;
  UIView* _entrypointItemsWrapper;
  UIImageView* _imageView;
  UILabel* _label;

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
  _entrypointItemsWrapper = [self configuredEntrypointItemsWrapper];
  _imageView = [self configuredImageView];
  _label = [self configuredLabel];

  [self.view addSubview:_entrypointContainer];
  [_entrypointContainer addSubview:_entrypointItemsWrapper];
  [_entrypointItemsWrapper addSubview:_imageView];
  [_entrypointItemsWrapper addSubview:_label];

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

  _entrypointItemsWrapper.layer.cornerRadius =
      _entrypointItemsWrapper.bounds.size.height / 2.0;
}

- (void)displayEntrypointView:(BOOL)display {
  self.view.hidden = !display || !_entrypointDisplayed;
}

#pragma mark - private

// Creates and configures the entrypoint's button container view.
- (UIButton*)configuredEntrypointContainer {
  UIButton* button = [[UIButton alloc] init];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.backgroundColor =
      [UIColor colorNamed:kContextualPanelEntrypointBackgroundColor];
  button.clipsToBounds = NO;

  // Configure shadow.
  button.layer.shadowColor = [[UIColor blackColor] CGColor];
  button.layer.shadowOpacity = kEntrypointContainerShadowOpacity;
  button.layer.shadowOffset = kEntrypointContainerShadowOffset;
  button.layer.shadowRadius = kEntrypointContainerShadowRadius;
  button.layer.masksToBounds = NO;

  [button addTarget:self
                action:@selector(userTappedEntrypoint)
      forControlEvents:UIControlEventTouchUpInside];

  return button;
}

// Creates and configures the entrypoint's items wrapper view which mirrors the
// container view but adds clipping to bounds.
- (UIView*)configuredEntrypointItemsWrapper {
  UIView* view = [[UIView alloc] init];
  view.translatesAutoresizingMaskIntoConstraints = NO;
  view.userInteractionEnabled = NO;
  view.clipsToBounds = YES;

  return view;
}

// Creates and configures the entrypoint's image view.
- (UIImageView*)configuredImageView {
  UIImageView* imageView = [[UIImageView alloc] init];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  imageView.isAccessibilityElement = NO;
  imageView.contentMode = UIViewContentModeCenter;
  imageView.tintColor = [UIColor colorNamed:kBlue600Color];
  imageView.accessibilityIdentifier =
      kContextualPanelEntrypointImageViewIdentifier;

  [imageView
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                      forAxis:UILayoutConstraintAxisHorizontal];

  UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
      configurationWithPointSize:kEntrypointSymbolPointSize
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
  [label
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                      forAxis:UILayoutConstraintAxisHorizontal];

  return label;
}

- (void)activateInitialConstraints {
  // Leading space before the start of the button container view.
  UILayoutGuide* entrypointLeadingSpace = [[UILayoutGuide alloc] init];
  [self.view addLayoutGuide:entrypointLeadingSpace];

  UILayoutGuide* labelLeadingSpace = [[UILayoutGuide alloc] init];
  UILayoutGuide* labelTrailingSpace = [[UILayoutGuide alloc] init];

  [_entrypointItemsWrapper addLayoutGuide:labelLeadingSpace];
  [_entrypointItemsWrapper addLayoutGuide:labelTrailingSpace];

  _smallTrailingConstraint = [_entrypointContainer.trailingAnchor
      constraintEqualToAnchor:_imageView.trailingAnchor];
  _largeTrailingConstraint = [_entrypointContainer.trailingAnchor
      constraintEqualToAnchor:labelTrailingSpace.trailingAnchor];

  [NSLayoutConstraint activateConstraints:@[
    _smallTrailingConstraint,
    // The entrypoint doesn't fully fill the height of the location bar, so to
    // make it exactly follow the curvature of the location bar's corner radius,
    // it must be placed with the same amount of margin space horizontally that
    // exists vertically between the entrypoint and the location bar itself.
    [entrypointLeadingSpace.widthAnchor
        constraintEqualToAnchor:self.view.heightAnchor
                     multiplier:((1 - kEntrypointHeightMultiplier) / 2)],
    [entrypointLeadingSpace.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [entrypointLeadingSpace.trailingAnchor
        constraintEqualToAnchor:_entrypointContainer.leadingAnchor],
    [_entrypointContainer.leadingAnchor
        constraintEqualToAnchor:entrypointLeadingSpace.trailingAnchor],
    [_entrypointContainer.heightAnchor
        constraintEqualToAnchor:self.view.heightAnchor
                     multiplier:kEntrypointHeightMultiplier],
    [_entrypointContainer.centerYAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
    [self.view.leadingAnchor
        constraintEqualToAnchor:entrypointLeadingSpace.leadingAnchor],
    [self.view.trailingAnchor
        constraintEqualToAnchor:_entrypointContainer.trailingAnchor],
    [_imageView.heightAnchor
        constraintEqualToAnchor:_entrypointContainer.heightAnchor],
    [_imageView.widthAnchor constraintEqualToAnchor:_imageView.heightAnchor],
    [_imageView.leadingAnchor
        constraintEqualToAnchor:_entrypointContainer.leadingAnchor],
    [_imageView.centerYAnchor
        constraintEqualToAnchor:_entrypointContainer.centerYAnchor],
    [labelLeadingSpace.leadingAnchor
        constraintEqualToAnchor:_imageView.trailingAnchor],
    [labelLeadingSpace.widthAnchor
        constraintEqualToAnchor:self.view.heightAnchor
                     multiplier:kLabelLeadingSpaceMultiplier],
    [labelTrailingSpace.widthAnchor
        constraintEqualToAnchor:self.view.heightAnchor
                     multiplier:kLabelTrailingSpaceMultiplier],
    [labelTrailingSpace.leadingAnchor
        constraintEqualToAnchor:_label.trailingAnchor],
    [_label.heightAnchor
        constraintEqualToAnchor:_entrypointContainer.heightAnchor],
    [_label.centerYAnchor
        constraintEqualToAnchor:_entrypointContainer.centerYAnchor],
    [_label.leadingAnchor
        constraintEqualToAnchor:labelLeadingSpace.trailingAnchor],
  ]];

  AddSameConstraints(_entrypointItemsWrapper, _entrypointContainer);
}

- (void)activateLargeEntrypointTrailingConstraint {
  _smallTrailingConstraint.active = NO;
  _largeTrailingConstraint.active = YES;
}

- (void)activateSmallEntrypointTrailingConstraint {
  _largeTrailingConstraint.active = NO;
  _smallTrailingConstraint.active = YES;
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
      kEntrypointSymbolPointSize);

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
                      options:UIViewAnimationOptionCurveEaseOut
                   animations:^{
                     self.view.alpha = 1;
                     self.view.transform = CGAffineTransformIdentity;
                   }
                   completion:nil];
}

- (void)hideEntrypoint {
  [self transitionToSmallEntrypoint];
  [self transitionToContextualPanelOpenedState:NO];

  _entrypointDisplayed = NO;
  self.view.hidden = YES;

  [self.mutator setLocationBarLabelCenteredBetweenContent:NO];

  [self.view layoutIfNeeded];
}

- (void)transitionToLargeEntrypoint {
  if (_largeTrailingConstraint.active) {
    return;
  }

  __weak ContextualPanelEntrypointViewController* weakSelf = self;

  void (^animateTransitionToLargeEntrypoint)() = ^{
    ContextualPanelEntrypointViewController* strongSelf = weakSelf;

    if (!strongSelf) {
      return;
    }

    [strongSelf activateLargeEntrypointTrailingConstraint];
    [strongSelf.mutator setLocationBarLabelCenteredBetweenContent:YES];
    [strongSelf.view layoutIfNeeded];
  };

  [UIView animateWithDuration:kLargeEntrypointDisplayingAnimationTime
                        delay:0
                      options:(UIViewAnimationOptionCurveEaseOut |
                               UIViewAnimationOptionAllowUserInteraction)
                   animations:animateTransitionToLargeEntrypoint
                   completion:nil];
}

- (void)transitionToSmallEntrypoint {
  if (_smallTrailingConstraint.active) {
    return;
  }

  __weak ContextualPanelEntrypointViewController* weakSelf = self;

  void (^animateTransitionToSmallEntrypoint)() = ^{
    ContextualPanelEntrypointViewController* strongSelf = weakSelf;

    if (!strongSelf) {
      return;
    }

    [strongSelf activateSmallEntrypointTrailingConstraint];
    [strongSelf.mutator setLocationBarLabelCenteredBetweenContent:NO];
    [strongSelf.view layoutIfNeeded];
  };

  [UIView animateWithDuration:kLargeEntrypointDisplayingAnimationTime
                        delay:0
                      options:(UIViewAnimationOptionCurveEaseOut |
                               UIViewAnimationOptionAllowUserInteraction)
                   animations:animateTransitionToSmallEntrypoint
                   completion:nil];
}

- (void)transitionToContextualPanelOpenedState:(BOOL)opened {
  // Using `clipsToBounds` to remove the shadow when in "opened" state.
  _entrypointContainer.clipsToBounds = opened;

  _imageView.tintColor = opened ? [UIColor colorNamed:kGrey600Color]
                                : [UIColor colorNamed:kBlue600Color];

  _entrypointContainer.backgroundColor =
      opened ? [UIColor colorNamed:kTertiaryBackgroundColor]
             : [UIColor colorNamed:kContextualPanelEntrypointBackgroundColor];

  [self transitionToSmallEntrypoint];
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
