// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_service_throttler.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "media/base/android/media_server_crash_listener.h"

namespace media {

namespace {

// Period of inactivity after which we stop listening for MediaServer crashes.
// NOTE: Server crashes don't count as activity. Only calls to
// GetDelayForClientCreation() do.
constexpr auto kReleaseInactivityDelay = base::Minutes(1);

// Elapsed time between crashes needed to completely reset the media server
// crash count.
constexpr auto kTimeUntilCrashReset = base::Minutes(1);

// Elapsed time between schedule calls needed to completely reset the
// scheduling clock.
constexpr auto kTimeUntilScheduleReset = base::Minutes(1);

// Rate at which client creations will be exponentially throttled based on the
// number of media server crashes.
// NOTE: Since our exponential delay formula is 2^(server crashes), 0 server
// crashes still result in this delay being added once.
constexpr auto kBaseExponentialDelay = base::Milliseconds(120);

// Base rate at which we schedule client creations.
// The minimal delay is |kLinearThrottlingDelay| + |kBaseExponentialDelay|.
constexpr auto kLinearThrottlingDelay =
    base::Seconds(0.2) - kBaseExponentialDelay;

// Max exponential throttling rate from media server crashes.
// The max delay will still be |kLinearThrottlingDelay| +
// |kMaxExponentialDelay|.
constexpr auto kMaxExponentialDelay = base::Seconds(3) - kLinearThrottlingDelay;

// Max number of clients to schedule immediately (e.g when loading a new page).
const uint32_t kMaxBurstClients = 10;

// The throttling progression based on number of crashes looks as follows:
//
// | # crashes | period  | clients/sec | clients/mins | # burst clients
// | 0         | 200  ms | 5.0         | 300          | 10
// | 1         | 320  ms | 3.1         | 188          | 6
// | 2         | 560  ms | 1.8         | 107          | 4
// | 3         | 1040 ms | 1.0         | 58           | 2
// | 4         | 2000 ms | 0.5         | 30           | 1
// | 5         | 3000 ms | 0.3         | 20           | 1
// | 6         | 3000 ms | 0.3         | 20           | 1
//
// NOTE: Since we use the floor function and a decay rate of 1 crash/minute when
// calculating the effective # of crashes, a single crash per minute will result
// in 0 effective crashes (since floor(1.0 - 'tiny decay') is 0). If we
// experience slightly more than 1 crash per 60 seconds, the effective number of
// crashes will go up as expected.
}

// static
MediaServiceThrottler* MediaServiceThrottler::GetInstance() {
  static MediaServiceThrottler* instance = new MediaServiceThrottler();
  return instance;
}

MediaServiceThrottler::~MediaServiceThrottler() {}

MediaServiceThrottler::MediaServiceThrottler()
    : clock_(base::DefaultTickClock::GetInstance()),
      current_crashes_(0),
      crash_listener_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()) {
  // base::Unretained is safe because the MediaServiceThrottler is supposed to
  // live until the process dies.
  release_crash_listener_cb_ = base::BindRepeating(
      &MediaServiceThrottler::ReleaseCrashListener, base::Unretained(this));
  EnsureCrashListenerStarted();
}

void MediaServiceThrottler::SetTickClockForTesting(
    const base::TickClock* clock) {
  clock_ = clock;
}

base::TimeDelta MediaServiceThrottler::GetBaseThrottlingRateForTesting() {
  return kBaseExponentialDelay + kLinearThrottlingDelay;
}

void MediaServiceThrottler::ResetInternalStateForTesting() {
  last_server_crash_ = base::TimeTicks();
  last_schedule_call_ = base::TimeTicks();
  next_schedulable_slot_ = clock_->NowTicks();
  last_current_crash_update_time_ = clock_->NowTicks();
  current_crashes_ = 0.0;
}

