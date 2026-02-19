// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/send_tab_to_self/ui/send_tab_promo_config.h"

#import "base/check_op.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/send_tab_to_self/ui/send_tab_promo_audience.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using l10n_util::GetNSString;

namespace {

// Size of the `fallbackSymbolImage` for the Send Tab Promo card.
const CGFloat kSymbolSize = 10;

// Accessibility ID for the view containing the Send Tab Promo card.
NSString* const kSendTabPromoViewID = @"kSendTabPromoViewID";

}  // namespace

@implementation SendTabPromoConfig

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  SendTabPromoConfig* config = [[super copyWithZone:zone] init];
  // The updates to properties must be reflected in the copy method.
  // LINT.IfChange(Copy)
  config.audience = self.audience;
  // LINT.ThenChange(send_tab_promo_config.h:Copy)
  return config;
}

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kSendTabPromo;
}

#pragma mark - StandaloneModuleViewConfig

- (UIImage*)fallbackSymbolImage {
  return DefaultSymbolWithPointSize(kIPhoneAndArrowForwardSymbol, kSymbolSize);
}

- (NSString*)titleText {
  return GetNSString(IDS_IOS_SEND_TAB_PROMO_TITLE);
}

- (NSString*)bodyText {
  return GetNSString(IDS_IOS_SEND_TAB_PROMO_BODY);
}

- (NSString*)buttonText {
  return GetNSString(IDS_IOS_SEND_TAB_PROMO_ALLOW_BUTTON);
}

- (NSString*)accessibilityIdentifier {
  return kSendTabPromoViewID;
}

#pragma mark - StandaloneModuleViewTapDelegate

- (void)buttonTappedForModuleType:(ContentSuggestionsModuleType)moduleType {
  CHECK_EQ(self.type, moduleType);
  [self.audience didSelectSendTabPromo];
}

@end
