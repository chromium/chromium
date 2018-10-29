// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_visibility_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_search_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tools_menu_button.h"
#import "ios/chrome/browser/ui/toolbar/public/features.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/images/branded_image_provider.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ToolbarButtonFactory

@synthesize toolbarConfiguration = _toolbarConfiguration;
@synthesize style = _style;
@synthesize dispatcher = _dispatcher;
@synthesize visibilityConfiguration = _visibilityConfiguration;

- (instancetype)initWithStyle:(ToolbarStyle)style {
  self = [super init];
  if (self) {
    _style = style;
    _toolbarConfiguration = [[ToolbarConfiguration alloc] initWithStyle:style];
  }
  return self;
}

#pragma mark - Buttons

- (ToolbarButton*)backButton {
  ToolbarButton* backButton = [ToolbarButton
      toolbarButtonWithImage:[[UIImage imageNamed:@"toolbar_back"]
                                 imageFlippedForRightToLeftLayoutDirection]];
  [self configureButton:backButton width:kAdaptiveToolbarButtonWidth];
  backButton.accessibilityLabel = l10n_util::GetNSString(IDS_ACCNAME_BACK);
  [backButton addTarget:self.dispatcher
                 action:@selector(goBack)
       forControlEvents:UIControlEventTouchUpInside];
  backButton.visibilityMask = self.visibilityConfiguration.backButtonVisibility;
  return backButton;
}

// Returns a forward button without visibility mask configured.
- (ToolbarButton*)forwardButton {
  ToolbarButton* forwardButton = [ToolbarButton
      toolbarButtonWithImage:[[UIImage imageNamed:@"toolbar_forward"]
                                 imageFlippedForRightToLeftLayoutDirection]];
  [self configureButton:forwardButton width:kAdaptiveToolbarButtonWidth];
  forwardButton.visibilityMask =
      self.visibilityConfiguration.forwardButtonVisibility;
  forwardButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_ACCNAME_FORWARD);
  [forwardButton addTarget:self.dispatcher
                    action:@selector(goForward)
          forControlEvents:UIControlEventTouchUpInside];
  return forwardButton;
}

- (ToolbarTabGridButton*)tabGridButton {
  ToolbarTabGridButton* tabGridButton = [ToolbarTabGridButton
      toolbarButtonWithImage:[UIImage imageNamed:@"toolbar_switcher"]];
  [self configureButton:tabGridButton width:kAdaptiveToolbarButtonWidth];
  SetA11yLabelAndUiAutomationName(tabGridButton, IDS_IOS_TOOLBAR_SHOW_TABS,
                                  kToolbarStackButtonIdentifier);

  // TODO(crbug.com/799601): Delete this once its not needed.
  if (base::FeatureList::IsEnabled(kMemexTabSwitcher)) {
    [tabGridButton addTarget:self.dispatcher
                      action:@selector(navigateToMemexTabSwitcher)
            forControlEvents:UIControlEventTouchUpInside];
  } else {
    [tabGridButton addTarget:self.dispatcher
                      action:@selector(prepareTabSwitcher)
            forControlEvents:UIControlEventTouchDown];
    [tabGridButton addTarget:self.dispatcher
                      action:@selector(displayTabSwitcher)
            forControlEvents:UIControlEventTouchUpInside];
  }

  tabGridButton.visibilityMask =
      self.visibilityConfiguration.tabGridButtonVisibility;
  return tabGridButton;
}

- (ToolbarToolsMenuButton*)toolsMenuButton {
  ToolbarToolsMenuButton* toolsMenuButton =
      [[ToolbarToolsMenuButton alloc] initWithFrame:CGRectZero];

  SetA11yLabelAndUiAutomationName(toolsMenuButton, IDS_IOS_TOOLBAR_SETTINGS,
                                  kToolbarToolsMenuButtonIdentifier);
  [self configureButton:toolsMenuButton width:kAdaptiveToolbarButtonWidth];
  [toolsMenuButton.heightAnchor
      constraintEqualToConstant:kAdaptiveToolbarButtonWidth]
      .active = YES;
  [toolsMenuButton addTarget:self.dispatcher
                      action:@selector(showToolsMenuPopup)
            forControlEvents:UIControlEventTouchUpInside];
  toolsMenuButton.visibilityMask =
      self.visibilityConfiguration.toolsMenuButtonVisibility;
  return toolsMenuButton;
}

- (ToolbarButton*)shareButton {
  ToolbarButton* shareButton = [ToolbarButton
      toolbarButtonWithImage:[UIImage imageNamed:@"toolbar_share"]];
  [self configureButton:shareButton width:kAdaptiveToolbarButtonWidth];
  SetA11yLabelAndUiAutomationName(shareButton, IDS_IOS_TOOLS_MENU_SHARE,
                                  kToolbarShareButtonIdentifier);
  shareButton.titleLabel.text = @"Share";
  [shareButton addTarget:self.dispatcher
                  action:@selector(sharePage)
        forControlEvents:UIControlEventTouchUpInside];
  shareButton.visibilityMask =
      self.visibilityConfiguration.shareButtonVisibility;
  return shareButton;
}

