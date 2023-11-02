// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_CACHE_OBSERVER_H_
#define NET_REPORTING_REPORTING_CACHE_OBSERVER_H_

#include <vector>

#include "net/base/net_export.h"
#include "net/reporting/reporting_endpoint.h"
#include "net/reporting/reporting_report.h"

namespace net {

class NET_EXPORT ReportingCacheObserver {
 public:
  ReportingCacheObserver(const ReportingCacheObserver&) = delete;
  ReportingCacheObserver& operator=(const ReportingCacheObserver&) = delete;

  // Called whenever any change is made to the reports in the ReportingCache.
  virtual void OnReportsUpdated();

  // Called whenever a new report is added to the ReportingCache.
  virtual void OnReportAdded(const ReportingReport* report);

  // Called whenever a report in the ReportingCache is updated.
  virtual void OnReportUpdated(const ReportingReport* report);

  // Called whenever any change is made to the client entries in the
  // ReportingCache.
  virtual void OnClientsUpdated();

  // Called when V1 reporting endpoints for an origin are updated in the
  // ReportingCache.
  virtual void OnEndpointsUpdatedForOrigin(
      const std::vector<ReportingEndpoint>& endpoints);

 protected:
  ReportingCacheObserver();

  ~ReportingCacheObserver();
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_CACHE_OBSERVER_H_
