// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_WEB_WIDGET_SCHEDULER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_WEB_WIDGET_SCHEDULER_H_

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {
namespace scheduler {

class BLINK_PLATFORM_EXPORT WebWidgetScheduler {
 public:
  virtual ~WebWidgetScheduler() = default;

  // Returns the input task runner.
  virtual scoped_refptr<base::SingleThreadTaskRunner> InputTaskRunner() = 0;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_WEB_WIDGET_SCHEDULER_H_
