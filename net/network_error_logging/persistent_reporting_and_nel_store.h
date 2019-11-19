// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NETWORK_ERROR_LOGGING_PERSISTENT_REPORTING_AND_NEL_STORE_H_
#define NET_NETWORK_ERROR_LOGGING_PERSISTENT_REPORTING_AND_NEL_STORE_H_

#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_cache.h"

namespace net {

// Stores Reporting reports, Reporting clients, and NEL policies.
class NET_EXPORT PersistentReportingAndNelStore
    : public ReportingCache::PersistentReportingStore,
      public NetworkErrorLoggingService::PersistentNelStore {};

}  // namespace net

#endif  // NET_NETWORK_ERROR_LOGGING_PERSISTENT_REPORTING_AND_NEL_STORE_H_
