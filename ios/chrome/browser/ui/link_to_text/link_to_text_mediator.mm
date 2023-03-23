// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/link_to_text/link_to_text_mediator.h"

#import "base/memory/weak_ptr.h"
#import "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#import "ios/chrome/browser/link_to_text/link_to_text_payload.h"
#import "ios/chrome/browser/link_to_text/link_to_text_response.h"
#import "ios/chrome/browser/link_to_text/link_to_text_tab_helper.h"
#import "ios/chrome/browser/ui/link_to_text/link_to_text_consumer.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using shared_highlighting::LinkGenerationError;

@interface LinkToTextMediator ()

// Instance in charge of handling link-to-text updates.
@property(nonatomic, readonly, weak) id<LinkToTextConsumer> consumer;

@end

@implementation LinkToTextMediator {
  // The Browser's WebStateList.
  base::WeakPtr<WebStateList> _webStateList;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                            consumer:(id<LinkToTextConsumer>)consumer {
  if (self = [super init]) {
    DCHECK(webStateList);
    DCHECK(consumer);
    _webStateList = webStateList->AsWeakPtr();
    _consumer = consumer;
  }
  return self;
}

- (BOOL)shouldOfferLinkToText {
  DCHECK(base::FeatureList::IsEnabled(kSharedHighlightingIOS));
  LinkToTextTabHelper* tabHelper = [self linkToTextTabHelper];
  if (!tabHelper) {
    return NO;
  }
  return tabHelper->ShouldOffer();
}

- (void)handleLinkToTextSelection {
  DCHECK(base::FeatureList::IsEnabled(kSharedHighlightingIOS));
  LinkToTextTabHelper* tabHelper = [self linkToTextTabHelper];
  if (!tabHelper) {
    return;
  }
  __weak __typeof(self) weakSelf = self;
  tabHelper->GetLinkToText(base::BindOnce(^(LinkToTextResponse* response) {
    [weakSelf receivedLinkToTextResponse:response];
  }));
}

- (void)receivedLinkToTextResponse:(LinkToTextResponse*)response {
  DCHECK(response);
  if (response.error.has_value()) {
    LinkGenerationError error = response.error.value();
    shared_highlighting::LogLinkGeneratedErrorUkmEvent(response.sourceID,
                                                       error);
    shared_highlighting::LogGenerateErrorLatency(response.latency);
    [self linkGenerationFailedWithError:error];
  } else {
    shared_highlighting::LogLinkGeneratedSuccessUkmEvent(response.sourceID);
    shared_highlighting::LogGenerateSuccessLatency(response.latency);
    [self shareLinkToText:response.payload];
  }
}

- (void)shareLinkToText:(LinkToTextPayload*)payload {
  DCHECK(payload);
  shared_highlighting::LogLinkGenerationStatus(
      shared_highlighting::LinkGenerationStatus::kSuccess);
  [self.consumer generatedPayload:payload];
}

- (void)linkGenerationFailedWithError:(LinkGenerationError)error {
  shared_highlighting::LogLinkGenerationStatus(
      shared_highlighting::LinkGenerationStatus::kFailure);
  shared_highlighting::LogLinkGenerationErrorReason(error);
  [self.consumer linkGenerationFailed];
}

- (LinkToTextTabHelper*)linkToTextTabHelper {
  web::WebState* webState =
      _webStateList ? _webStateList->GetActiveWebState() : nullptr;
  if (!webState) {
    return nullptr;
  }
  LinkToTextTabHelper* helper = LinkToTextTabHelper::FromWebState(webState);
  DCHECK(helper);
  return helper;
}

@end
