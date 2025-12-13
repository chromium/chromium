// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/scheduler/network_service_task_queues.h"

#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "net/base/request_priority.h"
#include "services/network/public/cpp/network_service_task_priority.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

using StrictMockTask =
    testing::StrictMock<base::MockCallback<base::RepeatingCallback<void()>>>;

// Test fixture for NetworkServiceTaskQueues. Sets up a SequenceManager and
// NetworkServiceTaskQueues instance for each test.
class NetworkServiceTaskQueuesTest : public testing::Test {
 protected:
  NetworkServiceTaskQueuesTest()
      : sequence_manager_(
            base::sequence_manager::
                CreateSequenceManagerOnCurrentThreadWithPump(
                    base::MessagePump::Create(base::MessagePumpType::DEFAULT),
                    base::sequence_manager::SequenceManager::Settings::Builder()
                        .SetPrioritySettings(
                            CreateNetworkServiceTaskPrioritySettings())
                        .Build())),
        queues_(sequence_manager_.get()) {
    sequence_manager_->SetDefaultTaskRunner(queues_.GetDefaultTaskRunner());
  }

  std::unique_ptr<base::sequence_manager::SequenceManager> sequence_manager_;
  NetworkServiceTaskQueues queues_;
};

// Tests that tasks posted to the default task runner are executed in order.
TEST_F(NetworkServiceTaskQueuesTest, SimplePosting) {
  scoped_refptr<base::SingleThreadTaskRunner> tq =
      queues_.GetDefaultTaskRunner();

  StrictMockTask task_1;
  StrictMockTask task_2;

  testing::InSequence s;
  EXPECT_CALL(task_1, Run);
  EXPECT_CALL(task_2, Run);

  base::RunLoop run_loop;
  tq->PostTask(FROM_HERE, task_1.Get());
  tq->PostTask(FROM_HERE, task_2.Get());
  tq->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// Tests that tasks posted to different priority queues are executed according
// to their priority (highest priority first, then default).
TEST_F(NetworkServiceTaskQueuesTest, PostingToMultipleQueues) {
  scoped_refptr<base::SingleThreadTaskRunner> tq1 =
      queues_.GetDefaultTaskRunner();
  scoped_refptr<base::SingleThreadTaskRunner> tq2 =
      queues_.GetTaskRunner(net::RequestPriority::HIGHEST);

  StrictMockTask task_1;
  StrictMockTask task_2;

  testing::InSequence s;
  EXPECT_CALL(task_2, Run);
  EXPECT_CALL(task_1, Run);

  base::RunLoop run_loop;

  tq1->PostTask(FROM_HERE, task_1.Get());
  tq2->PostTask(FROM_HERE, task_2.Get());

  tq1->PostTask(FROM_HERE, run_loop.QuitClosure());

  run_loop.Run();
}

}  // namespace
}  // namespace network
