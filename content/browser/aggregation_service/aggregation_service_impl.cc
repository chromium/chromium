// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_impl.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/task_traits.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/aggregation_service/aggregatable_report_assembler.h"
#include "content/browser/aggregation_service/aggregatable_report_scheduler.h"
#include "content/browser/aggregation_service/aggregatable_report_sender.h"
#include "content/browser/aggregation_service/aggregation_service_storage_sql.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/storage_partition.h"
#include "url/gurl.h"

namespace content {

namespace {

// The shared task runner for all aggregation service storage operations. Note
// that different AggregationServiceImpl instances perform operations on the
// same task runner. This prevents any potential races when a given storage
// context is destroyed and recreated using the same backing storage. This uses
// BLOCK_SHUTDOWN as some data deletion operations may be running when the
// browser is closed, and we want to ensure all data is deleted correctly.
base::LazyThreadPoolSequencedTaskRunner g_storage_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::TaskPriority::BEST_EFFORT,
                         base::MayBlock(),
                         base::TaskShutdownBehavior::BLOCK_SHUTDOWN));

}  // namespace

AggregationServiceImpl::AggregationServiceImpl(
    bool run_in_memory,
    const base::FilePath& user_data_directory,
    StoragePartitionImpl* storage_partition)
    : AggregationServiceImpl(
          run_in_memory,
          user_data_directory,
          base::DefaultClock::GetInstance(),
          std::make_unique<AggregatableReportScheduler>(
              /*storage_context=*/this,
              // `base::Unretained` is safe as the scheduler is owned by `this`.
              base::BindRepeating(
                  &AggregationServiceImpl::OnScheduledReportTimeReached,
                  base::Unretained(this))),
          std::make_unique<AggregatableReportAssembler>(this,
                                                        storage_partition),
          std::make_unique<AggregatableReportSender>(storage_partition)) {}

AggregationServiceImpl::~AggregationServiceImpl() = default;

// static
std::unique_ptr<AggregationServiceImpl>
AggregationServiceImpl::CreateForTesting(
    bool run_in_memory,
    const base::FilePath& user_data_directory,
    const base::Clock* clock,
    std::unique_ptr<AggregatableReportScheduler> scheduler,
    std::unique_ptr<AggregatableReportAssembler> assembler,
    std::unique_ptr<AggregatableReportSender> sender) {
  return base::WrapUnique<AggregationServiceImpl>(new AggregationServiceImpl(
      run_in_memory, user_data_directory, clock, std::move(scheduler),
      std::move(assembler), std::move(sender)));
}

AggregationServiceImpl::AggregationServiceImpl(
    bool run_in_memory,
    const base::FilePath& user_data_directory,
    const base::Clock* clock,
    std::unique_ptr<AggregatableReportScheduler> scheduler,
    std::unique_ptr<AggregatableReportAssembler> assembler,
    std::unique_ptr<AggregatableReportSender> sender)
    : scheduler_(std::move(scheduler)),
      storage_(base::SequenceBound<AggregationServiceStorageSql>(
          g_storage_task_runner.Get(),
          run_in_memory,
          user_data_directory,
          clock)),
      assembler_(std::move(assembler)),
      sender_(std::move(sender)) {}

void AggregationServiceImpl::AssembleReport(
    AggregatableReportRequest report_request,
    AssemblyCallback callback) {
  assembler_->AssembleReport(std::move(report_request), std::move(callback));
}

void AggregationServiceImpl::SendReport(const GURL& url,
                                        const AggregatableReport& report,
                                        SendCallback callback) {
  SendReport(url, base::Value(report.GetAsJson()), std::move(callback));
}

void AggregationServiceImpl::SendReport(const GURL& url,
                                        const base::Value& contents,
                                        SendCallback callback) {
  sender_->SendReport(url, contents, std::move(callback));
}

const base::SequenceBound<AggregationServiceStorage>&
AggregationServiceImpl::GetStorage() {
  return storage_;
}

void AggregationServiceImpl::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter,
    base::OnceClosure done) {
  storage_.AsyncCall(&AggregationServiceStorage::ClearDataBetween)
      .WithArgs(delete_begin, delete_end, std::move(filter))
      .Then(std::move(done));
}

void AggregationServiceImpl::ScheduleReport(
    AggregatableReportRequest report_request) {
  scheduler_->ScheduleRequest(std::move(report_request));
}

void AggregationServiceImpl::OnScheduledReportTimeReached(
    std::vector<AggregationServiceStorage::RequestAndId> requests_and_ids) {
  for (AggregationServiceStorage::RequestAndId& elem : requests_and_ids) {
    GURL reporting_url = elem.request.GetReportingUrl();
    AssembleReport(
        std::move(elem.request),
        base::BindOnce(
            &AggregationServiceImpl::OnReportAssemblyComplete,
            // `base::Unretained` is safe as the assembler is owned by `this`.
            base::Unretained(this), elem.id, std::move(reporting_url)));
  }
}

void AggregationServiceImpl::OnReportAssemblyComplete(
    AggregationServiceStorage::RequestId request_id,
    GURL reporting_url,
    absl::optional<AggregatableReport> report,
    AggregatableReportAssembler::AssemblyStatus status) {
  DCHECK_EQ(report.has_value(),
            status == AggregatableReportAssembler::AssemblyStatus::kOk);
  if (!report.has_value()) {
    scheduler_->NotifyInProgressRequestFailed(request_id);
    return;
  }

  SendReport(reporting_url, report.value(),
             /*callback=*/
             base::BindOnce(
                 &AggregationServiceImpl::OnReportSendingComplete,
                 // `base::Unretained` is safe as the sender is owned by `this`.
                 base::Unretained(this), request_id));
}

void AggregationServiceImpl::OnReportSendingComplete(
    AggregationServiceStorage::RequestId request_id,
    AggregatableReportSender::RequestStatus status) {
  if (status == AggregatableReportSender::RequestStatus::kOk) {
    scheduler_->NotifyInProgressRequestSucceeded(request_id);
  } else {
    scheduler_->NotifyInProgressRequestFailed(request_id);
  }
}

void AggregationServiceImpl::SetPublicKeysForTesting(
    const GURL& url,
    const PublicKeyset& keyset) {
  storage_.AsyncCall(&AggregationServiceStorage::SetPublicKeys)
      .WithArgs(url, keyset);
}

}  // namespace content