// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"

#import "base/ios/ios_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_actions_handler.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_visibility_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/button_configuration_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The size of the symbol image.
const CGFloat kSymbolToolbarPointSize = 24;

}  // namespace

@implementation ToolbarButtonFactory

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
  UIImage* backImage =
      DefaultSymbolWithPointSize(kBackSymbol, kSymbolToolbarPointSize);
  ToolbarButton* backButton = [[ToolbarButton alloc]
      initWithImage:[backImage imageFlippedForRightToLeftLayoutDirection]];
  [self configureButton:backButton width:kAdaptiveToolbarButtonWidth];
  backButton.accessibilityLabel = l10n_util::GetNSString(IDS_ACCNAME_BACK);
  [backButton addTarget:self.actionHandler
                 action:@selector(backAction)
       forControlEvents:UIControlEventTouchUpInside];
  backButton.visibilityMask = self.visibilityConfiguration.backButtonVisibility;
  return backButton;
}

// Returns a forward button without visibility mask configured.
- (ToolbarButton*)forwardButton {
  UIImage* forwardImage =
      DefaultSymbolWithPointSize(kForwardSymbol, kSymbolToolbarPointSize);
  ToolbarButton* forwardButton = [[ToolbarButton alloc]
      initWithImage:[forwardImage imageFlippedForRightToLeftLayoutDirection]];
  [self configureButton:forwardButton width:kAdaptiveToolbarButtonWidth];
  forwardButton.visibilityMask =
      self.visibilityConfiguration.forwardButtonVisibility;
  forwardButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_ACCNAME_FORWARD);
  [forwardButton addTarget:self.actionHandler
                    action:@selector(forwardAction)
          forControlEvents:UIControlEventTouchUpInside];
  return forwardButton;
}

- (ToolbarTabGridButton*)tabGridButton {
  UIImage* tabGridImage =
      CustomSymbolWithPointSize(kSquareNumberSymbol, kSymbolToolbarPointSize);
  ToolbarTabGridButton* tabGridButton =
      [[ToolbarTabGridButton alloc] initWithImage:tabGridImage];
  [self configureButton:tabGridButton width:kAdaptiveToolbarButtonWidth];
  SetA11yLabelAndUiAutomationName(tabGridButton, IDS_IOS_TOOLBAR_SHOW_TABS,
                                  kToolbarStackButtonIdentifier);
  [tabGridButton addTarget:self.actionHandler
                    action:@selector(tabGridTouchDown)
          forControlEvents:UIControlEventTouchDown];
  [tabGridButton addTarget:self.actionHandler
                    action:@selector(tabGridTouchUp)
          forControlEvents:UIControlEventTouchUpInside];
  tabGridButton.visibilityMask =
      self.visibilityConfiguration.tabGridButtonVisibility;
  return tabGridButton;
}

- (ToolbarButton*)toolsMenuButton {
  ToolbarButton* toolsMenuButton = [[ToolbarButton alloc]
      initWithImage:DefaultSymbolWithPointSize(kMenuSymbol,
                                               kSymbolToolbarPointSize)];

  SetA11yLabelAndUiAutomationName(toolsMenuButton, IDS_IOS_TOOLBAR_SETTINGS,
                                  kToolbarToolsMenuButtonIdentifier);
  [self configureButton:toolsMenuButton width:kAdaptiveToolbarButtonWidth];
  [toolsMenuButton.heightAnchor
      constraintEqualToConstant:kAdaptiveToolbarButtonWidth]
      .active = YES;
  [toolsMenuButton addTarget:self.actionHandler
                      action:@selector(toolsMenuAction)
            forControlEvents:UIControlEventTouchUpInside];
  toolsMenuButton.visibilityMask =
      self.visibilityConfiguration.toolsMenuButtonVisibility;
  return toolsMenuButton;
}

- (ToolbarButton*)shareButton {
  UIImage* shareImage =
      DefaultSymbolWithPointSize(kShareSymbol, kSymbolToolbarPointSize);
  ToolbarButton* shareButton = [[ToolbarButton alloc] initWithImage:shareImage];
  [self configureButton:shareButton width:kAdaptiveToolbarButtonWidth];
  SetA11yLabelAndUiAutomationName(shareButton, IDS_IOS_TOOLS_MENU_SHARE,
                                  kToolbarShareButtonIdentifier);
  shareButton.titleLabel.text = @"Share";
  [shareButton addTarget:self.actionHandler
                  action:@selector(shareAction)
        forControlEvents:UIControlEventTouchUpInside];
  shareButton.visibilityMask =
      self.visibilityConfiguration.shareButtonVisibility;
  return shareButton;
}

- (ToolbarButton*)reloadButton {
  UIImage* reloadImage =
      CustomSymbolWithPointSize(kArrowClockWiseSymbol, kSymbolToolbarPointSize);
  ToolbarButton* reloadButton = [[ToolbarButton alloc]
      initWithImage:[reloadImage imageFlippedForRightToLeftLayoutDirection]];
  [self configureButton:reloadButton width:kAdaptiveToolbarButtonWidth];
  reloadButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_ACCNAME_RELOAD);
  [reloadButton addTarget:self.actionHandler
                   action:@selector(reloadAction)
         forControlEvents:UIControlEventTouchUpInside];
  reloadButton.visibilityMask =
      self.visibilityConfiguration.reloadButtonVisibility;
  return reloadButton;
}

