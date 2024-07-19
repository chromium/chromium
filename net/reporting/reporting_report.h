// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_REPORT_H_
#define NET_REPORTING_REPORTING_REPORT_H_

#include <memory>
#include <optional>
#include <string>

#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/reporting/reporting_endpoint.h"
#include "net/reporting/reporting_target_type.h"
#include "url/gurl.h"

namespace net {

// An undelivered report.
struct NET_EXPORT ReportingReport {
 public:
  enum class Status {
    // Report has been queued and no attempt has been made to deliver it yet,
    // or attempted previous upload failed (impermanently).
    QUEUED,

    // There is an ongoing attempt to upload this report.
    PENDING,

    // Deletion of this report was requested while it was pending, so it should
    // be removed after the attempted upload completes.
    DOOMED,

    // Similar to DOOMED with the difference that the upload was already
    // successful.
    SUCCESS,
  };

  // TODO(chlily): Remove |attempts| argument as it is (almost?) always 0.
  ReportingReport(const std::optional<base::UnguessableToken>& reporting_source,
                  const NetworkAnonymizationKey& network_anonymization_key,
                  const GURL& url,
                  const std::string& user_agent,
                  const std::string& group,
                  const std::string& type,
                  base::Value::Dict body,
                  int depth,
                  base::TimeTicks queued,
                  int attempts,
                  ReportingTargetType target_type);

  // Do NOT use this constructor outside of mojo deserialization context.
  ReportingReport();
  ReportingReport(const ReportingReport&) = delete;
  ReportingReport(ReportingReport&& other);
  ReportingReport& operator=(const ReportingReport&) = delete;
  ReportingReport& operator=(ReportingReport&& other);
  ~ReportingReport();

  // Bundles together the NAK, reporting source, origin of the report URL, group
  // name, and target type. This is not exactly the same as the group key of the
  // endpoint that the report will be delivered to. The origin may differ if the
  // endpoint is configured for a superdomain of the report's origin. The NAK,
  // group name, and target type will be the same.
  ReportingEndpointGroupKey GetGroupKey() const;

  static void RecordReportDiscardedForNoURLRequestContext();
  static void RecordReportDiscardedForNoReportingService();

  // Whether the report is part of an ongoing delivery attempt.
  bool IsUploadPending() const;

  // The reporting source token for the document or worker which triggered this
  // report, if it can be associated with one, or nullopt otherwise (Network
  // reports, such as NEL, for instance, do not support such attribution.)
  // This is used to identify appropriate endpoints to deliver this report to;
  // reports with an attached source token may be delivered to a named endpoint
  // with a matching source, but are also eligible to be delivered to an
  // endpoint group without a source. Reports without a source token can only be
  // delivered to endpoint groups without one.
  // (Not included in the delivered report.)
  std::optional<base::UnguessableToken> reporting_source;

  // The NAK of the request that triggered this report. (Not included in the
  // delivered report.)
  NetworkAnonymizationKey network_anonymization_key;

  // The id of the report, used by DevTools to identify and tell apart
  // individual reports.
  base::UnguessableToken id;

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
  base::Value::Dict body;

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

  Status status = Status::QUEUED;

  // Used to distinguish web developer and enterprise entities so that
  // enterprise reports aren’t sent to web developer endpoints and web developer
  // reports aren’t sent to enterprise endpoints
  ReportingTargetType target_type = ReportingTargetType::kDeveloper;
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_REPORT_H_
