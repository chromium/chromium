// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/scoped_scheduler_overrider.h"

#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {

class ThreadWithCustomScheduler : public Thread {
 public:
  explicit ThreadWithCustomScheduler(ThreadScheduler* scheduler)
      : scheduler_(scheduler) {}
  ~ThreadWithCustomScheduler() override {}

  ThreadScheduler* Scheduler() override { return scheduler_; }

 private:
  ThreadScheduler* scheduler_;
};

}  // namespace

ScopedSchedulerOverrider::ScopedSchedulerOverrider(ThreadScheduler* scheduler)
    : main_thread_overrider_(
          std::make_unique<ThreadWithCustomScheduler>(scheduler)) {}

ScopedSchedulerOverrider::~ScopedSchedulerOverrider() {}

}  // namespace blink
