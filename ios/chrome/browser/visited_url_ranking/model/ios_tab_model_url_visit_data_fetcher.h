// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VISITED_URL_RANKING_MODEL_IOS_TAB_MODEL_URL_VISIT_DATA_FETCHER_H_
#define IOS_CHROME_BROWSER_VISITED_URL_RANKING_MODEL_IOS_TAB_MODEL_URL_VISIT_DATA_FETCHER_H_

#include "base/memory/raw_ptr.h"
#include "components/visited_url_ranking/public/fetch_result.h"
#include "components/visited_url_ranking/public/url_visit_data_fetcher.h"

class ChromeBrowserState;

namespace visited_url_ranking {

// The IOS implementation of URLVisitDataFetcher that gather the currently
// opened normal tabs.
class IOSTabModelURLVisitDataFetcher : public URLVisitDataFetcher {
 public:
  explicit IOSTabModelURLVisitDataFetcher(ChromeBrowserState* profile);
  ~IOSTabModelURLVisitDataFetcher() override;

  // Disallow copy.
  IOSTabModelURLVisitDataFetcher(const IOSTabModelURLVisitDataFetcher&) =
      delete;

  // URLVisitDataFetcher::
  void FetchURLVisitData(const FetchOptions& options,
                         FetchResultCallback callback) override;

 private:
  const raw_ptr<ChromeBrowserState> browser_state_;
};

}  // namespace visited_url_ranking

#endif  // IOS_CHROME_BROWSER_VISITED_URL_RANKING_MODEL_IOS_TAB_MODEL_URL_VISIT_DATA_FETCHER_H_
