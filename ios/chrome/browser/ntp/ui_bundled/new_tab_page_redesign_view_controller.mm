// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/526677926): Clean up or delete file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_redesign_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module_container.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_config.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_framing_coordinates.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_image_view.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_availability.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_state.h"
#import "ios/chrome/browser/ntp/ui_bundled/fake_location_bar_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_commands.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_mutator.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_quick_actions_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_shortcuts_handler.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/ntp_identity_disc_button.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Animation duration for wallpaper transition.
constexpr CGFloat kBackgroundImageAnimationDuration = 0.25;

// Spacing between fake omnibox and most visited tiles (MVTs) container.
constexpr CGFloat kOmniboxToMVTSpacing = 16.0;

// Spacing between fake omnibox and quick actions row.
constexpr CGFloat kQuickActionSpacingTop = 3.0;

// Spacing between quick actions row and the content below it.
constexpr CGFloat kQuickActionSpacingBottom = 19.0;

// Spacing between the Google logo and the fake location bar.
constexpr CGFloat kLogoToOmniboxSpacing = 24.0;

// Spacing from the top of the bottom sheet to the omnibox when expanded.
constexpr CGFloat kExpandedSheetOmniboxTopMargin = 16.0;

// Spacing from the top of the bottom sheet to the MVTs container when
// resting/collapsed.
constexpr CGFloat kRestingSheetMVTTopMargin = 12.0;

// Default fallback height for the MVTs container before initial layout.
constexpr CGFloat kDefaultMVTHeightFallback = 124.0;

// Top margin of the Google logo view.
constexpr CGFloat kLogoTopMargin = 40.0;

// Width dimensions for Doodle and Google logo layouts.
constexpr CGFloat kDoodleLogoWidth = 320.0;
constexpr CGFloat kGoogleLogoWidth = 170.0;

// Fakebox layout constants.
constexpr CGFloat kFakeboxImageSize = 20.0;
constexpr CGFloat kIconDividerHeight = 13.0;
constexpr CGFloat kButtonSpacing = 9.0;
constexpr CGFloat kHintLabelFakeboxTrailingSpace = 12.0f;
constexpr CGFloat kEndButtonFakeboxTrailingSpace = 13.0f;
constexpr CGFloat kEndButtonNormalSizeFakeboxWithBadgeTrailingSpace = 7.0f;

constexpr CGFloat kHintLabelFakeboxLeadingSpaceWithIcon = 42.0;
constexpr CGFloat kHintLabelFakeboxLeadingSpaceWithPlus = 46.0;
constexpr CGFloat kFakeboxImageLeadingSpace = 13.0;
constexpr CGFloat kFakeboxPlusLeadingSpace = 18.0;

// Vertical visual alignment nudges for fakebox elements.
constexpr CGFloat kLogoViewYOffset = 1.0;
constexpr CGFloat kHintLabelYOffset = -1.0;
}  // namespace

@interface NTPRedesignTouchAreaOverflowStackView : UIStackView
@end

@implementation NTPRedesignTouchAreaOverflowStackView

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
  for (UIView* subview in self.arrangedSubviews) {
    CGPoint convertedPoint = [self convertPoint:point toView:subview];
    if ([subview pointInside:convertedPoint withEvent:event]) {
      return YES;
    }
  }
  return NO;
}

@end

@interface NewTabPageRedesignViewController () <
    NewTabPageBottomSheetViewControllerDelegate>

// Properties conformed to by `NewTabPageConsumer`
@property(nonatomic, assign, readwrite) CGFloat collectionShiftingOffset;
@property(nonatomic, assign, readwrite) BOOL scrolledToMinimumHeight;

// The Most Visited Tiles (MVTs) view.
@property(nonatomic, strong) UIView* mostVisitedView;

// Private helpers
- (void)handleTraitChanges;

@end

