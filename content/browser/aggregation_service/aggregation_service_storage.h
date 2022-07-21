// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_STORAGE_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_STORAGE_H_

#include <stdint.h>

#include <vector>

#include "base/types/strong_alias.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace base {
class Time;
}  // namespace base

namespace content {

struct PublicKey;
struct PublicKeyset;

// This class provides an interface for persisting helper server public keys and
// aggregatable report requests, as well as performing queries on those. The
// public key and report request methods are in the same interface to allow a
// single SequenceBound to own the (joint) implementation class.
class AggregationServiceStorage {
 public:
  using RequestId = base::StrongAlias<AggregatableReportRequest, int64_t>;
  struct RequestAndId {
    AggregatableReportRequest request;
    RequestId id;
  };

  virtual ~AggregationServiceStorage() = default;

  // == Public key methods =====

  // Returns the public keys for `url` that are currently valid. The returned
  // value should not be stored for future operations as it may expire soon.
  virtual std::vector<PublicKey> GetPublicKeys(const GURL& url) = 0;

  // Sets the public keys for `url`.
  virtual void SetPublicKeys(const GURL& url, const PublicKeyset& keyset) = 0;

  // Clears the stored public keys for `url`.
  virtual void ClearPublicKeys(const GURL& url) = 0;

  // Clears the stored public keys that expire no later than `delete_end`
  // (inclusive).
  virtual void ClearPublicKeysExpiredBy(base::Time delete_end) = 0;

  // == Aggregatable report request methods =====

  // Persists the `request` with a report time of `*report_time_override`. If
  // the optional has no value, the `request`'s scheduled report time is used.
  virtual void StoreRequest(AggregatableReportRequest request) = 0;

  // Deletes the report request with the given `request_id`, if any.
  virtual void DeleteRequest(RequestId request_id) = 0;

  // Returns the earliest report time for a stored pending request strictly
  // after `strictly_after_time`. If there are no such requests stored, returns
  // `absl::nullopt`.
  virtual absl::optional<base::Time> NextReportTimeAfter(
      base::Time strictly_after_time) = 0;

  // Returns requests with report times on or before `not_after_time`. The
  // returned reports are ordered by report time.
  // TODO(crbug.com/1340046): Limit the number of in-progress reports kept in
  // memory at the same time.
  virtual std::vector<RequestAndId> GetRequestsReportingOnOrBefore(
      base::Time not_after_time) = 0;

  // TODO(crbug.com/1340042): Add a method to randomly delay all reports in the
  // past (for startup and coming online).

  // == Joint methods =====

  // Clears the stored public keys that were fetched between and the report
  // requests that were stored between `delete_begin` and `delete_end` time
  // (inclusive). Null times are treated as unbounded lower or upper range.  If
  // `!filter.is_null()`, requests with a reporting origin that does *not* match
  // the `filter` are retained (i.e. not cleared); `filter` does not affect
  // public key deletion.
  virtual void ClearDataBetween(
      base::Time delete_begin,
      base::Time delete_end,
      StoragePartition::StorageKeyMatcherFunction filter) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_STORAGE_H_
