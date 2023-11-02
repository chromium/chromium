// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_cache_observer.h"

namespace net {

void ReportingCacheObserver::OnReportsUpdated() {}

void ReportingCacheObserver::OnReportAdded(const ReportingReport* report) {}

void ReportingCacheObserver::OnReportUpdated(const ReportingReport* report) {}

void ReportingCacheObserver::OnClientsUpdated() {}

void ReportingCacheObserver::OnEndpointsUpdatedForOrigin(
    const std::vector<ReportingEndpoint>& endpoints) {}

ReportingCacheObserver::ReportingCacheObserver() = default;

ReportingCacheObserver::~ReportingCacheObserver() = default;

}  // namespace net
