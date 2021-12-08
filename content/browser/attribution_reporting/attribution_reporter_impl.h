// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORTER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORTER_IMPL_H_

#include <stdint.h>
#include <memory>
#include <queue>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace base {
class Clock;
}  // namespace base

namespace content {

class StoragePartitionImpl;

struct SentReport;

// This class is responsible for managing the dispatch of conversion reports to
// an AttributionReporterImpl::NetworkSender. It maintains a queue of reports
// and a timer to ensure all reports are sent at the correct time, since the
// time in which a conversion report is sent is potentially sensitive
// information. Created and owned by AttributionManager.
class CONTENT_EXPORT AttributionReporterImpl
    : public AttributionManagerImpl::AttributionReporter,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  // This class is responsible for sending conversion reports to their
  // configured endpoints over the network.
  class NetworkSender {
   public:
    virtual ~NetworkSender() = default;

    // Callback used to notify caller that the requested report has been sent.
    using ReportSentCallback = base::OnceCallback<void(SentReport)>;

    // Generates and sends a conversion report matching |report|. This should
    // generate a secure POST request with no-credentials.
    virtual void SendReport(AttributionReport report,
                            ReportSentCallback sent_callback) = 0;
  };

  using Callback = base::RepeatingCallback<void(SentReport)>;

  AttributionReporterImpl(StoragePartitionImpl* storage_partition,
                          const base::Clock* clock,
                          Callback callback);
  AttributionReporterImpl(const AttributionReporterImpl&) = delete;
  AttributionReporterImpl& operator=(const AttributionReporterImpl&) = delete;
  AttributionReporterImpl(AttributionReporterImpl&&) = delete;
  AttributionReporterImpl& operator=(AttributionReporterImpl&&) = delete;
  ~AttributionReporterImpl() override;

  // AttributionManagerImpl::AttributionReporter:
  void AddReportsToQueue(std::vector<AttributionReport> reports) override;
  void RemoveAllReportsFromQueue() override;

  void SetNetworkSenderForTesting(
      std::unique_ptr<NetworkSender> network_sender);

 private:
  friend class AttributionReporterImplTest;

  void SetNetworkConnectionTracker(
      network::NetworkConnectionTracker* network_connection_tracker);

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(
      network::mojom::ConnectionType connection_type) override;

  void MaybeScheduleNextReport();
  void SendNextReport();

  // Comparator used to order ConversionReports by their report time, with the
  // smallest time at the top of |report_queue_|.
  struct ReportComparator {
    bool operator()(const AttributionReport& a,
                    const AttributionReport& b) const;
  };

  // Priority queue which holds reports that are yet to be sent. Reports are
  // removed from the queue when they are delivered to the NetworkSender.
  std::priority_queue<AttributionReport,
                      std::vector<AttributionReport>,
                      ReportComparator>
      report_queue_;

  raw_ptr<const base::Clock> clock_;

  Callback callback_;

  // Should never be nullptr, since StoragePartition owns the AttributionManager
  // which owns |this|.
  raw_ptr<StoragePartitionImpl> partition_;

  // Timer which signals the next report in |report_queue_| should be sent.
  base::OneShotTimer send_report_timer_;

  // Responsible for issuing requests to network for report that need to be
  // sent. Calls `callback_` when a report has finished sending.
  //
  // Should never be nullptr.
  std::unique_ptr<NetworkSender> network_sender_;

  // Lazily initialized to track network availability.
  raw_ptr<network::NetworkConnectionTracker> network_connection_tracker_ =
      nullptr;

  // Assume that there is a network connection unless we hear otherwise.
  bool offline_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORTER_IMPL_H_