@implementation NewTabPageRedesignViewController {
  HomeCustomizationImageView* _backgroundImageView;
  UIImage* _backgroundImage;
  HomeCustomizationFramingCoordinates* _framingCoordinates;
  NewTabPageBottomSheetViewController* _bottomSheetViewController;
  UIViewController* _feedViewController;
  NSArray<NSLayoutConstraint*>* _logoConstraints;
  SearchEngineLogoState _logoState;

  FakeLocationBarView* _fakeLocationBar;
  UIView* _mostVisitedContainerView;
  NSLayoutConstraint* _fakeLocationBarTopConstraint;
  NTPIdentityDiscButton* _identityDiscButton;

  UIImage* _avatarImage;
  BOOL _hasAITier;
  NSString* _avatarName;
  NSString* _avatarEmail;
  BOOL _avatarImageLoaded;

  // Layout constraints for top content.
  NSLayoutConstraint* _mvtTopConstraint;
  NSLayoutConstraint* _qaTopConstraint;
  NewTabPageQuickActionsViewController* _quickActionsViewController;

  // Fake omnibox subviews and state
  NTPRedesignTouchAreaOverflowStackView* _buttonStack;
  ExtendedTouchTargetButton* _voiceSearchButton;
  ExtendedTouchTargetButton* _lensButton;
  ExtendedTouchTargetButton* _plusButton;
  UIView* _voiceAndLensDivider;
  UIImageView* _logoView;
  UILabel* _hintLabel;
  UIImage* _dseLogo;
  BOOL _voiceSearchIsEnabled;
  NSString* _defaultSearchEngineName;
  BOOL _isGoogleDefaultSearchEngine;
  BOOL _isAIMAllowed;
  BOOL _fuseboxEligible;
  BOOL _didNotifyLensBadgeDisplay;
  BOOL _lensButtonWithNewBadgeTapped;
  NSLayoutConstraint* _fakeLocationBarWidthConstraint;
  NSLayoutConstraint* _fakeLocationBarHeightConstraint;
  __weak UIView* _leadingView;
  NSLayoutConstraint* _leadingViewConstraint;
  NSLayoutConstraint* _hintLabelLeadingConstraint;
  NSLayoutConstraint* _hintLabelTrailingConstraint;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:@"ntp_background_color"];

  _backgroundImageView = [[HomeCustomizationImageView alloc] init];
  _backgroundImageView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_backgroundImageView];
  AddSameConstraints(_backgroundImageView, self.view);

  _bottomSheetViewController =
      [[NewTabPageBottomSheetViewController alloc] init];
  _bottomSheetViewController.delegate = self;
  _bottomSheetViewController.feedViewController = _feedViewController;
  _bottomSheetViewController.magicStackViewController =
      _magicStackViewController;
  [self addChildViewController:_bottomSheetViewController];
  [self.view addSubview:_bottomSheetViewController.view];
  [_bottomSheetViewController didMoveToParentViewController:self];

  _defaultSearchEngineName = @"Google";
  _isGoogleDefaultSearchEngine = YES;

  // Add fake location bar ON TOP of the sheet.
  _fakeLocationBar = [[FakeLocationBarView alloc] init];
  _fakeLocationBar.translatesAutoresizingMaskIntoConstraints = NO;
  [_fakeLocationBar addTarget:self
                       action:@selector(fakeLocationBarTapped)
             forControlEvents:UIControlEventTouchUpInside];
  _fakeLocationBar.isAccessibilityElement = YES;
  _fakeLocationBar.accessibilityIdentifier = @"ntp-redesign-fake-omnibox";
  [self.view addSubview:_fakeLocationBar];

  _hintLabel = [[UILabel alloc] init];
  _hintLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _hintLabel.textColor = [UIColor colorNamed:kTextfieldPlaceholderColor];
  _hintLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  _hintLabel.adjustsFontSizeToFitWidth = YES;
  _hintLabel.minimumScaleFactor = 0.57;
  _hintLabel.isAccessibilityElement = NO;
  [_hintLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [_fakeLocationBar addSubview:_hintLabel];

  [NSLayoutConstraint activateConstraints:@[
    [_hintLabel.centerYAnchor constraintEqualToAnchor:_fakeLocationBar.centerYAnchor
                                             constant:kHintLabelYOffset],
  ]];

  _buttonStack = [[NTPRedesignTouchAreaOverflowStackView alloc] init];
  _buttonStack.translatesAutoresizingMaskIntoConstraints = NO;
  _buttonStack.alignment = UIStackViewAlignmentCenter;
  _buttonStack.spacing = kButtonSpacing;
  _buttonStack.layoutMarginsRelativeArrangement = YES;
  [_fakeLocationBar addSubview:_buttonStack];

  [_fakeLocationBar applyBackgroundTheme];
  [_fakeLocationBar updateColorsWithProgress:0.0 colorPalette:nil];

  if (IsAimEnabledInNtp()) {
    _quickActionsViewController =
        [[NewTabPageQuickActionsViewController alloc] init];
    _quickActionsViewController.layoutGuideCenter = self.layoutGuideCenter;
    _quickActionsViewController.NTPShortcutsHandler = self.NTPShortcutsHandler;
    [self addChildViewController:_quickActionsViewController];

    // Insert BELOW the sheet.
    _quickActionsViewController.view.translatesAutoresizingMaskIntoConstraints =
        NO;
    [self.view insertSubview:_quickActionsViewController.view
                belowSubview:_bottomSheetViewController.view];
    [_quickActionsViewController didMoveToParentViewController:self];
    _quickActionsViewController.view.hidden = !self.quickActionsVisible;
  }

  // Add Most Visited Tiles (MVTs) container.
  _mostVisitedContainerView = [[UIView alloc] init];
  _mostVisitedContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  // Insert BELOW the sheet.
  [self.view insertSubview:_mostVisitedContainerView
              belowSubview:_bottomSheetViewController.view];

  // Configure layout constraints
  _fakeLocationBarTopConstraint = [_fakeLocationBar.topAnchor
      constraintEqualToAnchor:self.view.topAnchor
                     constant:[self centeredFakeOmniboxTop]];
  _fakeLocationBarWidthConstraint = [_fakeLocationBar.widthAnchor
      constraintEqualToConstant:[self fakeLocationBarWidth]];
  _fakeLocationBarHeightConstraint = [_fakeLocationBar.heightAnchor
      constraintEqualToConstant:content_suggestions::FakeOmniboxHeight()];

  [NSLayoutConstraint activateConstraints:@[
    _fakeLocationBarTopConstraint,
    [_fakeLocationBar.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    _fakeLocationBarWidthConstraint,
    _fakeLocationBarHeightConstraint,

    [_buttonStack.trailingAnchor
        constraintEqualToAnchor:_fakeLocationBar.trailingAnchor],
    [_buttonStack.centerYAnchor
        constraintEqualToAnchor:_fakeLocationBar.centerYAnchor],

    [_mostVisitedContainerView.widthAnchor
        constraintEqualToAnchor:_fakeLocationBar.widthAnchor],
    [_mostVisitedContainerView.centerXAnchor
        constraintEqualToAnchor:_fakeLocationBar.centerXAnchor],
  ]];

  if (IsAimEnabledInNtp()) {
    _qaTopConstraint = [_quickActionsViewController.view.topAnchor
        constraintEqualToAnchor:_fakeLocationBar.bottomAnchor
                       constant:kQuickActionSpacingTop];

    [NSLayoutConstraint activateConstraints:@[
      _qaTopConstraint,
      [_quickActionsViewController.view.widthAnchor
          constraintEqualToAnchor:_fakeLocationBar.widthAnchor],
      [_quickActionsViewController.view.centerXAnchor
          constraintEqualToAnchor:_fakeLocationBar.centerXAnchor],
    ]];
  }

  UIView* anchorView = self.quickActionsVisible
                           ? _quickActionsViewController.view
                           : _fakeLocationBar;
  CGFloat constant = self.quickActionsVisible ? kQuickActionSpacingBottom
                                              : kOmniboxToMVTSpacing;

  _mvtTopConstraint = [_mostVisitedContainerView.topAnchor
      constraintEqualToAnchor:anchorView.bottomAnchor
                     constant:constant];
  _mvtTopConstraint.active = YES;

  _fakeLocationBar.layer.cornerRadius =
      _fakeLocationBarHeightConstraint.constant / 2.0;

  [self refreshFakeboxContent];

  if (self.mostVisitedView) {
    [self embedMostVisitedView];
  }

  if (_searchEngineLogoView) {
    [self addSearchEngineLogoView];
  }

  // Add identity disc button.
  _identityDiscButton = [[NTPIdentityDiscButton alloc] init];
  _identityDiscButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_identityDiscButton addTarget:self
                          action:@selector(identityDiscButtonTapped:)
                forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:_identityDiscButton];

  [NSLayoutConstraint activateConstraints:@[
    [_identityDiscButton.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:12.0],
  ]];
  [_identityDiscButton
      setupConstraintsWithTrailingAnchor:self.view.safeAreaLayoutGuide
                                             .trailingAnchor];

  if (_avatarImageLoaded) {
    if (_avatarImage) {
      [_identityDiscButton updateAccountWithName:_avatarName
                                           email:_avatarEmail
                                     avatarImage:_avatarImage
                                       hasAITier:_hasAITier];
    } else {
      [_identityDiscButton setSignedOutAccountImage];
    }
  }
  __weak __typeof(self) weakSelf = self;
  [self registerForTraitChanges:@[
    UITraitHorizontalSizeClass.class, UITraitVerticalSizeClass.class,
    UITraitPreferredContentSizeCategory.class, UITraitUserInterfaceStyle.class
  ]
                    withHandler:^(id<UITraitEnvironment> traitEnvironment,
                                  UITraitCollection* previousCollection) {
                      [weakSelf handleTraitChanges];
                    }];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  _fakeLocationBarWidthConstraint.constant = [self fakeLocationBarWidth];
  _fakeLocationBarHeightConstraint.constant =
      content_suggestions::FakeOmniboxHeight();
  _fakeLocationBar.layer.cornerRadius =
      _fakeLocationBarHeightConstraint.constant / 2.0;

  // Update fake omnibox top if the bottom sheet is not pushing it
  if (_bottomSheetViewController) {
    CGFloat currentTopOffset = _bottomSheetViewController.view.frame.origin.y;
    [self bottomSheetViewController:_bottomSheetViewController
                 didUpdateTopOffset:currentTopOffset];
  }
}

