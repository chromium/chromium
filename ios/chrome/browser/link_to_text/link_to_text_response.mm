// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/link_to_text_response.h"

#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/shared_highlighting/core/common/text_fragment.h"
#import "components/shared_highlighting/core/common/text_fragments_utils.h"
#import "ios/chrome/browser/link_to_text/link_to_text_payload.h"
#import "ios/chrome/browser/link_to_text/link_to_text_utils.h"
#import "ios/chrome/browser/tabs/tab_title_util.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using shared_highlighting::LinkGenerationError;
using shared_highlighting::TextFragment;

@interface LinkToTextResponse ()

// Initializes an object with the given |payload| of the link generation
// request.
- (instancetype)initWithPayload:(LinkToTextPayload*)payload
    NS_DESIGNATED_INITIALIZER;

// Initializes an object with the given |error| which occurred while trying to
// generate a link.
- (instancetype)initWithError:(LinkGenerationError)error;

@end

@implementation LinkToTextResponse

- (instancetype)initWithPayload:(LinkToTextPayload*)payload {
  if (self = [super init]) {
    // Payload may be nil in cases of link generation error.
    _payload = payload;
  }
  return self;
}

- (instancetype)initWithError:(LinkGenerationError)error {
  if (self = [self initWithPayload:nil]) {
    _error = error;
  }
  return self;
}

+ (instancetype)createFromValue:(const base::Value*)value
                       webState:(web::WebState*)webState {
  if (!webState || !link_to_text::IsValidDictValue(value)) {
    return [LinkToTextResponse unknownError];
  }

  base::Optional<LinkGenerationOutcome> outcome =
      link_to_text::ParseStatus(value->FindDoubleKey("status"));
  if (!outcome.has_value()) {
    return [LinkToTextResponse unknownError];
  }

  if (outcome.value() != LinkGenerationOutcome::kSuccess) {
    // Convert to Error.
    return [[LinkToTextResponse alloc]
        initWithError:link_to_text::OutcomeToError(outcome.value())];
  }

  // Attempts to parse a payload from the response.
  NSString* title = tab_util::GetTabTitle(webState);
  base::Optional<TextFragment> fragment =
      TextFragment::FromValue(value->FindKey("fragment"));
  const std::string* selectedText = value->FindStringKey("selectedText");
  base::Optional<CGRect> sourceRect =
      link_to_text::ParseRect(value->FindKey("selectionRect"));

  // All values must be present to have a valid payload.
  if (!title || !fragment || !selectedText || !sourceRect) {
    // Library replied Success but some values are missing.
    return [LinkToTextResponse unknownError];
  }

  // Create the deep-link.
  GURL deep_link = shared_highlighting::AppendFragmentDirectives(
      webState->GetLastCommittedURL(), {fragment.value()});

  LinkToTextPayload* payload = [[LinkToTextPayload alloc]
       initWithURL:deep_link
             title:title
      selectedText:base::SysUTF8ToNSString(*selectedText)
        sourceView:webState->GetView()
        sourceRect:link_to_text::ConvertToBrowserRect(sourceRect.value(),
                                                      webState)];
  return [[LinkToTextResponse alloc] initWithPayload:payload];
}

#pragma mark - Private

+ (instancetype)unknownError {
  return
      [[LinkToTextResponse alloc] initWithError:LinkGenerationError::kUnknown];
}

@end
