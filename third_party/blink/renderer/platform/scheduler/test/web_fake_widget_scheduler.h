// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_WEB_FAKE_WIDGET_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_WEB_FAKE_WIDGET_SCHEDULER_H_

#include "third_party/blink/public/platform/scheduler/web_widget_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"

namespace blink {
namespace scheduler {

class WebFakeWidgetScheduler final : public WebWidgetScheduler {
 public:
  WebFakeWidgetScheduler() {
    input_task_runner_ = base::MakeRefCounted<FakeTaskRunner>();
  }
  WebFakeWidgetScheduler(const WebFakeWidgetScheduler&) = delete;
  WebFakeWidgetScheduler& operator=(const WebFakeWidgetScheduler&) = delete;
  ~WebFakeWidgetScheduler() override;

  // Returns the input task runner.
  scoped_refptr<base::SingleThreadTaskRunner> InputTaskRunner() override {
    return input_task_runner_;
  }

 private:
  scoped_refptr<FakeTaskRunner> input_task_runner_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_WEB_FAKE_WIDGET_SCHEDULER_H_
