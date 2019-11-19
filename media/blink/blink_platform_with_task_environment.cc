// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/blink_platform_with_task_environment.h"

namespace media {

BlinkPlatformWithTaskEnvironment::BlinkPlatformWithTaskEnvironment()
    : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
      main_thread_scheduler_(
          blink::scheduler::WebThreadScheduler::CreateMainThreadScheduler()) {}

BlinkPlatformWithTaskEnvironment::~BlinkPlatformWithTaskEnvironment() {
  main_thread_scheduler_->Shutdown();
}

blink::scheduler::WebThreadScheduler*
BlinkPlatformWithTaskEnvironment::GetMainThreadScheduler() {
  return main_thread_scheduler_.get();
}

// static
base::test::TaskEnvironment*
BlinkPlatformWithTaskEnvironment::GetTaskEnvironment() {
  return &static_cast<BlinkPlatformWithTaskEnvironment*>(
              blink::Platform::Current())
              ->task_environment_;
}

}  // namespace media
