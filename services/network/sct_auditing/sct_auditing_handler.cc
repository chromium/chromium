// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sct_auditing/sct_auditing_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "net/base/hash_value.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/proto/sct_audit_report.pb.h"
#include "services/network/sct_auditing/sct_auditing_cache.h"
#include "services/network/sct_auditing/sct_auditing_reporter.h"

namespace network {

SCTAuditingHandler::SCTAuditingHandler(NetworkContext* context,
                                       size_t cache_size)
    : owner_network_context_(context), pending_reporters_(cache_size) {}

SCTAuditingHandler::~SCTAuditingHandler() = default;

void SCTAuditingHandler::AddReporter(
    net::HashValue reporter_key,
    std::unique_ptr<sct_auditing::SCTClientReport> report) {
  if (!enabled_) {
    return;
  }

  // Get the ReportURI and traffic annotation as configured on the
  // SCTAuditingCache.
  auto report_uri = owner_network_context_->network_service()
                        ->sct_auditing_cache()
                        ->report_uri();
  auto traffic_annotation = owner_network_context_->network_service()
                                ->sct_auditing_cache()
                                ->traffic_annotation();

  auto reporter = std::make_unique<SCTAuditingReporter>(
      reporter_key, std::move(report), GetURLLoaderFactory(), report_uri,
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

void SCTAuditingHandler::OnReporterFinished(net::HashValue reporter_key) {
  auto it = pending_reporters_.Get(reporter_key);
  if (it != pending_reporters_.end()) {
    pending_reporters_.Erase(it);
  }
}

void SCTAuditingHandler::ReportHWMMetrics() {
  if (!enabled_) {
    return;
  }
  base::UmaHistogramCounts1000("Security.SCTAuditing.OptIn.ReportersHWM",
                               pending_reporters_size_hwm_);
}

network::mojom::URLLoaderFactory* SCTAuditingHandler::GetURLLoaderFactory() {
  // Create the URLLoaderFactory as needed.
  if (url_loader_factory_ && url_loader_factory_.is_connected()) {
    return url_loader_factory_.get();
  }

  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->process_id = network::mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  params->is_trusted = true;
  params->automatically_assign_isolation_info = true;

  url_loader_factory_.reset();
  owner_network_context_->CreateURLLoaderFactory(
      url_loader_factory_.BindNewPipeAndPassReceiver(), std::move(params));

  return url_loader_factory_.get();
}

}  // namespace network
