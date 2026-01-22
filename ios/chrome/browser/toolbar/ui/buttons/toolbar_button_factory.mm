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
  ToolbarButton* button = [[ToolbarButton alloc] initWithImageLoader:^UIImage* {
    return DefaultSymbolWithPointSize(kBackSymbol, kDefaultSymbolPointSize);
  }];
  button.visibilityMask = ToolbarButtonVisibility::kAlways;
  button.accessibilityIdentifier = kToolbarBackButtonIdentifier;
  return button;
}

- (ToolbarButton*)makeForwardButton {
  ToolbarButton* button = [[ToolbarButton alloc] initWithImageLoader:^UIImage* {
    return DefaultSymbolWithPointSize(kForwardSymbol, kDefaultSymbolPointSize);
  }];
  button.visibilityMask = ToolbarButtonVisibility::kWhenEnabled;
  button.accessibilityIdentifier = kToolbarForwardButtonIdentifier;
  return button;
}

- (ToolbarButton*)makeReloadButton {
  ToolbarButton* button = [[ToolbarButton alloc] initWithImageLoader:^UIImage* {
    return DefaultSymbolWithPointSize(kArrowClockWiseSymbol,
                                      kDefaultSymbolPointSize);
  }];
  button.visibilityMask = ToolbarButtonVisibility::kRegularRegular;
  button.accessibilityIdentifier = kToolbarReloadButtonIdentifier;
  return button;
}

- (ToolbarButton*)makeStopButton {
  ToolbarButton* button = [[ToolbarButton alloc] initWithImageLoader:^UIImage* {
    return DefaultSymbolWithPointSize(kXMarkSymbol, kDefaultSymbolPointSize);
  }];
  button.visibilityMask = ToolbarButtonVisibility::kRegularRegular;
  button.accessibilityIdentifier = kToolbarStopButtonIdentifier;
  return button;
}

- (ToolbarButton*)makeShareButton {
  ToolbarButton* button = [[ToolbarButton alloc] initWithImageLoader:^UIImage* {
    return DefaultSymbolWithPointSize(kShareSymbol, kDefaultSymbolPointSize);
  }];
  button.visibilityMask = ToolbarButtonVisibility::kCompactHeight;
  button.accessibilityIdentifier = kToolbarShareButtonIdentifier;
  return button;
}

- (ToolbarButton*)makeTabGridButton {
  ToolbarButton* button = [[ToolbarButton alloc] initWithImageLoader:^UIImage* {
    return CustomSymbolWithPointSize(kSquareNumberSymbol,
                                     kDefaultSymbolPointSize);
  }];
  button.visibilityMask = ToolbarButtonVisibility::kRegularRegular;
  button.accessibilityIdentifier = kToolbarTabGridButtonIdentifier;
  return button;
}

- (ToolbarButton*)makeToolsMenuButton {
  ToolbarButton* button = [[ToolbarButton alloc] initWithImageLoader:^UIImage* {
    return DefaultSymbolWithPointSize(kMenuSymbol, kDefaultSymbolPointSize);
  }];
  button.visibilityMask = ToolbarButtonVisibility::kAlways;
  button.accessibilityIdentifier = kToolbarToolsMenuButtonIdentifier;
  return button;
}

- (ToolbarButton*)makeAssistantButton {
  ToolbarButton* button = [[ToolbarButton alloc] initWithImageLoader:^UIImage* {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
    return CustomSymbolWithPointSize(kGeminiBrandedLogoSymbol,
                                     kDefaultSymbolPointSize);
#else
    return DefaultSymbolWithPointSize(kGeminiNonBrandedLogoSymbol,
                                          kDefaultSymbolPointSize);
#endif
  }];
  button.visibilityMask = ToolbarButtonVisibility::kRegularRegular;
  button.accessibilityIdentifier = kToolbarAssistantButtonIdentifier;
  return button;
}

@end
