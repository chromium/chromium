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
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/toolbar_utils.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_visibility.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_constants.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_height_delegate.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_mutator.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation ToolbarViewController {
  ToolbarButton* _backButton;
  ToolbarButton* _forwardButton;
  ToolbarButton* _reloadButton;
  ToolbarButton* _stopButton;
  ToolbarButton* _shareButton;
  ToolbarButton* _tabGridButton;
  ToolbarButton* _toolsMenuButton;
  UIButton* _omniboxButton;
  UIStackView* _stackView;
  BOOL _visible;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  // TODO(crbug.com/472279443): Use real color.
  self.view.backgroundColor = UIColor.greenColor;
  self.view.accessibilityIdentifier = kToolbarViewIdentifier;

  [self createButtons];
  [self setUpHierarchy];

  [self updateToolbarVisibility];
}

- (CGFloat)toolbarHeight {
  CGFloat height = 0;

  if (_visible) {
    height += ToolbarExpandedHeight(
        self.traitCollection.preferredContentSizeCategory);
  }
  return height;
}

#pragma mark - ToolbarConsumer

- (void)setCanGoBack:(BOOL)canGoBack {
  _backButton.enabled = canGoBack;
}

- (void)setCanGoForward:(BOOL)canGoForward {
  _forwardButton.enabled = canGoForward;
}

- (void)setIsLoading:(BOOL)isLoading {
  _reloadButton.hidden = isLoading;
  _stopButton.hidden = !isLoading;
}

- (void)setShareEnabled:(BOOL)enabled {
  _shareButton.enabled = enabled;
}

- (void)setLocationBarText:(NSString*)text {
  [_omniboxButton setTitle:text forState:UIControlStateNormal];
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

- (void)triggerToolbarSlideInAnimation {
  // TODO(crbug.com/472279443): Implement this.
  NOTREACHED();
}

- (void)focusLocationBarForVoiceOver {
  if (!_visible) {
    return;
  }
  // TODO(crbug.com/472279443): Focus the real location bar.
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  _omniboxButton);
}

#pragma mark - Private

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

// Creates the buttons.
- (void)createButtons {
  if (!self.buttonFactory) {
    return;
  }
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
                   action:@selector(shareButtonTapped)
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
  _omniboxButton = [self.buttonFactory makeOmniboxButton];
  [_omniboxButton addTarget:self
                     action:@selector(omniboxTapped)
           forControlEvents:UIControlEventTouchUpInside];
}

// Sets up the hierarchy of the buttons.
- (void)setUpHierarchy {
  _stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    _backButton, _forwardButton, _reloadButton, _stopButton, _omniboxButton,
    _shareButton, _tabGridButton, _toolsMenuButton
  ]];
  _stackView.translatesAutoresizingMaskIntoConstraints = NO;
  _stackView.axis = UILayoutConstraintAxisHorizontal;
  _stackView.distribution = UIStackViewDistributionEqualSpacing;
  _stackView.alignment = UIStackViewAlignmentCenter;

  [self.view addSubview:_stackView];
  AddSameConstraints(self.view.safeAreaLayoutGuide, _stackView);

  [self updateButtonVisibility];
  [self
      registerForTraitChanges:
          @[ UITraitVerticalSizeClass.class, UITraitHorizontalSizeClass.class ]
                   withAction:@selector(updateButtonVisibility)];
}

// Updates the visibility of the buttons based on the current size class and
// loading state.
- (void)updateButtonVisibility {
  for (UIView* view in _stackView.arrangedSubviews) {
    ToolbarButton* button = base::apple::ObjCCast<ToolbarButton>(view);
    [button updateVisibility];
  }
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
- (void)shareButtonTapped {
  [self.activityServiceHandler showShareSheet];
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
  self.view.hidden = !_visible;
}

@end
