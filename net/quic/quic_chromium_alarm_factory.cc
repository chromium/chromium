// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_alarm_factory.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/quic/platform/impl/quic_chromium_clock.h"

namespace net {

namespace {

class QuicChromeAlarm : public quic::QuicAlarm, public base::TickClock {
 public:
  QuicChromeAlarm(const quic::QuicClock* clock,
                  scoped_refptr<base::SequencedTaskRunner> task_runner,
                  quic::QuicArenaScopedPtr<quic::QuicAlarm::Delegate> delegate)
      : quic::QuicAlarm(std::move(delegate)),
        clock_(clock),
        // Unretained is safe because base::OneShotTimer never runs its task
        // after being deleted.
        on_alarm_callback_(base::BindRepeating(&QuicChromeAlarm::OnAlarm,
                                               base::Unretained(this))),
        timer_(std::make_unique<base::OneShotTimer>(this)) {
    timer_->SetTaskRunner(std::move(task_runner));
  }

 protected:
  void SetImpl() override {
    DCHECK(deadline().IsInitialized());
    const int64_t delay_us = (deadline() - clock_->Now()).ToMicroseconds();
    timer_->Start(FROM_HERE, base::Microseconds(delay_us), on_alarm_callback_);
  }

  void CancelImpl() override {
    DCHECK(!deadline().IsInitialized());
    timer_->Stop();
  }

 private:
  void OnAlarm() {
    DCHECK(deadline().IsInitialized());

    // In tests, the time source used by the scheduler may not be in sync with
    // |clock_|. Because of this, the scheduler may run this task when
    // |clock->Now()| is smaller than |deadline()|. In that case, retry later.
    // This shouldn't happen in production.
    if (clock_->Now() < deadline()) {
      SetImpl();
      return;
    }

    DCHECK_LE(deadline(), clock_->Now());
    Fire();
  }

  // base::TickClock:
  base::TimeTicks NowTicks() const override {
    return quic::QuicChromiumClock::QuicTimeToTimeTicks(clock_->Now());
  }

  const raw_ptr<const quic::QuicClock> clock_;
  base::RepeatingClosure on_alarm_callback_;
  const std::unique_ptr<base::OneShotTimer> timer_;
};

}  // namespace

QuicChromiumAlarmFactory::QuicChromiumAlarmFactory(
    base::SequencedTaskRunner* task_runner,
    const quic::QuicClock* clock)
    : task_runner_(task_runner), clock_(clock) {}

QuicChromiumAlarmFactory::~QuicChromiumAlarmFactory() = default;

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
