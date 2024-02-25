// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_REPORTER_H_
#define SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_REPORTER_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/backoff_entry.h"
#include "net/base/hash_value.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_service.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/proto/sct_audit_report.pb.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
}

namespace network {

class NetworkContext;
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
  using ReporterDoneCallback = base::OnceCallback<void(net::HashValue)>;
  // Callback to notify the SCTAuditingHandler that the reporter has updated
  // (e.g., the retry counter has been incremented).
  using ReporterUpdatedCallback = base::RepeatingCallback<void()>;

  // For hashdance requests, the client must select an SCT and set its metadata
  // when building an SCTAuditingReporter.
  struct COMPONENT_EXPORT(NETWORK_SERVICE) SCTHashdanceMetadata {
    // Construct an SCTHashdanceMetadata from the given |value|. Returns
    // std::nullopt if |value| cannot be parsed into a valid
    // SCTHashdanceMetadata.
    static std::optional<SCTHashdanceMetadata> FromValue(
        const base::Value& value);

    SCTHashdanceMetadata();
    ~SCTHashdanceMetadata();
    SCTHashdanceMetadata(const SCTHashdanceMetadata&) = delete;
    SCTHashdanceMetadata operator=(const SCTHashdanceMetadata&) = delete;
    SCTHashdanceMetadata(SCTHashdanceMetadata&&);
    SCTHashdanceMetadata& operator=(SCTHashdanceMetadata&&);

    // Returns a base::Value from which the SCTHashdanceMetadata can be
    // reconstructed.
    base::Value ToValue() const;

    // Merkle tree leaf hash.
    std::string leaf_hash;

    // Date and time when this SCT was issued.
    base::Time issued;

    // Corresponding CT Log ID.
    std::string log_id;

    // Corresponding CT Log Maximum Merge Delay.
    base::TimeDelta log_mmd;

    // The certificate expiry date.
    base::Time certificate_expiry;
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class LookupQueryResult {
    // Indicates a network status other than 200 OK.
    kHTTPError = 0,

    // The content returned by the server either did not parse as valid JSON or
    // was missing required fields.
    kInvalidJson = 1,

    // The server returned a `responseStatus` field other than "OK".
    kStatusNotOk = 2,

    // The certificate has expired according to the timestamp returned by the
    // server.
    kCertificateExpired = 3,

    // The server does not know about the log corresponding to the SCT.
    kLogNotFound = 4,

    // The log has not yet ingested the SCT.
    kLogNotYetIngested = 5,

    // The SCT suffix was found in the suffix list, so it should not be
    // reported.
    kSCTSuffixFound = 6,

    // The SCT suffix was NOT found in the suffix list, so it should be
    // reported.
    kSCTSuffixNotFound = 7,
    kMaxValue = kSCTSuffixNotFound,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CompletionStatus {
    kSuccessFirstTry = 0,
    kSuccessAfterRetries = 1,
    kRetriesExhausted = 2,
    kMaxValue = kRetriesExhausted,
  };

  SCTAuditingReporter(
      NetworkContext* owner_network_context_,
      net::HashValue reporter_key,
      std::unique_ptr<sct_auditing::SCTClientReport> report,
      bool is_hashdance,
      std::optional<SCTHashdanceMetadata> hashdance_metadata,
      mojom::SCTAuditingConfigurationPtr configuration,
      mojom::URLLoaderFactory* url_loader_factory,
      ReporterUpdatedCallback update_callback,
      ReporterDoneCallback done_callback,
      std::unique_ptr<net::BackoffEntry> backoff_entry = nullptr,
      bool counted_towards_report_limit = false);
  ~SCTAuditingReporter();

  SCTAuditingReporter(const SCTAuditingReporter&) = delete;
  SCTAuditingReporter& operator=(const SCTAuditingReporter&) = delete;

  static const net::BackoffEntry::Policy kDefaultBackoffPolicy;

  void Start();

  net::HashValue key() { return reporter_key_; }
  sct_auditing::SCTClientReport* report() { return report_.get(); }
  net::BackoffEntry* backoff_entry() { return backoff_entry_.get(); }
  const std::optional<SCTHashdanceMetadata>& sct_hashdance_metadata() {
    return sct_hashdance_metadata_;
  }
  bool counted_towards_report_limit() { return counted_towards_report_limit_; }

  static void SetRetryDelayForTesting(std::optional<base::TimeDelta> delay);

 private:
  void OnCheckReportAllowedStatusComplete(bool allowed);
  // Schedules a |request| using the backoff delay or |minimum_delay|, whichever
  // is greatest.
  void ScheduleRequestWithBackoff(base::OnceClosure request,
                                  base::TimeDelta minimum_delay);
  void SendLookupQuery();
  void OnSendLookupQueryComplete(std::unique_ptr<std::string> response_body);
  void SendReport();
  void OnSendReportComplete(scoped_refptr<net::HttpResponseHeaders> headers);
  void MaybeRetryRequest();

  // The NetworkContext which owns the SCTAuditingHandler that created this
  // Reporter.
  raw_ptr<NetworkContext> owner_network_context_;

  net::HashValue reporter_key_;
  std::unique_ptr<sct_auditing::SCTClientReport> report_;
  bool is_hashdance_;
  // If |is_hashdance_| is true, |sct_hashdance_metadata_| will contain metadata
  // for a randomly selected SCT from the report.
  std::optional<SCTHashdanceMetadata> sct_hashdance_metadata_;
  mojo::Remote<mojom::URLLoaderFactory> url_loader_factory_remote_;
  std::unique_ptr<SimpleURLLoader> url_loader_;
  mojom::SCTAuditingConfigurationPtr configuration_;
  ReporterUpdatedCallback update_callback_;
  ReporterDoneCallback done_callback_;

  net::BackoffEntry::Policy backoff_policy_;
  std::unique_ptr<net::BackoffEntry> backoff_entry_;

  int max_retries_;

  // Whether the report has been counted towards the max-reports limit. This is
  // used to determine whether to notify the embedder that a new report is being
  // sent by the client, to avoid overcounting how many unique reports have been
  // sent. (Without this flag, this could happen on retries if the hashdance
  // lookup query succeeds but then the full report upload fails.)
  bool counted_towards_report_limit_;

  base::WeakPtrFactory<SCTAuditingReporter> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_SCT_AUDITING_SCT_AUDITING_REPORTER_H_
