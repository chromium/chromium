// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_manager_impl.h"

#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/numerics/checked_math.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_budgeter.h"
#include "content/browser/private_aggregation/private_aggregation_host.h"
#include "content/browser/private_aggregation/private_aggregation_utils.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/aggregatable_report.mojom.h"
#include "content/common/private_aggregation_host.mojom.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

namespace {

// The shared task runner for all private aggregation storage operations. Note
// that different PrivateAggregationManagerImpl instances perform operations on
// the same task runner. This prevents any potential races when a given storage
// context is destroyed and recreated using the same backing storage. This uses
// BLOCK_SHUTDOWN as some data deletion operations may be running when the
// browser is closed, and we want to ensure all data is deleted correctly.
base::LazyThreadPoolSequencedTaskRunner g_storage_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::TaskPriority::BEST_EFFORT,
                         base::MayBlock(),
                         base::TaskShutdownBehavior::BLOCK_SHUTDOWN));

}  // namespace

PrivateAggregationManagerImpl::PrivateAggregationManagerImpl(
    bool exclusively_run_in_memory,
    const base::FilePath& user_data_directory,
    StoragePartitionImpl* storage_partition)
    : PrivateAggregationManagerImpl(
          std::make_unique<PrivateAggregationBudgeter>(
              g_storage_task_runner.Get(),
              exclusively_run_in_memory,
              /*path_to_db_dir=*/user_data_directory),
          std::make_unique<PrivateAggregationHost>(
              /*on_report_request_received=*/base::BindRepeating(
                  &PrivateAggregationManagerImpl::
                      OnReportRequestReceivedFromHost,
                  base::Unretained(this)),
              storage_partition ? storage_partition->browser_context()
                                : nullptr),
          storage_partition) {}

PrivateAggregationManagerImpl::PrivateAggregationManagerImpl(
    std::unique_ptr<PrivateAggregationBudgeter> budgeter,
    std::unique_ptr<PrivateAggregationHost> host,
    StoragePartitionImpl* storage_partition)
    : budgeter_(std::move(budgeter)),
      host_(std::move(host)),
      storage_partition_(storage_partition) {
  DCHECK(budgeter_);
  DCHECK(host_);
}

PrivateAggregationManagerImpl::~PrivateAggregationManagerImpl() = default;

bool PrivateAggregationManagerImpl::BindNewReceiver(
    url::Origin worklet_origin,
    url::Origin top_frame_origin,
    PrivateAggregationBudgetKey::Api api_for_budgeting,
    mojo::PendingReceiver<mojom::PrivateAggregationHost> pending_receiver) {
  return host_->BindNewReceiver(std::move(worklet_origin),
                                std::move(top_frame_origin), api_for_budgeting,
                                std::move(pending_receiver));
}

void PrivateAggregationManagerImpl::ClearBudgetData(
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter,
    base::OnceClosure done) {
  budgeter_->ClearData(delete_begin, delete_end, std::move(filter),
                       std::move(done));
}

void PrivateAggregationManagerImpl::OnReportRequestReceivedFromHost(
    AggregatableReportRequest report_request,
    PrivateAggregationBudgetKey budget_key) {
  const std::vector<mojom::AggregatableReportHistogramContribution>&
      contributions = report_request.payload_contents().contributions;

  base::CheckedNumeric<int> budget_needed = std::accumulate(
      contributions.begin(), contributions.end(),
      /*init=*/base::CheckedNumeric<int>(0), /*op=*/
      [](base::CheckedNumeric<int> running_sum,
         const mojom::AggregatableReportHistogramContribution& contribution) {
        return running_sum + contribution.value;
      });

  if (!budget_needed.IsValid()) {
    return;
  }

  PrivateAggregationBudgetKey::Api api_for_budgeting = budget_key.api();

  budgeter_->ConsumeBudget(
      budget_needed.ValueOrDie(), std::move(budget_key), /*on_done=*/
      // Unretained is safe as the `budgeter_` is owned by `this`.
      base::BindOnce(&PrivateAggregationManagerImpl::OnConsumeBudgetReturned,
                     base::Unretained(this), std::move(report_request),
                     api_for_budgeting));
}

AggregationService* PrivateAggregationManagerImpl::GetAggregationService() {
  DCHECK(storage_partition_);
  return AggregationService::GetService(storage_partition_->browser_context());
}

void PrivateAggregationManagerImpl::OnConsumeBudgetReturned(
    AggregatableReportRequest report_request,
    PrivateAggregationBudgetKey::Api api_for_budgeting,
    bool was_budget_use_approved) {
  // TODO(alexmt): Consider adding metrics for success and the different errors
  // here.
  if (!was_budget_use_approved) {
    return;
  }

  AggregationService* aggregation_service = GetAggregationService();
  if (!aggregation_service) {
    return;
  }

  // If the request has debug mode enabled, immediately send a duplicate of the
  // requested report to a special debug reporting endpoint.
  if (report_request.shared_info().debug_mode ==
      AggregatableReportSharedInfo::DebugMode::kEnabled) {
    std::string immediate_debug_reporting_path =
        private_aggregation::GetReportingPath(
            api_for_budgeting,
            /*is_immediate_debug_report=*/true);

    absl::optional<AggregatableReportRequest> debug_request =
        AggregatableReportRequest::Create(
            report_request.payload_contents(),
            report_request.shared_info().Clone(),
            std::move(immediate_debug_reporting_path),
            report_request.debug_key());
    DCHECK(debug_request.has_value());

    aggregation_service->AssembleAndSendReport(
        std::move(debug_request.value()));
  }

  aggregation_service->ScheduleReport(std::move(report_request));
}

}  // namespace content
