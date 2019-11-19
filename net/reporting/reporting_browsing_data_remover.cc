// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_browsing_data_remover.h"

#include <vector>

#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_report.h"

namespace net {

// static
void ReportingBrowsingDataRemover::RemoveBrowsingData(
    ReportingCache* cache,
    int data_type_mask,
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter) {
  if ((data_type_mask & DATA_TYPE_REPORTS) != 0) {
    std::vector<const ReportingReport*> all_reports;
    cache->GetReports(&all_reports);

    std::vector<const ReportingReport*> reports_to_remove;
    for (const ReportingReport* report : all_reports) {
      if (origin_filter.Run(report->url))
        reports_to_remove.push_back(report);
    }

    cache->RemoveReports(
        reports_to_remove,
        ReportingReport::Outcome::ERASED_BROWSING_DATA_REMOVED);
  }

  if ((data_type_mask & DATA_TYPE_CLIENTS) != 0) {
    for (const url::Origin& origin : cache->GetAllOrigins()) {
      if (origin_filter.Run(origin.GetURL()))
        cache->RemoveClient(origin);
    }
  }
  cache->Flush();
}

// static
void ReportingBrowsingDataRemover::RemoveAllBrowsingData(ReportingCache* cache,
                                                         int data_type_mask) {
  if ((data_type_mask & DATA_TYPE_REPORTS) != 0) {
    cache->RemoveAllReports(
        ReportingReport::Outcome::ERASED_BROWSING_DATA_REMOVED);
  }
  if ((data_type_mask & DATA_TYPE_CLIENTS) != 0) {
    cache->RemoveAllClients();
  }
  cache->Flush();
}

}  // namespace net
