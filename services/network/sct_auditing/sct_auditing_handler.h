// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_HANDLER_H_
#define SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_HANDLER_H_

#include <memory>

#include "base/containers/lru_cache.h"
#include "base/files/important_file_writer.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/backoff_entry.h"
#include "net/base/hash_value.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace sct_auditing {
class SCTClientReport;
}  // namespace sct_auditing

namespace network {

class NetworkContext;
class SCTAuditingReporter;

// SCTAuditingHandler owns SCT auditing reports for a specific NetworkContext.
// Each SCTAuditingHandler is owned by its matching NetworkContext. The
// SCTAuditingHandler is also responsible for persisting pending auditing
// reports to disk and loading them back on browser startup.
//
// Note: Persisted reports only repopulate the SCTAuditingHandler's
// `pending_reports_` cache, and *do not* repopulate the SCTAuditingCache's
// deduplication cache.
class COMPONENT_EXPORT(NETWORK_SERVICE) SCTAuditingHandler
    : public base::ImportantFileWriter::DataSerializer {
 public:
  SCTAuditingHandler(NetworkContext* context,
                     const base::FilePath& persistence_path,
                     size_t cache_size = 1024);
  ~SCTAuditingHandler() override;

  SCTAuditingHandler(const SCTAuditingHandler&) = delete;
  SCTAuditingHandler& operator=(const SCTAuditingHandler&) = delete;

  // base::ImportantFileWriter::DataSerializer:
  //
  // Serializes `pending_reporters_` into `*output`. Returns true if all
  // reporters were serialized correctly.
  //
  // The serialization format is a JSON list-of-dicts of the form:
  //   [
  //       {
  //          "reporter_key": <serialized HashValue>,
  //          "report": <serialized SCTClientReport>,
  //          "backoff_entry": <serialized BackoffEntry>
  //       }
  //   ]
  //
  // Each entry in the dictionary includes sufficient information to deserialize
  // and recreate the entries in the SCTAuditingHandler's pending reporters set.
  bool SerializeData(std::string* output) override;

  void DeserializeData(const std::string& serialized);

  void OnStartupFinished();

  // Creates a new SCTAuditingReporter for the report and adds it to this
  // SCTAuditingHandler's pending reporters set. After creating the reporter,
  // this will call SCTAuditingReporter::Start() to initiate sending the report.
  // Optionally takes in a BackoffEntry for recreating reporter state from
  // persisted storage.
  void AddReporter(net::HashValue reporter_key,
                   std::unique_ptr<sct_auditing::SCTClientReport> report,
                   std::unique_ptr<net::BackoffEntry> backoff_entry = nullptr);

  // Loads serialized reports from `serialized` and creates a new
  // SCTAuditingReporter for each (if a reporter for that report does not yet
  // exist). This results in the set of loaded reports being merged with any
  // existing pending reports in the SCTAuditingCache.
  // Returns true if all entries were parsed and deserialized correctly.
  // If data does not deserialize correctly, this drops the entries rather than
  // trying to recover. This means that reports currently (and intentionally,
  // for simplicity) do no persist over format/version changes.
  void OnReportsLoadedFromDisk(const std::string& serialized);

  // Clears the set of pending reporters for this SCTAuditingHandler.
  void ClearPendingReports();

  base::LRUCache<net::HashValue, std::unique_ptr<SCTAuditingReporter>>*
  GetPendingReportersForTesting() {
    return &pending_reporters_;
  }

  void SetEnabled(bool enabled);
  bool is_enabled() { return enabled_; }

  void SetURLLoaderFactoryForTesting(
      mojo::PendingRemote<mojom::URLLoaderFactory> factory) {
    url_loader_factory_.reset();
    url_loader_factory_.Bind(std::move(factory));
  }

  base::ImportantFileWriter* GetFileWriterForTesting() { return writer_.get(); }

  base::WeakPtr<SCTAuditingHandler> GetWeakPtr();

 private:
  void OnReporterStateUpdated();
  void OnReporterFinished(net::HashValue reporter_key);
  void ReportHWMMetrics();
  network::mojom::URLLoaderFactory* GetURLLoaderFactory();

  // The NetworkContext which owns this SCTAuditingHandler.
  NetworkContext* owner_network_context_;

  // The pending reporters set is an LRUCache, so that the total number of
  // pending reporters can be capped. The LRUCache means that reporters will be
  // evicted (and canceled) oldest first. If a new report is triggered for the
  // same SCTs it will get deduplicated if a previous report is still pending,
  // but the last-seen time will be updated.
  base::LRUCache<net::HashValue, std::unique_ptr<SCTAuditingReporter>>
      pending_reporters_;
  // Tracks high-water-mark of `pending_reporters_.size()`.
  size_t pending_reporters_size_hwm_ = 0;

  bool enabled_ = false;
  base::RepeatingTimer histogram_timer_;

  // Helper for safely writing data to disk.
  std::unique_ptr<base::ImportantFileWriter> writer_;

  // Used to send reports.
  mojo::Remote<mojom::URLLoaderFactory> url_loader_factory_;

  base::FilePath persistence_path_;
  scoped_refptr<base::SequencedTaskRunner> foreground_runner_;
  scoped_refptr<base::SequencedTaskRunner> background_runner_;
  bool is_after_startup_ = false;
  bool persisted_reports_read_ = false;

  base::WeakPtrFactory<SCTAuditingHandler> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_HANDLER_H_
