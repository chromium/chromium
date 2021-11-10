// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_REPORTER_H_
#define SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_REPORTER_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/backoff_entry.h"
#include "net/base/hash_value.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/proto/sct_audit_report.pb.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
}

namespace network {

class SimpleURLLoader;

// Owns an SCT auditing report and handles sending it and retrying on failures.
// An SCTAuditingReporter begins trying to send the report to the report server
// on creation, retrying over time based on the backoff policy defined in
// `kBackoffPolicy`, and then runs the ReporterDoneCallback provided by the
// SCTAuditingHandler to notify the handler when it has completed (either
// success or running out of retries). If an SCTAuditingReporter instance is
// deleted, any outstanding requests are canceled, pending retry tasks will
// fail, and `done_callback` will not called.
//
// An SCTAuditingReporter instance is uniquely identified by its `reporter_key`
// hash, which is the SHA256HashValue of the set of SCTs included in the
// SCTClientReport that the reporter owns.
class COMPONENT_EXPORT(NETWORK_SERVICE) SCTAuditingReporter {
 public:
  // Callback to notify the SCTAuditingHandler that this reporter has completed.
  // The SHA256HashValue `reporter_key` is passed to uniquely identify this
  // reporter instance.
  using ReporterDoneCallback = base::OnceCallback<void(net::SHA256HashValue)>;

  SCTAuditingReporter(
      net::SHA256HashValue reporter_key,
      std::unique_ptr<sct_auditing::SCTClientReport> report,
      mojom::URLLoaderFactory& url_loader_factory,
      const GURL& report_uri,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      ReporterDoneCallback done_callback);
  ~SCTAuditingReporter();

  SCTAuditingReporter(const SCTAuditingReporter&) = delete;
  SCTAuditingReporter& operator=(const SCTAuditingReporter&) = delete;

  static const net::BackoffEntry::Policy kDefaultBackoffPolicy;

  void Start();

  net::SHA256HashValue key() { return reporter_key_; }
  sct_auditing::SCTClientReport* report() { return report_.get(); }

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CompletionStatus {
    kSuccessFirstTry = 0,
    kSuccessAfterRetries = 1,
    kRetriesExhausted = 2,
    kMaxValue = kRetriesExhausted,
  };

  static void SetRetryDelayForTesting(absl::optional<base::TimeDelta> delay);

 private:
  void ScheduleReport();
  void SendReport();
  void OnSendReportComplete(scoped_refptr<net::HttpResponseHeaders> headers);

  net::SHA256HashValue reporter_key_;
  std::unique_ptr<sct_auditing::SCTClientReport> report_;
  mojo::Remote<mojom::URLLoaderFactory> url_loader_factory_remote_;
  std::unique_ptr<SimpleURLLoader> url_loader_;
  net::NetworkTrafficAnnotationTag traffic_annotation_;
  GURL report_uri_;
  ReporterDoneCallback done_callback_;

  net::BackoffEntry::Policy backoff_policy_;
  std::unique_ptr<net::BackoffEntry> backoff_entry_;

  size_t num_retries_;
  size_t max_retries_;

  base::WeakPtrFactory<SCTAuditingReporter> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_REPORTER_H_
