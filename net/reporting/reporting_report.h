// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_REPORT_H_
#define NET_REPORTING_REPORTING_REPORT_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/reporting/reporting_endpoint.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace base {
class Value;
}  // namespace base

namespace net {

// An undelivered report.
struct NET_EXPORT ReportingReport {
 public:
  // Used in histograms; please add new items at end and do not reorder.
  enum class Outcome {
    UNKNOWN = 0,
    DISCARDED_NO_URL_REQUEST_CONTEXT = 1,
    DISCARDED_NO_REPORTING_SERVICE = 2,
    ERASED_FAILED = 3,
    ERASED_EXPIRED = 4,
    ERASED_EVICTED = 5,
    ERASED_NETWORK_CHANGED = 6,
    ERASED_BROWSING_DATA_REMOVED = 7,
    ERASED_REPORTING_SHUT_DOWN = 8,
    DELIVERED = 9,

    MAX
  };

  enum class Status {
    // Report has been queued but no attempt has been made to deliver it yet.
    QUEUED,

    // There is an ongoing attempt to upload this report.
    PENDING,

    // Deletion of this report was requested while it was pending, so it should
    // be removed after the attempted upload completes.
    DOOMED,
  };

  // TODO(chlily): Remove |attempts| argument as it is (almost?) always 0.
  ReportingReport(const NetworkIsolationKey& network_isolation_key,
                  const GURL& url,
                  const std::string& user_agent,
                  const std::string& group,
                  const std::string& type,
                  std::unique_ptr<const base::Value> body,
                  int depth,
                  base::TimeTicks queued,
                  int attempts);

  // Records metrics about report outcome.
  ~ReportingReport();

  // Bundles together the NIK, origin of the report URL, and group name.
  // This is not exactly the same as the group key of the endpoint that the
  // report will be delivered to. The origin may differ if the endpoint is
  // configured for a superdomain of the report's origin. The NIK and group name
  // will be the same.
  ReportingEndpointGroupKey GetGroupKey() const;

  static void RecordReportDiscardedForNoURLRequestContext();
  static void RecordReportDiscardedForNoReportingService();

  // Whether the report is part of an ongoing delivery attempt.
  bool IsUploadPending() const;

  // The NIK of the request that triggered this report. (Not included in the
  // delivered report.)
  NetworkIsolationKey network_isolation_key;

  // The URL of the document that triggered the report. (Included in the
  // delivered report.)
  GURL url;

  // The User-Agent header that was used for the request.
  std::string user_agent;

  // The endpoint group that should be used to deliver the report. (Not included
  // in the delivered report.)
  std::string group;

  // The type of the report. (Included in the delivered report.)
  std::string type;

  // The body of the report. (Included in the delivered report.)
  std::unique_ptr<const base::Value> body;

  // How many uploads deep the related request was: 0 if the related request was
  // not an upload (or there was no related request), or n+1 if it was an upload
  // reporting on requests of at most depth n.
  int depth;

  // When the report was queued. (Included in the delivered report as an age
  // relative to the time of the delivery attempt.)
  base::TimeTicks queued;

  // The number of delivery attempts made so far, not including an active
  // attempt. (Not included in the delivered report.)
  int attempts = 0;

  Outcome outcome = Outcome::UNKNOWN;

  Status status = Status::QUEUED;

  DISALLOW_COPY_AND_ASSIGN(ReportingReport);
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_REPORT_H_
