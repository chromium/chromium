// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_manager_impl.h"

#include <stddef.h>

#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/updateable_sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_budgeter.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"
#include "content/browser/private_aggregation/private_aggregation_features.h"
#include "content/browser/private_aggregation/private_aggregation_host.h"
#include "content/browser/private_aggregation/private_aggregation_utils.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/private_aggregation_data_model.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

void RecordBudgeterResultHistogram(
    PrivateAggregationBudgeter::RequestResult request_result) {
  base::UmaHistogramEnumeration(
      "PrivacySandbox.PrivateAggregation.Budgeter.RequestResult3",
      request_result);
}

void RecordManagerResultHistogram(
    PrivateAggregationManagerImpl::RequestResult request_result) {
  base::UmaHistogramEnumeration(
      "PrivacySandbox.PrivateAggregation.Manager.RequestResult",
      request_result);
}

}  // namespace

PrivateAggregationManagerImpl::PrivateAggregationManagerImpl(
    bool exclusively_run_in_memory,
    const base::FilePath& user_data_directory,
    StoragePartitionImpl* storage_partition)
    : PrivateAggregationManagerImpl(
          std::make_unique<PrivateAggregationBudgeter>(
              // This uses BLOCK_SHUTDOWN as some data deletion operations may
              // be running when the browser is closed, and we want to ensure
              // all data is deleted correctly. Additionally, we use
              // MUST_USE_FOREGROUND to avoid priority inversions if a task is
              // already running when the priority is increased.
              base::ThreadPool::CreateUpdateableSequencedTaskRunner(
                  base::TaskTraits(base::TaskPriority::BEST_EFFORT,
                                   base::MayBlock(),
                                   base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
                                   base::ThreadPolicy::MUST_USE_FOREGROUND)),
              exclusively_run_in_memory,
              /*path_to_db_dir=*/user_data_directory),
          std::make_unique<PrivateAggregationHost>(
              /*on_report_request_details_received=*/base::BindRepeating(
                  &PrivateAggregationManagerImpl::
                      OnReportRequestDetailsReceivedFromHost,
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
  CHECK(budgeter_);
  CHECK(host_);
}

PrivateAggregationManagerImpl::~PrivateAggregationManagerImpl() = default;

bool PrivateAggregationManagerImpl::BindNewReceiver(
    url::Origin worklet_origin,
    url::Origin top_frame_origin,
    PrivateAggregationCallerApi api_for_budgeting,
    std::optional<std::string> context_id,
    std::optional<base::TimeDelta> timeout,
    std::optional<url::Origin> aggregation_coordinator_origin,
    size_t filtering_id_max_bytes,
    mojo::PendingReceiver<blink::mojom::PrivateAggregationHost>
        pending_receiver) {
  return host_->BindNewReceiver(
      std::move(worklet_origin), std::move(top_frame_origin), api_for_budgeting,
      std::move(context_id), std::move(timeout),
      std::move(aggregation_coordinator_origin), filtering_id_max_bytes,
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

bool PrivateAggregationManagerImpl::IsDebugModeAllowed(
    const url::Origin& top_frame_origin,
    const url::Origin& reporting_origin) {
  return host_->IsDebugModeAllowed(top_frame_origin, reporting_origin);
}

void PrivateAggregationManagerImpl::OnReportRequestDetailsReceivedFromHost(
    PrivateAggregationHost::ReportRequestGenerator report_request_generator,
    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions,
    PrivateAggregationBudgetKey budget_key,
    PrivateAggregationBudgeter::BudgetDeniedBehavior budget_denied_behavior) {
  base::CheckedNumeric<int> budget_needed = std::accumulate(
      contributions.begin(), contributions.end(),
      /*init=*/base::CheckedNumeric<int>(0), /*op=*/
      [](base::CheckedNumeric<int> running_sum,
         const blink::mojom::AggregatableReportHistogramContribution&
             contribution) { return running_sum + contribution.value; });

  PrivateAggregationCallerApi api_for_budgeting = budget_key.api();

  if (!budget_needed.IsValid()) {
    OnConsumeBudgetReturned(std::move(report_request_generator),
                            std::move(contributions), api_for_budgeting,
                            budget_denied_behavior,
                            PrivateAggregationBudgeter::RequestResult::
                                kRequestedMoreThanTotalBudget);
    return;
  }

  // No need to request budget if none is needed.
  if (budget_needed.ValueOrDie() == 0) {
    RecordManagerResultHistogram(RequestResult::kSentWithoutContributions);
    OnContributionsFinalized(std::move(report_request_generator),
                             std::move(contributions), api_for_budgeting);
    return;
  }

  CHECK(!contributions.empty());
  int minimum_value_for_metrics =
      base::ranges::min(
          contributions, /*comp=*/{}, /*proj=*/
          &blink::mojom::AggregatableReportHistogramContribution::value)
          .value;

  budgeter_->ConsumeBudget(
      budget_needed.ValueOrDie(), std::move(budget_key),
      minimum_value_for_metrics, /*on_done=*/
      // Unretained is safe as the `budgeter_` is owned by `this`.
      base::BindOnce(
          &PrivateAggregationManagerImpl::OnConsumeBudgetReturned,
          base::Unretained(this), std::move(report_request_generator),
          std::move(contributions), api_for_budgeting, budget_denied_behavior));
}

AggregationService* PrivateAggregationManagerImpl::GetAggregationService() {
  CHECK(storage_partition_);
  return AggregationService::GetService(storage_partition_->browser_context());
}

void PrivateAggregationManagerImpl::OnConsumeBudgetReturned(
    PrivateAggregationHost::ReportRequestGenerator report_request_generator,
    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions,
    PrivateAggregationCallerApi api_for_budgeting,
    PrivateAggregationBudgeter::BudgetDeniedBehavior budget_denied_behavior,
    PrivateAggregationBudgeter::RequestResult request_result) {
  RecordBudgeterResultHistogram(request_result);

  // TODO(crbug.com/355271550): Consider allowing a subset of contributions to
  // be sent if there's insufficient budget for them all.
  if (request_result == PrivateAggregationBudgeter::RequestResult::kApproved) {
    CHECK(!contributions.empty());
    RecordManagerResultHistogram(RequestResult::kSentWithContributions);
  } else {
    switch (budget_denied_behavior) {
      case PrivateAggregationBudgeter::BudgetDeniedBehavior::kDontSendReport:
        RecordManagerResultHistogram(RequestResult::kNotSent);
        return;
      case PrivateAggregationBudgeter::BudgetDeniedBehavior::kSendNullReport:
        RecordManagerResultHistogram(
            RequestResult::kSentButContributionsClearedDueToBudgetDenial);
        contributions.clear();
        break;
    }
  }

  OnContributionsFinalized(std::move(report_request_generator),
                           std::move(contributions), api_for_budgeting);
}

void PrivateAggregationManagerImpl::OnContributionsFinalized(
    PrivateAggregationHost::ReportRequestGenerator report_request_generator,
    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions,
    PrivateAggregationCallerApi api_for_budgeting) {
  AggregationService* aggregation_service = GetAggregationService();
  if (!aggregation_service) {
    return;
  }

  AggregatableReportRequest report_request =
      std::move(report_request_generator).Run(std::move(contributions));

  // If the request has debug mode enabled, immediately send a duplicate of the
  // requested report to a special debug reporting endpoint.
  if (report_request.shared_info().debug_mode ==
      AggregatableReportSharedInfo::DebugMode::kEnabled) {
    std::string immediate_debug_reporting_path =
        private_aggregation::GetReportingPath(
            api_for_budgeting,
            /*is_immediate_debug_report=*/true);

    std::optional<AggregatableReportRequest> debug_request =
        AggregatableReportRequest::Create(
            report_request.payload_contents(),
            report_request.shared_info().Clone(),
            AggregatableReportRequest::DelayType::Unscheduled,
            std::move(immediate_debug_reporting_path),
            report_request.debug_key(), report_request.additional_fields());
    CHECK(debug_request.has_value());

    aggregation_service->AssembleAndSendReport(
        std::move(debug_request.value()));
  }

  aggregation_service->ScheduleReport(std::move(report_request));
}

void PrivateAggregationManagerImpl::GetAllDataKeys(
    base::OnceCallback<void(std::set<DataKey>)> callback) {
  budgeter_->GetAllDataKeys(base::BindOnce(
      &PrivateAggregationManagerImpl::OnBudgeterGetAllDataKeysReturned,
      weak_factory_.GetWeakPtr(), std::move(callback)));
}

void PrivateAggregationManagerImpl::OnBudgeterGetAllDataKeysReturned(
    base::OnceCallback<void(std::set<DataKey>)> callback,
    std::set<DataKey> all_keys) {
  AggregationService* aggregation_service = GetAggregationService();
  if (!aggregation_service) {
    std::move(callback).Run(std::move(all_keys));
    return;
  }

  aggregation_service->GetPendingReportReportingOrigins(base::BindOnce(
      [](base::OnceCallback<void(std::set<DataKey>)> callback,
         std::set<DataKey> all_keys, std::set<url::Origin> pending_origins) {
        base::ranges::transform(
            std::make_move_iterator(pending_origins.begin()),
            std::make_move_iterator(pending_origins.end()),
            std::inserter(all_keys, all_keys.begin()), [](url::Origin elem) {
              return PrivateAggregationDataModel::DataKey(std::move(elem));
            });
        std::move(callback).Run(std::move(all_keys));
      },
      std::move(callback), std::move(all_keys)));
}

void PrivateAggregationManagerImpl::RemovePendingDataKey(
    const DataKey& data_key,
    base::OnceClosure callback) {
  base::RepeatingClosure barrier = base::BarrierClosure(2, std::move(callback));
  budgeter_->DeleteByDataKey(data_key, barrier);
  AggregationService* aggregation_service = GetAggregationService();
  if (!aggregation_service) {
    std::move(barrier).Run();
    return;
  }

  aggregation_service->ClearData(
      /*delete_begin=*/base::Time::Min(), /*delete_end=*/base::Time::Max(),
      /*filter=*/
      base::BindRepeating(
          std::equal_to<blink::StorageKey>(),
          blink::StorageKey::CreateFirstParty(data_key.reporting_origin())),
      /*done=*/std::move(barrier));
}

}  // namespace content
