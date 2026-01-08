// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/toolbar_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_visibility.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_constants.h"
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
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  // TODO(crbug.com/472279443): Use real color.
  self.view.backgroundColor = UIColor.greenColor;
  self.view.accessibilityIdentifier = kToolbarViewIdentifier;

  [self createButtons];
  [self setUpHierarchy];
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

#pragma mark - Private

// Creates the buttons.
- (void)createButtons {
  if (!self.buttonFactory) {
    return;
  }
  _backButton = [self.buttonFactory makeBackButton];
  _forwardButton = [self.buttonFactory makeForwardButton];
  _reloadButton = [self.buttonFactory makeReloadButton];
  _stopButton = [self.buttonFactory makeStopButton];
  _shareButton = [self.buttonFactory makeShareButton];
  _tabGridButton = [self.buttonFactory makeTabGridButton];
  _toolsMenuButton = [self.buttonFactory makeToolsMenuButton];
  _omniboxButton = [self.buttonFactory makeOmniboxButton];
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
  AddSameConstraints(self.view, _stackView);

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

@end
