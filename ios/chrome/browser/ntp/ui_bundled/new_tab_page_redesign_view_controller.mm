// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/526677926): Clean up or delete file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_redesign_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/content_suggestions/model/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_item.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_collection_view.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_config.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_framing_coordinates.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_image_view.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_availability.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_state.h"
#import "ios/chrome/browser/ntp/ui_bundled/fake_location_bar_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_commands.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_mutator.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_quick_actions_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_shortcuts_handler.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_utils.h"
#import "ios/chrome/browser/ntp/ui_bundled/ntp_identity_disc_button.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_constants.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Animation duration for wallpaper transition.
constexpr CGFloat kBackgroundImageAnimationDuration = 0.25;

// Spacing from the top of the bottom sheet to the MVTs container when
// resting/collapsed.
constexpr CGFloat kRestingSheetMVTTopMargin = 12.0;

constexpr CGFloat kLandscapeLogoTopMargin = 8.0;

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

const CGFloat kMinDragHandleHeight = 24.0;
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
  UIView* _mostVisitedView;
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
  ExtendedTouchTargetButton* _plusButton;
  UIImageView* _logoView;
  ExtendedTouchTargetButton* _voiceSearchButton;
  UIView* _voiceAndLensDivider;
  ExtendedTouchTargetButton* _lensButton;
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
  NSLayoutConstraint* _hintLabelLeadingConstraint;
  NSLayoutConstraint* _dividerWidthConstraint;
  BOOL _isBottomOmnibox;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kNTPRedesignBackgroundColor];

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

  // Add fake location bar.
  _fakeLocationBar = [[FakeLocationBarView alloc] init];
  _fakeLocationBar.translatesAutoresizingMaskIntoConstraints = NO;
  [_fakeLocationBar addTarget:self
                       action:@selector(fakeLocationBarTapped)
             forControlEvents:UIControlEventTouchUpInside];
  _fakeLocationBar.isAccessibilityElement = YES;
  _fakeLocationBar.accessibilityIdentifier = @"ntp-redesign-fake-omnibox";
  [self.view insertSubview:_fakeLocationBar
              belowSubview:_bottomSheetViewController.view];

  _plusButton = [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  _plusButton.translatesAutoresizingMaskIntoConstraints = NO;
  _plusButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_COMPOSEBOX_ADD_ATTACHMENT_BUTTON_ACCESSIBILITY_LABEL);
  [_plusButton setImage:SymbolWithPointSize(SymbolPlus, kSymbolActionPointSize)
               forState:UIControlStateNormal];
  [_plusButton addTarget:self
                  action:@selector(openMultimodalActionsMenu:)
        forControlEvents:UIControlEventTouchUpInside];
  [_fakeLocationBar addSubview:_plusButton];
  AddSquareConstraints(_plusButton, kFakeboxImageSize);

  _logoView = [[UIImageView alloc] init];
  _logoView.translatesAutoresizingMaskIntoConstraints = NO;
  _logoView.contentMode = UIViewContentModeScaleAspectFit;
  [_fakeLocationBar addSubview:_logoView];
  AddSquareConstraints(_logoView, kFakeboxImageSize);

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

  _buttonStack = [[NTPRedesignTouchAreaOverflowStackView alloc] init];
  _buttonStack.translatesAutoresizingMaskIntoConstraints = NO;
  _buttonStack.alignment = UIStackViewAlignmentCenter;
  _buttonStack.spacing = kButtonSpacing;
  _buttonStack.layoutMarginsRelativeArrangement = YES;
  [_fakeLocationBar addSubview:_buttonStack];

  _voiceSearchButton =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  _voiceSearchButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_voiceSearchButton addTarget:self
                         action:@selector(loadVoiceSearch:)
               forControlEvents:UIControlEventTouchUpInside];
  [_voiceSearchButton addTarget:self
                         action:@selector(preloadVoiceSearch:)
               forControlEvents:UIControlEventTouchDown];
  [_buttonStack addArrangedSubview:_voiceSearchButton];

  _voiceAndLensDivider = [self createDivider];
  [_buttonStack addArrangedSubview:_voiceAndLensDivider];

  _lensButton = [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  _lensButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_lensButton addTarget:self
                  action:@selector(openLensViewFinder)
        forControlEvents:UIControlEventTouchUpInside];
  [_lensButton addTarget:self
                  action:@selector(lensButtonWithNewBadgeTapped:)
        forControlEvents:UIControlEventTouchUpInside];
  [_buttonStack addArrangedSubview:_lensButton];

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

  // Add Most Visited Tiles (MVTs) container if not in bottom sheet.
  if (!IsMVTInBottomSheetEnabled()) {
    _mostVisitedContainerView = [[UIView alloc] init];
    _mostVisitedContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    // Insert BELOW the sheet.
    [self.view insertSubview:_mostVisitedContainerView
                belowSubview:_bottomSheetViewController.view];
  }

  // Configure layout constraints
  _fakeLocationBarTopConstraint = [_fakeLocationBar.topAnchor
      constraintEqualToAnchor:self.view.topAnchor
                     constant:[self centeredFakeOmniboxTop]];
  _fakeLocationBarWidthConstraint = [_fakeLocationBar.widthAnchor
      constraintEqualToConstant:[self fakeLocationBarWidth]];
  _fakeLocationBarHeightConstraint = [_fakeLocationBar.heightAnchor
      constraintEqualToConstant:content_suggestions::FakeOmniboxHeight()];

  _hintLabelLeadingConstraint = [_hintLabel.leadingAnchor
      constraintEqualToAnchor:_fakeLocationBar.leadingAnchor
                     constant:[self hintLabelFakeboxLeadingSpace]];

  NSLayoutConstraint* hintLabelTrailingConstraint = [_hintLabel.trailingAnchor
      constraintLessThanOrEqualToAnchor:_buttonStack.leadingAnchor
                               constant:-kHintLabelFakeboxTrailingSpace];
  hintLabelTrailingConstraint.priority = UILayoutPriorityDefaultHigh;

  [NSLayoutConstraint activateConstraints:@[
    _fakeLocationBarTopConstraint,
    [_fakeLocationBar.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    _fakeLocationBarWidthConstraint,
    _fakeLocationBarHeightConstraint,
    [_plusButton.leadingAnchor
        constraintEqualToAnchor:_fakeLocationBar.leadingAnchor
                       constant:kFakeboxPlusLeadingSpace],
    [_plusButton.centerYAnchor
        constraintEqualToAnchor:_fakeLocationBar.centerYAnchor],
    [_logoView.leadingAnchor
        constraintEqualToAnchor:_fakeLocationBar.leadingAnchor
                       constant:kFakeboxImageLeadingSpace],
    [_logoView.centerYAnchor
        constraintEqualToAnchor:_fakeLocationBar.centerYAnchor
                       constant:kLogoViewYOffset],
    _hintLabelLeadingConstraint,
    [_hintLabel.centerYAnchor
        constraintEqualToAnchor:_fakeLocationBar.centerYAnchor
                       constant:kHintLabelYOffset],
    hintLabelTrailingConstraint,
    [_buttonStack.trailingAnchor
        constraintEqualToAnchor:_fakeLocationBar.trailingAnchor],
    [_buttonStack.centerYAnchor
        constraintEqualToAnchor:_fakeLocationBar.centerYAnchor],
  ]];

  if (IsAimEnabledInNtp()) {
    _qaTopConstraint = [_quickActionsViewController.view.topAnchor
        constraintEqualToAnchor:_fakeLocationBar.bottomAnchor
                       constant:content_suggestions::QuickActionsTopPadding()];

    [NSLayoutConstraint activateConstraints:@[
      _qaTopConstraint,
      [_quickActionsViewController.view.widthAnchor
          constraintEqualToAnchor:_fakeLocationBar.widthAnchor],
      [_quickActionsViewController.view.centerXAnchor
          constraintEqualToAnchor:_fakeLocationBar.centerXAnchor],
    ]];
  }

  if (!IsMVTInBottomSheetEnabled()) {
    [NSLayoutConstraint activateConstraints:@[
      [_mostVisitedContainerView.widthAnchor
          constraintEqualToAnchor:_fakeLocationBar.widthAnchor],
      [_mostVisitedContainerView.centerXAnchor
          constraintEqualToAnchor:_fakeLocationBar.centerXAnchor],
    ]];

    UIView* anchorView = self.quickActionsVisible
                             ? _quickActionsViewController.view
                             : _fakeLocationBar;
    CGFloat constant = content_suggestions::MostVisitedTopPadding();

    _mvtTopConstraint = [_mostVisitedContainerView.topAnchor
        constraintEqualToAnchor:anchorView.bottomAnchor
                       constant:constant];
    _mvtTopConstraint.active = YES;
  }

  _fakeLocationBar.layer.cornerRadius =
      _fakeLocationBarHeightConstraint.constant / 2.0;

  [self updateLeadingView];
  [self updateActionButtons];
  [self updateHintLabel];

  if (_mostVisitedView) {
    if (IsMVTInBottomSheetEnabled()) {
      [_bottomSheetViewController embedMostVisitedView:_mostVisitedView];
    } else {
      [self embedMostVisitedView];
    }
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
  [self registerForTraitChanges:@[
    UITraitHorizontalSizeClass.class, UITraitVerticalSizeClass.class,
    UITraitPreferredContentSizeCategory.class, UITraitUserInterfaceStyle.class
  ]
                     withAction:@selector(handleTraitChanges)];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  _fakeLocationBarWidthConstraint.constant = [self fakeLocationBarWidth];
  _fakeLocationBarHeightConstraint.constant =
      content_suggestions::FakeOmniboxHeight();
  _fakeLocationBar.layer.cornerRadius =
      _fakeLocationBarHeightConstraint.constant / 2.0;
}

#pragma mark - UIViewController Overrides

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  self.viewDidAppear = YES;

  if (self.focusAccessibilityOmniboxWhenViewAppears) {
    UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                    _fakeLocationBar);
    self.focusAccessibilityOmniboxWhenViewAppears = NO;
  }

  [self maybeNotifyLensBadgeDisplayed];
}

