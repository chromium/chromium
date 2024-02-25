// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_THREAD_HANG_MONITOR_H_
#define MEDIA_AUDIO_AUDIO_THREAD_HANG_MONITOR_H_

#include <atomic>
#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_export.h"

namespace base {
class TickClock;
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

// This class detects if the audio manager thread is hung. It logs a histogram,
// and can optionally (if |dump_on_hang| is set) upload a crash dump when a hang
// is detected. It runs on a task runner from the task scheduler. It works by
// posting a task to the audio thread every minute and checking that it was
// executed. If three consecutive such pings are missed, the thread is
// considered hung.
class MEDIA_EXPORT AudioThreadHangMonitor final {
 public:
  using Ptr =
      std::unique_ptr<AudioThreadHangMonitor, base::OnTaskRunnerDeleter>;

  // These values are histogrammed over time; do not change their ordinal
  // values.
  enum class ThreadStatus {
    // kNone = 0, obsolete.
    kStarted = 1,
    kHung,
    kRecovered,
    kMaxValue = kRecovered
  };

  enum class HangAction {
    // Do nothing. (UMA logging is always done.)
    kDoNothing,
    // A crash dump will be collected the first time the thread is detected as
    // hung (note that no actual crashing is involved).
    kDump,
    // Terminate the current process with exit code 0.
    kTerminateCurrentProcess,
    // Terminate the current process with exit code 1, which yields a crash
    // dump.
    kDumpAndTerminateCurrentProcess
  };

  // |monitor_task_runner| may be set explicitly by tests only. Other callers
  // should use the default. If |hang_deadline| is not provided, or if it's
  // zero, a default value is used.
  static Ptr Create(
      HangAction hang_action,
      std::optional<base::TimeDelta> hang_deadline,
      const base::TickClock* clock,
      scoped_refptr<base::SingleThreadTaskRunner> audio_thread_task_runner,
      scoped_refptr<base::SequencedTaskRunner> monitor_task_runner = nullptr);

  AudioThreadHangMonitor(const AudioThreadHangMonitor&) = delete;
  AudioThreadHangMonitor& operator=(const AudioThreadHangMonitor&) = delete;

  ~AudioThreadHangMonitor();

  // Thread-safe.
  bool IsAudioThreadHung() const;

 private:
  friend class AudioThreadHangMonitorTest;

  class SharedAtomicFlag final
      : public base::RefCountedThreadSafe<SharedAtomicFlag> {
   public:
    SharedAtomicFlag();

    std::atomic_bool flag_ = {false};

   private:
    friend class base::RefCountedThreadSafe<SharedAtomicFlag>;
    ~SharedAtomicFlag();
  };

  AudioThreadHangMonitor(
      HangAction hang_action,
      std::optional<base::TimeDelta> hang_deadline,
      const base::TickClock* clock,
      scoped_refptr<base::SingleThreadTaskRunner> audio_thread_task_runner);

  void StartTimer();

  bool NeverLoggedThreadHung() const;
  bool NeverLoggedThreadRecoveredAfterHung() const;

  // This function is run by the |timer_|. It checks if the audio thread has
  // shown signs of life since the last time it was called (by checking the
  // |alive_flag_|) and updates the value of |successful_pings_| and
  // |failed_pings_| as appropriate. It also changes the thread status and logs
  // its value to a histogram.
  void CheckIfAudioThreadIsAlive();

  // LogHistogramThreadStatus logs |thread_status_| to a histogram.
  void LogHistogramThreadStatus();

  // For tests. See below functions.
  void SetHangActionCallbacksForTesting(
      base::RepeatingClosure dump_callback,
      base::RepeatingClosure terminate_process_callback);

  // Thin wrapper functions that either executes the default or runs a callback
  // set with SetHangActioncallbacksForTesting(), for testing purposes.
  void DumpWithoutCrashing();
  void TerminateCurrentProcess();

  const raw_ptr<const base::TickClock> clock_;

  // This flag is set to false on the monitor sequence and then set to true on
  // the audio thread to indicate that the audio thread is alive.
  const scoped_refptr<SharedAtomicFlag> alive_flag_;

  // |audio_task_runner_| is the task runner of the audio thread.
  const scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;

  // Which action(s) to take when detected hung thread.
  const HangAction hang_action_;

  // At which interval to ping and see if the thread is running.
  const base::TimeDelta ping_interval_;

  // For testing. See DumpWithoutCrashing() and TerminateCurrentProcess().
  base::RepeatingClosure dump_callback_;
  base::RepeatingClosure terminate_process_callback_;

  std::atomic<ThreadStatus> audio_thread_status_ = {ThreadStatus::kStarted};

  // All fields below are accessed on |monitor_sequence|.
  SEQUENCE_CHECKER(monitor_sequence_);

  // Timer to check |alive_flag_| regularly.
  base::RepeatingTimer timer_;

  // This variable is used to check to detect suspend/resume cycles.
  // If a long time has passed since the timer was last fired, it is likely due
  // to the machine being suspended. In such a case, we want to avoid falsely
  // detecting the audio thread as hung.
  base::TimeTicks last_check_time_ = base::TimeTicks();

  // |recent_ping_state_| tracks the recent life signs from the audio thread. If
  // the most recent ping was successful, the number indicates the number of
  // successive successful pings. If the most recent ping was failed, the number
  // is the negative of the number of successive failed pings.
  int recent_ping_state_ = 0;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_THREAD_HANG_MONITOR_H_
