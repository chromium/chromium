// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_granular_fill_mediator.h"

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_consumer.h"

@implementation AtMemoryGranularFillMediator

- (void)setConsumer:(id<AtMemoryGranularFillConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;

  // TODO(crbug.com/522340351): Set the title and granular fill items on the
  // consumer.
}

@end
