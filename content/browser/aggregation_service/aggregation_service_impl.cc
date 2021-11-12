// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_impl.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/task_traits.h"
#include "base/time/default_clock.h"
#include "content/browser/aggregation_service/aggregatable_report_assembler.h"
#include "content/browser/aggregation_service/aggregation_service_storage_sql.h"
#include "content/browser/storage_partition_impl.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
          std::make_unique<AggregatableReportAssembler>(this,
                                                        storage_partition)) {}

AggregationServiceImpl::~AggregationServiceImpl() = default;

// static
std::unique_ptr<AggregationServiceImpl>
AggregationServiceImpl::CreateForTesting(
    bool run_in_memory,
    const base::FilePath& user_data_directory,
    const base::Clock* clock,
    std::unique_ptr<AggregatableReportAssembler> assembler) {
  return base::WrapUnique<AggregationServiceImpl>(new AggregationServiceImpl(
      run_in_memory, user_data_directory, clock, std::move(assembler)));
}

AggregationServiceImpl::AggregationServiceImpl(
    bool run_in_memory,
    const base::FilePath& user_data_directory,
    const base::Clock* clock,
    std::unique_ptr<AggregatableReportAssembler> assembler)
    : key_storage_(base::SequenceBound<AggregationServiceStorageSql>(
          g_storage_task_runner.Get(),
          run_in_memory,
          user_data_directory,
          clock)),
      assembler_(std::move(assembler)) {}

void AggregationServiceImpl::AssembleReport(
    AggregatableReportRequest report_request,
    AssemblyCallback callback) {
  // `assembler_` is owned by `this`, so `base::Unretained()` is safe.
  assembler_->AssembleReport(
      std::move(report_request),
      base::BindOnce(&AggregationServiceImpl::OnAssembleReportComplete,
                     base::Unretained(this), std::move(callback)));
}

const base::SequenceBound<AggregationServiceKeyStorage>&
AggregationServiceImpl::GetKeyStorage() {
  return key_storage_;
}

void AggregationServiceImpl::OnAssembleReportComplete(
    AssemblyCallback callback,
    absl::optional<AggregatableReport> report,
    AggregatableReportAssembler::AssemblyStatus status) {
  DCHECK_EQ(report.has_value(),
            status == AggregatableReportAssembler::AssemblyStatus::kOk);

  std::move(callback).Run(std::move(report), status);
}

}  // namespace content