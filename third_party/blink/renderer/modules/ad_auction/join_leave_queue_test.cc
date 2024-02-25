// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ad_auction/join_leave_queue.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class JoinLeaveQueueTest : public testing::Test {
 public:
  JoinLeaveQueueTest()
      : queue_(std::make_unique<JoinLeaveQueue<int>>(
            /*max_active=*/2,
            WTF::BindRepeating(&JoinLeaveQueueTest::Start,
                               base::Unretained(this)))) {}

 protected:
  void Start(int&& i) { start_order_.push_back(i); }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<JoinLeaveQueue<int>> queue_;

  std::vector<int> start_order_;
};

TEST_F(JoinLeaveQueueTest, Basic) {
  EXPECT_EQ(0, queue_->num_active_for_testing());

  queue_->Enqueue(0);
  EXPECT_THAT(start_order_, testing::ElementsAre(0));
  EXPECT_EQ(1, queue_->num_active_for_testing());

  queue_->Enqueue(1);
  EXPECT_THAT(start_order_, testing::ElementsAre(0, 1));
  EXPECT_EQ(2, queue_->num_active_for_testing());

  queue_->OnComplete();
  queue_->OnComplete();
  EXPECT_THAT(start_order_, testing::ElementsAre(0, 1));
  EXPECT_EQ(0, queue_->num_active_for_testing());
}

TEST_F(JoinLeaveQueueTest, ExceedsLimit) {
  queue_->Enqueue(0);
  queue_->Enqueue(1);
  queue_->Enqueue(2);
  queue_->Enqueue(3);
  queue_->Enqueue(4);
  EXPECT_THAT(start_order_, testing::ElementsAre(0, 1));
  EXPECT_EQ(2, queue_->num_active_for_testing());

  queue_->OnComplete();
  EXPECT_THAT(start_order_, testing::ElementsAre(0, 1, 2));
  EXPECT_EQ(2, queue_->num_active_for_testing());

  queue_->OnComplete();
  EXPECT_THAT(start_order_, testing::ElementsAre(0, 1, 2, 3));
  EXPECT_EQ(2, queue_->num_active_for_testing());

  queue_->OnComplete();
  EXPECT_THAT(start_order_, testing::ElementsAre(0, 1, 2, 3, 4));
  EXPECT_EQ(2, queue_->num_active_for_testing());

  queue_->OnComplete();
  EXPECT_THAT(start_order_, testing::ElementsAre(0, 1, 2, 3, 4));
  EXPECT_EQ(1, queue_->num_active_for_testing());

  queue_->OnComplete();
  EXPECT_THAT(start_order_, testing::ElementsAre(0, 1, 2, 3, 4));
  EXPECT_EQ(0, queue_->num_active_for_testing());
}

TEST_F(JoinLeaveQueueTest, DestroyedWithRequestsQueued) {
  queue_->Enqueue(0);
  queue_->Enqueue(1);
  queue_->Enqueue(2);
  queue_->Enqueue(3);

  EXPECT_THAT(start_order_, testing::ElementsAre(0, 1));
  EXPECT_EQ(2, queue_->num_active_for_testing());

  queue_.reset();
  EXPECT_THAT(start_order_, testing::ElementsAre(0, 1));
}

}  // namespace blink
