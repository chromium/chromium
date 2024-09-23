// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/model/link_to_text_response.h"

#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/shared_highlighting/core/common/fragment_directives_utils.h"
#import "components/shared_highlighting/core/common/text_fragment.h"
#import "components/shared_highlighting/ios/parsing_utils.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/link_to_text/model/link_to_text_payload.h"
#import "ios/chrome/browser/link_to_text/model/link_to_text_utils.h"
#import "ios/chrome/browser/tabs/model/tab_title_util.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"

using shared_highlighting::LinkGenerationError;
using shared_highlighting::TextFragment;

@interface LinkToTextResponse ()

// Initializes an object with a `sourceID` representing the current WebState,
// along with the `latency` for link generation.
- (instancetype)initWithSourceID:(ukm::SourceId)sourceID
                         latency:(base::TimeDelta)latency
    NS_DESIGNATED_INITIALIZER;

// Initializes an object with the given `payload` of the link generation
// request, a `sourceID` representing the current WebState and the `latency` for
// link generation.
- (instancetype)initWithPayload:(LinkToTextPayload*)payload
                       sourceID:(ukm::SourceId)sourceID
                        latency:(base::TimeDelta)latency;

// Initializes an object with the given `error` which occurred while trying to
// generate a link, a `sourceID` representing the current WebState and the
// `latency` for link generation.
- (instancetype)initWithError:(LinkGenerationError)error
                     sourceID:(ukm::SourceId)sourceID
                      latency:(base::TimeDelta)latency;

@end

@implementation LinkToTextResponse

- (instancetype)initWithSourceID:(ukm::SourceId)sourceID
                         latency:(base::TimeDelta)latency {
  if ((self = [super init])) {
    _sourceID = sourceID;
    _latency = latency;
  }
  return self;
}

- (instancetype)initWithPayload:(LinkToTextPayload*)payload
                       sourceID:(ukm::SourceId)sourceID
                        latency:(base::TimeDelta)latency {
  DCHECK(payload);
  DCHECK(sourceID != ukm::kInvalidSourceId);
  if ((self = [self initWithSourceID:sourceID latency:latency])) {
    _payload = payload;
  }
  return self;
}

- (instancetype)initWithError:(LinkGenerationError)error
                     sourceID:(ukm::SourceId)sourceID
                      latency:(base::TimeDelta)latency {
  if ((self = [self initWithSourceID:sourceID latency:latency])) {
    _error = error;
  }
  return self;
}

+ (instancetype)linkToTextResponseWithValue:(const base::Value*)value
                                   webState:(web::WebState*)webState
                                    latency:(base::TimeDelta)latency {
  if (!webState) {
    return [LinkToTextResponse
        linkToTextResponseWithUnknownErrorAndLatency:latency];
  }

  ukm::SourceId sourceID = ukm::GetSourceIdForWebStateDocument(webState);

  if (!shared_highlighting::IsValidDictValue(value)) {
    if (link_to_text::IsLinkGenerationTimeout(latency)) {
      return [[self alloc] initWithError:LinkGenerationError::kTimeout
                                sourceID:sourceID
                                 latency:latency];
    }

    return [self linkToTextResponseWithUnknownErrorAndSourceID:sourceID
                                                       latency:latency];
  }

  const base::Value::Dict& dict = value->GetDict();
  std::optional<LinkGenerationOutcome> outcome =
      link_to_text::ParseStatus(dict.FindDouble("status"));
  if (!outcome.has_value()) {
    return [self linkToTextResponseWithUnknownErrorAndSourceID:sourceID
                                                       latency:latency];
  }

  if (outcome.value() != LinkGenerationOutcome::kSuccess) {
    // Convert to Error.
    return [[self alloc]
        initWithError:link_to_text::OutcomeToError(outcome.value())
             sourceID:sourceID
              latency:latency];
  }

  // Attempts to parse a payload from the response.
  NSString* title = tab_util::GetTabTitle(webState);
  std::optional<TextFragment> fragment =
      TextFragment::FromValue(dict.Find("fragment"));
  const std::string* selectedText = dict.FindString("selectedText");
  std::optional<CGRect> sourceRect =
      shared_highlighting::ParseRect(dict.FindDict("selectionRect"));

  // All values must be present to have a valid payload.
  if (!title || !fragment || !selectedText || !sourceRect) {
    // Library replied Success but some values are missing.
    return [self linkToTextResponseWithUnknownErrorAndSourceID:sourceID
                                                       latency:latency];
  }

  GURL baseURL = webState->GetLastCommittedURL();
  std::optional<GURL> canonicalURL =
      shared_highlighting::ParseURL(dict.FindString("canonicalUrl"));

  // Use the canonical URL as base when it exists, and only on HTTPS pages.
  if (baseURL.SchemeIsCryptographic() && canonicalURL) {
    baseURL = canonicalURL.value();
  }

  // Create the deep-link.
  GURL deepLink = shared_highlighting::AppendFragmentDirectives(
      baseURL, {fragment.value()});

  LinkToTextPayload* payload = [[LinkToTextPayload alloc]
       initWithURL:deepLink
             title:title
      selectedText:base::SysUTF8ToNSString(*selectedText)
        sourceView:webState->GetView()
        sourceRect:shared_highlighting::ConvertToBrowserRect(sourceRect.value(),
                                                             webState)];
  return [[self alloc] initWithPayload:payload
                              sourceID:sourceID
                               latency:latency];
}

#pragma mark - Private

+ (instancetype)linkToTextResponseWithUnknownErrorAndLatency:
    (base::TimeDelta)latency {
  return [[self alloc] initWithError:LinkGenerationError::kUnknown
                            sourceID:ukm::kInvalidSourceId
                             latency:latency];
}

+ (instancetype)
    linkToTextResponseWithUnknownErrorAndSourceID:(ukm::SourceId)sourceID
                                          latency:(base::TimeDelta)latency {
  return [[self alloc] initWithError:LinkGenerationError::kUnknown
                            sourceID:sourceID
                             latency:latency];
}

@end
