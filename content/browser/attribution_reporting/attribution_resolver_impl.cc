// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_resolver_impl.h"

#include <memory>

#include "base/check.h"
#include "content/browser/attribution_reporting/attribution_resolver_delegate.h"
#include "content/browser/attribution_reporting/attribution_storage_sql.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.h"

namespace content {

AttributionResolverImpl::AttributionResolverImpl(
    const base::FilePath& user_data_directory,
    std::unique_ptr<AttributionResolverDelegate> delegate)
    : delegate_(std::move(delegate)),
      storage_(user_data_directory, delegate_.get()) {
  DCHECK(delegate_);
}

AttributionResolverImpl::~AttributionResolverImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

StoreSourceResult AttributionResolverImpl::StoreSource(StorableSource source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.StoreSource(std::move(source));
}

CreateReportResult AttributionResolverImpl::MaybeCreateAndStoreReport(
    AttributionTrigger trigger) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.MaybeCreateAndStoreReport(std::move(trigger));
}

std::vector<AttributionReport> AttributionResolverImpl::GetAttributionReports(
    base::Time max_report_time,
    int limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.GetAttributionReports(max_report_time, limit);
}

std::optional<base::Time> AttributionResolverImpl::GetNextReportTime(
    base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.GetNextReportTime(time);
}

std::optional<AttributionReport> AttributionResolverImpl::GetReport(
    AttributionReport::Id id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.GetReport(id);
}

std::vector<StoredSource> AttributionResolverImpl::GetActiveSources(int limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.GetActiveSources(limit);
}

std::set<AttributionDataModel::DataKey>
AttributionResolverImpl::GetAllDataKeys() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.GetAllDataKeys();
}

void AttributionResolverImpl::DeleteByDataKey(
    const AttributionDataModel::DataKey& datakey) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  storage_.DeleteByDataKey(datakey);
}

bool AttributionResolverImpl::DeleteReport(AttributionReport::Id report_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.DeleteReport(report_id);
}

bool AttributionResolverImpl::UpdateReportForSendFailure(
    AttributionReport::Id report_id,
    base::Time new_report_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.UpdateReportForSendFailure(report_id, new_report_time);
}

std::optional<base::Time> AttributionResolverImpl::AdjustOfflineReportTimes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.AdjustOfflineReportTimes();
}

void AttributionResolverImpl::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter,
    bool delete_rate_limit_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  storage_.ClearData(delete_begin, delete_end, std::move(filter),
                     delete_rate_limit_data);
}

void AttributionResolverImpl::SetDelegate(
    std::unique_ptr<AttributionResolverDelegate> delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate);
  storage_.SetDelegate(delegate.get());
  delegate_ = std::move(delegate);
}

}  // namespace content
