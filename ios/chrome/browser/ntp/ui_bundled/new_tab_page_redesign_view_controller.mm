// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/526677926): Clean up or delete file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_redesign_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_framing_coordinates.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_image_view.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_state.h"
#import "ios/chrome/browser/ntp/ui_bundled/fake_location_bar_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_commands.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_mutator.h"
#import "ios/chrome/browser/ntp/ui_bundled/ntp_identity_disc_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Animation duration for wallpaper transition.
constexpr CGFloat kBackgroundImageAnimationDuration = 0.25;

// Height of the fake omnibox / location bar.
constexpr CGFloat kFakeLocationBarHeight = 56.0;

// Spacing between fake omnibox and most visited tiles (MVTs) container.
constexpr CGFloat kOmniboxToMVTSpacing = 16.0;

// Spacing between the Google logo and the fake location bar.
constexpr CGFloat kLogoToOmniboxSpacing = 24.0;

// Margin for elements on the leading/trailing edges of the screen.
constexpr CGFloat kHorizontalMargin = 16.0;

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
}  // namespace

@interface NewTabPageRedesignViewController () <
    NewTabPageBottomSheetViewControllerDelegate,
    // Delegate for the feed scroll view, forwarding scrolling events to the
    // bottom sheet.
    UIScrollViewDelegate>

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
  NSLayoutConstraint* _fakeLocationBarTopConstraint;
  NTPIdentityDiscButton* _identityDiscButton;

  UIImage* _avatarImage;
  NSString* _avatarName;
  NSString* _avatarEmail;
  BOOL _avatarImageLoaded;
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
  [self addChildViewController:_bottomSheetViewController];
  [self.view addSubview:_bottomSheetViewController.view];
  [_bottomSheetViewController didMoveToParentViewController:self];

  // Add fake location bar.
  _fakeLocationBar = [[FakeLocationBarView alloc] init];
  _fakeLocationBar.translatesAutoresizingMaskIntoConstraints = NO;
  [_fakeLocationBar addTarget:self
                       action:@selector(fakeLocationBarTapped)
             forControlEvents:UIControlEventTouchUpInside];
  _fakeLocationBar.isAccessibilityElement = YES;
  _fakeLocationBar.accessibilityIdentifier = @"ntp-redesign-fake-omnibox";
  [self.view addSubview:_fakeLocationBar];

  // Add search icon and placeholder text inside the fake location bar.
  UIImage* searchIconImage =
      DefaultSymbolTemplateWithPointSize(kMagnifyingglassSymbol, 18);
  UIImageView* searchIcon = [[UIImageView alloc] initWithImage:searchIconImage];
  searchIcon.translatesAutoresizingMaskIntoConstraints = NO;
  searchIcon.tintColor = [UIColor colorNamed:kTextfieldPlaceholderColor];
  [_fakeLocationBar addSubview:searchIcon];

  UILabel* hintLabel = [[UILabel alloc] init];
  hintLabel.translatesAutoresizingMaskIntoConstraints = NO;
  hintLabel.textColor = [UIColor colorNamed:kTextfieldPlaceholderColor];
  hintLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  [_fakeLocationBar addSubview:hintLabel];

  [NSLayoutConstraint activateConstraints:@[
    [searchIcon.leadingAnchor
        constraintEqualToAnchor:_fakeLocationBar.leadingAnchor
                       constant:16],
    [searchIcon.centerYAnchor
        constraintEqualToAnchor:_fakeLocationBar.centerYAnchor],
    [searchIcon.widthAnchor constraintEqualToConstant:18],
    [searchIcon.heightAnchor constraintEqualToConstant:18],

    [hintLabel.leadingAnchor constraintEqualToAnchor:searchIcon.trailingAnchor
                                            constant:8],
    [hintLabel.trailingAnchor
        constraintEqualToAnchor:_fakeLocationBar.trailingAnchor
                       constant:-16],
    [hintLabel.centerYAnchor
        constraintEqualToAnchor:_fakeLocationBar.centerYAnchor],
  ]];

  // Set initial accessibility label for fake location bar.
  NSString* askGoogleString = l10n_util::GetNSStringF(
      IDS_OMNIBOX_EMPTY_ASK_HINT_WITH_DSE_NAME, std::u16string(u"Google"));
  _fakeLocationBar.accessibilityLabel = askGoogleString;
  hintLabel.text = askGoogleString;

  [_fakeLocationBar applyBackgroundTheme];
  [_fakeLocationBar updateColorsWithProgress:0.0 colorPalette:nil];

  // Add Most Visited Tiles (MVTs) container.
  _mostVisitedContainerView = [[UIView alloc] init];
  _mostVisitedContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view insertSubview:_mostVisitedContainerView
              belowSubview:_bottomSheetViewController.view];

  // Configure layout constraints for fake location bar and MVTs.
  _fakeLocationBarTopConstraint = [_fakeLocationBar.topAnchor
      constraintEqualToAnchor:self.view.topAnchor
                     constant:[self centeredFakeOmniboxTop]];

  [NSLayoutConstraint activateConstraints:@[
    _fakeLocationBarTopConstraint,
    [_fakeLocationBar.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kHorizontalMargin],
    [_fakeLocationBar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor
                       constant:-kHorizontalMargin],
    [_fakeLocationBar.heightAnchor
        constraintEqualToConstant:kFakeLocationBarHeight],

    [_mostVisitedContainerView.topAnchor
        constraintEqualToAnchor:_fakeLocationBar.bottomAnchor
                       constant:kOmniboxToMVTSpacing],
    [_mostVisitedContainerView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [_mostVisitedContainerView.widthAnchor
        constraintEqualToAnchor:_fakeLocationBar.widthAnchor],
  ]];
  _fakeLocationBar.layer.cornerRadius = kFakeLocationBarHeight / 2.0;

  if (_mostVisitedViewController) {
    [self embedMostVisitedViewController];
  }

  if (_searchEngineLogoView) {
    [self addSearchEngineLogoView];
  }

  __weak __typeof(self) weakSelf = self;
  [self
      registerForTraitChanges:
          @[ UITraitHorizontalSizeClass.class, UITraitVerticalSizeClass.class ]
                  withHandler:^(id<UITraitEnvironment> traitEnvironment,
                                UITraitCollection* previousCollection) {
                    [weakSelf updateLogoConstraints];
                  }];

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
      [_identityDiscButton updateAccountImage:_avatarImage
                                         name:_avatarName
                                        email:_avatarEmail];
    } else {
      [_identityDiscButton setSignedOutAccountImage];
    }
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  // Update fake omnibox top if the bottom sheet is not pushing it
  if (_bottomSheetViewController) {
    CGFloat currentTopOffset = _bottomSheetViewController.view.frame.origin.y;
    [self bottomSheetViewController:_bottomSheetViewController
                 didUpdateTopOffset:currentTopOffset];
  }
}

