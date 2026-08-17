// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_granular_fill_mediator.h"

#import "base/check.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_fill_commands.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_consumer.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_item.h"
#import "ios/chrome/browser/autofill/atmemory/utils/atmemory_ui_util.h"

@implementation AtMemoryGranularFillMediator {
  // Search result containing attributes to display.
  std::optional<autofill::MemorySearchResult> _result;
}

- (instancetype)initWithResult:(autofill::MemorySearchResult&&)result {
  self = [super init];
  if (self) {
    _result = std::move(result);
  }
  return self;
}

- (void)setConsumer:(id<AtMemoryGranularFillConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  if (!_consumer) {
    return;
  }

  CHECK(_result.has_value());
  [_consumer setTitle:GetAtMemoryGranularFillTitle(*_result)];
  [_consumer
      setGranularFillItems:AtMemoryGranularFillItemsForSearchResult(*_result)];
}

#pragma mark - AtMemoryGranularFillMutator

- (void)didSelectContent:(NSString*)content {
  [self.fillHandler fillWithContent:content];
  [self.atMemoryHandler dismissAtMemory];
}

@end
