// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/toolbar_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_entrypoint.h"
#import "ios/chrome/browser/intents/model/intents_donation_helper.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_visibility.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_buttons_utils.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_constants.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_height_delegate.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_mutator.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

constexpr CGFloat kStackViewSpacing = 9;

constexpr CGFloat kButtonMinScale = 0.2;

}  // namespace

@implementation ToolbarViewController {
  ToolbarButton* _backButton;
  ToolbarButton* _forwardButton;
  ToolbarButton* _reloadButton;
  ToolbarButton* _stopButton;
  ToolbarButton* _shareButton;
  ToolbarButton* _tabGridButton;
  ToolbarButton* _toolsMenuButton;

  // The stack views that hold the buttons on the leading side.
  UIStackView* _leadingStackView;
  // The container for the location bar, which is transparent.
  UIView* _locationBarContainer;
  // The background for the location bar, which is a pill-shaped view.
  UIView* _locationBarBackground;
  // The stack views that hold the buttons on the trailing side.
  UIStackView* _trailingStackView;

  // The location bar height constraint.
  NSLayoutConstraint* _locationBarHeightConstraint;
  // The constraint for the bottom padding of the toolbar.
  NSLayoutConstraint* _locationBarBottomPaddingConstraint;

  // Whether this toolbar is currently visible.
  BOOL _visible;

  // Whether this toolbar is in incognito mode.
  BOOL _incognito;
}

- (instancetype)initInIncognito:(BOOL)incognito {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _incognito = incognito;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  // TODO(crbug.com/472279443): Use real color.
  self.view.backgroundColor = UIColor.greenColor;
  self.view.accessibilityIdentifier = kToolbarViewIdentifier;

  [self createView];
  [self setUpHierarchy];

  [self updateToolbarVisibility];
}

- (void)setLocationBarViewController:
    (UIViewController*)locationBarViewController {
  if (_locationBarViewController == locationBarViewController) {
    return;
  }
  [self loadViewIfNeeded];

  if (_locationBarViewController &&
      [_locationBarViewController.view isDescendantOfView:self.view]) {
    [_locationBarViewController willMoveToParentViewController:nil];
    [_locationBarViewController.view removeFromSuperview];
    [_locationBarViewController removeFromParentViewController];
  }

  _locationBarViewController = locationBarViewController;

  UIView* locationBarView = locationBarViewController.view;
  locationBarView.translatesAutoresizingMaskIntoConstraints = NO;
  [locationBarView setContentHuggingPriority:UILayoutPriorityDefaultLow
                                     forAxis:UILayoutConstraintAxisHorizontal];

  [self addChildViewController:_locationBarViewController];
  [_locationBarContainer addSubview:locationBarView];
  AddSameConstraints(locationBarView, _locationBarContainer);
  [_locationBarViewController didMoveToParentViewController:self];
}

#pragma mark - ToolbarConsumer

- (void)setCanGoBack:(BOOL)canGoBack {
  _backButton.enabled = canGoBack;
}

- (void)setCanGoForward:(BOOL)canGoForward {
  _forwardButton.enabled = canGoForward;
}

- (void)setIsLoading:(BOOL)isLoading {
  _reloadButton.forceHidden = isLoading;
  _stopButton.forceHidden = !isLoading;
}

- (void)setShareEnabled:(BOOL)enabled {
  _shareButton.enabled = enabled;
}

- (void)setVisible:(BOOL)visible {
  if (_visible == visible) {
    return;
  }
  _visible = visible;
  [self loadViewIfNeeded];
  [self updateToolbarVisibility];
  [self.toolbarHeightDelegate toolbarsHeightChanged];
}

