// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_SCHEDULER_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_SCHEDULER_H_

#include <optional>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/report_scheduler_timer.h"
#include "content/common/content_export.h"

namespace base {
class ElapsedTimer;
class Time;
}  // namespace base

namespace content {

class AggregatableReportRequest;
class AggregationServiceStorageContext;

// UI thread class that is responsible for interacting with the storage layer
// for report requests, waiting for a report's reporting time to be reached and
// then calling the appropriate callback.
//
// Note that report requests can have one of two states: "pending" or
// "in-progress". This represents whether the request is currently being
// processed (i.e. assembled and sent). To ensure that no report is 'lost',
// all requests (that haven't been deleted) become pending once more on start
// up. Note that all reports with future report times are pending (unless wall
// clock time goes backward).
class CONTENT_EXPORT AggregatableReportScheduler {
 public:
  // Add uniform random noise in the range of [0, 1 minutes] to the report time
  // when the browser comes back online. Aligned with
  // `AttributionStorageDelegateImpl::GetOfflineReportDelayConfig()`.
  static constexpr base::TimeDelta kOfflineReportTimeMinimumDelay =
      base::Minutes(0);
  static constexpr base::TimeDelta kOfflineReportTimeMaximumDelay =
      base::Minutes(1);

  // Configuration for retrying to send reports that failed to send
  static constexpr int kMaxRetries = 2;
  static constexpr base::TimeDelta kInitialRetryDelay = base::Minutes(5);
  static constexpr int kRetryDelayFactor = 3;

  AggregatableReportScheduler(
      AggregationServiceStorageContext* storage_context,
      base::RepeatingCallback<
          void(std::vector<AggregationServiceStorage::RequestAndId>)>
          on_scheduled_report_time_reached);

  AggregatableReportScheduler(const AggregatableReportScheduler& other) =
      delete;
  AggregatableReportScheduler& operator=(
      const AggregatableReportScheduler& other) = delete;
  virtual ~AggregatableReportScheduler();

  // Methods are virtual for testing.

  // Schedules the `request` to be assembled and sent at
  // `request.shared_info().scheduled_report_time`.
  virtual void ScheduleRequest(AggregatableReportRequest request);

  // Notifies that the request to assemble and send the report with `request_id`
  // was successfully completed. There must be an in-progress request stored
  // with that `request_id`.
  virtual void NotifyInProgressRequestSucceeded(
      AggregationServiceStorage::RequestId request_id);

  // Notifies that the request to assemble and send the report with `request_id`
  // completed unsuccessfully. There must be an in-progress request stored with
  // that `request_id`.`failed_attemps_before_sending` is the number of times
  // that this request previously failed. ie. not counting the failure being
  // notified on.
  // Returns true when the request will be scheduled to be retried.
  // Returns false when the request is dropped. ie. it wont be retried.
  virtual bool NotifyInProgressRequestFailed(
      AggregationServiceStorage::RequestId request_id,
      int previous_failed_attempts);

 private:
  class TimerDelegate : public ReportSchedulerTimer::Delegate {
   public:
    TimerDelegate(AggregationServiceStorageContext* storage_context,
                  base::RepeatingCallback<void(
                      std::vector<AggregationServiceStorage::RequestAndId>)>
                      on_scheduled_report_time_reached);
    ~TimerDelegate() override;

    TimerDelegate(const TimerDelegate&) = delete;
    TimerDelegate& operator=(const TimerDelegate&) = delete;
    TimerDelegate(TimerDelegate&&) = delete;
    TimerDelegate& operator=(TimerDelegate&&) = delete;

    // Notifies that we no longer need to track `request_id` as in-progress.
    void NotifySendAttemptCompleted(
        AggregationServiceStorage::RequestId request_id);

    base::WeakPtr<AggregatableReportScheduler::TimerDelegate> GetWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

   private:
    // ReportSchedulerTimer::Delegate:
    void GetNextReportTime(base::OnceCallback<void(std::optional<base::Time>)>,
                           base::Time now) override;
    void OnReportingTimeReached(base::Time now,
                                base::Time timer_desired_run_time) override;
    void AdjustOfflineReportTimes(
        base::OnceCallback<void(std::optional<base::Time>)>) override;

    void OnRequestsReturnedFromStorage(
        base::ElapsedTimer task_timer,
        std::vector<AggregationServiceStorage::RequestAndId> requests_and_ids);

    // Using a raw reference is safe because `storage_context_` is guaranteed to
    // outlive `this`.
    raw_ref<AggregationServiceStorageContext> storage_context_;

    // Called when a report request's scheduled report time is reached. May
    // return multiple requests if their report times were all reached before
    // the timer fired. If multiple requests are returned, they will be ordered
    // by the report time (which may differ from the initially scheduled report
    // time).
    base::RepeatingCallback<void(
        std::vector<AggregationServiceStorage::RequestAndId>)>
        on_scheduled_report_time_reached_;

    // The set of IDs of all in-progress report requests.
    // TODO(alexmt): Consider switching to a flat_set when we have metrics on
    // the typical size.
    std::set<AggregationServiceStorage::RequestId> in_progress_requests_;

    // Set iff the private aggregation developer mode is enabled.
    bool should_not_delay_reports_;

    base::WeakPtrFactory<AggregatableReportScheduler::TimerDelegate>
        weak_ptr_factory_{this};
  };

  // Returns how long to wait before attempting to send a report that has
  // previously failed to be sent failed_send_attempts times. Returns
  // `std::nullopt` to indicate that no more attempts should be made.
  // Otherwise, the return value must be positive. `failed_send_attempts`
  // must be positive.
  static std::optional<base::TimeDelta> GetFailedReportDelay(
      int failed_send_attempts);

  // Using a raw reference is safe because `storage_context_` is guaranteed to
  // outlive `this`.
  raw_ref<AggregationServiceStorageContext> storage_context_;

  // Using a raw pointer is safe because it's owned by `timer_`. Will be cleared
  // in the destructor to avoid a dangling pointer.
  raw_ptr<TimerDelegate> timer_delegate_;

  ReportSchedulerTimer timer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_SCHEDULER_H_
