// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_DELIVERY_AGENT_H_
#define NET_REPORTING_REPORTING_DELIVERY_AGENT_H_

#include <memory>

#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/base/rand_callback.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace net {

class ReportingContext;

// Takes reports from the ReportingCache, assembles reports into deliveries to
// endpoints, and sends those deliveries using ReportingUploader.
//
// Since the Reporting spec is completely silent on issues of concurrency, the
// delivery agent handles it as so:
//
// 1. An individual report can only be included in one delivery at once -- if
//    SendReports is called again while a report is being delivered, it won't
//    be included in another delivery during that call to SendReports. (This is,
//    in fact, made redundant by rule 3, but it's included anyway in case rule 3
//    changes.)
//
// 2. An endpoint can only be the target of one delivery at once -- if
//    SendReports is called again with reports that could be delivered to that
//    endpoint, they won't be delivered to that endpoint.
//
// 3. Reports for an (origin, group) tuple can only be included in one delivery
//    at once -- if SendReports is called again with reports in that (origin,
//    group), they won't be included in any delivery during that call to
//    SendReports. (This prevents the agent from getting around rule 2 by using
//    other endpoints in the same group.)
//
// 4. Reports for the same origin *can* be included in multiple parallel
//    deliveries if they are in different groups within that origin.
//
// (Note that a single delivery can contain an infinite number of reports.)
//
// TODO(juliatuttle): Consider capping the maximum number of reports per
// delivery attempt.
class NET_EXPORT ReportingDeliveryAgent {
 public:
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
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_DELIVERY_AGENT_H_
