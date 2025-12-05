// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_button_factory.h"

#import "base/ios/ios_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/omnibox/public/omnibox_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_button.h"
#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_button_actions_handler.h"
#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_button_visibility_configuration.h"
#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_tab_grid_button.h"
#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_tab_group_state.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ios/public/provider/chrome/browser/raccoon/raccoon_api.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The size of the symbol image.
const CGFloat kSymbolToolbarPointSize = 24;

// The size of the symbol image with Diamond.
const CGFloat kDiamondSymbolSize = 18;

// The padding to be added to the bottom of the system share icon to balance
// the white space on top.
const CGFloat kShareIconBalancingHeightPadding = 1;

// Size of the button with diamond enabled.
const CGFloat kDiamondButtonSize = 38;
// Alpha of the tint color of the button.
const CGFloat kDiamondTintAlpha = 0.9;
// Corner radius of the button with diamond.
const CGFloat kDiamondCornerRadius = 13;
/// The size for the close button.
const CGFloat kCloseButtonSize = 30.0f;
/// The alpha for the close button.
const CGFloat kCloseButtonAlpha = 0.6f;

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
  auto loadImageBlock = ^UIImage* {
    UIImage* backImage = DefaultSymbolWithPointSize(
        kBackSymbol, IsDiamondPrototypeEnabled() ? kDiamondSymbolSize
                                                 : kSymbolToolbarPointSize);
    return [backImage imageFlippedForRightToLeftLayoutDirection];
  };

  ToolbarButton* backButton =
      [[ToolbarButton alloc] initWithImageLoader:loadImageBlock];

  if (IsDiamondPrototypeEnabled()) {
    [self configureButton:backButton width:kDiamondButtonSize];
    backButton.tintColor = [UIColor colorNamed:kGrey700Color];
  } else {
    [self configureButton:backButton width:kAdaptiveToolbarButtonWidth];
  }
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
- (ToolbarButton*)forwardButton {
  auto loadImageBlock = ^UIImage* {
    UIImage* forwardImage = DefaultSymbolWithPointSize(
        kForwardSymbol, IsDiamondPrototypeEnabled() ? kDiamondSymbolSize
                                                    : kSymbolToolbarPointSize);
    return [forwardImage imageFlippedForRightToLeftLayoutDirection];
  };

  ToolbarButton* forwardButton =
      [[ToolbarButton alloc] initWithImageLoader:loadImageBlock];

  if (IsDiamondPrototypeEnabled()) {
    [self configureButton:forwardButton width:kDiamondButtonSize];
    forwardButton.tintColor = [UIColor colorNamed:kGrey700Color];
  } else {
    [self configureButton:forwardButton width:kAdaptiveToolbarButtonWidth];
  }
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

- (ToolbarButton*)toolsMenuButton {
  auto loadImageBlock = ^UIImage* {
    return DefaultSymbolWithPointSize(
        kMenuSymbol, IsDiamondPrototypeEnabled() ? kDiamondSymbolSize
                                                 : kSymbolToolbarPointSize);
  };
  UIColor* locationBarBackgroundColor =
      [self.toolbarConfiguration locationBarBackgroundColorWithVisibility:1];

  auto loadIPHHighlightedImageBlock = ^UIImage* {
    return SymbolWithPalette(
        CustomSymbolWithPointSize(kEllipsisSquareFillSymbol,
                                  kSymbolToolbarPointSize),
        @[ [UIColor colorNamed:kGrey600Color], locationBarBackgroundColor ]);
  };
  ToolbarButton* toolsMenuButton =
      [[ToolbarButton alloc] initWithImageLoader:loadImageBlock
                       IPHHighlightedImageLoader:loadIPHHighlightedImageBlock];

  SetA11yLabelAndUiAutomationName(toolsMenuButton, IDS_IOS_TOOLBAR_SETTINGS,
                                  kToolbarToolsMenuButtonIdentifier);
  if (IsDiamondPrototypeEnabled()) {
    [self configureButton:toolsMenuButton width:kDiamondButtonSize];
    [toolsMenuButton.heightAnchor constraintEqualToConstant:kDiamondButtonSize]
        .active = YES;
    toolsMenuButton.tintColor =
        [[UIColor colorNamed:kSolidBlackColor] colorWithAlphaComponent:0.9];

  } else {
    [self configureButton:toolsMenuButton width:kAdaptiveToolbarButtonWidth];
    [toolsMenuButton.heightAnchor
        constraintEqualToConstant:kAdaptiveToolbarButtonWidth]
        .active = YES;
  }
  [toolsMenuButton addTarget:self.actionHandler
                      action:@selector(toolsMenuAction)
            forControlEvents:UIControlEventTouchUpInside];
  if (IsDiamondPrototypeEnabled()) {
    toolsMenuButton.tintColor = [[UIColor colorNamed:kSolidBlackColor]
        colorWithAlphaComponent:kDiamondTintAlpha];
    toolsMenuButton.backgroundColor =
        [UIColor colorNamed:kTextfieldBackgroundColor];
    toolsMenuButton.layer.cornerRadius = kDiamondCornerRadius;
    if (self.visibilityConfiguration.type == ToolbarType::kPrimary) {
      toolsMenuButton.visibilityMask = ToolbarComponentVisibilityNone;
    } else {
      toolsMenuButton.visibilityMask = ToolbarComponentVisibilityAlways;
    }
  } else {
    toolsMenuButton.visibilityMask =
        self.visibilityConfiguration.toolsMenuButtonVisibility;
  }
  return toolsMenuButton;
}

- (ToolbarButton*)shareButton {
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

  ToolbarButton* shareButton =
      [[ToolbarButton alloc] initWithImageLoader:loadImageBlock];

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
  auto loadImageBlock = ^UIImage* {
    return CustomSymbolWithPointSize(kArrowClockWiseSymbol,
                                     kSymbolToolbarPointSize);
  };

  ToolbarButton* reloadButton =
      [[ToolbarButton alloc] initWithImageLoader:loadImageBlock];

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
  auto loadImageBlock = ^UIImage* {
    return DefaultSymbolWithPointSize(kXMarkSymbol, kSymbolToolbarPointSize);
  };

  ToolbarButton* stopButton =
      [[ToolbarButton alloc] initWithImageLoader:loadImageBlock];

  [self configureButton:stopButton width:kAdaptiveToolbarButtonWidth];
  stopButton.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_ACCNAME_STOP);
  [stopButton addTarget:self.actionHandler
                 action:@selector(stopAction)
       forControlEvents:UIControlEventTouchUpInside];
  stopButton.visibilityMask = self.visibilityConfiguration.stopButtonVisibility;
  return stopButton;
}

