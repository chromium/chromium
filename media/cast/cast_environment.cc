// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/cast_environment.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"

using base::SingleThreadTaskRunner;

namespace media {
namespace cast {

CastEnvironment::CastEnvironment(
    const base::TickClock* clock,
    scoped_refptr<SingleThreadTaskRunner> main_thread_proxy,
    scoped_refptr<SingleThreadTaskRunner> audio_thread_proxy,
    scoped_refptr<SingleThreadTaskRunner> video_thread_proxy)
    : main_thread_proxy_(main_thread_proxy),
      audio_thread_proxy_(audio_thread_proxy),
      video_thread_proxy_(video_thread_proxy),
      clock_(clock),
      logger_(this) {}

CastEnvironment::~CastEnvironment() = default;

bool CastEnvironment::PostTask(ThreadId identifier,
                               const base::Location& from_here,
                               const base::Closure& task) {
  return GetTaskRunner(identifier)->PostTask(from_here, task);
}

bool CastEnvironment::PostDelayedTask(ThreadId identifier,
                                      const base::Location& from_here,
                                      const base::Closure& task,
                                      base::TimeDelta delay) {
  return GetTaskRunner(identifier)->PostDelayedTask(from_here, task, delay);
}

scoped_refptr<SingleThreadTaskRunner> CastEnvironment::GetTaskRunner(
    ThreadId identifier) const {
  switch (identifier) {
    case CastEnvironment::MAIN:
      return main_thread_proxy_;
    case CastEnvironment::AUDIO:
      return audio_thread_proxy_;
    case CastEnvironment::VIDEO:
      return video_thread_proxy_;
    default:
      NOTREACHED() << "Invalid Thread identifier";
      return nullptr;
  }
}

bool CastEnvironment::CurrentlyOn(ThreadId identifier) {
  switch (identifier) {
    case CastEnvironment::MAIN:
      return main_thread_proxy_.get() &&
             main_thread_proxy_->RunsTasksInCurrentSequence();
    case CastEnvironment::AUDIO:
      return audio_thread_proxy_.get() &&
             audio_thread_proxy_->RunsTasksInCurrentSequence();
    case CastEnvironment::VIDEO:
      return video_thread_proxy_.get() &&
             video_thread_proxy_->RunsTasksInCurrentSequence();
    default:
      NOTREACHED() << "Invalid thread identifier";
      return false;
  }
}

}  // namespace cast
}  // namespace media
