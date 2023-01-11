// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_BROWSING_DATA_REMOVER_H_
#define NET_REPORTING_REPORTING_BROWSING_DATA_REMOVER_H_

#include "base/functional/callback.h"
#include "net/base/net_export.h"
#include "url/origin.h"

namespace net {

class ReportingCache;

// Clears browsing data (reports and clients) from the Reporting system.
class NET_EXPORT ReportingBrowsingDataRemover {
 public:
  enum DataType {
    DATA_TYPE_REPORTS = 0x1,
    DATA_TYPE_CLIENTS = 0x2,
  };

  ReportingBrowsingDataRemover() = delete;
  ReportingBrowsingDataRemover(const ReportingBrowsingDataRemover&) = delete;
  ReportingBrowsingDataRemover& operator=(const ReportingBrowsingDataRemover&) =
      delete;

  // Removes browsing data from the Reporting system. |data_type_mask| specifies
  // which types of data to remove: reports queued by browser features and/or
  // clients (endpoints configured by origins). |origin_filter| specifies which
  // origins' data to remove.
  //
  // Note: Currently this does not clear the endpoint backoff data in
  // ReportingEndpointManager because that's not persisted to disk. If it's ever
  // persisted, it will need to be cleared as well.
  static void RemoveBrowsingData(
      ReportingCache* cache,
      uint64_t data_type_mask,
      const base::RepeatingCallback<bool(const url::Origin&)>& origin_filter);

  // Like RemoveBrowsingData except removes data for all origins without a
  // filter. Allows slight optimization over passing an always-true filter to
  // RemoveBrowsingData.
  static void RemoveAllBrowsingData(ReportingCache* cache,
                                    uint64_t data_type_mask);
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_BROWSING_DATA_REMOVER_H_
