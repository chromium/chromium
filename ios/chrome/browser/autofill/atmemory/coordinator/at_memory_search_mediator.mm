// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_search_mediator.h"

#import <optional>
#import <string_view>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/integrators/at_memory/at_memory_query_service.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#import "components/autofill/core/browser/metrics/autofill_metrics.h"
#import "components/personal_context/first_run/personal_context_first_run_service.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_fill_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_search_result_commands.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_consumer.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_item.h"
#import "ios/web/public/web_state.h"

namespace {

// The UMA histogram to log AtMemory notice interactions.
constexpr std::string_view kNoticeInteractionsHistogram =
    "PersonalContext.AtMemory.NoticeInteractions";

}  // namespace

@implementation AtMemorySearchMediator {
  // Service for executing AtMemory queries.
  raw_ptr<autofill::AtMemoryQueryService> _atMemoryQueryService;
  // The WebState for the active tab.
  base::WeakPtr<web::WebState> _webState;
  // Service for managing the first-run notice state.
  raw_ptr<personal_context::PersonalContextFirstRunService> _firstRunService;

  // Results from the AtMemory query service.
  std::optional<autofill::MemorySearchResults> _searchResults;

  // Tells if the notice is visible.
  BOOL _noticeIsVisible;
  // Tracks if the notice impression metric has been logged.
  BOOL _noticeShownMetricLogged;
  // Tracks if the user interacted with the notice (either OK or Settings).
  BOOL _noticeInteractionLogged;
}

- (instancetype)
    initWithAtMemoryQueryService:
        (autofill::AtMemoryQueryService*)atMemoryQueryService
                        webState:(web::WebState*)webState
                 firstRunService:
                     (personal_context::PersonalContextFirstRunService*)
                         firstRunService {
  self = [super init];
  if (self) {
    _atMemoryQueryService = atMemoryQueryService;
    _webState = webState ? webState->GetWeakPtr() : nullptr;
    _firstRunService = firstRunService;

    _noticeIsVisible =
        _firstRunService &&
        _firstRunService->ShouldShowPersonalContextAtMemoryNotice();
  }
  return self;
}

- (void)disconnect {
  if (_noticeIsVisible && !_noticeInteractionLogged) {
    _noticeInteractionLogged = YES;
    base::UmaHistogramEnumeration(
        kNoticeInteractionsHistogram,
        autofill::AutofillMetrics::PopupNoticeInteractions::kDismissed);
  }
  _atMemoryQueryService = nullptr;
  _webState = nullptr;
  _firstRunService = nullptr;
  _searchResults.reset();
  _atMemoryHandler = nil;
  _searchResultHandler = nil;
}

#pragma mark - Consumer

- (void)setConsumer:(id<AtMemorySearchConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;

  [_consumer setNoticeVisible:_noticeIsVisible];

  if (_noticeIsVisible && !_noticeShownMetricLogged) {
    _noticeShownMetricLogged = YES;
    base::UmaHistogramEnumeration(
        kNoticeInteractionsHistogram,
        autofill::AutofillMetrics::PopupNoticeInteractions::kShown);
  }
}

#pragma mark - AtMemorySearchMutator

- (void)startSearchWithQuery:(NSString*)query {
  if (!_atMemoryQueryService || !_webState) {
    return;
  }

  // Request AtMemory search results from the AtMemory query service for the
  // given `query`.
  __weak __typeof(self) weakSelf = self;
  auto callback = base::BindRepeating(^(autofill::MemorySearchResults results) {
    [weakSelf handleAtMemorySearchResults:results];
  });

  _atMemoryQueryService->Query(base::SysNSStringToUTF16(query),
                               _webState->GetVisibleURL(),
                               _webState->GetTitle(), callback);
}

- (void)acknowledgePrivacyNotice {
  CHECK(_firstRunService);
  _firstRunService->MarkPersonalContextInAtMemoryNoticeAsAcknowledged();
  _noticeIsVisible = NO;
  _noticeInteractionLogged = YES;
  [self.consumer setNoticeVisible:NO];
  base::UmaHistogramEnumeration(
      kNoticeInteractionsHistogram,
      autofill::AutofillMetrics::PopupNoticeInteractions::kAcknowledged);
}

- (void)didTapSettingsLink {
  _noticeInteractionLogged = YES;
  base::UmaHistogramEnumeration(
      kNoticeInteractionsHistogram,
      autofill::AutofillMetrics::PopupNoticeInteractions::kLinkButtonClicked);
  [self.atMemoryHandler openAutofillSettings];
}

- (void)didSelectSearchResultItem:(AtMemorySearchItem*)item {
  [self.fillHandler fillWithContent:item.title];
  [self.atMemoryHandler dismissAtMemory];
}

- (void)openGranularFillForSearchResultAtIndex:(NSInteger)index {
  if (!_searchResults.has_value()) {
    return;
  }
  [self.searchResultHandler
      showAtMemoryGranularFillWithResult:_searchResults->entries[index]];
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
      return;
    case autofill::MemorySearchStatus::kUnsupportedQuery:
      [self.consumer setErrorType:AtMemoryErrorType::kUnsupportedQueryError];
      return;
    case autofill::MemorySearchStatus::kFinalResponseSuccess:
    case autofill::MemorySearchStatus::kPartialResponseSuccess:
      if (results.entries.empty()) {
        [self.consumer setErrorType:AtMemoryErrorType::kNoDataError];
      } else {
        [self pushResultsToConsumer:results];
      }
      return;
    case autofill::MemorySearchStatus::kInferenceFailure:
    case autofill::MemorySearchStatus::kInternalFailure:
      [self.consumer setErrorType:AtMemoryErrorType::kNoDataError];
      return;
  }
  NOTREACHED();
}

// Converts memory search results to items and sends them to the consumer.
- (void)pushResultsToConsumer:(const autofill::MemorySearchResults&)results {
  NSMutableArray<AtMemorySearchItem*>* searchItems = [NSMutableArray array];
  NSInteger index = 0;
  for (const autofill::MemorySearchResult& entry : results.entries) {
    [searchItems addObject:[[AtMemorySearchItem alloc]
                               initWithMemorySearchResult:entry
                                                    index:index++]];
  }
  [self.consumer setSearchResults:searchItems];
}

@end
