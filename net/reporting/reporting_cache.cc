// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_cache.h"

#include "net/reporting/reporting_cache_impl.h"
#include "net/reporting/reporting_context.h"

namespace net {

// static
std::unique_ptr<ReportingCache> ReportingCache::Create(
    ReportingContext* context,
    const base::flat_map<std::string, GURL>& enterprise_reporting_endpoints) {
  return std::make_unique<ReportingCacheImpl>(context,
                                              enterprise_reporting_endpoints);
}

ReportingCache::~ReportingCache() = default;

}  // namespace net
