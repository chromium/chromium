// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/link_to_text_response.h"

#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/shared_highlighting/core/common/text_fragment.h"
#import "components/shared_highlighting/core/common/text_fragments_utils.h"
#import "components/ukm/ios/ukm_url_recorder.h"
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

// Initializes an object with a |sourceID| representing the current WebState,
// along with the |latency| for link generation.
- (instancetype)initWithSourceID:(ukm::SourceId)sourceID
                         latency:(base::TimeDelta)latency
    NS_DESIGNATED_INITIALIZER;

// Initializes an object with the given |payload| of the link generation
// request, a |sourceID| representing the current WebState and the |latency| for
// link generation.
- (instancetype)initWithPayload:(LinkToTextPayload*)payload
                       sourceID:(ukm::SourceId)sourceID
                        latency:(base::TimeDelta)latency;

// Initializes an object with the given |error| which occurred while trying to
// generate a link, a |sourceID| representing the current WebState and the
// |latency| for link generation.
- (instancetype)initWithError:(LinkGenerationError)error
                     sourceID:(ukm::SourceId)sourceID
                      latency:(base::TimeDelta)latency;

@end

@implementation LinkToTextResponse

- (instancetype)initWithSourceID:(ukm::SourceId)sourceID
                         latency:(base::TimeDelta)latency {
  if (self = [super init]) {
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
  if (self = [self initWithSourceID:sourceID latency:latency]) {
    _payload = payload;
  }
  return self;
}

- (instancetype)initWithError:(LinkGenerationError)error
                     sourceID:(ukm::SourceId)sourceID
                      latency:(base::TimeDelta)latency {
  if (self = [self initWithSourceID:sourceID latency:latency]) {
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

  if (!link_to_text::IsValidDictValue(value)) {
    if (link_to_text::IsLinkGenerationTimeout(latency)) {
      return [[self alloc] initWithError:LinkGenerationError::kTimeout
                                sourceID:sourceID
                                 latency:latency];
    }

    return [self linkToTextResponseWithUnknownErrorAndSourceID:sourceID
                                                       latency:latency];
  }

  base::Optional<LinkGenerationOutcome> outcome =
      link_to_text::ParseStatus(value->FindDoubleKey("status"));
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
  base::Optional<TextFragment> fragment =
      TextFragment::FromValue(value->FindKey("fragment"));
  const std::string* selectedText = value->FindStringKey("selectedText");
  base::Optional<CGRect> sourceRect =
      link_to_text::ParseRect(value->FindKey("selectionRect"));

  // All values must be present to have a valid payload.
  if (!title || !fragment || !selectedText || !sourceRect) {
    // Library replied Success but some values are missing.
    return [self linkToTextResponseWithUnknownErrorAndSourceID:sourceID
                                                       latency:latency];
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