- (void)invalidate {
  self.mutator = nil;
  self.searchEngineLogoView = nil;
  self.NTPContentDelegate = nil;
  self.mostVisitedViewController = nil;
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

  // Read height dynamically from bounds, falling back to
  // kDefaultMVTHeightFallback if not laid out yet.
  CGFloat mvtHeight = CGRectGetHeight(_mostVisitedContainerView.bounds);
  if (mvtHeight <= 0) {
    mvtHeight = kDefaultMVTHeightFallback;
  }
  CGFloat restingOffsetFromSheet =
      -(kFakeLocationBarHeight + kOmniboxToMVTSpacing + mvtHeight +
        kRestingSheetMVTTopMargin);

  // Spacing offset from sheet top: restingOffsetFromSheet when
  // resting/collapsed (above sheet), +16 pt when expanded (inside sheet)
  CGFloat offsetFromSheet = progress * restingOffsetFromSheet +
                            (1.0 - progress) * kExpandedSheetOmniboxTopMargin;
  _fakeLocationBarTopConstraint.constant = topOffset + offsetFromSheet;

  // Interpolate opacity for logo, MVTs row, and identity disc
  _searchEngineLogoView.alpha = progress;
  _mostVisitedContainerView.alpha = progress;
  _identityDiscButton.alpha = progress;

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

- (void)setAIMAllowed:(BOOL)allowed {
  // TODO(crbug.com/526677926): To be implemented in Phase 2.
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

- (void)setMostVisitedViewController:
    (UIViewController*)mostVisitedViewController {
  if (_mostVisitedViewController == mostVisitedViewController) {
    return;
  }
  if (_mostVisitedViewController) {
    [_mostVisitedViewController willMoveToParentViewController:nil];
    [_mostVisitedViewController.view removeFromSuperview];
    [_mostVisitedViewController removeFromParentViewController];
  }
  _mostVisitedViewController = mostVisitedViewController;
  if (self.isViewLoaded && _mostVisitedViewController) {
    [self embedMostVisitedViewController];
  }
}

- (void)embedMostVisitedViewController {
  if (!_mostVisitedViewController || !_mostVisitedContainerView) {
    return;
  }
  [self addChildViewController:_mostVisitedViewController];
  _mostVisitedViewController.view.translatesAutoresizingMaskIntoConstraints =
      NO;
  [_mostVisitedContainerView addSubview:_mostVisitedViewController.view];
  AddSameConstraints(_mostVisitedViewController.view,
                     _mostVisitedContainerView);
  [_mostVisitedViewController didMoveToParentViewController:self];
}

#pragma mark - Private

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

- (CGFloat)centeredFakeOmniboxTop {
  CGFloat safeAreaTop = self.view.safeAreaInsets.top;
  CGFloat logoHeight =
      content_suggestions::DoodleHeight(_logoState, self.traitCollection);
  return safeAreaTop + kLogoTopMargin + logoHeight + kLogoToOmniboxSpacing;
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

- (void)didChangeOmniboxPosition:(NewTabPageHeaderView*)headerView {
  // TODO(crbug.com/526677926): To be implemented in Phase 2.
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [_bottomSheetViewController feedScrollViewDidScroll:scrollView];
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  [_bottomSheetViewController feedScrollViewDidEndDragging:scrollView
                                            willDecelerate:decelerate];
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

- (void)updateAccountImage:(UIImage*)image
                      name:(NSString*)name
                     email:(NSString*)email {
  _avatarImage = image;
  _avatarName = name;
  _avatarEmail = email;
  _avatarImageLoaded = YES;
  if (_identityDiscButton) {
    [_identityDiscButton updateAccountImage:image name:name email:email];
  }
}

#pragma mark - Actions

- (void)identityDiscButtonTapped:(UIButton*)sender {
  [self.headerCommandsHandler identityDiscWasTapped:sender];
}

@end
