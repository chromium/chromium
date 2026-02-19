// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/legacy_toolbar_button_factory.h"

#import "base/ios/ios_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/omnibox/public/omnibox_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/legacy_toolbar_button.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/toolbar_button_actions_handler.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/toolbar_button_visibility_configuration.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/toolbar_tab_grid_button.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/toolbar_tab_group_state.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/toolbar_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ios/public/provider/chrome/browser/raccoon/raccoon_api.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The size of the symbol image.
const CGFloat kSymbolToolbarPointSize = 24;

// The padding to be added to the bottom of the system share icon to balance
// the white space on top.
const CGFloat kShareIconBalancingHeightPadding = 1;

}  // namespace

@implementation LegacyToolbarButtonFactory

- (instancetype)initWithStyle:(ToolbarStyle)style {
  self = [super init];
  if (self) {
    _style = style;
    _toolbarConfiguration = [[ToolbarConfiguration alloc] initWithStyle:style];
  }
  return self;
}

#pragma mark - Buttons

- (LegacyToolbarButton*)backButton {
  auto loadImageBlock = ^UIImage* {
    UIImage* backImage =
        DefaultSymbolWithPointSize(kBackSymbol, kSymbolToolbarPointSize);
    return [backImage imageFlippedForRightToLeftLayoutDirection];
  };

  LegacyToolbarButton* backButton =
      [[LegacyToolbarButton alloc] initWithImageLoader:loadImageBlock];

  [self configureButton:backButton width:kAdaptiveToolbarButtonWidth];
  backButton.accessibilityLabel = l10n_util::GetNSString(IDS_ACCNAME_BACK);
  backButton.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_ACCESSIBILITY_HINT_BACK);
  [backButton addTarget:self.actionHandler
                 action:@selector(backAction)
       forControlEvents:UIControlEventTouchUpInside];
  backButton.visibilityMask = self.visibilityConfiguration.backButtonVisibility;
  return backButton;
}

// Returns a forward button without visibility mask configured.
- (LegacyToolbarButton*)forwardButton {
  auto loadImageBlock = ^UIImage* {
    UIImage* forwardImage =
        DefaultSymbolWithPointSize(kForwardSymbol, kSymbolToolbarPointSize);
    return [forwardImage imageFlippedForRightToLeftLayoutDirection];
  };

  LegacyToolbarButton* forwardButton =
      [[LegacyToolbarButton alloc] initWithImageLoader:loadImageBlock];

  [self configureButton:forwardButton width:kAdaptiveToolbarButtonWidth];
  forwardButton.visibilityMask =
      self.visibilityConfiguration.forwardButtonVisibility;
  forwardButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_ACCNAME_FORWARD);
  forwardButton.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_ACCESSIBILITY_HINT_FORWARD);
  [forwardButton addTarget:self.actionHandler
                    action:@selector(forwardAction)
          forControlEvents:UIControlEventTouchUpInside];
  return forwardButton;
}

- (ToolbarTabGridButton*)tabGridButton {
  auto imageBlock = ^UIImage*(ToolbarTabGroupState tabGroupState) {
    switch (tabGroupState) {
      case ToolbarTabGroupState::kNormal:
        return CustomSymbolWithPointSize(kSquareNumberSymbol,
                                         kSymbolToolbarPointSize);
      case ToolbarTabGroupState::kTabGroup:
        return DefaultSymbolWithPointSize(kSquareFilledOnSquareSymbol,
                                          kSymbolToolbarPointSize);
    }
  };

  ToolbarTabGridButton* tabGridButton = [[ToolbarTabGridButton alloc]
      initWithTabGroupStateImageLoader:imageBlock];

  tabGridButton.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_ACCESSIBILITY_HINT_TAB_GRID);
  [self configureButton:tabGridButton width:kAdaptiveToolbarButtonWidth];
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

- (LegacyToolbarButton*)toolsMenuButton {
  auto loadImageBlock = ^UIImage* {
    return DefaultSymbolWithPointSize(kMenuSymbol, kSymbolToolbarPointSize);
  };
  UIColor* locationBarBackgroundColor =
      [self.toolbarConfiguration locationBarBackgroundColorWithVisibility:1];

  auto loadIPHHighlightedImageBlock = ^UIImage* {
    return SymbolWithPalette(
        CustomSymbolWithPointSize(kEllipsisSquareFillSymbol,
                                  kSymbolToolbarPointSize),
        @[ [UIColor colorNamed:kGrey600Color], locationBarBackgroundColor ]);
  };
  LegacyToolbarButton* toolsMenuButton = [[LegacyToolbarButton alloc]
            initWithImageLoader:loadImageBlock
      IPHHighlightedImageLoader:loadIPHHighlightedImageBlock];

  SetA11yLabelAndUiAutomationName(toolsMenuButton, IDS_IOS_TOOLBAR_SETTINGS,
                                  kLegacyToolbarToolsMenuButtonIdentifier);

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

- (LegacyToolbarButton*)shareButton {
  auto loadImageBlock = ^UIImage* {
    UIImage* image =
        DefaultSymbolWithPointSize(kShareSymbol, kSymbolToolbarPointSize);

    // The system share image has uneven vertical padding. Add a small bottom
    // padding to balance it.
    UIGraphicsImageRendererFormat* format =
        [UIGraphicsImageRendererFormat preferredFormat];
    format.scale = 0.0;
    format.opaque = NO;
    UIGraphicsImageRenderer* renderer = [[UIGraphicsImageRenderer alloc]
        initWithSize:CGSizeMake(
                         image.size.width,
                         image.size.height + kShareIconBalancingHeightPadding)
              format:format];

    return
        [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
          [image
              drawInRect:CGRectMake(0, 0, image.size.width, image.size.height)];
        }];
  };

  LegacyToolbarButton* shareButton =
      [[LegacyToolbarButton alloc] initWithImageLoader:loadImageBlock];

  [self configureButton:shareButton width:kAdaptiveToolbarButtonWidth];
  SetA11yLabelAndUiAutomationName(shareButton, IDS_IOS_TOOLS_MENU_SHARE,
                                  kLegacyToolbarShareButtonIdentifier);
  shareButton.titleLabel.text = @"Share";
  [shareButton addTarget:self.actionHandler
                  action:@selector(shareAction:)
        forControlEvents:UIControlEventTouchUpInside];
  shareButton.visibilityMask =
      self.visibilityConfiguration.shareButtonVisibility;
  return shareButton;
}

