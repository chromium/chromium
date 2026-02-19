// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/default_browser/ui/default_browser_config.h"

#import "ios/chrome/browser/content_suggestions/default_browser/ui/default_browser_commands.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/icon_view_configuration.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ui/base/l10n/l10n_util.h"

using l10n_util::GetNSString;

namespace {

// `DefaultBrowserView` accessibility ID.
NSString* const kDefaultBrowserViewAccessibilityId =
    @"DefaultBrowserViewAccessibilityId";

//  Constants for the icon container view.
constexpr CGFloat kIconSize = 40;

}  // namespace

@implementation DefaultBrowserConfig

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  DefaultBrowserConfig* config = [[super copyWithZone:zone] init];
  config.defaultBrowserHandler = self.defaultBrowserHandler;
  return config;
}

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kDefaultBrowser;
}

#pragma mark - IconDetailViewConfig

- (NSString*)titleText {
  return GetNSString(IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_TITLE);
}

- (NSString*)descriptionText {
  return GetNSString(
      IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_MAGIC_STACK_DESCRIPTION);
}

- (IconDetailViewLayoutType)layoutType {
  return IconDetailViewLayoutType::kHero;
}

- (NSString*)accessibilityIdentifier {
  return kDefaultBrowserViewAccessibilityId;
}

- (NSString*)iconName {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  return kMulticolorChromeballSymbol;
#else
  return kChromeProductSymbol;
#endif  // BUILDFLAG(IOS_USE_BRANDED_ASSETS)
}

- (IconViewSourceType)iconSource {
  return IconViewSourceType::kSymbol;
}

- (UIColor*)symbolBackgroundColor {
  return [UIColor clearColor];
}

- (CGFloat)iconWidth {
  return kIconSize;
}

#pragma mark - IconDetailViewTapDelegate

- (void)didTapIconDetailView:(IconDetailView*)view {
  [self.defaultBrowserHandler didTapDefaultBrowserPromo];
}

@end
