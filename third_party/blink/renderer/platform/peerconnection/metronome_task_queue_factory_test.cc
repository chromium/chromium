// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/metronome_task_queue_factory.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/task_queue/task_queue_test.h"
#include "third_party/webrtc/rtc_base/task_utils/to_queued_task.h"
#include "third_party/webrtc_overrides/metronome_source.h"
#include "third_party/webrtc_overrides/test/metronome_like_task_queue_test.h"

namespace blink {

namespace {

using ::webrtc::TaskQueueTest;

constexpr base::TimeDelta kMetronomeTick = base::Hertz(64);

// Wrapper needed for the TaskQueueTest and MetronomeLikeTaskQueueTest suites.
class TestMetronomeTaskQueueFactory final : public webrtc::TaskQueueFactory {
 public:
  static std::unique_ptr<TestMetronomeTaskQueueFactory>
  CreateWithTestEnvironment() {
    return std::unique_ptr<TestMetronomeTaskQueueFactory>(
        new TestMetronomeTaskQueueFactory(
            std::make_unique<base::test::TaskEnvironment>()));
  }
  static std::unique_ptr<TestMetronomeTaskQueueFactory>
  CreateWithoutTestEnvironment() {
    return std::unique_ptr<TestMetronomeTaskQueueFactory>(
        new TestMetronomeTaskQueueFactory(nullptr));
  }

  std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
  CreateTaskQueue(absl::string_view name, Priority priority) const override {
    return factory_->CreateTaskQueue(name, priority);
  }

 private:
  explicit TestMetronomeTaskQueueFactory(
      std::unique_ptr<base::test::TaskEnvironment> task_environment)
      : task_environment_(std::move(task_environment)),
        metronome_source_(
            base::MakeRefCounted<blink::MetronomeSource>(kMetronomeTick)),
        factory_(CreateWebRtcMetronomeTaskQueueFactory(metronome_source_)) {}

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  scoped_refptr<blink::MetronomeSource> metronome_source_;
  std::unique_ptr<webrtc::TaskQueueFactory> factory_;
};

// Instantiate suite to run all tests defined in
// third_party/webrtc/api/task_queue/task_queue_test.h.
INSTANTIATE_TEST_SUITE_P(
    WebRtcMetronomeTaskQueue,
    TaskQueueTest,
    ::testing::Values([]() {
      return TestMetronomeTaskQueueFactory::CreateWithTestEnvironment();
    }));

// Instantiate suite to run all tests defined in
// third_party/webrtc_overrides/test/metronome_like_task_queue_test.h
INSTANTIATE_TEST_SUITE_P(WebRtcMetronomeTaskQueue,
                         MetronomeLikeTaskQueueTest,
                         ::testing::Values(MetronomeLikeTaskQueueTestParams{
                             .task_queue_factory =
                                 []() {
                                   return TestMetronomeTaskQueueFactory::
                                       CreateWithoutTestEnvironment();
                                 },
                             .metronome_tick = kMetronomeTick}));

}  // namespace

}  // namespace blink
