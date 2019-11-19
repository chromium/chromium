// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_alarm_factory.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/sparse_histogram.h"
#include "base/task_runner.h"
#include "base/time/time.h"

namespace net {

namespace {

class QuicChromeAlarm : public quic::QuicAlarm {
 public:
  QuicChromeAlarm(const quic::QuicClock* clock,
                  base::TaskRunner* task_runner,
                  quic::QuicArenaScopedPtr<quic::QuicAlarm::Delegate> delegate)
      : quic::QuicAlarm(std::move(delegate)),
        clock_(clock),
        task_runner_(task_runner),
        task_deadline_(quic::QuicTime::Zero()) {}

 protected:
  void SetImpl() override {
    DCHECK(deadline().IsInitialized());
    if (task_deadline_.IsInitialized()) {
      if (task_deadline_ <= deadline()) {
        // Since tasks can not be un-posted, OnAlarm will be invoked which
        // will notice that deadline has not yet been reached, and will set
        // the alarm for the new deadline.
        return;
      }
      // The scheduled task is after new deadline.  Invalidate the weak ptrs
      // so that task does not execute when we're not expecting it.
      weak_factory_.InvalidateWeakPtrs();
    }

    int64_t delay_us = (deadline() - (clock_->Now())).ToMicroseconds();
    if (delay_us < 0) {
      delay_us = 0;
    }
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&QuicChromeAlarm::OnAlarm, weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMicroseconds(delay_us));
    task_deadline_ = deadline();
  }

  void CancelImpl() override {
    DCHECK(!deadline().IsInitialized());
    // Since tasks can not be un-posted, OnAlarm will be invoked which
    // will notice that deadline is not Initialized and will do nothing.
  }

 private:
  void OnAlarm() {
    DCHECK(task_deadline_.IsInitialized());
    task_deadline_ = quic::QuicTime::Zero();
    // The alarm may have been cancelled.
    if (!deadline().IsInitialized()) {
      return;
    }

    // The alarm may have been re-set to a later time.
    if (clock_->Now() < deadline()) {
      SetImpl();
      return;
    }

    Fire();
  }

  const quic::QuicClock* clock_;
  base::TaskRunner* task_runner_;
  // If a task has been posted to the message loop, this is the time it
  // was scheduled to fire.  Tracking this allows us to avoid posting a
  // new tast if the new deadline is in the future, but permits us to
  // post a new task when the new deadline now earlier than when
  // previously posted.
  quic::QuicTime task_deadline_;
  base::WeakPtrFactory<QuicChromeAlarm> weak_factory_{this};
};

}  // namespace

QuicChromiumAlarmFactory::QuicChromiumAlarmFactory(
    base::TaskRunner* task_runner,
    const quic::QuicClock* clock)
    : task_runner_(task_runner), clock_(clock) {}

QuicChromiumAlarmFactory::~QuicChromiumAlarmFactory() {}

quic::QuicArenaScopedPtr<quic::QuicAlarm> QuicChromiumAlarmFactory::CreateAlarm(
    quic::QuicArenaScopedPtr<quic::QuicAlarm::Delegate> delegate,
    quic::QuicConnectionArena* arena) {
  if (arena != nullptr) {
    return arena->New<QuicChromeAlarm>(clock_, task_runner_,
                                       std::move(delegate));
  } else {
    return quic::QuicArenaScopedPtr<quic::QuicAlarm>(
        new QuicChromeAlarm(clock_, task_runner_, std::move(delegate)));
  }
}

quic::QuicAlarm* QuicChromiumAlarmFactory::CreateAlarm(
    quic::QuicAlarm::Delegate* delegate) {
  return new QuicChromeAlarm(
      clock_, task_runner_,
      quic::QuicArenaScopedPtr<quic::QuicAlarm::Delegate>(delegate));
}

}  // namespace net
