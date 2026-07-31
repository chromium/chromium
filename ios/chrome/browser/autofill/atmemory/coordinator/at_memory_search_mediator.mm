// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_search_mediator.h"

#import <utility>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/integrators/at_memory/at_memory_query_service.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#import "ios/web/public/web_state.h"

@implementation AtMemorySearchMediator {
  // Service for executing AtMemory queries.
  raw_ptr<autofill::AtMemoryQueryService> _atMemoryQueryService;
  // The WebState for the active tab.
  base::WeakPtr<web::WebState> _webState;
}

- (instancetype)initWithAtMemoryQueryService:
                    (autofill::AtMemoryQueryService*)atMemoryQueryService
                                    webState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    _atMemoryQueryService = atMemoryQueryService;
    _webState = webState ? webState->GetWeakPtr() : nullptr;
  }
  return self;
}

- (void)disconnect {
  _atMemoryQueryService = nullptr;
  _webState = nullptr;
}

#pragma mark - Private

// Handles the `results` returned by the AtMemory query service.
- (void)handleAtMemorySearchResults:
    (const autofill::MemorySearchResults&)results {
  // TODO(crbug.com/540126524): Handle the search results when the consumer is
  // available.
  switch (results.status) {
    case autofill::MemorySearchStatus::kNoConnectionFailure:
    case autofill::MemorySearchStatus::kUnsupportedQuery:
    case autofill::MemorySearchStatus::kFinalResponseSuccess:
    case autofill::MemorySearchStatus::kPartialResponseSuccess:
    case autofill::MemorySearchStatus::kInferenceFailure:
    case autofill::MemorySearchStatus::kInternalFailure:
      break;
  }
}

// TODO(crbug.com/540127498): This method will be updated to be used by the
// AtMemorySearchMutator. Requests AtMemory search results from the AtMemory
// query service for the given `query`.
- (void)requestResultsForQuery:(NSString*)query {
  if (!_atMemoryQueryService || !_webState) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  auto callback = base::BindRepeating(^(autofill::MemorySearchResults results) {
    [weakSelf handleAtMemorySearchResults:results];
  });

  _atMemoryQueryService->Query(base::SysNSStringToUTF16(query),
                               _webState->GetVisibleURL(),
                               _webState->GetTitle(), callback);
}

@end
