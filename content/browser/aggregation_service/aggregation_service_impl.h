// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_IMPL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/aggregation_service/aggregatable_report_assembler.h"
#include "content/browser/aggregation_service/aggregatable_report_scheduler.h"
#include "content/browser/aggregation_service/aggregatable_report_sender.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_storage_context.h"
#include "content/common/content_export.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace base {
class Clock;
class FilePath;
}  // namespace base

namespace content {

struct PublicKeyset;
class AggregatableReport;
class AggregationServiceStorage;
class AggregatableReportScheduler;
class StoragePartitionImpl;

// UI thread class that manages the lifetime of the underlying storage. Owned by
// the StoragePartitionImpl. Lifetime is bound to lifetime of the
// StoragePartitionImpl.
class CONTENT_EXPORT AggregationServiceImpl
    : public AggregationService,
      public AggregationServiceStorageContext {
 public:
  static std::unique_ptr<AggregationServiceImpl> CreateForTesting(
      bool run_in_memory,
      const base::FilePath& user_data_directory,
      const base::Clock* clock,
      std::unique_ptr<AggregatableReportScheduler> scheduler,
      std::unique_ptr<AggregatableReportAssembler> assembler,
      std::unique_ptr<AggregatableReportSender> sender);

  AggregationServiceImpl(bool run_in_memory,
                         const base::FilePath& user_data_directory,
                         StoragePartitionImpl* storage_partition);
  AggregationServiceImpl(const AggregationServiceImpl& other) = delete;
  AggregationServiceImpl& operator=(const AggregationServiceImpl& other) =
      delete;
  AggregationServiceImpl(AggregationServiceImpl&& other) = delete;
  AggregationServiceImpl& operator=(AggregationServiceImpl&& other) = delete;
  ~AggregationServiceImpl() override;

  // AggregationService:
  void AssembleReport(AggregatableReportRequest report_request,
                      AssemblyCallback callback) override;
  void SendReport(const GURL& url,
                  const AggregatableReport& report,
                  SendCallback callback) override;
  void SendReport(const GURL& url,
                  const base::Value& contents,
                  SendCallback callback) override;
  void ClearData(base::Time delete_begin,
                 base::Time delete_end,
                 StoragePartition::StorageKeyMatcherFunction filter,
                 base::OnceClosure done) override;
  void ScheduleReport(AggregatableReportRequest report_request) override;

  // AggregationServiceStorageContext:
  const base::SequenceBound<AggregationServiceStorage>& GetStorage() override;

  // Sets the public keys for `url` in storage to allow testing without network.
  void SetPublicKeysForTesting(const GURL& url, const PublicKeyset& keyset);

 private:
  // Allows access to `OnScheduledReportTimeReached()`.
  friend class AggregationServiceImplTest;

  AggregationServiceImpl(bool run_in_memory,
                         const base::FilePath& user_data_directory,
                         const base::Clock* clock,
                         std::unique_ptr<AggregatableReportScheduler> scheduler,
                         std::unique_ptr<AggregatableReportAssembler> assembler,
                         std::unique_ptr<AggregatableReportSender> sender);

  void OnScheduledReportTimeReached(
      std::vector<AggregationServiceStorage::RequestAndId> requests_and_ids);

  void OnReportAssemblyComplete(
      AggregationServiceStorage::RequestId request_id,
      GURL reporting_url,
      absl::optional<AggregatableReport> report,
      AggregatableReportAssembler::AssemblyStatus status);

  void OnReportSendingComplete(AggregationServiceStorage::RequestId request_id,
                               AggregatableReportSender::RequestStatus status);

  std::unique_ptr<AggregatableReportScheduler> scheduler_;
  base::SequenceBound<AggregationServiceStorage> storage_;
  std::unique_ptr<AggregatableReportAssembler> assembler_;
  std::unique_ptr<AggregatableReportSender> sender_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_IMPL_H_