- (void)setLocationIndicatorVisible:(BOOL)locationIndicatorVisible
                    forNotification:(NSNotification*)notification {
  if (locationIndicatorVisible) {
    [self.toolbarHeightDelegate secondaryToolbarMovedAboveKeyboard];
  } else {
    [self.toolbarHeightDelegate secondaryToolbarRemovedFromKeyboard];
    [GetFirstResponder() resignFirstResponder];
  }

  [self.view layoutIfNeeded];

  NSDictionary* userInfo = notification.userInfo;
  NSTimeInterval duration =
      [userInfo[UIKeyboardAnimationDurationUserInfoKey] doubleValue];
  UIViewAnimationCurve curve = static_cast<UIViewAnimationCurve>(
      [userInfo[UIKeyboardAnimationCurveUserInfoKey] integerValue]);

  CGFloat visibleKeyboardHeight = 0;
  if (locationIndicatorVisible) {
    if ([self useAccessoryViewPosition]) {
      visibleKeyboardHeight = [self inputAccessoryHeightInWindow];
    } else {
      visibleKeyboardHeight =
          [self keyboardHeightInWindowFromNotification:notification];
    }
  }

  [self.toolbarHeightDelegate
      adjustSecondaryToolbarForKeyboardHeight:visibleKeyboardHeight
                                  isCollapsed:locationIndicatorVisible
                                     duration:duration
                                        curve:curve];
}

- (void)setLocationBarHidden:(BOOL)hidden {
  _locationBarContainer.hidden = hidden;
}

- (UIView*)locationBarContainerCopy {
  UIView* locationBarContainerCopy = [self createLocationBarBackground];
  locationBarContainerCopy.translatesAutoresizingMaskIntoConstraints = YES;
  locationBarContainerCopy.frame =
      [_locationBarBackground convertRect:_locationBarBackground.bounds
                                   toView:nil];
  return locationBarContainerCopy;
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  CGFloat locationBarHeight = progress * kLocationBarHeight +
                              (1 - progress) * kLocationBarHeightFullscreen;
  _locationBarHeightConstraint.constant = locationBarHeight;
  _locationBarBackground.layer.cornerRadius = locationBarHeight / 2.0;

  _locationBarBackground.alpha = progress;

  CGFloat toolbarPadding =
      progress * kToolbarPadding + (1 - progress) * kToolbarPaddingFullscreen;
  _locationBarBottomPaddingConstraint.constant = -toolbarPadding;

  [self updateButtons:_leadingStackView.arrangedSubviews
      forFullscreenProgress:progress];
  [self updateButtons:_trailingStackView.arrangedSubviews
      forFullscreenProgress:progress];
}

#pragma mark - Private

// Updates all the `buttons` according to the fullscreen `progress`.
- (void)updateButtons:(NSArray<UIView*>*)buttons
    forFullscreenProgress:(CGFloat)progress {
  for (UIView* button in buttons) {
    if (progress > 0.99) {
      button.alpha = 1;
      button.transform = CGAffineTransformIdentity;
    } else {
      button.alpha = progress;
      CGFloat scale = progress + (1 - progress) * kButtonMinScale;
      button.transform = CGAffineTransformMakeScale(scale, scale);
    }
  }
}

// Returns whether the a accessory view position should be used.
- (BOOL)useAccessoryViewPosition {
  UIView* inputAccessory = [self.layoutGuideCenter
      referencedViewUnderName:kInputAccessoryViewLayoutGuide];
  return inputAccessory != nil;
}

// Returns the input accessory view height, in window coordinates.
- (CGFloat)inputAccessoryHeightInWindow {
  UIView* inputAccessory = [self.layoutGuideCenter
      referencedViewUnderName:kInputAccessoryViewLayoutGuide];
  CGRect rectInWindow =
      [inputAccessory convertRect:inputAccessory.layer.presentationLayer.frame
                           toView:self.view.window];
  return self.view.window.frame.size.height - rectInWindow.origin.y;
}