base::TimeDelta MediaServiceThrottler::GetDelayForClientCreation() {
  // Make sure the listener is started and the crashes decayed.
  EnsureCrashListenerStarted();
  UpdateServerCrashes();

  base::TimeTicks now = clock_->NowTicks();

  // If we are passed the next time slot or if it has been 1 minute since the
  // last call to GetDelayForClientCreation(), reset the next time to now.
  if (now > next_schedulable_slot_ ||
      (now - last_schedule_call_) > kTimeUntilScheduleReset) {
    next_schedulable_slot_ = now;
  }

  last_schedule_call_ = now;

  // Increment the next scheduled time between 0.2s and 3s, which allows the
  // creation of between 50 and 3 clients per 10s.
  next_schedulable_slot_ +=
      kLinearThrottlingDelay + GetThrottlingDelayFromServerCrashes();

  // Calculate how long to delay the creation so it isn't scheduled before
  // |next_schedulable_slot_|.
  base::TimeDelta delay = next_schedulable_slot_ - now;

  // If the scheduling delay is low enough, schedule it immediately instead.
  // This allows up to kMaxBurstClients clients to be scheduled immediately.
  if (delay <=
      (kLinearThrottlingDelay + kBaseExponentialDelay) * kMaxBurstClients)
    return base::TimeDelta();

  return delay;
}

base::TimeDelta MediaServiceThrottler::GetThrottlingDelayFromServerCrashes() {
  // The combination of rounding down the number of crashes down and decaying
  // at the rate of 1 crash / min means that a single crash will very quickly be
  // rounded down to 0. Effectively, this means that we only start exponentially
  // backing off if we have more than 1 crash in a 60 second window.
  uint32_t num_crashes = static_cast<uint32_t>(current_crashes_);
  DCHECK_GE(num_crashes, 0u);

  // Prevents overflow/undefined behavior. We already reach kMaxExponentialDelay
  // at 5 crashes in any case.
  num_crashes = std::min(num_crashes, 10u);

  return std::min(kBaseExponentialDelay * (1 << num_crashes),
                  kMaxExponentialDelay);
}

void MediaServiceThrottler::OnMediaServerCrash(bool watchdog_needs_release) {
  if (watchdog_needs_release && crash_listener_)
    crash_listener_->ReleaseWatchdog();

  UpdateServerCrashes();

  last_server_crash_ = clock_->NowTicks();
  current_crashes_ += 1.0;
}

void MediaServiceThrottler::UpdateServerCrashes() {
  base::TimeTicks now = clock_->NowTicks();
  base::TimeDelta time_since_last_crash = now - last_server_crash_;

  if (time_since_last_crash > kTimeUntilCrashReset) {
    // Reset the number of crashes if we haven't had a crash in the past minute.
    current_crashes_ = 0.0;
  } else {
    // Decay at the rate of 1 crash/minute otherwise.
    const double decay =
        (now - last_current_crash_update_time_) / base::Minutes(1);
    current_crashes_ = std::max(0.0, current_crashes_ - decay);
  }

  last_current_crash_update_time_ = now;
}

void MediaServiceThrottler::ReleaseCrashListener() {
  crash_listener_.reset(nullptr);
}

void MediaServiceThrottler::EnsureCrashListenerStarted() {
  if (!crash_listener_) {
    // base::Unretained is safe here because the MediaServiceThrottler will live
    // until the process is terminated.
    crash_listener_ = std::make_unique<MediaServerCrashListener>(
        base::BindRepeating(&MediaServiceThrottler::OnMediaServerCrash,
                            base::Unretained(this)),
        crash_listener_task_runner_);
  } else {
    crash_listener_->EnsureListening();
  }

  // Cancels outstanding/pending versions of the callback.
  cancelable_release_crash_listener_cb_.Reset(release_crash_listener_cb_);

  // Schedule the release of |crash_listener_| a minute from now. This will be
  // updated anytime GetDelayForClientCreation() is called.
  crash_listener_task_runner_->PostDelayedTask(
      FROM_HERE, cancelable_release_crash_listener_cb_.callback(),
      kReleaseInactivityDelay);
}

bool MediaServiceThrottler::IsCrashListenerAliveForTesting() {
  return !!crash_listener_;
}

void MediaServiceThrottler::SetCrashListenerTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> crash_listener_task_runner) {
  // Set the task runner so |crash_listener_| be deleted on the right thread.
  crash_listener_task_runner_ = crash_listener_task_runner;

  // Re-create the crash listener.
  crash_listener_ = std::make_unique<MediaServerCrashListener>(
      base::NullCallback(), crash_listener_task_runner_);
}

}  // namespace media