- (LegacyToolbarButton*)reloadButton {
  auto loadImageBlock = ^UIImage* {
    return CustomSymbolWithPointSize(kArrowClockWiseSymbol,
                                     kSymbolToolbarPointSize);
  };

  LegacyToolbarButton* reloadButton =
      [[LegacyToolbarButton alloc] initWithImageLoader:loadImageBlock];

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

- (LegacyToolbarButton*)stopButton {
  auto loadImageBlock = ^UIImage* {
    return DefaultSymbolWithPointSize(kXMarkSymbol, kSymbolToolbarPointSize);
  };

  LegacyToolbarButton* stopButton =
      [[LegacyToolbarButton alloc] initWithImageLoader:loadImageBlock];

  [self configureButton:stopButton width:kAdaptiveToolbarButtonWidth];
  stopButton.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_ACCNAME_STOP);
  [stopButton addTarget:self.actionHandler
                 action:@selector(stopAction)
       forControlEvents:UIControlEventTouchUpInside];
  stopButton.visibilityMask = self.visibilityConfiguration.stopButtonVisibility;
  return stopButton;
}

- (LegacyToolbarButton*)openNewTabButton {
  UIColor* locationBarBackgroundColor =
      [self.toolbarConfiguration locationBarBackgroundColorWithVisibility:1];
  UIColor* buttonsTintColorIPHHighlighted =
      self.toolbarConfiguration.buttonsTintColorIPHHighlighted;
  UIColor* buttonsIPHHighlightColor =
      self.toolbarConfiguration.buttonsIPHHighlightColor;

  auto loadImageBlock = ^UIImage* {
    return SymbolWithPalette(
        CustomSymbolWithPointSize(kPlusCircleFillSymbol,
                                  kSymbolToolbarPointSize),
        @[ [UIColor colorNamed:kGrey600Color], locationBarBackgroundColor ]);
  };

  auto loadIPHHighlightedImageBlock = ^UIImage* {
    return SymbolWithPalette(CustomSymbolWithPointSize(kPlusCircleFillSymbol,
                                                       kSymbolToolbarPointSize),
                             @[
                               // The color of the 'plus'.
                               buttonsTintColorIPHHighlighted,
                               // The filling color of the circle.
                               buttonsIPHHighlightColor,
                             ]);
  };

  LegacyToolbarButton* newTabButton = [[LegacyToolbarButton alloc]
            initWithImageLoader:loadImageBlock
      IPHHighlightedImageLoader:loadIPHHighlightedImageBlock];

  [newTabButton addTarget:self.actionHandler
                   action:@selector(newTabAction:)
         forControlEvents:UIControlEventTouchUpInside];

  [self configureButton:newTabButton width:kAdaptiveToolbarButtonWidth];

  newTabButton.accessibilityLabel = [self.toolbarConfiguration
      accessibilityLabelForOpenNewTabButtonInGroup:NO];
  newTabButton.accessibilityIdentifier = kToolbarNewTabButtonIdentifier;
  newTabButton.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_ACCESSIBILITY_HINT_NEW_TAB);

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

  cancelButton.hidden = YES;
  [cancelButton addTarget:self.actionHandler
                   action:@selector(cancelOmniboxFocusAction)
         forControlEvents:UIControlEventTouchUpInside];
  cancelButton.accessibilityIdentifier =
      kOmniboxCancelButtonAccessibilityIdentifier;
  return cancelButton;
}

#pragma mark - Helpers

// Sets the `button` width to `width` with a priority of
// UILayoutPriorityRequired - 1. If the priority is `UILayoutPriorityRequired`,
// there is a conflict when the buttons are hidden as the stack view is setting
// their width to 0. Setting the priority to UILayoutPriorityDefaultHigh doesn't
// work as they would have a lower priority than other elements.
- (void)configureButton:(LegacyToolbarButton*)button width:(CGFloat)width {
  NSLayoutConstraint* constraint =
      [button.widthAnchor constraintEqualToConstant:width];
  constraint.priority = UILayoutPriorityRequired - 1;
  constraint.active = YES;
  button.toolbarConfiguration = self.toolbarConfiguration;
  button.exclusiveTouch = YES;
  button.pointerInteractionEnabled = YES;
  if (IsGeminiCopresenceEnabled()) {
    button.geminiHandler = self.geminiHandler;
  }
  if (ios::provider::IsRaccoonEnabled()) {
    button.hoverStyle = [UIHoverStyle
        styleWithShape:[UIShape rectShapeWithCornerRadius:width / 4]];
  }
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