// Returns the user visible height of the keyboard.
- (CGFloat)keyboardHeightInWindowFromNotification:
    (NSNotification*)notification {
  NSDictionary* userInfo = notification.userInfo;
  // Part of the keyboard might be hidden. Keep only the visible area.
  CGRect keyboardFrame = [userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
  id<UICoordinateSpace> fromCoordinateSpace =
      ((UIScreen*)notification.object).coordinateSpace;
  id<UICoordinateSpace> toCoordinateSpace = self.view.window;
  CGRect keyboardFrameInWindow =
      [fromCoordinateSpace convertRect:keyboardFrame
                     toCoordinateSpace:toCoordinateSpace];
  return CGRectIntersection(keyboardFrameInWindow, self.view.window.bounds)
      .size.height;
}

// Returns a new background for the location bar.
- (UIView*)createLocationBarBackground {
  UIView* locationBarBackground = [[UIView alloc] init];
  locationBarBackground.translatesAutoresizingMaskIntoConstraints = NO;
  locationBarBackground.layer.cornerRadius = kLocationBarHeight / 2.0;

  locationBarBackground.backgroundColor =
      ToolbarLocationBarBackgroundColor(_incognito);

  ConfigureShadowForToolbarButton(locationBarBackground);

  return locationBarBackground;
}

// Returns a new location bar container.
- (UIView*)createLocationBarContainerWithBackground:
    (UIView*)locationBarBackground {
  UIView* locationBarContainer = [[UIView alloc] init];
  locationBarContainer.translatesAutoresizingMaskIntoConstraints = NO;

  [locationBarContainer addSubview:locationBarBackground];
  AddSameConstraints(locationBarContainer, locationBarBackground);

  [locationBarContainer
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [locationBarContainer
      setContentHuggingPriority:UILayoutPriorityDefaultLow
                        forAxis:UILayoutConstraintAxisHorizontal];

  return locationBarContainer;
}

// Creates the views.
- (void)createView {
  CHECK(self.buttonFactory);
  _locationBarBackground = [self createLocationBarBackground];
  _locationBarContainer =
      [self createLocationBarContainerWithBackground:_locationBarBackground];

  _backButton = [self.buttonFactory makeBackButton];
  [_backButton addTarget:self
                  action:@selector(backButtonTapped)
        forControlEvents:UIControlEventTouchUpInside];
  _forwardButton = [self.buttonFactory makeForwardButton];
  [_forwardButton addTarget:self
                     action:@selector(forwardButtonTapped)
           forControlEvents:UIControlEventTouchUpInside];
  _reloadButton = [self.buttonFactory makeReloadButton];
  [_reloadButton addTarget:self
                    action:@selector(reloadButtonTapped)
          forControlEvents:UIControlEventTouchUpInside];
  _stopButton = [self.buttonFactory makeStopButton];
  [_stopButton addTarget:self
                  action:@selector(stopButtonTapped)
        forControlEvents:UIControlEventTouchUpInside];
  _shareButton = [self.buttonFactory makeShareButton];
  [_shareButton addTarget:self
                   action:@selector(shareButtonTapped:)
         forControlEvents:UIControlEventTouchUpInside];
  _tabGridButton = [self.buttonFactory makeTabGridButton];
  [_tabGridButton addTarget:self
                     action:@selector(tabGridTouchDown)
           forControlEvents:UIControlEventTouchDown];
  [_tabGridButton addTarget:self
                     action:@selector(tabGridTouchUp)
           forControlEvents:UIControlEventTouchUpInside];
  _toolsMenuButton = [self.buttonFactory makeToolsMenuButton];
  [_toolsMenuButton addTarget:self
                       action:@selector(toolsMenuButtonTapped)
             forControlEvents:UIControlEventTouchUpInside];
}

- (UIStackView*)makeStackViewWithButtons:(NSArray<UIButton*>*)buttons {
  UIStackView* stackView =
      [[UIStackView alloc] initWithArrangedSubviews:buttons];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.distribution = UIStackViewDistributionFill;
  stackView.alignment = UIStackViewAlignmentCenter;
  stackView.spacing = kStackViewSpacing;
  return stackView;
}

// Sets up the hierarchy of the buttons.
- (void)setUpHierarchy {
  _leadingStackView = [self makeStackViewWithButtons:@[
    _backButton,
    _forwardButton,
    _reloadButton,
    _stopButton,
  ]];
  [self.view addSubview:_leadingStackView];

  [self.view addSubview:_locationBarContainer];
  NSLayoutConstraint* widthConstraint = [_locationBarContainer.widthAnchor
      constraintEqualToAnchor:self.view.widthAnchor];
  widthConstraint.priority = UILayoutPriorityRequired - 1;
  widthConstraint.active = YES;

  _trailingStackView = [self makeStackViewWithButtons:@[
    _shareButton, _tabGridButton, _toolsMenuButton
  ]];
  [self.view addSubview:_trailingStackView];

  _locationBarHeightConstraint = [_locationBarContainer.heightAnchor
      constraintEqualToConstant:kLocationBarHeight];
  _locationBarHeightConstraint.active = YES;

  _locationBarBottomPaddingConstraint = [_locationBarContainer.bottomAnchor
      constraintEqualToAnchor:self.view.bottomAnchor
                     constant:-kToolbarPadding];
  _locationBarBottomPaddingConstraint.active = YES;

  UILayoutGuide* safeAreaGuide = self.view.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [_leadingStackView.leadingAnchor
        constraintEqualToAnchor:safeAreaGuide.leadingAnchor
                       constant:kStackViewSpacing],
    [_leadingStackView.trailingAnchor
        constraintLessThanOrEqualToAnchor:_locationBarContainer.leadingAnchor
                                 constant:-kStackViewSpacing],
    [_leadingStackView.centerYAnchor
        constraintEqualToAnchor:_locationBarContainer.centerYAnchor],

    [_trailingStackView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:_locationBarContainer
                                                 .trailingAnchor
                                    constant:kStackViewSpacing],
    [_trailingStackView.trailingAnchor
        constraintEqualToAnchor:safeAreaGuide.trailingAnchor
                       constant:-kStackViewSpacing],
    [_trailingStackView.centerYAnchor
        constraintEqualToAnchor:_locationBarContainer.centerYAnchor],

    [_locationBarContainer.centerXAnchor
        constraintEqualToAnchor:safeAreaGuide.centerXAnchor],
  ]];
}

