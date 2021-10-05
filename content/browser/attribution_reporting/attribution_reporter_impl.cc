// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_reporter_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/rand_util.h"
#include "base/time/clock.h"
#include "content/browser/attribution_reporting/attribution_network_sender_impl.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/conversion_manager.h"
#include "content/browser/attribution_reporting/sent_report_info.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_client.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace content {

AttributionReporterImpl::AttributionReporterImpl(
    StoragePartitionImpl* storage_partition,
    const base::Clock* clock,
    base::RepeatingCallback<void(SentReportInfo)> callback)
    : clock_(clock),
      callback_(std::move(callback)),
      partition_(storage_partition),
      network_sender_(
          std::make_unique<AttributionNetworkSenderImpl>(storage_partition)) {
  DCHECK(clock_);
  DCHECK(partition_);
}

AttributionReporterImpl::~AttributionReporterImpl() {
  if (network_connection_tracker_)
    network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

void AttributionReporterImpl::AddReportsToQueue(
    std::vector<AttributionReport> reports) {
  DCHECK(!reports.empty());

  // Shuffle new reports to provide plausible deniability on the ordering of
  // reports that share the same |report_time|. This is important because
  // multiple conversions for the same impression share the same report time if
  // they are within the same reporting window, and we do not want to allow
  // ordering on their conversion metadata bits.
  base::RandomShuffle(reports.begin(), reports.end());

  for (AttributionReport& report : reports) {
    DCHECK(report.conversion_id.has_value());
    // If the given report is already being processed, ignore it.
    if (reports_being_sent_.contains(*report.conversion_id))
      continue;
    bool inserted = queued_reports_.emplace(*report.conversion_id).second;
    if (inserted)
      report_queue_.push(std::move(report));
  }
  MaybeScheduleNextReport();
}

void AttributionReporterImpl::RemoveAllReportsFromQueue() {
  while (!report_queue_.empty()) {
    AttributionReport report = report_queue_.top();
    DCHECK(report.conversion_id.has_value());
    report_queue_.pop();
    OnReportSent(SentReportInfo(std::move(report),
                                SentReportInfo::Status::kRemovedFromQueue,
                                /*http_response_code=*/0));
  }
  queued_reports_.clear();
}

void AttributionReporterImpl::SetNetworkSenderForTesting(
    std::unique_ptr<NetworkSender> network_sender) {
  network_sender_ = std::move(network_sender);
}

void AttributionReporterImpl::SetNetworkConnectionTracker(
    network::NetworkConnectionTracker* network_connection_tracker) {
  DCHECK(network_connection_tracker);
  DCHECK(!network_connection_tracker_);

  network_connection_tracker_ = network_connection_tracker;
  network_connection_tracker_->AddNetworkConnectionObserver(this);
}

void AttributionReporterImpl::OnConnectionChanged(
    network::mojom::ConnectionType connection_type) {
  offline_ = connection_type == network::mojom::ConnectionType::CONNECTION_NONE;
}

void AttributionReporterImpl::SendNextReport() {
  if (report_queue_.empty())
    return;

  // Send the next report and remove it from the queue.
  AttributionReport report = report_queue_.top();
  DCHECK(report.conversion_id.has_value());
  report_queue_.pop();
  size_t num_removed = queued_reports_.erase(*report.conversion_id);
  DCHECK_EQ(num_removed, 1u);
  if (GetContentClient()->browser()->IsConversionMeasurementOperationAllowed(
          partition_->browser_context(),
          ContentBrowserClient::ConversionMeasurementOperation::kReport,
          &report.impression.impression_origin(),
          &report.impression.conversion_origin(),
          &report.impression.reporting_origin())) {
    if (!network_connection_tracker_)
      SetNetworkConnectionTracker(content::GetNetworkConnectionTracker());

    // If there's no network connection, drop the report and tell the manager to
    // retry it later.
    if (offline_) {
      OnReportSent(SentReportInfo(std::move(report),
                                  SentReportInfo::Status::kOffline,
                                  /*http_response_code=*/0));
    } else {
      bool inserted = reports_being_sent_.emplace(*report.conversion_id).second;
      DCHECK(inserted);
      network_sender_->SendReport(
          std::move(report),
          base::BindOnce(&AttributionReporterImpl::OnReportSent,
                         base::Unretained(this)));
    }
  } else {
    // If measurement is disallowed, just drop the report on the floor. We need
    // to make sure we forward that the report was "sent" to ensure it is
    // deleted from storage, etc. This simulates sending the report through a
    // null channel.
    OnReportSent(SentReportInfo(std::move(report),
                                SentReportInfo::Status::kDropped,
                                /*http_response_code=*/0));
  }
  MaybeScheduleNextReport();
}

void AttributionReporterImpl::MaybeScheduleNextReport() {
  if (report_queue_.empty())
    return;

  send_report_timer_.Stop();
  base::Time current_time = clock_->Now();
  base::Time report_time = report_queue_.top().report_time;

  // Start a timer to wait until the next report is ready to be sent. This
  // purposefully yields the thread for every report that gets scheduled.
  // Unretained is safe because the task should never actually be posted if the
  // timer itself is destroyed
  send_report_timer_.Start(
      FROM_HERE,
      (report_time < current_time) ? base::TimeDelta()
                                   : report_time - current_time,
      base::BindOnce(&AttributionReporterImpl::SendNextReport,
                     base::Unretained(this)));
}

void AttributionReporterImpl::OnReportSent(SentReportInfo info) {
  DCHECK(info.report.conversion_id.has_value());
  reports_being_sent_.erase(*info.report.conversion_id);
  callback_.Run(std::move(info));
}

bool AttributionReporterImpl::ReportComparator::operator()(
    const AttributionReport& a,
    const AttributionReport& b) const {
  // Returns whether a should appear before b in ordering. Because
  // std::priority_queue is max priority queue, we used greater then to make a
  // min priority queue.
  return a.report_time > b.report_time;
}

}  // namespace content