- (void)maybeNotifyLensBadgeDisplayed {
  if (self.viewDidAppear && _lensButton && !_lensButton.hidden &&
      self.useNewBadgeForLensButton && !_didNotifyLensBadgeDisplay) {
    [self.mutator notifyLensBadgeDisplayed];
    _didNotifyLensBadgeDisplay = YES;
  }
}

- (void)handleTraitChanges {
  [self updateLogoConstraints];
  _fakeLocationBarTopConstraint.constant = [self centeredFakeOmniboxTop];
  if (_dividerWidthConstraint) {
    _dividerWidthConstraint.constant = 1.0 / self.traitCollection.displayScale;
  }
  [self updateButtonsForCurrentTraitCollection];
  [self updateLeadingView];
  if (_bottomSheetViewController) {
    [_bottomSheetViewController updateBottomSheetPositionAnimated:NO];
  }
}

- (void)detachChildViewController:(UIViewController*)child {
  if (!child || child.parentViewController != self) {
    return;
  }
  [child willMoveToParentViewController:nil];
  [child.view removeFromSuperview];
  [child removeFromParentViewController];
}

- (void)containChildViewController:(UIViewController*)child
                      insideParent:(UIViewController*)parent
                     containerView:(UIView*)containerView {
  if (!child || !parent || !containerView) {
    return;
  }
  if (child.parentViewController == parent &&
      child.view.superview == containerView) {
    return;
  }
  if (child.parentViewController) {
    [child willMoveToParentViewController:nil];
    [child.view removeFromSuperview];
    [child removeFromParentViewController];
  }
  [parent addChildViewController:child];
  child.view.translatesAutoresizingMaskIntoConstraints = NO;
  [containerView addSubview:child.view];
  AddSameConstraints(child.view, containerView);
  [child didMoveToParentViewController:parent];
}