#pragma mark - UIViewController Overrides

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  self.viewDidAppear = YES;

  if (_lensButton && self.useNewBadgeForLensButton &&
      !_didNotifyLensBadgeDisplay) {
    [self.mutator notifyLensBadgeDisplayed];
    _didNotifyLensBadgeDisplay = YES;
  }
}

#pragma mark - Private Helper

- (void)handleTraitChanges {
  [self updateLogoConstraints];
  [self refreshFakeboxContent];
}

- (void)setUseNewBadgeForLensButton:(BOOL)useNewBadgeForLensButton {
  if (_useNewBadgeForLensButton == useNewBadgeForLensButton) {
    return;
  }
  _useNewBadgeForLensButton = useNewBadgeForLensButton;
  if (self.isViewLoaded) {
    [self refreshFakeboxContent];
  }
}

- (void)invalidate {
  self.mutator = nil;
  self.searchEngineLogoView = nil;
  self.NTPContentDelegate = nil;
  self.NTPShortcutsHandler = nil;
  self.mostVisitedView = nil;
  self.magicStackViewController = nil;
  [self setFeedViewController:nil];
  [_bottomSheetViewController invalidate];
  _bottomSheetViewController = nil;
  _identityDiscButton = nil;
  _avatarImage = nil;
  _avatarName = nil;
  _avatarEmail = nil;
}

