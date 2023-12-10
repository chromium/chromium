// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_ASSEMBLER_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_ASSEMBLER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_key_fetcher.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/common/content_export.h"

template <class T>
class scoped_refptr;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

class AggregationServiceStorageContext;
class StoragePartition;

// This class provides an interface for assembling an aggregatable report. It is
// therefore responsible for taking a request, identifying and requesting the
// appropriate public keys, and generating and returning the AggregatableReport.
class CONTENT_EXPORT AggregatableReportAssembler {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class AssemblyStatus {
    kOk = 0,

    // The attempt to fetch a public key failed.
    kPublicKeyFetchFailed = 1,

    // An internal error occurred while attempting to construct the report.
    kAssemblyFailed = 2,

    // The limit on the number of simultenous requests has been reached.
    kTooManySimultaneousRequests = 3,
    kMaxValue = kTooManySimultaneousRequests,
  };

  using AssemblyCallback =
      base::OnceCallback<void(AggregatableReportRequest,
                              std::optional<AggregatableReport>,
                              AssemblyStatus)>;

  // While we shouldn't hit these limits in typical usage, we protect against
  // the possibility of unbounded memory growth
  static constexpr size_t kMaxSimultaneousRequests = 1000;

  AggregatableReportAssembler(AggregationServiceStorageContext* storage_context,
                              StoragePartition* storage_partition);
  // Not copyable or movable.
  AggregatableReportAssembler(const AggregatableReportAssembler& other) =
      delete;
  AggregatableReportAssembler& operator=(
      const AggregatableReportAssembler& other) = delete;
  virtual ~AggregatableReportAssembler();

  static std::unique_ptr<AggregatableReportAssembler> CreateForTesting(
      std::unique_ptr<AggregationServiceKeyFetcher> fetcher,
      std::unique_ptr<AggregatableReport::Provider> report_provider);

  // Used by the aggregation service tool to inject a `url_loader_factory` to
  // AggregationServiceNetworkFetcherImpl if one is provided.
  static std::unique_ptr<AggregatableReportAssembler> CreateForTesting(
      AggregationServiceStorageContext* storage_context,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      bool enable_debug_logging);

  // Fetches the necessary public keys and uses it to construct an
  // AggregatableReport from the information in `report_request`. See the
  // AggregatableReport documentation for more detail on the returned report.
  virtual void AssembleReport(AggregatableReportRequest report_request,
                              AssemblyCallback callback);

 protected:
  // For testing only.
  AggregatableReportAssembler(
      AggregationServiceStorageContext* storage_context,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      bool enable_debug_logging = false);

 private:
  // Represents a request to assemble a report that has not completed.
  struct PendingRequest {
    PendingRequest(AggregatableReportRequest report_request,
                   AssemblyCallback callback,
                   size_t num_processing_urls);
    // Move-only.
    PendingRequest(PendingRequest&& other);
    PendingRequest& operator=(PendingRequest&& other);
    ~PendingRequest();

    AggregatableReportRequest report_request;
    AssemblyCallback callback;

    // How many key fetches for this request have returned, including errors.
    size_t num_returned_key_fetches = 0;

    // The PublicKey returned for each key fetch request. Indices correspond to
    // the ordering of `report_request.processing_urls`. Each element is
    // `std::nullopt` if that key fetch either has not yet returned or has
    // returned an error.
    std::vector<std::optional<PublicKey>> processing_url_keys;
  };

  AggregatableReportAssembler(
      std::unique_ptr<AggregationServiceKeyFetcher> fetcher,
      std::unique_ptr<AggregatableReport::Provider> report_provider);

  // Called when a result is returned from the key fetcher. Handles throwing
  // errors on a failed fetch, waiting for both results to return and calling
  // into `OnAllPublicKeysFetched()` when appropriate. `processing_url_index` is
  // an index into the corresponding AggregatableReportRequest's
  // `processing_urls` vector, indicating which URL this fetch is for.
  void OnPublicKeyFetched(
      int64_t report_id,
      size_t processing_url_index,
      std::optional<PublicKey> key,
      AggregationServiceKeyFetcher::PublicKeyFetchStatus status);

  // Call when all results have been returned from the key fetcher. Handles
  // calling into `AssembleReportUsingKeys()` when appropriate and returning
  // any assembled report or throwing an error if assembly fails.
  void OnAllPublicKeysFetched(int64_t report_id,
                              PendingRequest& pending_request);

  // Keyed by a token for easier lookup.
  base::flat_map<int64_t, PendingRequest> pending_requests_;

  // Used to generate unique ids for PendingRequests. These need to be unique
  // per Assembler for tracking pending requests.
  int64_t unique_id_counter_ = 0;

  std::unique_ptr<AggregationServiceKeyFetcher> fetcher_;
  std::unique_ptr<AggregatableReport::Provider> report_provider_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_ASSEMBLER_H_
