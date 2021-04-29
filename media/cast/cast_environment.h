// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_CAST_ENVIRONMENT_H_
#define MEDIA_CAST_CAST_ENVIRONMENT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/logging/log_event_dispatcher.h"

namespace media {
namespace cast {

class CastEnvironment : public base::RefCountedThreadSafe<CastEnvironment> {
 public:
  // An enumeration of the cast threads.
  enum ThreadId {
    // The main thread is where the cast system is configured and where timers
    // and network IO is performed.
    MAIN,
    // The audio thread is where all send side audio processing is done,
    // primarily encoding / decoding but also re-sampling.
    AUDIO,
    // The video encoder thread is where the video processing is done.
    VIDEO,
  };

  CastEnvironment(
      const base::TickClock* clock,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_proxy,
      scoped_refptr<base::SingleThreadTaskRunner> audio_thread_proxy,
      scoped_refptr<base::SingleThreadTaskRunner> video_thread_proxy);

  // These are the same methods in message_loop.h, but are guaranteed to either
  // get posted to the MessageLoop if it's still alive, or be deleted otherwise.
  // They return true iff the thread existed and the task was posted.  Note that
  // even if the task is posted, there's no guarantee that it will run, since
  // the target thread may already have a Quit message in its queue.
  bool PostTask(ThreadId identifier,
                const base::Location& from_here,
                base::OnceClosure task);

  bool PostDelayedTask(ThreadId identifier,
                       const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay);

  bool CurrentlyOn(ThreadId identifier);

  // All of the media::cast implementation must use this TickClock.
  const base::TickClock* Clock() const { return clock_; }

  // Thread-safe log event dispatcher.
  LogEventDispatcher* logger() { return &logger_; }

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      ThreadId identifier) const;

  bool HasAudioThread() { return audio_thread_proxy_.get() ? true : false; }

  bool HasVideoThread() { return video_thread_proxy_.get() ? true : false; }

 protected:
  virtual ~CastEnvironment();

  // Subclasses may final these.
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_proxy_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_thread_proxy_;
  scoped_refptr<base::SingleThreadTaskRunner> video_thread_proxy_;
  const base::TickClock* clock_;
  LogEventDispatcher logger_;

 private:
  friend class base::RefCountedThreadSafe<CastEnvironment>;

  DISALLOW_COPY_AND_ASSIGN(CastEnvironment);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_ENVIRONMENT_H_