// Handles back button tap.
- (void)backButtonTapped {
  [self.mutator goBack];
}

// Handles forward button tap.
- (void)forwardButtonTapped {
  [self.mutator goForward];
}

// Handles reload button tap.
- (void)reloadButtonTapped {
  [self.mutator reload];
}

// Handles stop button tap.
- (void)stopButtonTapped {
  [self.mutator stop];
}

// Handles share button tap.
- (void)shareButtonTapped:(UIView*)sender {
  [self.activityServiceHandler showShareSheetFromShareButton:sender];
}

// Handles tools menu button tap.
- (void)toolsMenuButtonTapped {
  [self.popupMenuHandler showToolsMenuPopup];
}

// Handles omnibox tap.
- (void)omniboxTapped {
  [self.browserCoordinatorHandler showComposebox];
}

// Handles tab grid button touch down.
- (void)tabGridTouchDown {
  [IntentDonationHelper donateIntent:IntentType::kOpenTabGrid];
  [self.sceneHandler prepareTabSwitcher];
}

// Handles tab grid button touch up.
- (void)tabGridTouchUp {
  [self.sceneHandler displayTabGridInMode:TabGridOpeningMode::kDefault];
}

// Updates the visibility of the toolbar.
- (void)updateToolbarVisibility {
  _leadingStackView.hidden = !_visible;
  _locationBarContainer.hidden = !_visible;
  _trailingStackView.hidden = !_visible;
}

@end
