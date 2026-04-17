// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_content_entry_point.h"

#import "base/check.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The size of icons displayed in the footer.
const CGFloat kIconSize = 20;

}  // namespace

@implementation PageActionMenuContentEntryPoint

- (instancetype)initWithEnabled:(BOOL)enabled {
  return [self initWithEnabled:enabled footerItem:nil];
}

- (instancetype)initWithEnabled:(BOOL)enabled
                     footerItem:(ContentEntryPointUnavailabilityItem*)item {
  self = [super init];
  if (self) {
    _enabled = enabled;
    _unavailabilityItem = item;
  }
  return self;
}

@end

@implementation ContentEntryPointUnavailabilityItem

- (instancetype)initWithText:(NSString*)text {
  return [self initWithText:text icon:nil actionIdentifier:nil];
}

- (instancetype)initWithText:(NSString*)text icon:(UIImage*)icon {
  return [self initWithText:text icon:icon actionIdentifier:nil];
}

- (instancetype)initWithText:(NSString*)text
                        icon:(UIImage*)icon
            actionIdentifier:(NSString*)actionIdentifier {
  CHECK(text);
  self = [super init];
  if (self) {
    _text = [text copy];
    _icon = icon;
    _actionIdentifier = [actionIdentifier copy];
  }
  return self;
}

+ (instancetype)geminiEnterprise {
  NSString* text = l10n_util::GetNSString(
      IDS_IOS_AI_HUB_GEMINI_UNAVAILABLE_ENTERPRISE_LABEL);
  UIImage* icon = [CustomSymbolWithPointSize(kEnterpriseSymbol, kIconSize)
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  return [[ContentEntryPointUnavailabilityItem alloc] initWithText:text
                                                              icon:icon];
}

+ (instancetype)lensEnterprise {
  NSString* text =
      l10n_util::GetNSString(IDS_IOS_AI_HUB_LENS_UNAVAILABLE_ENTERPRISE_LABEL);
  UIImage* icon = [CustomSymbolWithPointSize(kEnterpriseSymbol, kIconSize)
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  return [[ContentEntryPointUnavailabilityItem alloc] initWithText:text
                                                              icon:icon];
}

+ (instancetype)lensSearchEngine {
  NSString* text = l10n_util::GetNSString(
      IDS_IOS_AI_HUB_LENS_UNAVAILABLE_SEARCH_ENGINE_LABEL);
  return [[ContentEntryPointUnavailabilityItem alloc]
          initWithText:text
                  icon:nil
      actionIdentifier:kSearchEngineSettingsActionIdentifier];
}
@end
