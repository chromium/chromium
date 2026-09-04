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
#import "components/autofill/core/browser/at_memory/at_memory_manager.h"
#import "components/autofill/core/browser/autofill_trigger_source.h"
#import "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#import "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#import "components/autofill/core/browser/metrics/autofill_metrics.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/personal_context/first_run/personal_context_first_run_service.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_fill_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_search_result_commands.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_consumer.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_item.h"
#import "ios/web/public/web_state.h"
#import "services/metrics/public/cpp/ukm_source_id.h"
#import "url/origin.h"

namespace {

// The UMA histogram to log AtMemory notice interactions.
constexpr std::string_view kNoticeInteractionsHistogram =
    "PersonalContext.AtMemory.NoticeInteractions";

// TODO(crbug.com/556290278): Fix placeholder once the focused field ID is
// plumbed.
autofill::FieldGlobalId GetPlaceholderFieldId() {
  return {autofill::LocalFrameToken(), autofill::FieldRendererId(1)};
}

}  // namespace

@implementation AtMemorySearchMediator {
  // Manager for AtMemory operations.
  raw_ptr<autofill::AtMemoryManager> _atMemoryManager;
  // Manager for Browser Autofill operations.
  raw_ptr<autofill::BrowserAutofillManager> _autofillManager;
  // The WebState for the active tab.
  base::WeakPtr<web::WebState> _webState;
  // Service for managing the first-run notice state.
  raw_ptr<personal_context::PersonalContextFirstRunService> _firstRunService;

  // Suggestions returned by AtMemoryManager.
  std::vector<autofill::Suggestion> _suggestions;

  // Tells if the notice is visible.
  BOOL _noticeIsVisible;
  // Tracks if the notice impression metric has been logged.
  BOOL _noticeShownMetricLogged;
  // Tracks if the user interacted with the notice (either OK or Settings).
  BOOL _noticeInteractionLogged;
}

- (instancetype)
    initWithAtMemoryManager:(autofill::AtMemoryManager*)atMemoryManager
            autofillManager:(autofill::BrowserAutofillManager*)autofillManager
                   webState:(web::WebState*)webState
            firstRunService:(personal_context::PersonalContextFirstRunService*)
                                firstRunService {
  self = [super init];
  if (self) {
    CHECK(atMemoryManager);
    CHECK(autofillManager);
    _atMemoryManager = atMemoryManager;
    _autofillManager = autofillManager;
    _webState = webState ? webState->GetWeakPtr() : nullptr;
    _firstRunService = firstRunService;

    _noticeIsVisible =
        _firstRunService &&
        _firstRunService->ShouldShowPersonalContextAtMemoryNotice();

    // Force reset any existing popup state from the main autofill popup,
    // so that our new updateCallback is correctly registered.
    _atMemoryManager->OnPopupHidden();

    ukm::SourceId ukmSourceId =
        webState ? ukm::GetSourceIdForWebStateDocument(webState)
                 : ukm::kInvalidSourceId;

    __weak __typeof(self) weakSelf = self;
    auto updateCallback = base::BindRepeating(
        ^(std::vector<autofill::Suggestion> suggestions,
          autofill::AutofillSuggestionTriggerSource triggerSource) {
          [weakSelf onAtMemorySuggestionsReceived:suggestions];
        });

    url::Origin origin =
        webState ? url::Origin::Create(webState->GetLastCommittedURL())
                 : url::Origin();
    _atMemoryManager->GetStateForField(GetPlaceholderFieldId(), origin);

    // TODO(crbug.com/527392582): Update trigger source once a dedicated
    // manual fallback / accessory trigger source is introduced.
    _atMemoryManager->OnPopupShown(
        /*bam=*/*autofillManager,
        /*form_id=*/autofill::FormGlobalId(),
        /*field_id=*/GetPlaceholderFieldId(),
        /*trigger_source=*/
        autofill::AutofillSuggestionTriggerSource::kAtMemoryContextMenu,
        /*metadata=*/{},
        /*update_callback=*/std::move(updateCallback),
        /*ukm_source_id=*/ukmSourceId);
    _atMemoryManager->OnFilterChanged(u"");
  }
  return self;
}

- (void)dealloc {
  [self disconnect];
}