- (ToolbarButton*)stopButton {
  UIImage* stopImage =
      DefaultSymbolWithPointSize(kXMarkSymbol, kSymbolToolbarPointSize);
  ToolbarButton* stopButton = [[ToolbarButton alloc] initWithImage:stopImage];
  [self configureButton:stopButton width:kAdaptiveToolbarButtonWidth];
  stopButton.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_ACCNAME_STOP);
  [stopButton addTarget:self.actionHandler
                 action:@selector(stopAction)
       forControlEvents:UIControlEventTouchUpInside];
  stopButton.visibilityMask = self.visibilityConfiguration.stopButtonVisibility;
  return stopButton;
}

- (ToolbarButton*)openNewTabButton {
  UIImage* image = SymbolWithPalette(
      CustomSymbolWithPointSize(kPlusCircleFillSymbol, kSymbolToolbarPointSize),
      @[
        [UIColor colorNamed:kGrey600Color],
        [self.toolbarConfiguration locationBarBackgroundColorWithVisibility:1]
      ]);
  UIImage* IPHHighlightedImage = SymbolWithPalette(
      CustomSymbolWithPointSize(kPlusCircleFillSymbol, kSymbolToolbarPointSize),
      @[
        // The color of the 'plus'.
        _toolbarConfiguration.buttonsTintColorIPHHighlighted,
        // The filling color of the circle.
        _toolbarConfiguration.buttonsIPHHighlightColor
      ]);
  ToolbarButton* newTabButton =
      [[ToolbarButton alloc] initWithImage:image
                       IPHHighlightedImage:IPHHighlightedImage];

  [newTabButton addTarget:self.actionHandler
                   action:@selector(newTabAction:)
         forControlEvents:UIControlEventTouchUpInside];
  BOOL isIncognito = self.style == ToolbarStyle::kIncognito;

  [self configureButton:newTabButton width:kAdaptiveToolbarButtonWidth];

  newTabButton.accessibilityLabel =
      l10n_util::GetNSString(isIncognito ? IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB
                                         : IDS_IOS_TOOLS_MENU_NEW_TAB);

  newTabButton.accessibilityIdentifier = kToolbarNewTabButtonIdentifier;

  newTabButton.visibilityMask =
      self.visibilityConfiguration.newTabButtonVisibility;
  return newTabButton;
}

- (UIButton*)cancelButton {
  UIButton* cancelButton = [UIButton buttonWithType:UIButtonTypeSystem];
  cancelButton.tintColor = [UIColor colorNamed:kBlueColor];
  [cancelButton setContentHuggingPriority:UILayoutPriorityRequired
                                  forAxis:UILayoutConstraintAxisHorizontal];
  [cancelButton
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

  if (IsUIButtonConfigurationEnabled()) {
    UIButtonConfiguration* buttonConfiguration =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
        0, kCancelButtonHorizontalInset, 0, kCancelButtonHorizontalInset);
    UIFont* font = [UIFont systemFontOfSize:kLocationBarFontSize];
    NSDictionary* attributes = @{NSFontAttributeName : font};
    NSMutableAttributedString* attributedString =
        [[NSMutableAttributedString alloc]
            initWithString:l10n_util::GetNSString(IDS_CANCEL)
                attributes:attributes];
    buttonConfiguration.attributedTitle = attributedString;
    cancelButton.configuration = buttonConfiguration;
  } else {
    cancelButton.titleLabel.font =
        [UIFont systemFontOfSize:kLocationBarFontSize];
    [cancelButton setTitle:l10n_util::GetNSString(IDS_CANCEL)
                  forState:UIControlStateNormal];
    UIEdgeInsets contentInsets = UIEdgeInsetsMake(
        0, kCancelButtonHorizontalInset, 0, kCancelButtonHorizontalInset);
    SetContentEdgeInsets(cancelButton, contentInsets);
  }

  cancelButton.hidden = YES;
  [cancelButton addTarget:self.actionHandler
                   action:@selector(cancelOmniboxFocusAction)
         forControlEvents:UIControlEventTouchUpInside];
  cancelButton.accessibilityIdentifier =
      kToolbarCancelOmniboxEditButtonIdentifier;
  return cancelButton;
}

#pragma mark - Helpers

// Sets the `button` width to `width` with a priority of
// UILayoutPriorityRequired - 1. If the priority is `UILayoutPriorityRequired`,
// there is a conflict when the buttons are hidden as the stack view is setting
// their width to 0. Setting the priority to UILayoutPriorityDefaultHigh doesn't
// work as they would have a lower priority than other elements.
- (void)configureButton:(ToolbarButton*)button width:(CGFloat)width {
  NSLayoutConstraint* constraint =
      [button.widthAnchor constraintEqualToConstant:width];
  constraint.priority = UILayoutPriorityRequired - 1;
  constraint.active = YES;
  button.toolbarConfiguration = self.toolbarConfiguration;
  button.exclusiveTouch = YES;
  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider =
      ^UIPointerStyle*(UIButton* uiButton, UIPointerEffect* proposedEffect,
                       UIPointerShape* proposedShape) {
    // This gets rid of a thin border on a spotlighted bookmarks button.
    // This is applied to all toolbar buttons for consistency.
    CGRect rect = CGRectInset(uiButton.frame, 1, 1);
    UIPointerShape* shape = [UIPointerShape shapeWithRoundedRect:rect];
    return [UIPointerStyle styleWithEffect:proposedEffect shape:shape];
  };
}

@end
