// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/coordinator/ttc_mediator.h"

#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_consumer.h"

@implementation TTCMediator {
  TTCSessionState _currentState;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _currentState = TTCSessionState::kIdle;
  }
  return self;
}

#pragma mark - Setters

- (void)setConsumer:(id<TTCConsumer>)consumer {
  _consumer = consumer;
  if (!_consumer) {
    return;
  }
  [self hydrateConsumer];
}

#pragma mark - TTCMutator

- (void)startSession {
  _currentState = TTCSessionState::kListening;
  [self.consumer setSessionState:TTCSessionState::kListening];
}

- (void)stopSession {
  _currentState = TTCSessionState::kIdle;
  [self.consumer setSessionState:TTCSessionState::kIdle];
}

- (void)viewWillAppear {
  [self hydrateConsumer];
}

#pragma mark - Public

- (void)disconnect {
  if (_currentState != TTCSessionState::kIdle) {
    [self stopSession];
  }
  _consumer = nil;
}

#pragma mark - Private

// Hydrates the consumer with current state and resets audio energy.
- (void)hydrateConsumer {
  if (!_consumer) {
    return;
  }
  [_consumer setSessionState:_currentState];
  [_consumer setMicEnergyLevel:0.0f];
}

@end
