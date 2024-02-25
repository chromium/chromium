// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/fake_audio_worker.h"

#include <utility>

#include "base/cancelable_callback.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

class FakeAudioWorker::Worker
    : public base::RefCountedThreadSafe<FakeAudioWorker::Worker> {
 public:
  Worker(const scoped_refptr<base::SequencedTaskRunner>& worker_task_runner,
         const AudioParameters& params);

  Worker(const Worker&) = delete;
  Worker& operator=(const Worker&) = delete;

  bool IsStopped();
  void Start(FakeAudioWorker::Callback worker_cb);
  void Stop();

 private:
  friend class base::RefCountedThreadSafe<Worker>;
  ~Worker();

  // Initialize and start regular calls to DoRead() on the worker thread.
  void DoStart();

  // Cancel any delayed callbacks to DoRead() in the worker loop's queue.
  void DoCancel();

  // Task that regularly calls |worker_cb_| according to the playback rate as
  // determined by the audio parameters given during construction.  Runs on
  // the worker loop.
  void DoRead();

  const scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;
  const int sample_rate_;
  const int frames_per_read_;

  base::Lock worker_cb_lock_;  // Held while mutating or running |worker_cb_|.
  FakeAudioWorker::Callback worker_cb_ GUARDED_BY(worker_cb_lock_);
  base::TimeTicks first_read_time_;
  int64_t frames_elapsed_;

  // Used to cancel any delayed tasks still inside the worker loop's queue.
  base::CancelableRepeatingClosure worker_task_cb_;

  SEQUENCE_CHECKER(sequence_checker_);
};

FakeAudioWorker::FakeAudioWorker(
    const scoped_refptr<base::SequencedTaskRunner>& worker_task_runner,
    const AudioParameters& params)
    : worker_(new Worker(worker_task_runner, params)) {}

FakeAudioWorker::~FakeAudioWorker() {
  DCHECK(worker_->IsStopped());
}

void FakeAudioWorker::Start(FakeAudioWorker::Callback worker_cb) {
  DCHECK(worker_->IsStopped());
  worker_->Start(std::move(worker_cb));
}

void FakeAudioWorker::Stop() {
  worker_->Stop();
}

// static
base::TimeDelta FakeAudioWorker::ComputeFakeOutputDelay(
    const AudioParameters& params) {
  // Typical delay values used by real AudioOutputStreams on Win, Mac, and Linux
  // tend to be around 1.5X to 3X of the buffer duration. So, 2X is chosen as a
  // general-purpose value.
  constexpr int kDelayFactor = 2;
  return AudioTimestampHelper::FramesToTime(
      params.frames_per_buffer() * kDelayFactor, params.sample_rate());
}

FakeAudioWorker::Worker::Worker(
    const scoped_refptr<base::SequencedTaskRunner>& worker_task_runner,
    const AudioParameters& params)
    : worker_task_runner_(worker_task_runner),
      sample_rate_(params.sample_rate()),
      frames_per_read_(params.frames_per_buffer()) {
  // Worker can be constructed on any thread, but will DCHECK that its
  // Start/Stop methods are called from the same thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FakeAudioWorker::Worker::~Worker() {
  DCHECK(!worker_cb_);
}

bool FakeAudioWorker::Worker::IsStopped() {
  base::AutoLock scoped_lock(worker_cb_lock_);
  return !worker_cb_;
}

void FakeAudioWorker::Worker::Start(FakeAudioWorker::Callback worker_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(worker_cb);
  {
    base::AutoLock scoped_lock(worker_cb_lock_);
    DCHECK(!worker_cb_);
    worker_cb_ = std::move(worker_cb);
  }
  worker_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(&Worker::DoStart, this));
}

void FakeAudioWorker::Worker::DoStart() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  first_read_time_ = base::TimeTicks::Now();
  frames_elapsed_ = 0;
  worker_task_cb_.Reset(base::BindRepeating(&Worker::DoRead, this));
  worker_task_cb_.callback().Run();
}

void FakeAudioWorker::Worker::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  {
    base::AutoLock scoped_lock(worker_cb_lock_);
    if (!worker_cb_)
      return;
    worker_cb_.Reset();
  }
  worker_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(&Worker::DoCancel, this));
}

void FakeAudioWorker::Worker::DoCancel() {
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  worker_task_cb_.Cancel();
}

void FakeAudioWorker::Worker::DoRead() {
  TRACE_EVENT_BEGIN0(TRACE_DISABLED_BY_DEFAULT("audio"), "Worker::DoRead");
  DCHECK(worker_task_runner_->RunsTasksInCurrentSequence());

  const base::TimeTicks read_time =
      first_read_time_ +
      AudioTimestampHelper::FramesToTime(frames_elapsed_, sample_rate_);
  frames_elapsed_ += frames_per_read_;
  base::TimeTicks next_read_time =
      first_read_time_ +
      AudioTimestampHelper::FramesToTime(frames_elapsed_, sample_rate_);

  base::TimeTicks now;
  {
    base::AutoLock scoped_lock(worker_cb_lock_);
    // Important to sample the clock after waiting to acquire the lock.
    now = base::TimeTicks::Now();

    // Note: Even if we're late, this callback must be called. In many cases we
    // are driving an underlying "samples consumed" based clock with these
    // calls.
    if (worker_cb_)
      worker_cb_.Run(read_time, now);
  }

  // If we're behind, find the next nearest ontime interval. Note, we could be
  // behind many intervals (e.g., if the system is resuming from sleep).
  if (next_read_time <= now) {
    frames_elapsed_ = AudioTimestampHelper::TimeToFrames(now - first_read_time_,
                                                         sample_rate_);
    frames_elapsed_ =
        ((frames_elapsed_ / frames_per_read_) + 1) * frames_per_read_;
    next_read_time = first_read_time_ + AudioTimestampHelper::FramesToTime(
                                            frames_elapsed_, sample_rate_);
  }

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("audio"), "Worker post",
               "next_read_time",
               (next_read_time - base::TimeTicks()).InMilliseconds());
  worker_task_runner_->PostDelayedTaskAt(base::subtle::PostDelayedTaskPassKey(),
                                         FROM_HERE, worker_task_cb_.callback(),
                                         next_read_time,
                                         base::subtle::DelayPolicy::kPrecise);
  TRACE_EVENT_END2(TRACE_DISABLED_BY_DEFAULT("audio"), "Worker::DoRead",
                   "read_time",
                   (read_time - base::TimeTicks()).InMilliseconds(), "now",
                   (now - base::TimeTicks()).InMilliseconds());
}

}  // namespace media
