// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_mediator.h"

#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/web/public/web_state.h"

@implementation ReaderModeMediator {
  base::WeakPtr<web::WebState> _sourceWebState;
}

#pragma mark - Initialization

- (instancetype)initWithWebState:(raw_ptr<web::WebState>)webState {
  self = [super init];
  if (self) {
    CHECK(webState);
    // TODO(crbug.com/409940117): Ensure the UI reacts to a change in active
    // WebState e.g. new active WebState or no active WebState.
    _sourceWebState = webState->GetWeakPtr();
  }
  return self;
}

#pragma mark - Properties

- (void)setConsumer:(id<ReaderModeConsumer>)consumer {
  CHECK(consumer);
  _consumer = consumer;
  ReaderModeTabHelper* tabHelper =
      ReaderModeTabHelper::FromWebState(_sourceWebState.get());
  CHECK(tabHelper);
  [self.consumer setContentView:tabHelper->GetReaderModeContentView()];
}

#pragma mark - Public

- (void)disconnect {
  _sourceWebState = nil;
}

@end
