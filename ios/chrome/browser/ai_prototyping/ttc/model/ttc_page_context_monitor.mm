// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_page_context_monitor.h"

#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_utils.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

TtcPageContextMonitor::TtcPageContextMonitor(web::WebState& web_state,
                                             Delegate* delegate)
    : web_state_(web_state.GetWeakPtr()), delegate_(delegate) {
  CHECK(delegate_);
  web_state_observation_.Observe(&web_state);
  FetchPageContextIfNeeded();
}

TtcPageContextMonitor::~TtcPageContextMonitor() {
  Disconnect();
}

void TtcPageContextMonitor::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CancelPendingExtraction();
  web_state_observation_.Reset();
  web_state_ = nullptr;
  delegate_ = nullptr;
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
    return;
  }
  FetchPageContextIfNeeded();
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
  if (!web_state || !web_state->IsRealized() || web_state->IsLoading() ||
      !CanExtractPageContextForWebState(web_state,
                                        /*pdf_enabled=*/false)) {
    return;
  }

  const GURL expected_url = web_state->GetLastCommittedURL();
  PageContextWrapperConfig config =
      PageContextWrapperConfigBuilder().SetDefaultRichExtraction(true).Build();
  page_context_wrapper_ = [[PageContextWrapper alloc]
        initWithWebState:web_state
                  config:config
      completionCallback:base::BindOnce(
                             &TtcPageContextMonitor::OnPageContextExtracted,
                             extraction_weak_factory_.GetWeakPtr(),
                             expected_url)];
  page_context_wrapper_.shouldGetAnnotatedPageContent = YES;
  [page_context_wrapper_ populatePageContextFieldsAsync];
}

void TtcPageContextMonitor::CancelPendingExtraction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  extraction_weak_factory_.InvalidateWeakPtrs();
  page_context_wrapper_ = nil;
}

void TtcPageContextMonitor::OnPageContextExtracted(
    const GURL& expected_url,
    PageContextWrapperCallbackResponse response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  page_context_wrapper_ = nil;
  web::WebState* web_state = web_state_.get();
  if (!web_state || web_state->GetLastCommittedURL() != expected_url) {
    return;
  }
  if (delegate_) {
    delegate_->OnPageContextFetched(std::move(response));
  }
}
