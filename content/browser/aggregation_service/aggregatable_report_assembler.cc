// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregatable_report_assembler.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/time/default_clock.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_key_fetcher.h"
#include "content/browser/aggregation_service/aggregation_service_network_fetcher_impl.h"
#include "content/browser/aggregation_service/aggregation_service_storage_context.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {

namespace {

void RecordAssemblyStatus(AggregatableReportAssembler::AssemblyStatus status) {
  base::UmaHistogramEnumeration(
      "PrivacySandbox.AggregationService.ReportAssembler.Status", status);
}

}  // namespace

AggregatableReportAssembler::AggregatableReportAssembler(
    AggregationServiceStorageContext* storage_context,
    StoragePartition* storage_partition)
    : AggregatableReportAssembler(
          std::make_unique<AggregationServiceKeyFetcher>(
              storage_context,
              std::make_unique<AggregationServiceNetworkFetcherImpl>(
                  base::DefaultClock::GetInstance(),
                  storage_partition)),
          std::make_unique<AggregatableReport::Provider>()) {}

AggregatableReportAssembler::AggregatableReportAssembler(
    AggregationServiceStorageContext* storage_context,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    bool enable_debug_logging)
    : AggregatableReportAssembler(
          std::make_unique<AggregationServiceKeyFetcher>(
              storage_context,
              AggregationServiceNetworkFetcherImpl::
                  CreateForTesting(  // IN-TEST
                      base::DefaultClock::GetInstance(),
                      std::move(url_loader_factory),
                      enable_debug_logging)),
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
    size_t num_processing_urls)
    : report_request(std::move(report_request)),
      callback(std::move(callback)),
      processing_url_keys(num_processing_urls) {
  CHECK(this->callback);
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

// static
std::unique_ptr<AggregatableReportAssembler>
AggregatableReportAssembler::CreateForTesting(
    AggregationServiceStorageContext* storage_context,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    bool enable_debug_logging) {
  return base::WrapUnique(new AggregatableReportAssembler(
      storage_context, std::move(url_loader_factory), enable_debug_logging));
}

void AggregatableReportAssembler::AssembleReport(
    AggregatableReportRequest report_request,
    AssemblyCallback callback) {
  CHECK(base::ranges::is_sorted(report_request.processing_urls()));
  const size_t num_processing_urls = report_request.processing_urls().size();
  CHECK(AggregatableReport::IsNumberOfProcessingUrlsValid(
      num_processing_urls, report_request.payload_contents().aggregation_mode));

  const AggregationServicePayloadContents& contents =
      report_request.payload_contents();

  // Currently, this is the only supported operation.
  CHECK_EQ(contents.operation,
           AggregationServicePayloadContents::Operation::kHistogram);

  if (pending_requests_.size() >= kMaxSimultaneousRequests) {
    RecordAssemblyStatus(AssemblyStatus::kTooManySimultaneousRequests);

    std::move(callback).Run(std::move(report_request), std::nullopt,
                            AssemblyStatus::kTooManySimultaneousRequests);
    return;
  }

  int64_t id = unique_id_counter_++;
  CHECK(!base::Contains(pending_requests_, id));

  const PendingRequest& pending_request =
      pending_requests_
          .emplace(id, PendingRequest(std::move(report_request),
                                      std::move(callback), num_processing_urls))
          .first->second;

  for (size_t i = 0; i < num_processing_urls; ++i) {
    // `fetcher_` is owned by `this`, so `base::Unretained()` is safe.
    fetcher_->GetPublicKey(
        pending_request.report_request.processing_urls()[i],
        base::BindOnce(&AggregatableReportAssembler::OnPublicKeyFetched,
                       base::Unretained(this), /*report_id=*/id,
                       /*processing_url_index=*/i));
  }
}

void AggregatableReportAssembler::OnPublicKeyFetched(
    int64_t report_id,
    size_t processing_url_index,
    std::optional<PublicKey> key,
    AggregationServiceKeyFetcher::PublicKeyFetchStatus status) {
  CHECK_EQ(key.has_value(),
           status == AggregationServiceKeyFetcher::PublicKeyFetchStatus::kOk);
  auto pending_request_it = pending_requests_.find(report_id);
  CHECK(pending_request_it != pending_requests_.end());

  PendingRequest& pending_request = pending_request_it->second;

  // TODO(crbug.com/40199738): Consider implementing some retry logic.

  ++pending_request.num_returned_key_fetches;
  pending_request.processing_url_keys[processing_url_index] = std::move(key);

  if (pending_request.num_returned_key_fetches ==
      pending_request.report_request.processing_urls().size()) {
    OnAllPublicKeysFetched(report_id, pending_request);
  }
}

void AggregatableReportAssembler::OnAllPublicKeysFetched(
    int64_t report_id,
    PendingRequest& pending_request) {
  std::vector<PublicKey> public_keys;
  for (std::optional<PublicKey> elem : pending_request.processing_url_keys) {
    if (!elem.has_value()) {
      RecordAssemblyStatus(AssemblyStatus::kPublicKeyFetchFailed);

      std::move(pending_request.callback)
          .Run(std::move(pending_request.report_request), std::nullopt,
               AssemblyStatus::kPublicKeyFetchFailed);
      pending_requests_.erase(report_id);
      return;
    }

    public_keys.push_back(std::move(elem.value()));
  }

  std::optional<AggregatableReport> assembled_report =
      report_provider_->CreateFromRequestAndPublicKeys(
          pending_request.report_request, std::move(public_keys));
  AssemblyStatus assembly_status =
      assembled_report ? AssemblyStatus::kOk : AssemblyStatus::kAssemblyFailed;
  RecordAssemblyStatus(assembly_status);

  std::move(pending_request.callback)
      .Run(std::move(pending_request.report_request),
           std::move(assembled_report), assembly_status);

  pending_requests_.erase(report_id);
}

}  // namespace content
