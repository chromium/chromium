// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/cast_environment.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/tick_clock.h"

namespace media::cast {

CastEnvironment::CastEnvironment(
    const base::TickClock& clock,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_proxy,
    scoped_refptr<base::SingleThreadTaskRunner> audio_thread_proxy,
    scoped_refptr<base::SingleThreadTaskRunner> video_thread_proxy,
    base::OnceClosure deletion_cb)
    : main_thread_proxy_(main_thread_proxy),
      audio_thread_proxy_(audio_thread_proxy),
      video_thread_proxy_(video_thread_proxy),
      clock_(clock),
      logger_(main_thread_proxy, std::move(deletion_cb)) {
  CHECK(main_thread_proxy);
  CHECK(audio_thread_proxy);
  CHECK(video_thread_proxy);
}
CastEnvironment::~CastEnvironment() = default;

bool CastEnvironment::PostTask(ThreadId identifier,
                               const base::Location& from_here,
                               base::OnceClosure task) {
  return GetTaskRunner(identifier)->PostTask(from_here, std::move(task));
}

bool CastEnvironment::PostDelayedTask(ThreadId identifier,
                                      const base::Location& from_here,
                                      base::OnceClosure task,
                                      base::TimeDelta delay) {
  return GetTaskRunner(identifier)
      ->PostDelayedTask(from_here, std::move(task), delay);
}

base::TimeTicks CastEnvironment::NowTicks() const {
  return Clock().NowTicks();
}

scoped_refptr<base::SingleThreadTaskRunner> CastEnvironment::GetTaskRunner(
    ThreadId identifier) const {
  switch (identifier) {
    case ThreadId::kMain:
      return main_thread_proxy_;
    case ThreadId::kAudio:
      return audio_thread_proxy_;
    case ThreadId::kVideo:
      return video_thread_proxy_;
  }
}

bool CastEnvironment::CurrentlyOn(ThreadId identifier) const {
  return GetTaskRunner(identifier)->RunsTasksInCurrentSequence();
}

}  // namespace media::cast