#pragma mark - Public

- (void)focusOmnibox {
  [self.NTPContentDelegate focusOmnibox];
}

#pragma mark - Action Targets

- (void)fakeLocationBarTapped {
  [self focusOmnibox];
}

#pragma mark - NewTabPageBottomSheetViewControllerDelegate

- (CGFloat)restingOffsetForBottomSheetViewController:
    (NewTabPageBottomSheetViewController*)viewController {
  return [self centeredFakeOmniboxTop] + [self topContentHeight] +
         kRestingSheetMVTTopMargin;
}

- (CGFloat)collapsedOffsetForBottomSheetViewController:
    (NewTabPageBottomSheetViewController*)viewController {
  UIView* superview = self.view;
  CGFloat safeAreaBottom = superview.safeAreaInsets.bottom;
  CGFloat collapsedHeight = safeAreaBottom + 80.0;
  return superview.bounds.size.height - collapsedHeight;
}

- (void)bottomSheetViewController:
            (NewTabPageBottomSheetViewController*)bottomSheetViewController
               didUpdateTopOffset:(CGFloat)topOffset {
  // Interpolate fake omnibox position relative to the sheet top
  CGFloat expandedOffset = [_bottomSheetViewController expandedOffset];
  CGFloat restingOffset = [_bottomSheetViewController restingOffset];

  CGFloat progress = 1.0;
  if (restingOffset > expandedOffset) {
    progress = (topOffset - expandedOffset) / (restingOffset - expandedOffset);
    progress = MIN(1.0, MAX(0.0, progress));
  }

  CGFloat restingOffsetFromSheet =
      -([self topContentHeight] + kRestingSheetMVTTopMargin);

  // Spacing offset from sheet top: restingOffsetFromSheet when
  // resting/collapsed (above sheet), +16 pt when expanded (inside sheet)
  CGFloat offsetFromSheet = progress * restingOffsetFromSheet +
                            (1.0 - progress) * kExpandedSheetOmniboxTopMargin;
  _fakeLocationBarTopConstraint.constant = topOffset + offsetFromSheet;

  // Interpolate opacity for logo, MVTs row, and identity disc
  _searchEngineLogoView.alpha = progress;
  _mostVisitedContainerView.alpha = progress;
  _identityDiscButton.alpha = progress;
  if (_quickActionsViewController) {
    _quickActionsViewController.view.alpha = progress;
  }

  [self.view layoutIfNeeded];
}

#pragma mark - NewTabPageConsumer

- (void)omniboxDidBecomeFirstResponder {
  // TODO(crbug.com/526677926): To be implemented in Phase 2/3.
}

- (void)omniboxWillResignFirstResponder {
  // TODO(crbug.com/526677926): To be implemented in Phase 2/3.
}

- (void)omniboxDidEndEditing {
  // TODO(crbug.com/526677926): To be implemented in Phase 2/3.
}

- (void)restoreScrollPosition:(CGFloat)scrollPosition {
  // TODO(crbug.com/526677926): To be implemented in Phase 2/3.
}

- (void)restoreScrollPositionToTopOfFeed {
  // TODO(crbug.com/526677926): To be implemented in Phase 2/3.
}

- (CGFloat)heightAboveFeed {
  return 0.0;
}

- (CGFloat)scrollPosition {
  return 0.0;
}

- (CGFloat)pinnedOffsetY {
  return 0.0;
}

- (void)setBackgroundImage:(UIImage*)backgroundImage
        framingCoordinates:
            (HomeCustomizationFramingCoordinates*)framingCoordinates {
  _backgroundImage = backgroundImage;
  _framingCoordinates = framingCoordinates;

  __weak HomeCustomizationImageView* view = _backgroundImageView;
  __weak UIImage* image = _backgroundImage;
  __weak HomeCustomizationFramingCoordinates* weakFramingCoordinates =
      _framingCoordinates;
  [UIView transitionWithView:view
                    duration:kBackgroundImageAnimationDuration
                     options:UIViewAnimationOptionTransitionCrossDissolve
                  animations:^{
                    [view setImage:image
                        framingCoordinates:weakFramingCoordinates];
                  }
                  completion:nil];
}

