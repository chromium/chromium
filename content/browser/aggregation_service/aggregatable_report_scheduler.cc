// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregatable_report_scheduler.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/aggregation_service_storage_context.h"
#include "content/public/common/content_switches.h"

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
  CHECK(storage_context);
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
      .Then(base::BindOnce(&TimerDelegate::NotifySendAttemptCompleted,
                           timer_delegate_->GetWeakPtr(), request_id));
}

bool AggregatableReportScheduler::NotifyInProgressRequestFailed(
    AggregationServiceStorage::RequestId request_id,
    int previous_failed_attempts) {
  CHECK_GE(previous_failed_attempts, 0);
  std::optional<base::TimeDelta> delay =
      GetFailedReportDelay(previous_failed_attempts + 1);

  if (delay.has_value()) {
    base::Time next_report_time = base::Time::Now() + *delay;
    storage_context_->GetStorage()
        .AsyncCall(&AggregationServiceStorage::UpdateReportForSendFailure)
        .WithArgs(request_id, next_report_time)
        .Then(base::BindOnce(&TimerDelegate::NotifySendAttemptCompleted,
                             timer_delegate_->GetWeakPtr(), request_id));

    timer_.MaybeSet(next_report_time);
    return true;
  }

  // no retries left, dropping request
  storage_context_->GetStorage()
      .AsyncCall(&AggregationServiceStorage::DeleteRequest)
      .WithArgs(request_id)
      .Then(base::BindOnce(&TimerDelegate::NotifySendAttemptCompleted,
                           timer_delegate_->GetWeakPtr(), request_id));
  return false;
}

std::optional<base::TimeDelta>
AggregatableReportScheduler::GetFailedReportDelay(int failed_send_attempts) {
  CHECK_GT(failed_send_attempts, 0);

  if (failed_send_attempts > kMaxRetries)
    return std::nullopt;

  return kInitialRetryDelay *
         std::pow(kRetryDelayFactor, failed_send_attempts - 1);
}

AggregatableReportScheduler::TimerDelegate::TimerDelegate(
    AggregationServiceStorageContext* storage_context,
    base::RepeatingCallback<
        void(std::vector<AggregationServiceStorage::RequestAndId>)>
        on_scheduled_report_time_reached)
    : storage_context_(*storage_context),
      on_scheduled_report_time_reached_(
          std::move(on_scheduled_report_time_reached)),
      should_not_delay_reports_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kPrivateAggregationDeveloperMode)) {
  CHECK(storage_context);
}
AggregatableReportScheduler::TimerDelegate::~TimerDelegate() = default;

void AggregatableReportScheduler::TimerDelegate::GetNextReportTime(
    base::OnceCallback<void(std::optional<base::Time>)> callback,
    base::Time now) {
  storage_context_->GetStorage()
      .AsyncCall(&AggregationServiceStorage::NextReportTimeAfter)
      .WithArgs(now)
      .Then(std::move(callback));
}

void AggregatableReportScheduler::TimerDelegate::OnReportingTimeReached(
    base::Time now,
    base::Time timer_desired_run_time) {
  base::UmaHistogramLongTimes100(
      "PrivacySandbox.AggregationService.Scheduler.TimerFireDelay",
      now - timer_desired_run_time);

  storage_context_->GetStorage()
      .AsyncCall(&AggregationServiceStorage::GetRequestsReportingOnOrBefore)
      .WithArgs(now, /*limit=*/std::nullopt)
      .Then(base::BindOnce(&AggregatableReportScheduler::TimerDelegate::
                               OnRequestsReturnedFromStorage,
                           weak_ptr_factory_.GetWeakPtr(),
                           /*task_timer=*/base::ElapsedTimer()));
}

void AggregatableReportScheduler::TimerDelegate::AdjustOfflineReportTimes(
    base::OnceCallback<void(std::optional<base::Time>)> maybe_set_timer_cb) {
  if (should_not_delay_reports_) {
    // No need to adjust the report times, just set the timer as appropriate.
    storage_context_->GetStorage()
        .AsyncCall(&AggregationServiceStorage::NextReportTimeAfter)
        .WithArgs(base::Time::Min())
        .Then(std::move(maybe_set_timer_cb));
    return;
  }

  storage_context_->GetStorage()
      .AsyncCall(&AggregationServiceStorage::AdjustOfflineReportTimes)
      .WithArgs(base::Time::Now(), kOfflineReportTimeMinimumDelay,
                kOfflineReportTimeMaximumDelay)
      .Then(std::move(maybe_set_timer_cb));
}

void AggregatableReportScheduler::TimerDelegate::NotifySendAttemptCompleted(
    AggregationServiceStorage::RequestId request_id) {
  in_progress_requests_.erase(request_id);
}

void AggregatableReportScheduler::TimerDelegate::OnRequestsReturnedFromStorage(
    base::ElapsedTimer task_timer,
    std::vector<AggregationServiceStorage::RequestAndId> requests_and_ids) {
  base::UmaHistogramLongTimes100(
      "PrivacySandbox.AggregationService.Storage.RequestsRetrievalTime",
      task_timer.Elapsed());

  // TODO(alexmt): Consider adding metrics of the number of in-progress requests
  // erased to see if optimizations would be desirable.
  std::erase_if(
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
