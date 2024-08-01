// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_DELIVERY_AGENT_H_
#define NET_REPORTING_REPORTING_DELIVERY_AGENT_H_

#include <memory>

#include "base/unguessable_token.h"
#include "net/base/net_export.h"
#include "net/base/rand_callback.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace net {

class ReportingContext;

// Batches reports fetched from the ReportingCache and uploads them using the
// ReportingUploader.
//
// Reports are only considered for delivery if all of the following are true:
//  - The report is not already part of a pending upload request.
//  - Uploads are allowed for the report's origin (i.e. the origin of the URL
//    associated with the reported event).
//  - There is not already a pending upload for any reports sharing the same
//    (NAK, origin, group) key.
//
// Reports are batched for upload to an endpoint URL such that:
//  - The available reports with the same (NAK, origin, group) are always
//    uploaded together.
//  - All reports uploaded together must share a NAK and origin.
//  - Reports for the same (NAK, origin) can be uploaded separately if they are
//    for different groups.
//  - Reports for different groups can be batched together, if they are assigned
//    to ReportingEndpoints sharing a URL (that is, the upload URL).
//
// There is no limit to the number of reports that can be uploaded together.
// (Aside from the global cap on total reports.)
//
// TODO(juliatuttle): Consider capping the maximum number of reports per
// delivery attempt.
class NET_EXPORT ReportingDeliveryAgent {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // They should also be kept in sync with the NetReportingUploadHeaderType
  // enum in tools/metrics/histograms/enums.xml
  enum class ReportingUploadHeaderType {
    kReportTo = 0,
    kReportingEndpoints = 1,
    kMaxValue = kReportingEndpoints
  };
  // Creates a ReportingDeliveryAgent. |context| must outlive the agent.
  static std::unique_ptr<ReportingDeliveryAgent> Create(
      ReportingContext* context,
      const RandIntCallback& rand_callback);

  virtual ~ReportingDeliveryAgent();

  // Replaces the internal OneShotTimer used for scheduling report delivery
  // attempts with a caller-specified one so that unittests can provide a
  // MockOneShotTimer.
  virtual void SetTimerForTesting(
      std::unique_ptr<base::OneShotTimer> timer) = 0;

  // Bypasses the schedule to attempt delivery of all outstanding reports
  // for a single `reporting_source`. Called when the source document or worker
  // is being destroyed.
  virtual void SendReportsForSource(
      base::UnguessableToken reporting_source) = 0;

 private:
  // Bypasses the schedule to attempt delivery of all outstanding reports.
  virtual void SendReportsForTesting() = 0;

  friend class ReportingDeliveryAgentTest;
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_DELIVERY_AGENT_H_
