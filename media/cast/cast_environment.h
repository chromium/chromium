// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_CAST_ENVIRONMENT_H_
#define MEDIA_CAST_CAST_ENVIRONMENT_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "media/cast/logging/log_event_dispatcher.h"

namespace base {
class SingleThreadTaskRunner;
class TimeDelta;
class TimeTicks;
class TickClock;
}  // namespace base

namespace media::cast {

class CastEnvironment : public base::RefCountedThreadSafe<CastEnvironment> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  // An enumeration of the cast threads.
  enum class ThreadId {
    // The main thread is where the cast system is configured and where timers
    // and network IO is performed.
    kMain,
    // The audio thread is where all send side audio processing is done,
    // primarily encoding / decoding but also re-sampling.
    kAudio,
    // The video encoder thread is where the video processing is done.
    kVideo,
  };

  CastEnvironment(
      const base::TickClock& clock,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_proxy,
      scoped_refptr<base::SingleThreadTaskRunner> audio_thread_proxy,
      scoped_refptr<base::SingleThreadTaskRunner> video_thread_proxy,
      base::OnceClosure deletion_cb);
  CastEnvironment(const CastEnvironment&) = delete;
  CastEnvironment(CastEnvironment&&) = delete;
  CastEnvironment& operator=(const CastEnvironment&) = delete;
  CastEnvironment& operator=(CastEnvironment&&) = delete;

  // Convenience methods for posting tasks to the task runner associated with
  // `identifier`. They return true iff the thread existed and the task was
  // posted.  Note that even if the task is posted, there's no guarantee that it
  // will run, since the target thread may already have a Quit message in its
  // queue.
  bool PostTask(ThreadId identifier,
                const base::Location& location,
                base::OnceClosure task);
  bool PostDelayedTask(ThreadId identifier,
                       const base::Location& location,
                       base::OnceClosure task,
                       base::TimeDelta delay);

  [[nodiscard]] bool CurrentlyOn(ThreadId identifier) const;

  // All of the media::cast implementation must use this TickClock.
  const base::TickClock& Clock() const { return *clock_; }

  // Convenience method for accessing the current time from the clock.
  base::TimeTicks NowTicks() const;

  // Thread-safe log event dispatcher.
  LogEventDispatcher& logger() { return logger_; }

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      ThreadId identifier) const;

 private:
  friend class base::RefCountedThreadSafe<CastEnvironment>;
  ~CastEnvironment();

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_proxy_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_thread_proxy_;
  scoped_refptr<base::SingleThreadTaskRunner> video_thread_proxy_;
  raw_ref<const base::TickClock> clock_;
  LogEventDispatcher logger_;
};

}  // namespace media::cast

#endif  // MEDIA_CAST_CAST_ENVIRONMENT_H_
