// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/task_queue_factory.h"

#include "base/task/task_traits.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/webrtc/api/task_queue/task_queue_test.h"

namespace {

using ::webrtc::TaskQueueTest;

// Wrapper around WebrtcTaskQueueFactory to set up required testing environment.
class TestTaskQueueFactory final : public webrtc::TaskQueueFactory {
 public:
  TestTaskQueueFactory() : factory_(CreateWebRtcTaskQueueFactory()) {}

  std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
  CreateTaskQueue(absl::string_view name, Priority priority) const override {
    return factory_->CreateTaskQueue(name, priority);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<webrtc::TaskQueueFactory> factory_;
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(
    WebRtc,
    TaskQueueTest,
    ::testing::Values(std::make_unique<TestTaskQueueFactory>));
