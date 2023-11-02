// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace blink {

ThreadScheduler* ThreadScheduler::Current() {
  return Thread::Current()->Scheduler();
}

}  // namespace blink
