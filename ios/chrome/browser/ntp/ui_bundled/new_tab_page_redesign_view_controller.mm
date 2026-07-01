// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_redesign_view_controller.h"

#import "ios/chrome/browser/home_customization/ui/home_customization_framing_coordinates.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_image_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_mutator.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// Animation duration for wallpaper transition.
constexpr CGFloat kBackgroundImageAnimationDuration = 0.25;
}  // namespace

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

  if (_searchEngineLogoView) {
    [self addSearchEngineLogoView];
  }
}

- (void)invalidate {
  self.mutator = nil;
  self.searchEngineLogoView = nil;
  self.NTPContentDelegate = nil;
  [self setFeedViewController:nil];
  [_bottomSheetViewController invalidate];
  _bottomSheetViewController = nil;
}

#pragma mark - Public

- (void)focusOmnibox {
  [self.NTPContentDelegate focusOmnibox];
}

#pragma mark - NewTabPageBottomSheetViewControllerDelegate

- (void)bottomSheetViewControllerDidTapFakeLocationBar:
    (NewTabPageBottomSheetViewController*)bottomSheetViewController {
  [self focusOmnibox];
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

#pragma mark - Private

- (void)addSearchEngineLogoView {
  if (!_searchEngineLogoView || !_bottomSheetViewController.view) {
    return;
  }
  [self.view insertSubview:_searchEngineLogoView
              belowSubview:_bottomSheetViewController.view];
  _searchEngineLogoView.translatesAutoresizingMaskIntoConstraints = NO;

  [NSLayoutConstraint activateConstraints:@[
    [_searchEngineLogoView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [_searchEngineLogoView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:40],
    [_searchEngineLogoView.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor
                     multiplier:0.8],
    [_searchEngineLogoView.heightAnchor constraintEqualToConstant:120]
  ]];
}

#pragma mark - SearchEngineLogoConsumer

- (void)searchEngineLogoStateDidChange:(SearchEngineLogoState)logoState {
  // TODO(crbug.com/526677926): To be implemented in Phase 2.
}

#pragma mark - NewTabPageHeaderViewDelegate

- (BOOL)shouldPinFakeOmnibox {
  return NO;
}

- (void)didChangeOmniboxPosition:(NewTabPageHeaderView*)headerView {
  // TODO(crbug.com/526677926): To be implemented in Phase 2.
}

@end
