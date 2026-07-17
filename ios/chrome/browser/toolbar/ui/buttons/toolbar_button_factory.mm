// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_factory.h"

#import "base/check.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_visibility.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_buttons_utils.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_tab_grid_badge_button.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Default point size for toolbar buttons.
constexpr CGFloat kDefaultSymbolPointSize = 19;
}  // namespace

@implementation ToolbarButtonFactory {
  BOOL _incognito;
}

- (instancetype)initWithIncognito:(BOOL)incognito {
  if ((self = [super init])) {
    _incognito = incognito;
  }
  return self;
}

- (ToolbarButton*)makeBackButton {
  ToolbarButton* button = [self toolbarButtonForImageNamed:kBackSymbol
                                              defaultImage:YES];
  button.visibilityMask = ToolbarButtonVisibility::kAlways;
  button.accessibilityIdentifier = kToolbarBackButtonIdentifier;
  button.accessibilityLabel = l10n_util::GetNSString(IDS_ACCNAME_BACK);
  button.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_ACCESSIBILITY_HINT_BACK);
  return button;
}

- (ToolbarButton*)makeForwardButton {
  ToolbarButton* button = [self toolbarButtonForImageNamed:kForwardSymbol
                                              defaultImage:YES];
  button.visibilityMask = ToolbarButtonVisibility::kWhenEnabled;
  button.accessibilityIdentifier = kToolbarForwardButtonIdentifier;
  button.accessibilityLabel = l10n_util::GetNSString(IDS_ACCNAME_FORWARD);
  button.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_ACCESSIBILITY_HINT_FORWARD);
  return button;
}

- (UIView*)makeConjoinedBackButton:(ToolbarButton*)backButton
                     forwardButton:(ToolbarButton*)forwardButton {
  CHECK(backButton);
  CHECK(forwardButton);
  UIView* buttonsContainer = [[UIView alloc] init];
  buttonsContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [buttonsContainer
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [buttonsContainer setContentHuggingPriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

  UIView* backgroundView = [[UIView alloc] init];
  backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  backgroundView.backgroundColor = ToolbarElementBackgroundColor(_incognito);
  [buttonsContainer addSubview:backgroundView];
  AddSameConstraints(backgroundView, buttonsContainer);

  // Internal stack view to handle dynamic resizing when the forward button
  // visibility changes.
  UIStackView* buttonsStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ backButton, forwardButton ]];
  buttonsStack.translatesAutoresizingMaskIntoConstraints = NO;
  buttonsStack.axis = UILayoutConstraintAxisHorizontal;
  buttonsStack.distribution = UIStackViewDistributionFill;
  buttonsStack.alignment = UIStackViewAlignmentFill;

  [backgroundView addSubview:buttonsStack];
  AddSameConstraints(buttonsStack, backgroundView);

  [NSLayoutConstraint activateConstraints:@[
    [buttonsContainer.heightAnchor
        constraintEqualToAnchor:backButton.heightAnchor]
  ]];

  ConfigureCornerRadiusForToolbarButtonContainer(
      backgroundView, buttonsContainer.traitCollection);
  backgroundView.clipsToBounds = YES;
  ConfigureShadowForToolbarElement(buttonsContainer);

  // Remove effects from the standalone buttons in the container
  ConfigureShadowForToolbarElement(backButton, /*remove_shadow*/ YES);
  ConfigureShadowForToolbarElement(forwardButton, /*remove_shadow*/ YES);

  [buttonsContainer
      registerForTraitChanges:
          @[ UITraitVerticalSizeClass.class, UITraitHorizontalSizeClass.class ]
                  withHandler:^(id<UITraitEnvironment>, UITraitCollection*) {
                    ConfigureCornerRadiusForToolbarButtonContainer(
                        backgroundView, buttonsContainer.traitCollection);
                  }];
  return buttonsContainer;
}

- (ToolbarButton*)makeReloadButton {
  ToolbarButton* button = [self toolbarButtonForImageNamed:kArrowClockWiseSymbol
                                              defaultImage:NO];
  button.visibilityMask = ToolbarButtonVisibility::kWideLayout;
  button.accessibilityIdentifier = kToolbarReloadButtonIdentifier;
  button.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_ACCNAME_RELOAD);
  return button;
}