- (void)disconnect {
  if (_noticeIsVisible && !_noticeInteractionLogged) {
    _noticeInteractionLogged = YES;
    base::UmaHistogramEnumeration(
        kNoticeInteractionsHistogram,
        autofill::AutofillMetrics::PopupNoticeInteractions::kDismissed);
  }
  if (_atMemoryManager) {
    _atMemoryManager->OnPopupHidden();
  }
  _atMemoryManager = nullptr;
  _autofillManager = nullptr;
  _webState = nullptr;
  _firstRunService = nullptr;
  _suggestions.clear();
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

  if (!_suggestions.empty()) {
    [self pushSuggestionsToConsumer];
  }
}

#pragma mark - AtMemorySearchMutator

- (void)startSearchWithQuery:(NSString*)query {
  if (!_atMemoryManager) {
    return;
  }

  _atMemoryManager->OnSearchSubmitted(base::SysNSStringToUTF16(query));
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
  if (!item || !_webState) {
    return;
  }

  if (_atMemoryManager && _autofillManager && item.index >= 0 &&
      static_cast<size_t>(item.index) < _suggestions.size()) {
    _atMemoryManager->FillSearchResult(
        *_autofillManager, autofill::FormGlobalId(), GetPlaceholderFieldId(),
        _suggestions[item.index], /*metadata=*/{});
  }

  [self.fillHandler fillWithContent:item.title];
  [self.atMemoryHandler dismissAtMemory];
}

- (void)openGranularFillForSearchResultAtIndex:(NSInteger)index {
  if (index < 0 || static_cast<size_t>(index) >= _suggestions.size()) {
    return;
  }

  const autofill::Suggestion& suggestion = _suggestions[index];
  if (suggestion.type != autofill::SuggestionType::kAtMemorySearchResult) {
    return;
  }

  [self.searchResultHandler showAtMemoryGranularFill:suggestion];
}

#pragma mark - Private

// Handles suggestions returned by the AtMemoryManager.
- (void)onAtMemorySuggestionsReceived:
    (const std::vector<autofill::Suggestion>&)suggestions {
  _suggestions = suggestions;
  [self pushSuggestionsToConsumer];
}

// Pushes current suggestions to the consumer.
- (void)pushSuggestionsToConsumer {
  NSMutableArray<AtMemorySearchItem*>* searchItems =
      [[NSMutableArray alloc] init];
  NSMutableArray<AtMemorySearchItem*>* recentFillItems =
      [[NSMutableArray alloc] init];

  bool isRecentFills = false;
  for (size_t i = 0; i < _suggestions.size(); ++i) {
    const auto& suggestion = _suggestions[i];
    switch (suggestion.type) {
      case autofill::SuggestionType::kTitle:
        isRecentFills = true;
        break;
      case autofill::SuggestionType::kAtMemoryNoConnection:
        [self.consumer setErrorType:AtMemoryErrorType::kNoConnectionError];
        return;
      case autofill::SuggestionType::kAtMemoryGenericError:
        [self.consumer setErrorType:AtMemoryErrorType::kNoDataError];
        return;
      case autofill::SuggestionType::kAtMemoryFetching:
        [self.consumer setFetchingSubtitle:base::SysUTF16ToNSString(
                                               suggestion.main_text.value)];
        return;
      case autofill::SuggestionType::kAtMemorySearchResult: {
        if (!std::holds_alternative<autofill::Suggestion::AtMemoryPayload>(
                suggestion.payload)) {
          // The backend uses kAtMemorySearchResult without a payload for the
          // "No Data" state.
          [self.consumer setErrorType:AtMemoryErrorType::kNoDataError];
          return;
        }

        AtMemorySearchItem* item =
            [[AtMemorySearchItem alloc] initWithSuggestion:suggestion index:i];
        if (isRecentFills) {
          [recentFillItems addObject:item];
        } else {
          [searchItems addObject:item];
        }
        break;
      }
      default:
        break;
    }
  }

  if (recentFillItems.count > 0) {
    [self.consumer setRecentFills:recentFillItems];
  } else if (searchItems.count > 0) {
    [self.consumer setSearchResults:searchItems];
  } else if (_atMemoryManager && _atMemoryManager->IsSearching()) {
    [self.consumer setErrorType:AtMemoryErrorType::kNoDataError];
  } else {
    [self.consumer setRecentFills:@[]];
  }
}

@end
