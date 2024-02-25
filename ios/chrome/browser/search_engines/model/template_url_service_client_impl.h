// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_SERVICE_CLIENT_IMPL_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_SERVICE_CLIENT_IMPL_H_

#import "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/search_engines/template_url_service_client.h"

class TemplateURLService;

namespace ios {

// TemplateURLServiceClientImpl provides keyword related history functionality
// for TemplateURLService.
class TemplateURLServiceClientImpl : public TemplateURLServiceClient,
                                     public history::HistoryServiceObserver {
 public:
  explicit TemplateURLServiceClientImpl(
      history::HistoryService* history_service);

  TemplateURLServiceClientImpl(const TemplateURLServiceClientImpl&) = delete;
  TemplateURLServiceClientImpl& operator=(const TemplateURLServiceClientImpl&) =
      delete;

  ~TemplateURLServiceClientImpl() override;

 private:
  // TemplateURLServiceClient:
  void Shutdown() override;
  void SetOwner(TemplateURLService* owner) override;
  void DeleteAllSearchTermsForKeyword(history::KeywordID keyword_id) override;
  void SetKeywordSearchTermsForURL(const GURL& url,
                                   TemplateURLID id,
                                   const std::u16string& term) override;
  void AddKeywordGeneratedVisit(const GURL& url) override;

  // history::HistoryServiceObserver:
  void OnURLVisited(history::HistoryService* history_service,
                    const history::URLRow& url_row,
                    const history::VisitRow& new_visit) override;

  raw_ptr<TemplateURLService> owner_;
  raw_ptr<history::HistoryService> history_service_;
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_TEMPLATE_URL_SERVICE_CLIENT_IMPL_H_
