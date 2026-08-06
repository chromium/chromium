// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_search_mediator.h"

#import <optional>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/integrators/at_memory/at_memory_query_service.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_consumer.h"
#import "ios/web/public/web_state.h"

@implementation AtMemorySearchMediator {
  // Service for executing AtMemory queries.
  raw_ptr<autofill::AtMemoryQueryService> _atMemoryQueryService;
  // The WebState for the active tab.
  base::WeakPtr<web::WebState> _webState;

  // Results from the AtMemory query service.
  std::optional<autofill::MemorySearchResults> _searchResults;

  // Tells if the notice is visible.
  BOOL _noticeIsVisible;
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
  _searchResults.reset();
}

#pragma mark - Consumer

- (void)setConsumer:(id<AtMemorySearchConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;

  [_consumer setNoticeVisible:_noticeIsVisible];
  [_consumer updateTableViewBackgroundStyle:[self tableViewBackgroundStyle]];
}

#pragma mark - Private

// Handles the `results` returned by the AtMemory query service. If the results
// are empty, the error type is provided to the consumer.
- (void)handleAtMemorySearchResults:
    (const autofill::MemorySearchResults&)results {
  _searchResults = results;
  switch (results.status) {
    case autofill::MemorySearchStatus::kNoConnectionFailure:
      [self.consumer setErrorType:AtMemoryErrorType::kNoConnectionError];
      break;
    case autofill::MemorySearchStatus::kUnsupportedQuery:
      [self.consumer setErrorType:AtMemoryErrorType::kUnsupportedQueryError];
      break;
    case autofill::MemorySearchStatus::kFinalResponseSuccess:
    case autofill::MemorySearchStatus::kPartialResponseSuccess:
      if (results.entries.empty()) {
        [self.consumer setErrorType:AtMemoryErrorType::kNoDataError];
      } else {
        // TODO(crbug.com/543036121): Add a method to push results to the
        // consumer once `AtMemorySearchItem` has been created.
      }
      break;
    case autofill::MemorySearchStatus::kInferenceFailure:
    case autofill::MemorySearchStatus::kInternalFailure:
      [self.consumer setErrorType:AtMemoryErrorType::kNoDataError];
      break;
  }
  // TODO(crbug.com/543036121): Here, an array with the results will be provided
  // to the consumer. If the array is nil, there was an error.
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

- (AtMemoryBackgroundStyle)tableViewBackgroundStyle {
  // TODO(crbug.com/540877897): Verify if there are any recent fills. If yes,
  // show kDefaultStyle.
  if (_noticeIsVisible) {
    return AtMemoryBackgroundStyle::kDefaultStyle;
  }
  return AtMemoryBackgroundStyle::kEmptyStyle;
}

@end