- (ToolbarButton*)reloadButton {
  ToolbarButton* reloadButton = [ToolbarButton
      toolbarButtonWithImage:[[UIImage imageNamed:@"toolbar_reload"]
                                 imageFlippedForRightToLeftLayoutDirection]];
  [self configureButton:reloadButton width:kAdaptiveToolbarButtonWidth];
  reloadButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_ACCNAME_RELOAD);
  [reloadButton addTarget:self.dispatcher
                   action:@selector(reload)
         forControlEvents:UIControlEventTouchUpInside];
  reloadButton.visibilityMask =
      self.visibilityConfiguration.reloadButtonVisibility;
  return reloadButton;
}

- (ToolbarButton*)stopButton {
  ToolbarButton* stopButton = [ToolbarButton
      toolbarButtonWithImage:[UIImage imageNamed:@"toolbar_stop"]];
  [self configureButton:stopButton width:kAdaptiveToolbarButtonWidth];
  stopButton.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_ACCNAME_STOP);
  [stopButton addTarget:self.dispatcher
                 action:@selector(stopLoading)
       forControlEvents:UIControlEventTouchUpInside];
  stopButton.visibilityMask = self.visibilityConfiguration.stopButtonVisibility;
  return stopButton;
}

- (ToolbarButton*)bookmarkButton {
  ToolbarButton* bookmarkButton = [ToolbarButton
      toolbarButtonWithImage:[UIImage imageNamed:@"toolbar_bookmark"]];
  [bookmarkButton setImage:[UIImage imageNamed:@"toolbar_bookmark_active"]
                  forState:ControlStateSpotlighted];
  [self configureButton:bookmarkButton width:kAdaptiveToolbarButtonWidth];
  bookmarkButton.adjustsImageWhenHighlighted = NO;
  [bookmarkButton
      setImage:[bookmarkButton imageForState:UIControlStateHighlighted]
      forState:UIControlStateSelected];
  bookmarkButton.accessibilityLabel = l10n_util::GetNSString(IDS_TOOLTIP_STAR);
  [bookmarkButton addTarget:self.dispatcher
                     action:@selector(bookmarkPage)
           forControlEvents:UIControlEventTouchUpInside];

  bookmarkButton.visibilityMask =
      self.visibilityConfiguration.bookmarkButtonVisibility;
  return bookmarkButton;
}

- (ToolbarButton*)omniboxButton {
  ToolbarSearchButton* omniboxButton = [ToolbarSearchButton
      toolbarButtonWithImage:[UIImage imageNamed:@"toolbar_search"]];

  [self configureButton:omniboxButton width:kOmniboxButtonWidth];
  [omniboxButton addTarget:self.dispatcher
                    action:@selector(closeFindInPage)
          forControlEvents:UIControlEventTouchUpInside];
  [omniboxButton addTarget:self.dispatcher
                    action:@selector(focusOmniboxFromSearchButton)
          forControlEvents:UIControlEventTouchUpInside];

  omniboxButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_SEARCH);
  omniboxButton.accessibilityIdentifier = kToolbarOmniboxButtonIdentifier;

  omniboxButton.visibilityMask =
      self.visibilityConfiguration.omniboxButtonVisibility;
  return omniboxButton;
}

- (UIButton*)cancelButton {
  UIButton* cancelButton = [UIButton buttonWithType:UIButtonTypeSystem];
  cancelButton.titleLabel.font = [UIFont systemFontOfSize:kLocationBarFontSize];
  cancelButton.tintColor = self.style == NORMAL
                               ? UIColorFromRGB(kLocationBarTintBlue)
                               : [UIColor whiteColor];
  [cancelButton setTitle:l10n_util::GetNSString(IDS_CANCEL)
                forState:UIControlStateNormal];
  [cancelButton setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                  forAxis:UILayoutConstraintAxisHorizontal];
  [cancelButton
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  cancelButton.contentEdgeInsets = UIEdgeInsetsMake(
      0, kCancelButtonHorizontalInset, 0, kCancelButtonHorizontalInset);
  cancelButton.hidden = YES;
  [cancelButton addTarget:self.dispatcher
                   action:@selector(cancelOmniboxEdit)
         forControlEvents:UIControlEventTouchUpInside];
  cancelButton.accessibilityIdentifier =
      kToolbarCancelOmniboxEditButtonIdentifier;
  return cancelButton;
}

#pragma mark - Helpers

// Sets the |button| width to |width| with a priority of
// UILayoutPriorityRequired - 1. If the priority is |UILayoutPriorityRequired|,
// there is a conflict when the buttons are hidden as the stack view is setting
// their width to 0. Setting the priority to UILayoutPriorityDefaultHigh doesn't
// work as they would have a lower priority than other elements.
- (void)configureButton:(ToolbarButton*)button width:(CGFloat)width {
  NSLayoutConstraint* constraint =
      [button.widthAnchor constraintEqualToConstant:width];
  constraint.priority = UILayoutPriorityRequired - 1;
  constraint.active = YES;
  button.configuration = self.toolbarConfiguration;
  button.exclusiveTouch = YES;
}

@end
