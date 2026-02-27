// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/coordinator/autofill_ai_save_entity_mediator.h"

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_consumer.h"

@implementation AutofillAISaveEntityMediator {
  std::optional<autofill::SaveEntityParams> _params;
}

- (instancetype)initWithParams:(autofill::SaveEntityParams)params {
  self = [super init];
  if (self) {
    _params = std::move(params);
  }
  return self;
}

- (void)setConsumer:(id<AutofillAISaveEntityConsumer>)consumer {
  _consumer = consumer;
  [self pushDataToConsumer];
}

- (void)disconnect {
  if (_params && !_params->callback.is_null()) {
    std::move(_params->callback)
        .Run(autofill::AutofillClient::AutofillAiBubbleResult::kUnknown);
  }
  _params = std::nullopt;
  _consumer = nil;
}

#pragma mark - AutofillAISaveEntityMutator

- (void)acceptSaving {
  if (_params && !_params->callback.is_null()) {
    std::move(_params->callback)
        .Run(autofill::AutofillClient::AutofillAiBubbleResult::kAccepted);
  }
}

- (void)cancelSaving {
  if (_params && !_params->callback.is_null()) {
    std::move(_params->callback)
        .Run(autofill::AutofillClient::AutofillAiBubbleResult::kCancelled);
  }
}

- (void)dismissSaving {
  [self cancelSaving];
}

#pragma mark - Private

// Pushes the current data to the view controller.
- (void)pushDataToConsumer {
  if (!_consumer || !_params) {
    return;
  }

  [_consumer setNewEntity:_params->new_entity
                oldEntity:_params->old_entity
                userEmail:_params->user_email];
}

@end