#pragma mark - Setters

- (void)setSearchEngineLogoView:(UIView*)searchEngineLogoView {
  if (_searchEngineLogoView == searchEngineLogoView) {
    return;
  }
  if (_searchEngineLogoView && _searchEngineLogoView.superview) {
    [_searchEngineLogoView removeFromSuperview];
  }
  _searchEngineLogoView = searchEngineLogoView;
  if (_searchEngineLogoView && _bottomSheetViewController) {
    [self addSearchEngineLogoView];
  }
}

- (void)setFeedViewController:(UIViewController*)feedViewController {
  if (_feedViewController == feedViewController) {
    return;
  }
  _feedViewController = feedViewController;
  if (_bottomSheetViewController) {
    _bottomSheetViewController.feedViewController = feedViewController;
  }
}

- (void)setMagicStackViewController:
    (UIViewController*)magicStackViewController {
  if (_magicStackViewController == magicStackViewController) {
    return;
  }
  _magicStackViewController = magicStackViewController;
  if (_bottomSheetViewController) {
    _bottomSheetViewController.magicStackViewController =
        magicStackViewController;
  }
}

- (void)setMostVisitedView:(UIView*)mostVisitedView {
  if (_mostVisitedView == mostVisitedView) {
    return;
  }
  if (_mostVisitedView) {
    [_mostVisitedView removeFromSuperview];
  }
  _mostVisitedView = mostVisitedView;
  if (self.isViewLoaded && _mostVisitedView) {
    [self embedMostVisitedView];
  }
}

#pragma mark - Private

// Add _mostVisitedView to the view hierarchy.
- (void)embedMostVisitedView {
  if (!_mostVisitedView || !_mostVisitedContainerView) {
    return;
  }
  _mostVisitedView.translatesAutoresizingMaskIntoConstraints = NO;
  [_mostVisitedContainerView addSubview:_mostVisitedView];
  AddSameConstraints(_mostVisitedView, _mostVisitedContainerView);
}

- (void)addSearchEngineLogoView {
  if (!_searchEngineLogoView || !_bottomSheetViewController.view) {
    return;
  }
  [self.view insertSubview:_searchEngineLogoView
              belowSubview:_bottomSheetViewController.view];
  _searchEngineLogoView.translatesAutoresizingMaskIntoConstraints = NO;
  [self updateLogoConstraints];
}