- (void)setUseNewBadgeForLensButton:(BOOL)useNewBadgeForLensButton {
  if (_useNewBadgeForLensButton == useNewBadgeForLensButton) {
    return;
  }
  _useNewBadgeForLensButton = useNewBadgeForLensButton;
  if (self.isViewLoaded) {
    [self updateActionButtons];
  }
}

- (void)invalidate {
  self.mutator = nil;
  self.searchEngineLogoView = nil;
  self.NTPContentDelegate = nil;
  self.NTPShortcutsHandler = nil;
  _mostVisitedView = nil;
  self.magicStackViewController = nil;
  [self setFeedViewController:nil];
  if (_quickActionsViewController) {
    [self detachChildViewController:_quickActionsViewController];
    _quickActionsViewController = nil;
  }
  if (_bottomSheetViewController) {
    [_bottomSheetViewController invalidate];
    [self detachChildViewController:_bottomSheetViewController];
    _bottomSheetViewController = nil;
  }
  _identityDiscButton = nil;
  _avatarImage = nil;
  _avatarName = nil;
  _avatarEmail = nil;
  _plusButton = nil;
  _logoView = nil;
  _voiceSearchButton = nil;
  _lensButton = nil;
  _voiceAndLensDivider = nil;
  _hintLabel = nil;
  _buttonStack = nil;
  _fakeLocationBar = nil;
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
  CGFloat offset = [self centeredFakeOmniboxTop] + [self topContentHeight];
  if (!IsMVTInBottomSheetEnabled()) {
    offset += kRestingSheetMVTTopMargin;
  }

  // Safety guard: guarantee the drag handle is always visible and reachable
  CGFloat screenHeight = self.view.bounds.size.height;
  CGFloat safeAreaBottom = self.view.safeAreaInsets.bottom;
  CGFloat maxAllowedOffset =
      screenHeight - safeAreaBottom - kMinDragHandleHeight;

  return MIN(offset, maxAllowedOffset);
}

