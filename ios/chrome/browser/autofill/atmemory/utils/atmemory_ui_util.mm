// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/utils/atmemory_ui_util.h"

#import <variant>

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/ios/browser/autofill_client_ios.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_constants.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_item.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

using autofill::AutofillClientIOS;
using autofill::Suggestion;
using autofill::SuggestionType;

AtMemoryFailedToOpenReason GetAtMemoryFailedToOpenReason(Browser* browser) {
  if (!browser || !browser->GetWebStateList()) {
    return AtMemoryFailedToOpenReason::kNoBrowser;
  }

  web::WebState* web_state = browser->GetWebStateList()->GetActiveWebState();
  if (!web_state) {
    return AtMemoryFailedToOpenReason::kNoActiveWebState;
  }

  AutofillClientIOS* autofill_client =
      AutofillClientIOS::FromWebState(web_state);
  if (!autofill_client) {
    return AtMemoryFailedToOpenReason::kNoAutofillClient;
  }

  if (!autofill_client->GetAtMemoryManager()) {
    return AtMemoryFailedToOpenReason::kNoAtMemoryManager;
  }

  if (!autofill_client->GetAutofillManagerForPrimaryMainFrame()) {
    return AtMemoryFailedToOpenReason::kNoAutofillManager;
  }

  return AtMemoryFailedToOpenReason::kNone;
}

NSString* GetAtMemoryGranularFillTitle(const Suggestion& suggestion) {
  const Suggestion::AtMemoryPayload* payload =
      std::get_if<Suggestion::AtMemoryPayload>(&suggestion.payload);
  if (!payload) {
    return @"";
  }
  return base::SysUTF16ToNSString(payload->type_name);
}

NSArray<AtMemoryGranularFillItem*>* AtMemoryGranularFillItemsForSuggestion(
    const Suggestion& suggestion) {
  NSMutableArray<AtMemoryGranularFillItem*>* items =
      [[NSMutableArray alloc] init];

  for (size_t i = 0; i < suggestion.children.size(); ++i) {
    const Suggestion& child = suggestion.children[i];
    if (child.type != SuggestionType::kAtMemorySearchResult) {
      continue;
    }
    const Suggestion::AtMemoryPayload* child_payload =
        std::get_if<Suggestion::AtMemoryPayload>(&child.payload);
    if (!child_payload || child_payload->value.empty()) {
      continue;
    }
    [items addObject:[[AtMemoryGranularFillItem alloc]
                         initWithAttributeName:base::SysUTF16ToNSString(
                                                   child_payload->type_name)
                                attributeValue:base::SysUTF16ToNSString(
                                                   child_payload->value)
                                         index:static_cast<NSInteger>(i)]];
  }

  return [items copy];
}

NSString* GetAtMemoryGranularFillCellAccessibilityIdentifier(
    NSString* attribute_name) {
  return [NSString
      stringWithFormat:@"%@%@",
                       kAtMemoryGranularFillCellAccessibilityIdentifierPrefix,
                       attribute_name];
}

NSString* GetAtMemoryGranularFillAttributeLabelAccessibilityIdentifier(
    NSString* attribute_name) {
  return [NSString
      stringWithFormat:
          @"%@%@",
          kAtMemoryGranularFillAttributeLabelAccessibilityIdentifierPrefix,
          attribute_name];
}

NSString* GetAtMemoryGranularFillChipButtonAccessibilityIdentifier(
    NSString* attribute_name) {
  return [NSString
      stringWithFormat:
          @"%@%@", kAtMemoryGranularFillChipButtonAccessibilityIdentifierPrefix,
          attribute_name];
}

NSString* GetAtMemorySearchResultCellAccessibilityIdentifier(NSString* title) {
  return [NSString
      stringWithFormat:@"%@%@",
                       kAtMemorySearchResultCellAccessibilityIdentifierPrefix,
                       title];
}

NSString* GetAtMemorySearchResultInfoButtonAccessibilityIdentifier(
    NSString* title) {
  return [NSString
      stringWithFormat:
          @"%@%@", kAtMemorySearchResultInfoButtonAccessibilityIdentifierPrefix,
          title];
}
