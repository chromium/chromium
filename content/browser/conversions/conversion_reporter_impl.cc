// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_reporter_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/rand_util.h"
#include "base/time/clock.h"
#include "content/browser/conversions/conversion_manager.h"
#include "content/browser/conversions/conversion_network_sender_impl.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/sent_report_info.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {

ConversionReporterImpl::ConversionReporterImpl(
    StoragePartitionImpl* storage_partition,
    const base::Clock* clock)
    : clock_(clock),
      partition_(storage_partition),
      network_sender_(
          std::make_unique<ConversionNetworkSenderImpl>(storage_partition)) {
  DCHECK(clock_);
  DCHECK(partition_);
}

ConversionReporterImpl::~ConversionReporterImpl() = default;

void ConversionReporterImpl::AddReportsToQueue(
    std::vector<ConversionReport> reports,
    base::RepeatingCallback<void(SentReportInfo)> report_sent_callback) {
  DCHECK(!reports.empty());

  // Shuffle new reports to provide plausible deniability on the ordering of
  // reports that share the same |report_time|. This is important because
  // multiple conversions for the same impression share the same report time if
  // they are within the same reporting window, and we do not want to allow
  // ordering on their conversion metadata bits.
  base::RandomShuffle(reports.begin(), reports.end());

  for (ConversionReport& report : reports) {
    // If the given report is already being processed, ignore it.
    bool inserted = conversion_report_callbacks_
                        .emplace(*report.conversion_id, report_sent_callback)
                        .second;
    if (inserted)
      report_queue_.push(std::move(report));
  }
  MaybeScheduleNextReport();
}

void ConversionReporterImpl::SetNetworkSenderForTesting(
    std::unique_ptr<NetworkSender> network_sender) {
  network_sender_ = std::move(network_sender);
}

void ConversionReporterImpl::SendNextReport() {
  DCHECK(!report_queue_.empty());

  // Send the next report and remove it from the queue. Bind the conversion id
  // to the sent callback so we know which conversion report has finished
  // sending.
  const ConversionReport& report = report_queue_.top();
  DCHECK(report.conversion_id.has_value());
  if (GetContentClient()->browser()->IsConversionMeasurementOperationAllowed(
          partition_->browser_context(),
          ContentBrowserClient::ConversionMeasurementOperation::kReport,
          &report.impression.impression_origin(),
          &report.impression.conversion_origin(),
          &report.impression.reporting_origin())) {
    network_sender_->SendReport(
        report, base::BindOnce(&ConversionReporterImpl::OnReportSent,
                               base::Unretained(this)));
  } else {
    // If measurement is disallowed, just drop the report on the floor. We need
    // to make sure we forward that the report was "sent" to ensure it is
    // deleted from storage, etc. This simulates sending the report through a
    // null channel.
    OnReportSent(SentReportInfo(*report.conversion_id,
                                report.original_report_time,
                                /*report_url=*/GURL(),
                                /*report_body=*/"",
                                /*http_response_code=*/0,
                                /*should_retry*/ false));
  }
  report_queue_.pop();
  MaybeScheduleNextReport();
}

void ConversionReporterImpl::MaybeScheduleNextReport() {
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
      base::BindOnce(&ConversionReporterImpl::SendNextReport,
                     base::Unretained(this)));
}

void ConversionReporterImpl::OnReportSent(SentReportInfo info) {
  auto it = conversion_report_callbacks_.find(info.conversion_id);
  DCHECK(it != conversion_report_callbacks_.end());
  std::move(it->second).Run(std::move(info));
  conversion_report_callbacks_.erase(it);
}

bool ConversionReporterImpl::ReportComparator::operator()(
    const ConversionReport& a,
    const ConversionReport& b) const {
  // Returns whether a should appear before b in ordering. Because
  // std::priority_queue is max priority queue, we used greater then to make a
  // min priority queue.
  return a.report_time > b.report_time;
}

}  // namespace content