- (CGFloat)collapsedOffsetForBottomSheetViewController:
    (NewTabPageBottomSheetViewController*)viewController {
  UIView* superview = self.view;
  CGFloat safeAreaBottom = superview.safeAreaInsets.bottom;
  if ([self isCompactHeight]) {
    return superview.bounds.size.height - safeAreaBottom - kMinDragHandleHeight;
  }
  CGFloat collapsedHeight = safeAreaBottom + 80.0;
  return superview.bounds.size.height - collapsedHeight;
}

- (CGFloat)expandedOffsetForBottomSheetViewController:
    (NewTabPageBottomSheetViewController*)viewController {
  CGFloat safeAreaTop = self.view.safeAreaInsets.top;
  if (_isBottomOmnibox && !CanShowTabStrip(self)) {
    return safeAreaTop;
  }
  return safeAreaTop + kToolbarHeight;
}

- (void)bottomSheetViewController:
            (NewTabPageBottomSheetViewController*)bottomSheetViewController
               didUpdateTopOffset:(CGFloat)topOffset {
  CGFloat expandedOffset = [_bottomSheetViewController expandedOffset];
  CGFloat restingOffset = [_bottomSheetViewController restingOffset];

  CGFloat progress = 1.0;
  if (restingOffset > expandedOffset) {
    progress = (topOffset - expandedOffset) / (restingOffset - expandedOffset);
    progress = MIN(1.0, MAX(0.0, progress));
  }

  if (topOffset > restingOffset) {
    // Collapsed range: Move top content down with sheet
    CGFloat downwardDelta = topOffset - restingOffset;
    _fakeLocationBarTopConstraint.constant =
        [self centeredFakeOmniboxTop] + downwardDelta;
    _fakeLocationBar.alpha = 1.0;
    [self.NTPContentDelegate didUpdateNTPTabOmniboxScrollProgress:0.0];
  } else {
    // Expanded range: Fakebox stays static at centered position & fades out
    _fakeLocationBarTopConstraint.constant = [self centeredFakeOmniboxTop];
    _fakeLocationBar.alpha = progress;
    CGFloat expansionProgress = 1.0 - progress;
    [self.NTPContentDelegate
        didUpdateNTPTabOmniboxScrollProgress:expansionProgress];
  }

  // Opacity for Logo, MVT, Identity Disc, and Quick Actions
  _searchEngineLogoView.alpha = progress;
  if (!IsMVTInBottomSheetEnabled()) {
    _mostVisitedContainerView.alpha = progress;
  }
  _identityDiscButton.alpha = progress;
  if (_quickActionsViewController) {
    _quickActionsViewController.view.alpha = progress;
  }

  [self.view layoutIfNeeded];
}

- (void)bottomSheetViewControllerDidEscape:
    (NewTabPageBottomSheetViewController*)bottomSheetViewController {
  if (_fakeLocationBar) {
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                    _fakeLocationBar);
  }
}

#pragma mark - ContentSuggestionsConsumer

