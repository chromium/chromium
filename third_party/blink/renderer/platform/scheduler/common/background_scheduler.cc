// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/background_scheduler.h"

#include "base/location.h"
#include "base/task/post_task.h"

namespace blink {

void background_scheduler::PostOnBackgroundThread(
    const base::Location& location,
    CrossThreadClosure closure) {
  PostOnBackgroundThreadWithTraits(
      location, {base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      std::move(closure));
}

void background_scheduler::PostOnBackgroundThreadWithTraits(
    const base::Location& location,
    const base::TaskTraits& traits,
    CrossThreadClosure closure) {
  base::PostTaskWithTraits(location, traits,
                           ConvertToBaseCallback(std::move(closure)));
}

}  // namespace blink