- (ToolbarButton*)makeStopButton {
  ToolbarButton* button = [self toolbarButtonForImageNamed:kXMarkSymbol
                                              defaultImage:YES];
  button.visibilityMask = ToolbarButtonVisibility::kWideLayout;
  button.accessibilityIdentifier = kToolbarStopButtonIdentifier;
  button.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_ACCNAME_STOP);
  return button;
}

- (ToolbarButton*)makeShareButton {
  // Shift the button up 2px by adding 4px of padding at the bottom.
  UIImage* (^imageLoader)(void) = ^UIImage* {
    UIImage* image =
        DefaultSymbolWithPointSize(kShareSymbol, kDefaultSymbolPointSize);
    CGSize newSize = CGSizeMake(image.size.width, image.size.height + 4);

    UIGraphicsImageRendererFormat* format =
        [UIGraphicsImageRendererFormat preferredFormat];
    format.opaque = NO;
    format.scale = image.scale;
    UIGraphicsImageRenderer* renderer =
        [[UIGraphicsImageRenderer alloc] initWithSize:newSize format:format];

    UIImage* newImage = [renderer
        imageWithActions:^(UIGraphicsImageRendererContext* rendererContext) {
          [image
              drawInRect:CGRectMake(0, 0, image.size.width, image.size.height)];
        }];

    return [newImage imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  };

  ToolbarButton* button =
      [[ToolbarButton alloc] initWithImageLoader:imageLoader
                                       incognito:_incognito];
  button.visibilityMask = ToolbarButtonVisibility::kCompactHeight;
  button.accessibilityIdentifier = kToolbarShareButtonIdentifier;
  button.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_SHARE);
  return button;
}

- (ToolbarTabGridBadgeButton*)makeTabGridButton {
  ToolbarTabGridBadgeButton* button =
      [[ToolbarTabGridBadgeButton alloc] initWithImageLoader:nil
                                                   incognito:_incognito];
  button.visibilityMask = ToolbarButtonVisibility::kRegularRegular;
  button.accessibilityIdentifier = kToolbarTabGridButtonIdentifier;
  button.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_ACCESSIBILITY_HINT_TAB_GRID);
  return button;
}

- (ToolbarButton*)makeToolsMenuButton {
  ToolbarButton* button = [self toolbarButtonForImageNamed:kMenuSymbol
                                              defaultImage:YES];
  button.visibilityMask = ToolbarButtonVisibility::kAlways;
  button.accessibilityIdentifier = kToolbarToolsMenuButtonIdentifier;
  button.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_TOOLBAR_SETTINGS);
  return button;
}

- (ToolbarButton*)makeAssistantButton {
  /// TODO(crbug.com/493956100): Update the icon for the Assistant button in the
  /// toolbar.
  ToolbarButton* button =
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
      [self toolbarButtonForImageNamed:kGeminiBrandedLogoSymbol
                          defaultImage:NO];
#else
      [self toolbarButtonForImageNamed:kGeminiNonBrandedLogoSymbol
                          defaultImage:YES];
#endif
  button.visibilityMask = ToolbarButtonVisibility::kRegularRegular;
  button.accessibilityIdentifier = kToolbarAssistantButtonIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_APP_BAR_ASK_GEMINI);
  return button;
}

#pragma mark - Private

// Returns a toolbar button with the given image, which can be a default symbol
// or not.
- (ToolbarButton*)toolbarButtonForImageNamed:(NSString*)imageName
                                defaultImage:(BOOL)defaultImage {
  if (defaultImage) {
    return [[ToolbarButton alloc]
        initWithImageLoader:^UIImage* {
          return DefaultSymbolWithPointSize(imageName, kDefaultSymbolPointSize);
        }
                  incognito:_incognito];
  }
  return [[ToolbarButton alloc]
      initWithImageLoader:^UIImage* {
        return CustomSymbolWithPointSize(imageName, kDefaultSymbolPointSize);
      }
                incognito:_incognito];
}

@end