- (void)setMostVisitedTilesConfig:(MostVisitedTilesConfig*)config {
  if (_mostVisitedView) {
    [_mostVisitedView removeFromSuperview];
  }
  if (!config) {
    _mostVisitedView = nil;
    return;
  }

  MostVisitedTilesCollectionView* collectionView =
      [[MostVisitedTilesCollectionView alloc] initWithConfig:config];

  if (!IsMVTInBottomSheetEnabled()) {
    __weak __typeof(_bottomSheetViewController) weakBottomSheetViewController =
        _bottomSheetViewController;
    collectionView.onContentSizeChanged = ^(CGSize) {
      [weakBottomSheetViewController updateBottomSheetPositionAnimated:YES];
    };
  }

  _mostVisitedView = CreateMostVisitedContainerView(collectionView, YES);

  if (IsMVTInBottomSheetEnabled()) {
    if (_bottomSheetViewController) {
      [_bottomSheetViewController embedMostVisitedView:_mostVisitedView];
    }
  } else {
    if (self.isViewLoaded) {
      [self embedMostVisitedView];
    }
  }

  for (MostVisitedItem* item in config.mostVisitedItems) {
    [ContentSuggestionsMetricsRecorder recordMostVisitedTileShown:item
                                                          atIndex:item.index];
  }
  [ContentSuggestionsMetricsRecorder recordMostVisitedTilesShown];
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

#pragma mark - Private

// Add _mostVisitedView to the view hierarchy.
- (void)embedMostVisitedView {
  if (IsMVTInBottomSheetEnabled()) {
    return;
  }
  if (!_mostVisitedView || !_mostVisitedContainerView) {
    return;
  }
  _mostVisitedView.translatesAutoresizingMaskIntoConstraints = NO;
  [_mostVisitedContainerView addSubview:_mostVisitedView];
  AddSameConstraints(_mostVisitedView, _mostVisitedContainerView);
  [self.view setNeedsLayout];
  [self.view layoutIfNeeded];
  if (_bottomSheetViewController) {
    [_bottomSheetViewController updateBottomSheetPositionAnimated:NO];
  }
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
                       constant:-content_suggestions::LogoToFakeboxPadding(
                                    _logoState)],
    [_searchEngineLogoView.widthAnchor constraintEqualToConstant:width],
    [_searchEngineLogoView.heightAnchor constraintEqualToConstant:height]
  ];
  [NSLayoutConstraint activateConstraints:_logoConstraints];
}

- (CGFloat)topContentHeight {
  CGFloat height = content_suggestions::FakeOmniboxHeight();

  if (self.quickActionsVisible && _quickActionsViewController) {
    height += content_suggestions::QuickActionsTopPadding();
    height += _quickActionsViewController.preferredContentSize.height;
    height += content_suggestions::MostVisitedTopPadding();
  } else {
    height += content_suggestions::MostVisitedTopPadding();
  }

  if (!IsMVTInBottomSheetEnabled()) {
    height +=
        MostVisitedContainerHeight(_mostVisitedContainerView, _mostVisitedView);
  }

  return height;
}
- (BOOL)isCompactHeight {
  return self.traitCollection.verticalSizeClass ==
         UIUserInterfaceSizeClassCompact;
}

- (CGFloat)logoTopPaddingForCurrentOrientation {
  if ([self isCompactHeight]) {
    return kLandscapeLogoTopMargin;
  }
  return content_suggestions::LogoTopPadding(_logoState, self.traitCollection);
}

- (CGFloat)centeredFakeOmniboxTop {
  CGFloat safeAreaTop = self.view.safeAreaInsets.top;
  CGFloat logoHeight =
      content_suggestions::DoodleHeight(_logoState, self.traitCollection);
  CGFloat logoTopMargin = [self logoTopPaddingForCurrentOrientation];
  return safeAreaTop + logoTopMargin + logoHeight +
         content_suggestions::LogoToFakeboxPadding(_logoState);
}



#pragma mark - SearchEngineLogoConsumer

