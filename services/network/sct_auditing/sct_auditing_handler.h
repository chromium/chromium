// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_HANDLER_H_
#define SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_HANDLER_H_

#include <memory>

#include "base/containers/lru_cache.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "net/base/hash_value.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace sct_auditing {
class SCTClientReport;
}  // namespace sct_auditing

namespace network {

class SCTAuditingReporter;

// SCTAuditingHandler owns SCT auditing reports for a specific NetworkContext.
// Each SCTAuditingHandler is owned by its matching NetworkContext.
class SCTAuditingHandler {
 public:
  explicit SCTAuditingHandler(size_t cache_size = 1024);
  ~SCTAuditingHandler();

  SCTAuditingHandler(const SCTAuditingHandler&) = delete;
  SCTAuditingHandler& operator=(const SCTAuditingHandler&) = delete;

  // Creates a new SCTAuditingReporter for the report and adds it to this
  // SCTAuditingHandler's pending reporters set. After creating the reporter,
  // this will call SCTAuditingReporter::Start() to initiate sending the report.
  void AddReporter(
      net::SHA256HashValue,
      std::unique_ptr<sct_auditing::SCTClientReport> report,
      mojom::URLLoaderFactory& url_loader_factory,
      const GURL& report_uri,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);

  // Clears the set of pending reporters for this SCTAuditingHandler.
  void ClearPendingReports();

  base::LRUCache<net::SHA256HashValue, std::unique_ptr<SCTAuditingReporter>>*
  GetPendingReportersForTesting() {
    return &pending_reporters_;
  }

  void SetEnabled(bool enabled);
  bool is_enabled() { return enabled_; }

  base::WeakPtr<SCTAuditingHandler> GetWeakPtr();

 private:
  void OnReporterFinished(net::SHA256HashValue reporter_key);
  void ReportHWMMetrics();

  // The pending reporters set is an LRUCache, so that the total number of
  // pending reporters can be capped. The LRUCache means that reporters will be
  // evicted (and canceled) oldest first. If a new report is triggered for the
  // same SCTs it will get deduplicated if a previous report is still pending,
  // but the last-seen time will be updated.
  base::LRUCache<net::SHA256HashValue, std::unique_ptr<SCTAuditingReporter>>
      pending_reporters_;
  // Tracks high-water-mark of `pending_reporters_.size()`.
  size_t pending_reporters_size_hwm_ = 0;

  bool enabled_ = false;
  base::RepeatingTimer histogram_timer_;

  base::WeakPtrFactory<SCTAuditingHandler> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_HANDLER_H_
