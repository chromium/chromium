// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_REPORT_SCHEDULER_TIMER_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_REPORT_SCHEDULER_TIMER_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"

namespace base {
class Time;
}  // namespace base

namespace content {

// This class consolidates logic regarding when to schedule the browser to send
// reports for APIs using the aggregation service and for event-level
// attribution reporting. This includes handling network changes and browser
// startup. It delegates certain operations to API-specific implementations.
// TODO(alexmt): Consider moving out of the aggregation_service directory to a
// separate shared directory.
class CONTENT_EXPORT ReportSchedulerTimer
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Should be overridden with a method that gets the next report time that
    // the timer should fire at and returns it via the callback. If there is no
    // next report time, `std::nullopt` should be returned instead.
    virtual void GetNextReportTime(
        base::OnceCallback<void(std::optional<base::Time>)>,
        base::Time now) = 0;

    // Called when the timer is fired, with the current time `now` and the time
    // the timer was instructed to fire `timer_desired_run_time`. `Refresh()` is
    // automatically called after. If this causes a `GetNextReportTime()` call,
    // that will be passed the same `now`.
    virtual void OnReportingTimeReached(base::Time now,
                                        base::Time timer_desired_run_time) = 0;

    // Called when the connection changes from online to offline. When this
    // happens the timer is paused which means `OnReportingTimeReached` will not
    // be called until it gets resumed. Before resuming the timer,
    // `AdjustOfflineReportTimes` will be called.
    virtual void OnReportingPaused() {}

    // Called when the connection changes from offline to online. May also be
    // called on a connection change if there are no stored reports, see
    // `OnConnectionChanged()`. Running the callback will call `MaybeSet()` with
    // the given argument; this may be necessary after the report times were
    // adjusted.
    virtual void AdjustOfflineReportTimes(
        base::OnceCallback<void(std::optional<base::Time>)>) = 0;
  };

  explicit ReportSchedulerTimer(std::unique_ptr<Delegate> delegate);

  ReportSchedulerTimer(const ReportSchedulerTimer&) = delete;
  ReportSchedulerTimer& operator=(const ReportSchedulerTimer&) = delete;
  ReportSchedulerTimer(ReportSchedulerTimer&&) = delete;
  ReportSchedulerTimer& operator=(ReportSchedulerTimer&&) = delete;

  ~ReportSchedulerTimer() override;

  network::mojom::ConnectionType connection_type() const;

  // Schedules `reporting_time_reached_timer_` to fire at that time, unless the
  // timer is already set to fire earlier.
  void MaybeSet(std::optional<base::Time> reporting_time);

 private:
  void OnTimerFired();
  void Refresh(base::Time now) VALID_CONTEXT_REQUIRED(sequence_checker_);

  // This method is marked `final` to enable the constructor to call it while
  // complying with the style guide, which forbids constructors from making
  // virtual method calls.
  // https://google.github.io/styleguide/cppguide.html#Doing_Work_in_Constructors
  //
  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType) final;

  bool IsOffline() const VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Fires whenever a reporting time is reached for a report. Must be updated
  // whenever the next report time changes.
  base::WallClockTimer reporting_time_reached_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  const std::unique_ptr<Delegate> delegate_
      GUARDED_BY_CONTEXT(sequence_checker_);

  network::mojom::ConnectionType connection_type_ GUARDED_BY_CONTEXT(
      sequence_checker_) = network::mojom::ConnectionType::CONNECTION_NONE;

  base::ScopedObservation<
      network::NetworkConnectionTracker,
      network::NetworkConnectionTracker::NetworkConnectionObserver>
      obs_ GUARDED_BY_CONTEXT(sequence_checker_){this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ReportSchedulerTimer> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_REPORT_SCHEDULER_TIMER_H_
