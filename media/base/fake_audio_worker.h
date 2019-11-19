// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FAKE_AUDIO_WORKER_H_
#define MEDIA_BASE_FAKE_AUDIO_WORKER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {
class AudioParameters;

// A fake audio worker.  Using a provided message loop, FakeAudioWorker will
// call back the provided callback like a real audio consumer or producer would.
class MEDIA_EXPORT FakeAudioWorker {
 public:
  // The worker callback, which is run at regular intervals. |ideal_time| is
  // when the callback was scheduled to run, while |now| is when the callback is
  // actually being run.
  using Callback = base::RepeatingCallback<void(base::TimeTicks ideal_time,
                                                base::TimeTicks now)>;

  // |worker_task_runner| is the task runner on which the closure provided to
  // Start() will be executed on.  This may or may not be the be for the same
  // thread that invokes the Start/Stop methods.
  // |params| is used to determine the frequency of callbacks.
  FakeAudioWorker(
      const scoped_refptr<base::SingleThreadTaskRunner>& worker_task_runner,
      const AudioParameters& params);
  ~FakeAudioWorker();

  // Start executing |worker_cb| at a regular intervals.  Stop() must be called
  // by the same thread before destroying FakeAudioWorker.
  void Start(Callback worker_cb);

  // Stop executing the closure provided to Start(). Blocks until the worker
  // loop is not inside a closure invocation. Safe to call multiple times.
  // Must be called on the same thread that called Start().
  void Stop();

  // Returns a reasonable fixed output delay value for a "sink" using a
  // FakeAudioWorker.
  static base::TimeDelta ComputeFakeOutputDelay(const AudioParameters& params);

 private:
  // All state and implementation is kept within this ref-counted class because
  // cancellation of posted tasks must happen on the worker thread some time
  // after the call to Stop() (on the main thread) returns.
  class Worker;
  const scoped_refptr<Worker> worker_;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioWorker);
};

}  // namespace media

#endif  // MEDIA_BASE_FAKE_AUDIO_WORKER_H_
