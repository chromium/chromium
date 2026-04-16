// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_factory.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_visibility.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_buttons_utils.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// Default point size for toolbar buttons.
constexpr CGFloat kDefaultSymbolPointSize = 19;
}  // namespace

@implementation ToolbarButtonFactory

- (ToolbarButton*)makeBackButton {
  ToolbarButton* button = [self toolbarButtonForImageNamed:kBackSymbol
                                              defaultImage:YES];
  button.visibilityMask = ToolbarButtonVisibility::kAlways;
  button.accessibilityIdentifier = kToolbarBackButtonIdentifier;
  return button;
}

- (ToolbarButton*)makeForwardButton {
  ToolbarButton* button = [self toolbarButtonForImageNamed:kForwardSymbol
                                              defaultImage:YES];
  button.visibilityMask = ToolbarButtonVisibility::kWhenEnabled;
  button.accessibilityIdentifier = kToolbarForwardButtonIdentifier;
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

  // Internal stack view to handle dynamic resizing when the forward button
  // visibility changes.
  UIStackView* buttonsStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ backButton, forwardButton ]];
  buttonsStack.translatesAutoresizingMaskIntoConstraints = NO;
  buttonsStack.axis = UILayoutConstraintAxisHorizontal;
  buttonsStack.distribution = UIStackViewDistributionFill;
  buttonsStack.alignment = UIStackViewAlignmentFill;

  [buttonsContainer addSubview:buttonsStack];
  AddSameConstraints(buttonsStack, buttonsContainer);

  backButton.translatesAutoresizingMaskIntoConstraints = NO;
  forwardButton.translatesAutoresizingMaskIntoConstraints = NO;

  [NSLayoutConstraint activateConstraints:@[
    [buttonsContainer.heightAnchor
        constraintEqualToAnchor:backButton.heightAnchor]
  ]];

  buttonsContainer.backgroundColor = ToolbarButtonColor();
  ConfigureCornerRadiusForToolbarButtonContainer(
      buttonsContainer, buttonsContainer.traitCollection);
  buttonsContainer.clipsToBounds = YES;
  buttonsContainer.layer.masksToBounds = YES;
  ConfigureShadowForToolbarButton(buttonsContainer);

  backButton.backgroundColor = [UIColor clearColor];
  forwardButton.backgroundColor = [UIColor clearColor];

  [buttonsContainer
      registerForTraitChanges:
          @[ UITraitVerticalSizeClass.class, UITraitHorizontalSizeClass.class ]
                  withHandler:^(id<UITraitEnvironment>, UITraitCollection*) {
                    ConfigureCornerRadiusForToolbarButtonContainer(
                        buttonsContainer, buttonsContainer.traitCollection);
                  }];
  return buttonsContainer;
}

- (ToolbarButton*)makeReloadButton {
  ToolbarButton* button = [self toolbarButtonForImageNamed:kArrowClockWiseSymbol
                                              defaultImage:NO];
  button.visibilityMask = ToolbarButtonVisibility::kWideLayout;
  button.accessibilityIdentifier = kToolbarReloadButtonIdentifier;
  return button;
}

- (ToolbarButton*)makeStopButton {
  ToolbarButton* button = [self toolbarButtonForImageNamed:kXMarkSymbol
                                              defaultImage:YES];
  button.visibilityMask = ToolbarButtonVisibility::kWideLayout;
  button.accessibilityIdentifier = kToolbarStopButtonIdentifier;
  return button;
}

- (ToolbarButton*)makeShareButton {
  ToolbarButton* button = [self toolbarButtonForImageNamed:kShareSymbol
                                              defaultImage:YES];
  button.visibilityMask = ToolbarButtonVisibility::kCompactHeight;
  button.accessibilityIdentifier = kToolbarShareButtonIdentifier;
  return button;
}

- (ToolbarButton*)makeTabGridButton {
  ToolbarButton* button = [self toolbarButtonForImageNamed:kSquareNumberSymbol
                                              defaultImage:NO];
  button.visibilityMask = ToolbarButtonVisibility::kRegularRegular;
  button.accessibilityIdentifier = kToolbarTabGridButtonIdentifier;
  return button;
}

- (ToolbarButton*)makeToolsMenuButton {
  ToolbarButton* button = [self toolbarButtonForImageNamed:kMenuSymbol
                                              defaultImage:YES];
  button.visibilityMask = ToolbarButtonVisibility::kAlways;
  button.accessibilityIdentifier = kToolbarToolsMenuButtonIdentifier;
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
  return button;
}

#pragma mark - Private

// Returns a toolbar button with the given image, which can be a default symbol
// or not.
- (ToolbarButton*)toolbarButtonForImageNamed:(NSString*)imageName
                                defaultImage:(BOOL)defaultImage {
  if (defaultImage) {
    return [[ToolbarButton alloc] initWithImageLoader:^UIImage* {
      return DefaultSymbolWithPointSize(imageName, kDefaultSymbolPointSize);
    }];
  }
  return [[ToolbarButton alloc] initWithImageLoader:^UIImage* {
    return CustomSymbolWithPointSize(imageName, kDefaultSymbolPointSize);
  }];
}

@end
