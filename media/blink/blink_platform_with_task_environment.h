// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_BLINK_PLATFORM_WITH_TASK_ENVIRONMENT_H_
#define MEDIA_BLINK_BLINK_PLATFORM_WITH_TASK_ENVIRONMENT_H_

#include "base/macros.h"
#include "base/test/task_environment.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/web/blink.h"

namespace media {

// We must use a custom blink::Platform that ensures the main thread scheduler
// knows about the TaskEnvironment.
class BlinkPlatformWithTaskEnvironment : public blink::Platform {
 public:
  BlinkPlatformWithTaskEnvironment();
  ~BlinkPlatformWithTaskEnvironment() override;

  blink::scheduler::WebThreadScheduler* GetMainThreadScheduler();

  // Returns |task_environment_| from the current blink::Platform.
  static base::test::TaskEnvironment* GetTaskEnvironment();

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<blink::scheduler::WebThreadScheduler> main_thread_scheduler_;

  DISALLOW_COPY_AND_ASSIGN(BlinkPlatformWithTaskEnvironment);
};

}  // namespace media

#endif  // MEDIA_BLINK_BLINK_PLATFORM_WITH_TASK_ENVIRONMENT_H_