- (void)searchEngineLogoStateDidChange:(SearchEngineLogoState)logoState {
  _logoState = logoState;
  [self updateLogoConstraints];
  _fakeLocationBarTopConstraint.constant = [self centeredFakeOmniboxTop];
  if (_bottomSheetViewController) {
    [_bottomSheetViewController
        updateBottomSheetPositionAnimated:self.viewDidAppear];
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
  [self updateActionButtons];
}

- (void)setDefaultSearchEngineName:(NSString*)dseName {
  if ([_defaultSearchEngineName isEqualToString:dseName]) {
    return;
  }
  _defaultSearchEngineName = [dseName copy];
  _isGoogleDefaultSearchEngine =
      [_defaultSearchEngineName isEqualToString:@"Google"];
  [self updateHintLabel];
  [self updateActionButtons];
}

- (void)setDefaultSearchEngineImage:(UIImage*)image {
  _dseLogo = image;
  [self updateLeadingView];
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

    if (!IsMVTInBottomSheetEnabled()) {
      _mvtTopConstraint.active = NO;

      UIView* anchorView =
          isVisible ? _quickActionsViewController.view : _fakeLocationBar;
      CGFloat constant = content_suggestions::MostVisitedTopPadding();

      _mvtTopConstraint = [_mostVisitedContainerView.topAnchor
          constraintEqualToAnchor:anchorView.bottomAnchor
                         constant:constant];
      _mvtTopConstraint.active = YES;
    }

    [self.view layoutIfNeeded];
  }
  [self updateLeadingView];
  [self updateActionButtons];
}

- (void)setFuseboxEligible:(BOOL)eligible {
  if (_fuseboxEligible == eligible) {
    return;
  }
  _fuseboxEligible = eligible;
  [self updateLeadingView];
  [self updateActionButtons];
}

- (void)setOmniboxInBottomPosition:(BOOL)isBottomOmnibox {
  if (_isBottomOmnibox == isBottomOmnibox) {
    return;
  }
  _isBottomOmnibox = isBottomOmnibox;
  [_bottomSheetViewController setOmniboxInBottomPosition:isBottomOmnibox];
  if (self.isViewLoaded) {
    [_bottomSheetViewController updateBottomSheetPositionAnimated:YES];
    [self.view setNeedsLayout];
  }
}

- (void)setFeedBottomInset:(CGFloat)bottomInset {
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

- (UIView*)createDivider {
  UIView* divider = [[UIView alloc] init];
  divider.translatesAutoresizingMaskIntoConstraints = NO;
  divider.backgroundColor = [UIColor colorNamed:kToolbarButtonColor];
  CGFloat dividerWidth = 1.0 / self.traitCollection.displayScale;
  _dividerWidthConstraint =
      [divider.widthAnchor constraintEqualToConstant:dividerWidth];

  [NSLayoutConstraint activateConstraints:@[
    [divider.heightAnchor constraintEqualToConstant:kIconDividerHeight],
    _dividerWidthConstraint,
  ]];

  return divider;
}

- (void)updateLeadingView {
  if (!self.isViewLoaded) {
    return;
  }
  const BOOL shouldShowPlus = [self shouldShowPlusButton];
  _plusButton.hidden = !shouldShowPlus;
  _logoView.hidden = shouldShowPlus;
  _logoView.image = _dseLogo;
  _hintLabelLeadingConstraint.constant = [self hintLabelFakeboxLeadingSpace];
}

- (void)updateActionButtons {
  if (!self.isViewLoaded) {
    return;
  }
  _voiceSearchButton.enabled = _voiceSearchIsEnabled;
  _voiceSearchButton.isAccessibilityElement = _voiceSearchIsEnabled;

  const BOOL useLens =
      lens_availability::CheckAndLogAvailabilityForLensEntryPoint(
          LensEntrypoint::NewTabPage, _isGoogleDefaultSearchEngine);
  _lensButton.hidden = !useLens;
  _voiceAndLensDivider.hidden = !useLens;

  _buttonStack.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(
      0, 0, 0, [self endButtonFakeboxTrailingSpace]);

  [self updateButtonsForCurrentTraitCollection];
  [self maybeNotifyLensBadgeDisplayed];
}

- (void)updateHintLabel {
  if (!self.isViewLoaded) {
    return;
  }
  NSString* placeholder = [self placeholderText];
  _hintLabel.text = placeholder;
  _fakeLocationBar.accessibilityLabel = placeholder;
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

- (void)openMultimodalActionsMenu:(id)sender {
  [self.NTPShortcutsHandler openMultimodalActionsMenu];
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
  if (self.useNewBadgeForLensButton && !_lensButtonWithNewBadgeTapped) {
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
