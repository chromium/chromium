// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_page_context_monitor.h"

#import <stddef.h>

#import <memory>
#import <string_view>
#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/strings/stringprintf.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_utils.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

namespace {

// Journal event name for asynchronous page context extraction.
constexpr char kPageContextExtractionEventName[] = "TtcPageContextExtraction";

// Returns a human-readable string label for `error`.
std::string_view PageContextWrapperErrorToString(
    PageContextWrapperError error) {
  switch (error) {
    case PageContextWrapperError::kGenericError:
      return "generic_error";
    case PageContextWrapperError::kAPCError:
      return "apc_error";
    case PageContextWrapperError::kScreenshotError:
      return "screenshot_error";
    case PageContextWrapperError::kPDFDataError:
      return "pdf_data_error";
    case PageContextWrapperError::kForceDetachError:
      return "force_detach_error";
    case PageContextWrapperError::kTimeout:
      return "timeout";
    case PageContextWrapperError::kInnerTextError:
      return "inner_text_error";
    case PageContextWrapperError::kPageNotExtractableError:
      return "page_not_extractable_error";
    case PageContextWrapperError::kPageUnsafeError:
      return "page_unsafe_error";
  }
}

}  // namespace

TtcPageContextMonitor::TtcPageContextMonitor(web::WebState& web_state,
                                             Delegate* delegate,
                                             TTCJournal* journal)
    : web_state_(web_state.GetWeakPtr()),
      delegate_(delegate),
      journal_(journal ? journal->GetWeakPtr() : nullptr) {
  CHECK(delegate_);
  web_state_observation_.Observe(&web_state);
  FetchPageContextIfNeeded();
}

TtcPageContextMonitor::~TtcPageContextMonitor() {
  Disconnect();
}

void TtcPageContextMonitor::set_journal(TTCJournal* journal) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  journal_ = journal ? journal->GetWeakPtr() : nullptr;
}

void TtcPageContextMonitor::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CancelPendingExtraction();
  web_state_observation_.Reset();
  web_state_ = nullptr;
  delegate_ = nullptr;
  journal_.reset();
}

#pragma mark - web::WebStateObserver

void TtcPageContextMonitor::DidFinishNavigation(
    web::WebState* /*web_state*/,
    web::NavigationContext* navigation_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!navigation_context || !navigation_context->HasCommitted()) {
    return;
  }

  // Cancel any in-flight extraction from the previous document.
  CancelPendingExtraction();

  // Same-document navigations (such as SPA route changes) do not trigger a
  // subsequent `PageLoaded` callback, so extract fresh page context now.
  if (navigation_context->IsSameDocument() && !navigation_context->GetError()) {
    FetchPageContextIfNeeded();
  }
}

void TtcPageContextMonitor::PageLoaded(
    web::WebState* /*web_state*/,
    web::PageLoadCompletionStatus load_completion_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (load_completion_status != web::PageLoadCompletionStatus::SUCCESS) {
    CancelPendingExtraction();
    if (delegate_) {
      delegate_->OnPageContextFetched(
          base::unexpected(PageContextWrapperError::kGenericError));
    }
    return;
  }
  FetchPageContextIfNeeded();
}

void TtcPageContextMonitor::RenderProcessGone(web::WebState* /*web_state*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CancelPendingExtraction();
  if (delegate_) {
    delegate_->OnPageContextFetched(
        base::unexpected(PageContextWrapperError::kPageNotExtractableError));
  }
}

void TtcPageContextMonitor::WebStateDestroyed(web::WebState* /*web_state*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CancelPendingExtraction();
  web_state_observation_.Reset();
  web_state_ = nullptr;
}

#pragma mark - Private

void TtcPageContextMonitor::FetchPageContextIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CancelPendingExtraction();
  web::WebState* web_state = web_state_.get();
  if (!web_state || !web_state->IsRealized() || web_state->IsLoading()) {
    return;
  }
  if (!CanExtractPageContextForWebState(web_state, /*pdf_enabled=*/false)) {
    if (delegate_) {
      delegate_->OnPageContextFetched(
          base::unexpected(PageContextWrapperError::kPageNotExtractableError));
    }
    return;
  }

  const GURL expected_url = web_state->GetLastCommittedURL();
  if (journal_) {
    active_async_entry_ = journal_->BeginAsyncEvent(
        TTCJournalComponent::kSession, kPageContextExtractionEventName,
        TTCJournalDetailsBuilder()
            .Add("url", expected_url.possibly_invalid_spec())
            .Add("actionable", true)
            .Build());
  }
  PageContextWrapperConfig config =
      PageContextWrapperConfigBuilder()
          .SetDefaultRichExtraction(true)
          .SetUseRichExtractionWithActionable(true)
          .SetIncludeSameSiteOnly(true)
          .Build();
  page_context_wrapper_ = [[PageContextWrapper alloc]
        initWithWebState:web_state
                  config:config
      completionCallback:base::BindOnce(
                             &TtcPageContextMonitor::OnPageContextExtracted,
                             extraction_weak_factory_.GetWeakPtr(),
                             expected_url)];
  page_context_wrapper_.shouldGetAnnotatedPageContent = YES;
  [page_context_wrapper_ populatePageContextFieldsAsyncWithTimeout:
                             GetActorTabObservationPageContextTimeout()];
}

void TtcPageContextMonitor::CancelPendingExtraction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  extraction_weak_factory_.InvalidateWeakPtrs();
  page_context_wrapper_ = nil;
  if (active_async_entry_) {
    active_async_entry_->EndEntry(
        TTCJournalDetailsBuilder().Add("result", "cancelled").Build());
    active_async_entry_.reset();
  }
}

void TtcPageContextMonitor::OnPageContextExtracted(
    const GURL& expected_url,
    PageContextWrapperCallbackResponse response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<TTCJournal::PendingAsyncEntry> async_entry =
      std::move(active_async_entry_);
  page_context_wrapper_ = nil;
  web::WebState* web_state = web_state_.get();
  if (!web_state || web_state->GetLastCommittedURL() != expected_url) {
    if (async_entry) {
      async_entry->EndEntry(
          TTCJournalDetailsBuilder().Add("result", "stale_url").Build());
    }
    return;
  }
  if (async_entry) {
    TTCJournalDetailsBuilder details_builder;
    const optimization_guide::proto::PageContext* page_context =
        response.has_value() ? response.value().get() : nullptr;
    if (page_context && page_context->has_annotated_page_content()) {
      const size_t size_bytes =
          page_context->annotated_page_content().ByteSizeLong();
      details_builder
          .SetSummary(base::StringPrintf("Extracted APC (%zu B)", size_bytes))
          .Add("result", "ok")
          .Add("size_bytes", size_bytes);
    } else {
      const std::string_view error_reason =
          response.has_value()
              ? "missing_apc"
              : PageContextWrapperErrorToString(response.error());
      details_builder.Add("result", "error").AddError(error_reason);
    }
    async_entry->EndEntry(std::move(details_builder).Build());
  }
  if (delegate_) {
    delegate_->OnPageContextFetched(std::move(response));
  }
}
