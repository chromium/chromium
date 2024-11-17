// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/send_tab_to_self/send_tab_promo_item.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
const CGFloat kSymbolSize = 10;
}  // namespace

@implementation SendTabPromoItem

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kSendTabPromo;
}

#pragma mark - StandaloneModuleItem

- (UIImage*)fallbackSymbolImage {
  return DefaultSymbolWithPointSize(kIPhoneAndArrowForwardSymbol, kSymbolSize);
}

- (NSString*)titleText {
  return l10n_util::GetNSString(IDS_IOS_SEND_TAB_PROMO_TITLE);
}

- (NSString*)bodyText {
  return l10n_util::GetNSString(IDS_IOS_SEND_TAB_PROMO_BODY);
}

- (NSString*)buttonText {
  return l10n_util::GetNSString(IDS_IOS_SEND_TAB_PROMO_ALLOW_BUTTON);
}

@end
