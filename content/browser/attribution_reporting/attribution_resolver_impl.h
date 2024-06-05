// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_RESOLVER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_RESOLVER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_resolver.h"
#include "content/browser/attribution_reporting/attribution_storage_sql.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/rate_limit_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/common/content_export.h"
#include "content/public/browser/attribution_data_model.h"
#include "content/public/browser/storage_partition.h"

namespace content {

class AttributionResolverDelegate;
class StorableSource;
struct AttributionInfo;

// This class may be constructed on any sequence but must be accessed and
// destroyed on the same sequence. The sequence must outlive |this|.
class CONTENT_EXPORT AttributionResolverImpl : public AttributionResolver {
 public:
  AttributionResolverImpl(
      const base::FilePath& user_data_directory,
      std::unique_ptr<AttributionResolverDelegate> delegate);
  AttributionResolverImpl(const AttributionResolverImpl&) = delete;
  AttributionResolverImpl& operator=(const AttributionResolverImpl&) = delete;
  AttributionResolverImpl(AttributionResolverImpl&&) = delete;
  AttributionResolverImpl& operator=(AttributionResolverImpl&&) = delete;
  ~AttributionResolverImpl() override;

 private:
  // AttributionResolver:
  StoreSourceResult StoreSource(StorableSource source) override;
  CreateReportResult MaybeCreateAndStoreReport(
      AttributionTrigger trigger) override;
  std::vector<AttributionReport> GetAttributionReports(
      base::Time max_report_time,
      int limit = -1) override;
  std::optional<base::Time> GetNextReportTime(base::Time time) override;
  std::optional<AttributionReport> GetReport(AttributionReport::Id) override;
  std::vector<StoredSource> GetActiveSources(int limit = -1) override;
  std::set<AttributionDataModel::DataKey> GetAllDataKeys() override;
  void DeleteByDataKey(const AttributionDataModel::DataKey& datakey) override;
  bool DeleteReport(AttributionReport::Id report_id) override;
  bool UpdateReportForSendFailure(AttributionReport::Id report_id,
                                  base::Time new_report_time) override;
  std::optional<base::Time> AdjustOfflineReportTimes() override;
  void ClearData(base::Time delete_begin,
                 base::Time delete_end,
                 StoragePartition::StorageKeyMatcherFunction filter,
                 bool delete_rate_limit_data) override;
  ProcessAggregatableDebugReportResult ProcessAggregatableDebugReport(
      AggregatableDebugReport,
      std::optional<int> remaining_budget,
      std::optional<StoredSource::Id>) override;
  void SetDelegate(std::unique_ptr<AttributionResolverDelegate>) override;

  std::unique_ptr<AttributionResolverDelegate> delegate_
      GUARDED_BY_CONTEXT(sequence_checker_);

  AttributionStorageSql storage_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_RESOLVER_IMPL_H_
