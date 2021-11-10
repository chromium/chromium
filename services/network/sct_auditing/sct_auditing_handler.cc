// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sct_auditing/sct_auditing_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "net/base/hash_value.h"
#include "services/network/public/proto/sct_audit_report.pb.h"
#include "services/network/sct_auditing/sct_auditing_reporter.h"

namespace network {

SCTAuditingHandler::SCTAuditingHandler(size_t cache_size)
    : pending_reporters_(cache_size) {}

SCTAuditingHandler::~SCTAuditingHandler() = default;

void SCTAuditingHandler::AddReporter(
    net::SHA256HashValue reporter_key,
    std::unique_ptr<sct_auditing::SCTClientReport> report,
    mojom::URLLoaderFactory& url_loader_factory,
    const GURL& report_uri,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (!enabled_) {
    return;
  }

  auto reporter = std::make_unique<SCTAuditingReporter>(
      reporter_key, std::move(report), url_loader_factory, report_uri,
      traffic_annotation,
      base::BindOnce(&SCTAuditingHandler::OnReporterFinished, GetWeakPtr()));
  reporter->Start();
  pending_reporters_.Put(reporter->key(), std::move(reporter));

  if (pending_reporters_.size() > pending_reporters_size_hwm_)
    pending_reporters_size_hwm_ = pending_reporters_.size();
}

void SCTAuditingHandler::ClearPendingReports() {
  // Delete any outstanding Reporters. This will delete any extant URLLoader
  // instances owned by the Reporters, which will cancel any outstanding
  // requests/connections. Pending (delayed) retry tasks will fast-fail when
  // they trigger as they use a WeakPtr to the Reporter instance that posted the
  // task.
  pending_reporters_.Clear();
  // TODO(crbug.com/1144205): Clear any persisted state.
}

void SCTAuditingHandler::SetEnabled(bool enabled) {
  enabled_ = enabled;

  // High-water-mark metrics get logged hourly (rather than once-per-session at
  // shutdown, as Network Service shutdown is not consistent and non-browser
  // processes can fail to report metrics during shutdown). The timer should
  // only be running if SCT auditing is enabled.
  if (enabled) {
    histogram_timer_.Start(FROM_HERE, base::Hours(1), this,
                           &SCTAuditingHandler::ReportHWMMetrics);
  } else {
    histogram_timer_.Stop();
    ClearPendingReports();
  }
}

base::WeakPtr<SCTAuditingHandler> SCTAuditingHandler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void SCTAuditingHandler::OnReporterFinished(net::SHA256HashValue reporter_key) {
  auto it = pending_reporters_.Get(reporter_key);
  if (it != pending_reporters_.end()) {
    pending_reporters_.Erase(it);
  }
  // TODO(crbug.com/1144205): Delete any persisted state for the reporter.
}

void SCTAuditingHandler::ReportHWMMetrics() {
  if (!enabled_) {
    return;
  }
  base::UmaHistogramCounts1000("Security.SCTAuditing.OptIn.ReportersHWM",
                               pending_reporters_size_hwm_);
}

}  // namespace network
