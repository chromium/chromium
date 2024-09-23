// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/search_engines/model/template_url_service_client_impl.h"

#include "base/check_op.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/search_engines/template_url_service.h"
#include "ui/base/page_transition_types.h"

namespace ios {

TemplateURLServiceClientImpl::TemplateURLServiceClientImpl(
    history::HistoryService* history_service)
    : owner_(nullptr), history_service_(history_service) {
  // TODO(crbug.com/40164129): The keywords should be moved into the history
  // db, which will mean we no longer need this notification and the history
  // backend can handle automatically adding the search terms as the user
  // navigates.
  if (history_service_)
    history_service_observation_.Observe(history_service_.get());
}

TemplateURLServiceClientImpl::~TemplateURLServiceClientImpl() {}

void TemplateURLServiceClientImpl::Shutdown() {
  // TemplateURLServiceClientImpl is owned by TemplateURLService which is a
  // KeyedService with a dependency on HistoryService, thus `history_service_`
  // outlives the ChromeTemplateURLServiceClient.
  //
  // Remove self from `history_service_` observers in the shutdown phase of the
  // two-phases since KeyedService are not supposed to use a dependend service
  // after the Shutdown call.
  if (history_service_) {
    history_service_observation_.Reset();
    history_service_ = nullptr;
  }
}

void TemplateURLServiceClientImpl::SetOwner(TemplateURLService* owner) {
  DCHECK(owner);
  DCHECK(!owner_);
  owner_ = owner;
}

void TemplateURLServiceClientImpl::DeleteAllSearchTermsForKeyword(
    history::KeywordID keyword_id) {
  if (history_service_)
    history_service_->DeleteAllSearchTermsForKeyword(keyword_id);
}

void TemplateURLServiceClientImpl::SetKeywordSearchTermsForURL(
    const GURL& url,
    TemplateURLID id,
    const std::u16string& term) {
  if (history_service_)
    history_service_->SetKeywordSearchTermsForURL(url, id, term);
}

void TemplateURLServiceClientImpl::AddKeywordGeneratedVisit(const GURL& url) {
  if (history_service_) {
    history_service_->AddPage(
        url, base::Time::Now(), /*context_id=*/0, /*nav_entry_id=*/0,
        /*referrer=*/GURL(), history::RedirectList(),
        ui::PAGE_TRANSITION_KEYWORD_GENERATED, history::SOURCE_BROWSED,
        /*did_replace_entry=*/false);
  }
}

void TemplateURLServiceClientImpl::OnURLVisited(
    history::HistoryService* history_service,
    const history::URLRow& url_row,
    const history::VisitRow& new_visit) {
  DCHECK_EQ(history_service, history_service_);
  if (!owner_)
    return;

  TemplateURLService::URLVisitedDetails details = {
      url_row.url(),
      ui::PageTransitionCoreTypeIs(new_visit.transition,
                                   ui::PAGE_TRANSITION_KEYWORD),
  };
  owner_->OnHistoryURLVisited(details);
}

}  // namespace ios