- (ToolbarButton*)openNewTabButton {
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

  ToolbarButton* newTabButton =
      [[ToolbarButton alloc] initWithImageLoader:loadImageBlock
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

- (ToolbarButton*)diamondPrototypeButton {
  CHECK(IsDiamondPrototypeEnabled());

  auto loadImageBlock = ^UIImage* {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
    return MakeSymbolMonochrome(
        CustomSymbolWithPointSize(kCameraLensSymbol, kDiamondSymbolSize));
#endif
    return nil;
  };

  auto loadIPHHighlightedImageBlock = ^UIImage* {
    NOTREACHED();
  };

  ToolbarButton* diamondPrototypeButton =
      [[ToolbarButton alloc] initWithImageLoader:loadImageBlock
                       IPHHighlightedImageLoader:loadIPHHighlightedImageBlock];

  [diamondPrototypeButton addTarget:self.actionHandler
                             action:@selector(diamondPrototypeAction:)
                   forControlEvents:UIControlEventTouchUpInside];

  diamondPrototypeButton.tintColor = [[UIColor colorNamed:kSolidBlackColor]
      colorWithAlphaComponent:kDiamondTintAlpha];
  diamondPrototypeButton.backgroundColor =
      [UIColor colorNamed:kTextfieldBackgroundColor];
  diamondPrototypeButton.layer.cornerRadius = kDiamondCornerRadius;
  diamondPrototypeButton.visibilityMask = ToolbarComponentVisibilityAlways;

  [self configureButton:diamondPrototypeButton width:kDiamondButtonSize];
  [diamondPrototypeButton.heightAnchor
      constraintEqualToConstant:kDiamondButtonSize]
      .active = YES;

  diamondPrototypeButton.tintColor =
      [[UIColor colorNamed:kSolidBlackColor] colorWithAlphaComponent:0.9];

  diamondPrototypeButton.visibilityMask = ToolbarComponentVisibilityAlways;
  return diamondPrototypeButton;
}

- (UIButton*)cancelButton {
  return [self cancelButtonWithStyle:ToolbarCancelButtonStyle::kCancelLabel];
}

- (UIButton*)cancelButtonWithStyle:(ToolbarCancelButtonStyle)style {
  UIButton* cancelButton = [UIButton buttonWithType:UIButtonTypeSystem];
  cancelButton.tintColor = [UIColor colorNamed:kBlueColor];
  [cancelButton setContentHuggingPriority:UILayoutPriorityRequired
                                  forAxis:UILayoutConstraintAxisHorizontal];
  [cancelButton
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

  if (style == ToolbarCancelButtonStyle::kXCircle) {
    UIImageSymbolConfiguration* symbolConfiguration =
        [UIImageSymbolConfiguration
            configurationWithPointSize:kCloseButtonSize
                                weight:UIImageSymbolWeightRegular
                                 scale:UIImageSymbolScaleMedium];
    UIImage* buttonImage =
        SymbolWithPalette(DefaultSymbolWithConfiguration(kXMarkCircleFillSymbol,
                                                         symbolConfiguration),
                          @[
                            [[UIColor tertiaryLabelColor]
                                colorWithAlphaComponent:kCloseButtonAlpha],
                            [UIColor tertiarySystemFillColor]
                          ]);
    [cancelButton setImage:buttonImage forState:UIControlStateNormal];
  } else {
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
  }

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
- (void)configureButton:(ToolbarButton*)button width:(CGFloat)width {
  NSLayoutConstraint* constraint =
      [button.widthAnchor constraintEqualToConstant:width];
  constraint.priority = UILayoutPriorityRequired - 1;
  constraint.active = YES;
  button.toolbarConfiguration = self.toolbarConfiguration;
  button.exclusiveTouch = YES;
  button.pointerInteractionEnabled = YES;
  if (ios::provider::IsRaccoonEnabled()) {
    if (@available(iOS 17.0, *)) {
      button.hoverStyle = [UIHoverStyle
          styleWithShape:[UIShape rectShapeWithCornerRadius:width / 4]];
    }
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
