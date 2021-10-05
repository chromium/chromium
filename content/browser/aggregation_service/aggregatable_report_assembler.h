// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_ASSEMBLER_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_ASSEMBLER_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_key_fetcher.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

class AggregatableReportManager;
class StoragePartition;

// This class provides an interface for assembling an aggregatable report. It is
// therefore responsible for taking a request, identifying and requesting the
// appropriate public keys, and generating and returning the AggregatableReport.
class CONTENT_EXPORT AggregatableReportAssembler {
 public:
  enum class AssemblyStatus {
    kOk,

    // The attempt to fetch a public key failed.
    kPublicKeyFetchFailed,

    // An internal error occurred while attempting to construct the report.
    kAssemblyFailed,

    // The limit on the number of simultenous requests has been reached.
    kTooManySimultaneousRequests,
    kMaxValue = kTooManySimultaneousRequests,
  };

  using AssemblyCallback =
      base::OnceCallback<void(absl::optional<AggregatableReport>,
                              AssemblyStatus)>;

  // While we shouldn't hit these limits in typical usage, we protect against
  // the possibility of unbounded memory growth
  static constexpr size_t kMaxSimultaneousRequests = 1000;

  explicit AggregatableReportAssembler(AggregatableReportManager* manager,
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

  // Fetches the necessary public keys and uses it to construct an
  // AggregatableReport from the information in `report_request`. See the
  // AggregatableReport documentation for more detail on the returned report.
  void AssembleReport(AggregatableReportRequest report_request,
                      AssemblyCallback callback);

 private:
  // Represents a request to assemble a report that has not completed.
  struct PendingRequest {
    PendingRequest(AggregatableReportRequest report_request,
                   AssemblyCallback callback,
                   size_t num_processing_origins);
    // Move-only.
    PendingRequest(PendingRequest&& other);
    PendingRequest& operator=(PendingRequest&& other);
    ~PendingRequest();

    AggregatableReportRequest report_request;
    AssemblyCallback callback;

    // How many key fetches for this request have returned, including errors.
    size_t num_returned_key_fetches = 0;

    // The PublicKey returned for each key fetch request. Indices correspond to
    // the ordering of `report_request.processing_origins`. Each element is
    // `absl::nullopt` if that key fetch either has not yet returned or has
    // returned an error.
    std::vector<absl::optional<PublicKey>> processing_origin_keys;
  };

  AggregatableReportAssembler(
      std::unique_ptr<AggregationServiceKeyFetcher> fetcher,
      std::unique_ptr<AggregatableReport::Provider> report_provider);

  // Called when a result is returned from the key fetcher. Handles throwing
  // errors on a failed fetch, waiting for both results to return and calling
  // into `OnBothPublicKeysFetched()` when appropriate.
  // `processing_origin_index` is an index into the corresponding
  // AggregatableReportRequest's `processing_origins` vector, indicating which
  // origin this fetch is for.
  void OnPublicKeyFetched(
      int64_t report_id,
      size_t processing_origin_index,
      absl::optional<PublicKey> key,
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
