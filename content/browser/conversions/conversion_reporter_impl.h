// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_REPORTER_IMPL_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_REPORTER_IMPL_H_

#include <stdint.h>
#include <memory>
#include <queue>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/timer/timer.h"
#include "content/browser/conversions/conversion_manager_impl.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace base {
class Clock;
}  // namespace base

namespace content {

class StoragePartitionImpl;

struct SentReportInfo;

// This class is responsible for managing the dispatch of conversion reports to
// a ConversionReporterImpl::NetworkSender. It maintains a queue of reports and
// a timer to ensure all reports are sent at the correct time, since the time in
// which a conversion report is sent is potentially sensitive information.
// Created and owned by ConversionManager.
class CONTENT_EXPORT ConversionReporterImpl
    : public ConversionManagerImpl::ConversionReporter,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  // This class is responsible for sending conversion reports to their
  // configured endpoints over the network.
  class NetworkSender {
   public:
    virtual ~NetworkSender() = default;

    // Callback used to notify caller that the requested report has been sent.
    using ReportSentCallback = base::OnceCallback<void(SentReportInfo)>;

    // Generates and sends a conversion report matching |report|. This should
    // generate a secure POST request with no-credentials.
    virtual void SendReport(ConversionReport report,
                            ReportSentCallback sent_callback) = 0;
  };

  ConversionReporterImpl(
      StoragePartitionImpl* storage_partition,
      const base::Clock* clock,
      base::RepeatingCallback<void(SentReportInfo)> callback);
  ConversionReporterImpl(const ConversionReporterImpl&) = delete;
  ConversionReporterImpl& operator=(const ConversionReporterImpl&) = delete;
  ConversionReporterImpl(ConversionReporterImpl&&) = delete;
  ConversionReporterImpl& operator=(ConversionReporterImpl&&) = delete;
  ~ConversionReporterImpl() override;

  // ConversionManagerImpl::ConversionReporter:
  void AddReportsToQueue(std::vector<ConversionReport> reports) override;

  void SetNetworkSenderForTesting(
      std::unique_ptr<NetworkSender> network_sender);

 private:
  friend class ConversionReporterImplTest;

  void SetNetworkConnectionTracker(
      network::NetworkConnectionTracker* network_connection_tracker);

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(
      network::mojom::ConnectionType connection_type) override;

  void MaybeScheduleNextReport();
  void SendNextReport();

  // Called when a conversion report sent via NetworkSender::SendReport() has
  // completed loading.
  void OnReportSent(SentReportInfo info);

  // Comparator used to order ConversionReports by their report time, with the
  // smallest time at the top of |report_queue_|.
  struct ReportComparator {
    bool operator()(const ConversionReport& a, const ConversionReport& b) const;
  };

  // Priority queue which holds reports that are yet to be sent. Reports are
  // removed from the queue when they are delivered to the NetworkSender.
  std::priority_queue<ConversionReport,
                      std::vector<ConversionReport>,
                      ReportComparator>
      report_queue_;

  // Set of all conversion IDs that are currently in |report_queue_| or are
  // being sent by |network_sender_|. The number of concurrent conversion
  // reports being sent at any time is expected to be small, so a `flat_set` is
  // used.
  base::flat_set<ConversionReport::Id> pending_reports_;

  const base::Clock* clock_;

  base::RepeatingCallback<void(SentReportInfo)> callback_;

  // Should never be nullptr, since StoragePartition owns the ConversionManager
  // which owns |this|.
  StoragePartitionImpl* partition_;

  // Timer which signals the next report in |report_queue_| should be sent.
  base::OneShotTimer send_report_timer_;

  // Responsible for issuing requests to network for report that need to be
  // sent. Calls OnReportSent() when a report has finished sending.
  //
  // Should never be nullptr.
  std::unique_ptr<NetworkSender> network_sender_;

  // Lazily initialized to track network availability.
  network::NetworkConnectionTracker* network_connection_tracker_ = nullptr;

  // Assume that there is a network connection unless we hear otherwise.
  bool offline_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_REPORTER_IMPL_H_
