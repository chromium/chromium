// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregatable_report_scheduler.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/containers/cxx20_erase.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/aggregation_service_storage_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

AggregatableReportScheduler::AggregatableReportScheduler(
    AggregationServiceStorageContext* storage_context,
    base::RepeatingCallback<
        void(std::vector<AggregationServiceStorage::RequestAndId>)>
        on_scheduled_report_time_reached)
    : storage_context_(*storage_context),
      timer_delegate_(
          new TimerDelegate(storage_context,
                            std::move(on_scheduled_report_time_reached))),
      timer_(base::WrapUnique(timer_delegate_.get())) {
  DCHECK(storage_context);
}
AggregatableReportScheduler::~AggregatableReportScheduler() {
  timer_delegate_ = nullptr;
}

void AggregatableReportScheduler::ScheduleRequest(
    AggregatableReportRequest request) {
  base::Time report_time = request.shared_info().scheduled_report_time;
  storage_context_->GetStorage()
      .AsyncCall(&AggregationServiceStorage::StoreRequest)
      .WithArgs(std::move(request));

  // If the time is in the past, the timer will fire immediately.
  timer_.MaybeSet(report_time);
}

void AggregatableReportScheduler::NotifyInProgressRequestSucceeded(
    AggregationServiceStorage::RequestId request_id) {
  storage_context_->GetStorage()
      .AsyncCall(&AggregationServiceStorage::DeleteRequest)
      .WithArgs(request_id)
      .Then(base::BindOnce(&TimerDelegate::NotifyRequestCompleted,
                           timer_delegate_->GetWeakPtr(), request_id));
}

void AggregatableReportScheduler::NotifyInProgressRequestFailed(
    AggregationServiceStorage::RequestId request_id) {
  // TODO(crbug.com/1340040): Implement retry handling. Ideally also handle
  // different errors differently. Also, ensure this composes well with offline
  // handling.
  storage_context_->GetStorage()
      .AsyncCall(&AggregationServiceStorage::DeleteRequest)
      .WithArgs(request_id)
      .Then(base::BindOnce(&TimerDelegate::NotifyRequestCompleted,
                           timer_delegate_->GetWeakPtr(), request_id));
}

AggregatableReportScheduler::TimerDelegate::TimerDelegate(
    AggregationServiceStorageContext* storage_context,
    base::RepeatingCallback<
        void(std::vector<AggregationServiceStorage::RequestAndId>)>
        on_scheduled_report_time_reached)
    : storage_context_(*storage_context),
      on_scheduled_report_time_reached_(
          std::move(on_scheduled_report_time_reached)) {
  DCHECK(storage_context);
}
AggregatableReportScheduler::TimerDelegate::~TimerDelegate() = default;

void AggregatableReportScheduler::TimerDelegate::GetNextReportTime(
    base::OnceCallback<void(absl::optional<base::Time>)> callback,
    base::Time now) {
  storage_context_->GetStorage()
      .AsyncCall(&AggregationServiceStorage::NextReportTimeAfter)
      .WithArgs(now)
      .Then(std::move(callback));
}

void AggregatableReportScheduler::TimerDelegate::OnReportingTimeReached(
    base::Time now) {
  storage_context_->GetStorage()
      .AsyncCall(&AggregationServiceStorage::GetRequestsReportingOnOrBefore)
      .WithArgs(now)
      .Then(base::BindOnce(&AggregatableReportScheduler::TimerDelegate::
                               OnRequestsReturnedFromStorage,
                           weak_ptr_factory_.GetWeakPtr()));
}

void AggregatableReportScheduler::TimerDelegate::AdjustOfflineReportTimes(
    base::OnceCallback<void(absl::optional<base::Time>)> maybe_set_timer_cb) {
  storage_context_->GetStorage()
      .AsyncCall(&AggregationServiceStorage::AdjustOfflineReportTimes)
      .WithArgs(base::Time::Now(), kOfflineReportTimeMinimumDelay,
                kOfflineReportTimeMaximumDelay)
      .Then(std::move(maybe_set_timer_cb));
}

void AggregatableReportScheduler::TimerDelegate::NotifyRequestCompleted(
    AggregationServiceStorage::RequestId request_id) {
  in_progress_requests_.erase(request_id);
}

void AggregatableReportScheduler::TimerDelegate::OnRequestsReturnedFromStorage(
    std::vector<AggregationServiceStorage::RequestAndId> requests_and_ids) {
  // TODO(alexmt): Consider adding metrics of the number of in-progress requests
  // erased to see if optimizations would be desirable.
  base::EraseIf(
      requests_and_ids,
      [this](const AggregationServiceStorage::RequestAndId& request_and_id) {
        return base::Contains(in_progress_requests_, request_and_id.id);
      });
  for (const AggregationServiceStorage::RequestAndId& request_and_id :
       requests_and_ids) {
    in_progress_requests_.insert(request_and_id.id);
  }

  if (!requests_and_ids.empty()) {
    on_scheduled_report_time_reached_.Run(std::move(requests_and_ids));
  }
}

}  // namespace content