- (void)updateLogoConstraints {
  if (!_searchEngineLogoView || !_fakeLocationBar) {
    return;
  }
  if (_logoConstraints) {
    [NSLayoutConstraint deactivateConstraints:_logoConstraints];
  }

  CGFloat height =
      content_suggestions::DoodleHeight(_logoState, self.traitCollection);
  CGFloat width = (_logoState == SearchEngineLogoState::kDoodle)
                      ? kDoodleLogoWidth
                      : kGoogleLogoWidth;

  _logoConstraints = @[
    [_searchEngineLogoView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [_searchEngineLogoView.bottomAnchor
        constraintEqualToAnchor:_fakeLocationBar.topAnchor
                       constant:-kLogoToOmniboxSpacing],
    [_searchEngineLogoView.widthAnchor constraintEqualToConstant:width],
    [_searchEngineLogoView.heightAnchor constraintEqualToConstant:height]
  ];
  [NSLayoutConstraint activateConstraints:_logoConstraints];
}

- (CGFloat)topContentHeight {
  CGFloat height = content_suggestions::FakeOmniboxHeight();

  if (self.quickActionsVisible && _quickActionsViewController) {
    height += kQuickActionSpacingTop;
    height += _quickActionsViewController.preferredContentSize.height;
    height += kQuickActionSpacingBottom;
  } else {
    height += kOmniboxToMVTSpacing;
  }

  CGFloat mvtHeight = CGRectGetHeight(_mostVisitedContainerView.bounds);
  if (mvtHeight <= 0) {
    mvtHeight = kDefaultMVTHeightFallback;
  }
  height += mvtHeight;

  return height;
}
- (CGFloat)centeredFakeOmniboxTop {
  CGFloat screenHeight = self.view.bounds.size.height;
  // During the initial view loading sequence (e.g. before initial layout pass
  // occurs), screen height bounds will be 0. We fallback to the dynamic
  // top-down logo offset to avoid negative constraint values during early
  // configuration.
  if (screenHeight <= 0) {
    CGFloat safeAreaTop = self.view.safeAreaInsets.top;
    CGFloat logoHeight =
        content_suggestions::DoodleHeight(_logoState, self.traitCollection);
    return safeAreaTop + kLogoTopMargin + logoHeight + kLogoToOmniboxSpacing;
  }
  return screenHeight * 0.35;
}

#pragma mark - ContentSuggestionsConsumer

- (void)setMostVisitedTilesConfig:(MostVisitedTilesConfig*)config {
  MagicStackModuleContainer* container =
      [[MagicStackModuleContainer alloc] initWithFrame:CGRectZero noInset:YES];
  [container configureWithConfig:config];

  self.mostVisitedView = container;
}

#pragma mark - SearchEngineLogoConsumer

- (void)searchEngineLogoStateDidChange:(SearchEngineLogoState)logoState {
  _logoState = logoState;
  [self updateLogoConstraints];
  if (_bottomSheetViewController) {
    CGFloat currentTopOffset = _bottomSheetViewController.view.frame.origin.y;
    [self bottomSheetViewController:_bottomSheetViewController
                 didUpdateTopOffset:currentTopOffset];
  }
}

#pragma mark - NewTabPageHeaderViewDelegate

- (BOOL)shouldPinFakeOmnibox {
  return NO;
}

#pragma mark - UserAccountImageUpdateDelegate

- (void)setSignedOutAccountImage {
  _avatarImage = nil;
  _avatarName = nil;
  _avatarEmail = nil;
  _avatarImageLoaded = YES;
  if (_identityDiscButton) {
    [_identityDiscButton setSignedOutAccountImage];
  }
}

- (void)updateAccountWithName:(NSString*)name
                        email:(NSString*)email
                  avatarImage:(UIImage*)avatarImage
                    hasAITier:(BOOL)hasAITier {
  _avatarImage = avatarImage;
  _hasAITier = hasAITier;
  _avatarName = name;
  _avatarEmail = email;
  _avatarImageLoaded = YES;
  if (_identityDiscButton) {
    [_identityDiscButton updateAccountWithName:name
                                         email:email
                                   avatarImage:avatarImage
                                     hasAITier:hasAITier];
  }
}

#pragma mark - Actions

- (void)identityDiscButtonTapped:(UIButton*)sender {
  [self.headerCommandsHandler identityDiscWasTapped:sender];
}

#pragma mark - NewTabPageHeaderConsumer

- (void)setVoiceSearchIsEnabled:(BOOL)voiceSearchIsEnabled {
  if (_voiceSearchIsEnabled == voiceSearchIsEnabled) {
    return;
  }
  _voiceSearchIsEnabled = voiceSearchIsEnabled;
  [self refreshFakeboxContent];
}

- (void)setDefaultSearchEngineName:(NSString*)dseName {
  if ([_defaultSearchEngineName isEqualToString:dseName]) {
    return;
  }
  _defaultSearchEngineName = [dseName copy];
  _isGoogleDefaultSearchEngine =
      [_defaultSearchEngineName isEqualToString:@"Google"];
  [self refreshFakeboxContent];
}

- (void)setDefaultSearchEngineImage:(UIImage*)image {
  _dseLogo = image;
  [self refreshFakeboxContent];
}

// Whether the quick actions button row is visible.
- (BOOL)quickActionsVisible {
  return _isAIMAllowed && IsAimEnabledInNtp();
}

- (void)setAIMAllowed:(BOOL)allowed {
  if (_isAIMAllowed == allowed) {
    return;
  }
  _isAIMAllowed = allowed;
  if (_quickActionsViewController) {
    BOOL isVisible = self.quickActionsVisible;
    _quickActionsViewController.view.hidden = !isVisible;

    _mvtTopConstraint.active = NO;

    UIView* anchorView =
        isVisible ? _quickActionsViewController.view : _fakeLocationBar;
    CGFloat constant =
        isVisible ? kQuickActionSpacingBottom : kOmniboxToMVTSpacing;

    _mvtTopConstraint = [_mostVisitedContainerView.topAnchor
        constraintEqualToAnchor:anchorView.bottomAnchor
                       constant:constant];
    _mvtTopConstraint.active = YES;

    [self.view layoutIfNeeded];
  }
  [self refreshFakeboxContent];
}

- (void)setFuseboxEligible:(BOOL)eligible {
  if (_fuseboxEligible == eligible) {
    return;
  }
  _fuseboxEligible = eligible;
  [self refreshFakeboxContent];
}

- (void)setOmniboxInBottomPosition:(BOOL)isBottomOmnibox {
  // No-op for redesign.
}

- (void)updateADPBadgeWithErrorFound:(BOOL)hasAccountError
                                name:(NSString*)name
                               email:(NSString*)email {
  // No-op for redesign.
}

#pragma mark - FakeboxButtonsSnapshotProvider

- (UIView*)fakeboxButtonsSnapshot {
  return [_buttonStack snapshotViewAfterScreenUpdates:NO];
}



#pragma mark - Private Fakebox Helpers

- (BOOL)shouldShowPlusButton {
  return IsPlusButtonInFakeboxEnabled() && _isAIMAllowed && _fuseboxEligible;
}

- (CGFloat)fakeLocationBarWidth {
  return content_suggestions::SearchFieldWidth(self.view.bounds.size.width,
                                               self.traitCollection);
}

- (CGFloat)fakeboxLeadingSpace {
  if ([self shouldShowPlusButton]) {
    return kFakeboxPlusLeadingSpace;
  }
  return kFakeboxImageLeadingSpace;
}

- (CGFloat)hintLabelFakeboxLeadingSpace {
  if ([self shouldShowPlusButton]) {
    return kHintLabelFakeboxLeadingSpaceWithPlus;
  }
  return kHintLabelFakeboxLeadingSpaceWithIcon;
}

- (CGFloat)endButtonFakeboxTrailingSpace {
  if (self.useNewBadgeForLensButton && !IsAimEnabledInNtp()) {
    return kEndButtonNormalSizeFakeboxWithBadgeTrailingSpace;
  }
  return kEndButtonFakeboxTrailingSpace;
}

- (NSString*)placeholderText {
  NSString* dseName = _defaultSearchEngineName ?: @"Google";
  if (IsAIOmniboxAskPlaceholderEnabled() && _isGoogleDefaultSearchEngine) {
    return l10n_util::GetNSStringF(IDS_OMNIBOX_EMPTY_ASK_HINT_WITH_DSE_NAME,
                                   dseName.cr_UTF16String);
  } else {
    return l10n_util::GetNSStringF(IDS_OMNIBOX_EMPTY_HINT_WITH_DSE_NAME,
                                   dseName.cr_UTF16String);
  }
}

- (void)addVoiceAndLensDivider {
  UIView* divider = [self createDivider];
  _voiceAndLensDivider = divider;
  [_buttonStack addArrangedSubview:divider];
}

- (UIView*)createDivider {
  UIView* divider = [[UIView alloc] init];
  divider.translatesAutoresizingMaskIntoConstraints = NO;
  CGFloat dividerWidth = 1.0 / self.traitCollection.displayScale;

  [NSLayoutConstraint activateConstraints:@[
    [divider.heightAnchor constraintEqualToConstant:kIconDividerHeight],
    [divider.widthAnchor constraintEqualToConstant:dividerWidth],
  ]];

  return divider;
}

- (void)refreshFakeboxContent {
  if (!self.isViewLoaded) {
    return;
  }
  // 1. Remove existing subviews and constraints.
  [_plusButton removeFromSuperview];
  [_logoView removeFromSuperview];
  _plusButton = nil;
  _logoView = nil;
  _leadingView = nil;

  _leadingViewConstraint.active = NO;
  _leadingViewConstraint = nil;
  _hintLabelLeadingConstraint.active = NO;
  _hintLabelLeadingConstraint = nil;
  _hintLabelTrailingConstraint.active = NO;
  _hintLabelTrailingConstraint = nil;

  for (UIView* view in _buttonStack.arrangedSubviews) {
    [view removeFromSuperview];
  }
  _voiceSearchButton = nil;
  _lensButton = nil;
  _voiceAndLensDivider = nil;

  // 2. Set up leading view.
  UIView* leadingView = nil;
  CGFloat leadingViewYOffset = 0;
  if ([self shouldShowPlusButton]) {
    _plusButton = [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
    _plusButton.accessibilityLabel = l10n_util::GetNSString(
        IDS_IOS_COMPOSEBOX_ADD_ATTACHMENT_BUTTON_ACCESSIBILITY_LABEL);
    [_plusButton
        setImage:SymbolWithPointSize(SymbolPlus, kSymbolActionPointSize)
        forState:UIControlStateNormal];
    [_plusButton addTarget:self.NTPShortcutsHandler
                    action:@selector(openMultimodalActionsMenu)
          forControlEvents:UIControlEventTouchUpInside];
    leadingView = _plusButton;
  } else {
    _logoView = [[UIImageView alloc] init];
    _logoView.contentMode = UIViewContentModeScaleAspectFit;
    _logoView.image = _dseLogo;
    leadingView = _logoView;
    leadingViewYOffset = kLogoViewYOffset;
  }

  if (leadingView) {
    leadingView.translatesAutoresizingMaskIntoConstraints = NO;
    [_fakeLocationBar addSubview:leadingView];
    AddSquareConstraints(leadingView, kFakeboxImageSize);
    _leadingView = leadingView;

    _leadingViewConstraint = [leadingView.leadingAnchor
        constraintEqualToAnchor:_fakeLocationBar.leadingAnchor
                       constant:[self fakeboxLeadingSpace]];

    [NSLayoutConstraint activateConstraints:@[
      _leadingViewConstraint,
      [leadingView.centerYAnchor
          constraintEqualToAnchor:_fakeLocationBar.centerYAnchor
                         constant:leadingViewYOffset]
    ]];
  }

  // 3. Set up trailing buttons stack.
  _buttonStack.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(
      0, 0, 0, [self endButtonFakeboxTrailingSpace]);

  // Voice Search Button.
  _voiceSearchButton =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  _voiceSearchButton.enabled = _voiceSearchIsEnabled;
  _voiceSearchButton.isAccessibilityElement = _voiceSearchIsEnabled;
  [_voiceSearchButton addTarget:self
                         action:@selector(loadVoiceSearch:)
               forControlEvents:UIControlEventTouchUpInside];
  [_voiceSearchButton addTarget:self
                         action:@selector(preloadVoiceSearch:)
               forControlEvents:UIControlEventTouchDown];
  [_buttonStack addArrangedSubview:_voiceSearchButton];

  // Lens Button.
  const BOOL useLens =
      lens_availability::CheckAndLogAvailabilityForLensEntryPoint(
          LensEntrypoint::NewTabPage, _isGoogleDefaultSearchEngine);
  if (useLens) {
    [self addVoiceAndLensDivider];
    _lensButton = [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
    [_lensButton addTarget:self
                    action:@selector(openLensViewFinder)
          forControlEvents:UIControlEventTouchUpInside];
    if (self.useNewBadgeForLensButton) {
      [_lensButton addTarget:self
                      action:@selector(lensButtonWithNewBadgeTapped:)
            forControlEvents:UIControlEventTouchUpInside];
    }
    [_buttonStack addArrangedSubview:_lensButton];
  }

  [self updateButtonsForCurrentTraitCollection];

  // 4. Set placeholder text and hint label.
  NSString* placeholder = [self placeholderText];
  _hintLabel.text = placeholder;
  _fakeLocationBar.accessibilityLabel = placeholder;

  // 5. Update hint label constraints.
  _hintLabelLeadingConstraint = [_hintLabel.leadingAnchor
      constraintEqualToAnchor:_fakeLocationBar.leadingAnchor
                     constant:[self hintLabelFakeboxLeadingSpace]];
  _hintLabelLeadingConstraint.active = YES;

  UIView* referenceView = _buttonStack.arrangedSubviews.firstObject;
  NSLayoutXAxisAnchor* trailingAnchor = referenceView ? referenceView.leadingAnchor
                                                      : _fakeLocationBar.trailingAnchor;

  _hintLabelTrailingConstraint = [_hintLabel.trailingAnchor
      constraintLessThanOrEqualToAnchor:trailingAnchor
                               constant:-kHintLabelFakeboxTrailingSpace];
  _hintLabelTrailingConstraint.priority = UILayoutPriorityDefaultHigh;
  _hintLabelTrailingConstraint.active = YES;
}

- (void)updateButtonsForCurrentTraitCollection {
  const BOOL forceDisableColors = IsAimEnabledInNtp();
  const BOOL darkUIStyle =
      self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark;
  const BOOL ntpHasCustomBackground =
      [self.traitCollection boolForNewTabPageImageBackgroundTrait] ||
      [self.traitCollection objectForNewTabPageTrait];
  const BOOL useColorIcon =
      !darkUIStyle && !forceDisableColors && !ntpHasCustomBackground;

  content_suggestions::ConfigureVoiceSearchButton(_voiceSearchButton,
                                                  useColorIcon);
  if (_lensButton) {
    UIColor* newBadgeColor =
        [self.traitCollection boolForNewTabPageImageBackgroundTrait]
            ? nil
            : [self.traitCollection objectForNewTabPageTrait].tintColor;
    content_suggestions::ConfigureLensButtonAppearance(
        _lensButton, self.useNewBadgeForLensButton, useColorIcon,
        newBadgeColor);
    if (self.useNewBadgeForLensButton) {
      content_suggestions::ConfigureLensButtonWithNewBadgeAlpha(
          _lensButton, _lensButtonWithNewBadgeTapped ? 0 : 1);
    }
  }
}

- (void)loadVoiceSearch:(id)sender {
  [self.NTPShortcutsHandler preloadVoiceSearch];
  [self.NTPShortcutsHandler loadVoiceSearchFromView:_voiceSearchButton];
}

- (void)preloadVoiceSearch:(id)sender {
  [self.NTPShortcutsHandler preloadVoiceSearch];
}

- (void)openLensViewFinder {
  [self.NTPShortcutsHandler openLensViewFinder];
}

- (void)lensButtonWithNewBadgeTapped:(id)sender {
  if (!_lensButtonWithNewBadgeTapped) {
    _lensButtonWithNewBadgeTapped = YES;
    ExtendedTouchTargetButton* lensButton = _lensButton;
    [UIView
        animateWithDuration:kMaterialDuration1
                 animations:^{
                   content_suggestions::ConfigureLensButtonWithNewBadgeAlpha(
                       lensButton, 0);
                 }];
  }
}

@end
