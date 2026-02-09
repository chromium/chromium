// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_factory.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_visibility.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_constants.h"

namespace {
// Default point size for toolbar buttons.
constexpr CGFloat kDefaultSymbolPointSize = 22;
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

- (ToolbarButton*)makeReloadButton {
  ToolbarButton* button = [self toolbarButtonForImageNamed:kArrowClockWiseSymbol
                                              defaultImage:NO];
  button.visibilityMask = ToolbarButtonVisibility::kRegularRegular;
  button.accessibilityIdentifier = kToolbarReloadButtonIdentifier;
  return button;
}

- (ToolbarButton*)makeStopButton {
  ToolbarButton* button = [self toolbarButtonForImageNamed:kXMarkSymbol
                                              defaultImage:YES];
  button.visibilityMask = ToolbarButtonVisibility::kRegularRegular;
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
