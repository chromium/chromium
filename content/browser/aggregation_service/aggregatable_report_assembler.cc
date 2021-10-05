// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregatable_report_assembler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/time/default_clock.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregatable_report_manager.h"
#include "content/browser/aggregation_service/aggregation_service_key_fetcher.h"
#include "content/browser/aggregation_service/aggregation_service_network_fetcher_impl.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

AggregatableReportAssembler::AggregatableReportAssembler(
    AggregatableReportManager* manager,
    StoragePartition* storage_partition)
    : AggregatableReportAssembler(
          std::make_unique<AggregationServiceKeyFetcher>(
              manager,
              std::make_unique<AggregationServiceNetworkFetcherImpl>(
                  base::DefaultClock::GetInstance(),
                  storage_partition)),
          std::make_unique<AggregatableReport::Provider>()) {}

AggregatableReportAssembler::AggregatableReportAssembler(
    std::unique_ptr<AggregationServiceKeyFetcher> fetcher,
    std::unique_ptr<AggregatableReport::Provider> report_provider)
    : fetcher_(std::move(fetcher)),
      report_provider_(std::move(report_provider)) {}

AggregatableReportAssembler::~AggregatableReportAssembler() = default;

AggregatableReportAssembler::PendingRequest::PendingRequest(
    AggregatableReportRequest report_request,
    AggregatableReportAssembler::AssemblyCallback callback,
    size_t num_processing_origins)
    : report_request(std::move(report_request)),
      callback(std::move(callback)),
      processing_origin_keys(num_processing_origins) {
  DCHECK(this->callback);
}

AggregatableReportAssembler::PendingRequest::PendingRequest(
    AggregatableReportAssembler::PendingRequest&& other) = default;

AggregatableReportAssembler::PendingRequest&
AggregatableReportAssembler::PendingRequest::operator=(
    AggregatableReportAssembler::PendingRequest&& other) = default;

AggregatableReportAssembler::PendingRequest::~PendingRequest() = default;

// static
std::unique_ptr<AggregatableReportAssembler>
AggregatableReportAssembler::CreateForTesting(
    std::unique_ptr<AggregationServiceKeyFetcher> fetcher,
    std::unique_ptr<AggregatableReport::Provider> report_provider) {
  return base::WrapUnique(new AggregatableReportAssembler(
      std::move(fetcher), std::move(report_provider)));
}

void AggregatableReportAssembler::AssembleReport(
    AggregatableReportRequest report_request,
    AssemblyCallback callback) {
  DCHECK_EQ(report_request.processing_origins().size(),
            AggregatableReport::kNumberOfProcessingOrigins);
  DCHECK(base::ranges::is_sorted(report_request.processing_origins()));

  const AggregationServicePayloadContents& contents =
      report_request.payload_contents();

  // Currently, these should be the only possible enum values.
  DCHECK_EQ(contents.operation,
            AggregationServicePayloadContents::Operation::kCountValueHistogram);
  DCHECK_EQ(contents.processing_type,
            AggregationServicePayloadContents::ProcessingType::kTwoParty);

  if (pending_requests_.size() >= kMaxSimultaneousRequests) {
    std::move(callback).Run(absl::nullopt,
                            AssemblyStatus::kTooManySimultaneousRequests);
    return;
  }

  int64_t id = unique_id_counter_++;
  DCHECK(!base::Contains(pending_requests_, id));

  const PendingRequest& pending_request =
      pending_requests_
          .emplace(id, PendingRequest(
                           std::move(report_request), std::move(callback),
                           /*num_processing_origins=*/
                           AggregatableReport::kNumberOfProcessingOrigins))
          .first->second;

  for (size_t i = 0; i < AggregatableReport::kNumberOfProcessingOrigins; ++i) {
    // `fetcher_` is owned by `this`, so `base::Unretained()` is safe.
    fetcher_->GetPublicKey(
        pending_request.report_request.processing_origins()[i],
        base::BindOnce(&AggregatableReportAssembler::OnPublicKeyFetched,
                       base::Unretained(this), /*report_id=*/id,
                       /*processing_origin_index=*/i));
  }
}

void AggregatableReportAssembler::OnPublicKeyFetched(
    int64_t report_id,
    size_t processing_origin_index,
    absl::optional<PublicKey> key,
    AggregationServiceKeyFetcher::PublicKeyFetchStatus status) {
  DCHECK_EQ(key.has_value(),
            status == AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk);
  auto pending_request_it = pending_requests_.find(report_id);

  // This should only be possible if we have already thrown an error.
  if (pending_request_it == pending_requests_.end())
    return;

  PendingRequest& pending_request = pending_request_it->second;

  // TODO(crbug.com/1254792): Consider implementing some retry logic.
  if (!key.has_value()) {
    std::move(pending_request.callback)
        .Run(absl::nullopt, AssemblyStatus::kPublicKeyFetchFailed);
    pending_requests_.erase(pending_request_it);
    return;
  }

  ++pending_request.num_returned_key_fetches;
  pending_request.processing_origin_keys[processing_origin_index] =
      std::move(key);

  if (pending_request.num_returned_key_fetches ==
      AggregatableReport::kNumberOfProcessingOrigins) {
    OnAllPublicKeysFetched(report_id, pending_request);
  }
}

void AggregatableReportAssembler::OnAllPublicKeysFetched(
    int64_t report_id,
    PendingRequest& pending_request) {
  std::vector<PublicKey> public_keys;
  for (absl::optional<PublicKey> elem :
       pending_request.processing_origin_keys) {
    DCHECK(elem.has_value());
    public_keys.push_back(std::move(elem.value()));
  }

  absl::optional<AggregatableReport> assembled_report =
      report_provider_->CreateFromRequestAndPublicKeys(
          std::move(pending_request.report_request), std::move(public_keys));
  AssemblyStatus assembly_status =
      assembled_report ? AssemblyStatus::kOk : AssemblyStatus::kAssemblyFailed;
  std::move(pending_request.callback)
      .Run(std::move(assembled_report), assembly_status);

  pending_requests_.erase(report_id);
}

}  // namespace content
