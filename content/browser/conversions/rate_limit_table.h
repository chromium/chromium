// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_RATE_LIMIT_TABLE_H_
#define CONTENT_BROWSER_CONVERSIONS_RATE_LIMIT_TABLE_H_

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/conversion_storage.h"
#include "content/common/content_export.h"
#include "sql/database.h"

namespace base {
class Clock;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace content {

// Manages storage for rate-limiting reports.
// This class may be constructed on any sequence but must be accessed and
// destroyed on the same sequence. The sequence must outlive |this|.
class CONTENT_EXPORT RateLimitTable {
 public:
  enum class AttributionType {
    kNavigation = 0,
    kMaxValue = kNavigation,
  };

  RateLimitTable(const ConversionStorage::Delegate* delegate,
                 const base::Clock* clock);
  RateLimitTable(const RateLimitTable& other) = delete;
  RateLimitTable& operator=(const RateLimitTable& other) = delete;
  ~RateLimitTable();

  // Creates the table in |db| if it doesn't exist.
  // Returns false on failure.
  bool CreateTable(sql::Database* db);

  // Adds a rate limit to the table.
  // Returns false on failure.
  bool AddRateLimit(sql::Database* db, const ConversionReport& report);

  // Checks if the given attribution is allowed according to the data in the
  // table and policy as specified by the delegate.
  bool IsAttributionAllowed(sql::Database* db,
                            const ConversionReport& report,
                            base::Time now);

  // These should be 1:1 with |ConversionStorageSql|'s |ClearData| functions.
  // Returns false on failure.
  bool ClearAllDataInRange(sql::Database* db,
                           base::Time delete_begin,
                           base::Time delete_end);
  // Returns false on failure.
  bool ClearAllDataAllTime(sql::Database* db);
  // Returns false on failure.
  bool ClearDataForOriginsInRange(
      sql::Database* db,
      base::Time delete_begin,
      base::Time delete_end,
      base::RepeatingCallback<bool(const url::Origin&)> filter);
  bool ClearDataForImpressionIds(sql::Database* db,
                                 const base::flat_set<int64_t>& impression_ids);

 private:
  // Deletes data in the table older than the window determined by |clock_| and
  // |delegate_->GetRateLimits()|.
  // Returns the number of deleted rows.
  int DeleteExpiredRateLimits(sql::Database* db);

  // Must outlive |this|.
  const ConversionStorage::Delegate* delegate_;

  // Must outlive |this|.
  const base::Clock* clock_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_RATE_LIMIT_TABLE_H_
