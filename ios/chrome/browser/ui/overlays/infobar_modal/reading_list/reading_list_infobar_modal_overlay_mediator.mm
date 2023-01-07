// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/reading_list/reading_list_infobar_modal_overlay_mediator.h"

#import "ios/chrome/browser/overlays/public/infobar_modal/reading_list_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/reading_list_modal_overlay_responses.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ReadingListInfobarModalOverlayMediator ()
// The add to reading list modal config from the request.
@property(nonatomic, readonly)
    ReadingListInfobarModalOverlayRequestConfig* config;
@end

@implementation ReadingListInfobarModalOverlayMediator

#pragma mark - Public

- (void)setConsumer:(id<InfobarReadingListModalConsumer>)consumer {
  if (_consumer == consumer)
    return;

  _consumer = consumer;

  ReadingListInfobarModalOverlayRequestConfig* config = self.config;
  if (!_consumer || !config)
    return;

  [_consumer setCurrentPageAdded:config->current_page_added()];
}

#pragma mark - Accessors

- (ReadingListInfobarModalOverlayRequestConfig*)config {
  return self.request
             ? self.request
                   ->GetConfig<ReadingListInfobarModalOverlayRequestConfig>()
             : nullptr;
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return ReadingListInfobarModalOverlayRequestConfig::RequestSupport();
}

#pragma mark - InfobarReadingListModalDelegate

- (void)neverAskToAddToReadingList {
  [self dispatchResponse:OverlayResponse::CreateWithInfo<
                             reading_list_infobar_modal_responses::NeverAsk>()];

  [self dismissOverlay];
}

@end
