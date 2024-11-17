// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/backing_store_pre_close_task_queue.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "base/timer/timer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"

using blink::IndexedDBDatabaseMetadata;

namespace content::indexed_db {

using PreCloseTask = BackingStorePreCloseTaskQueue::PreCloseTask;

namespace {
constexpr base::TimeDelta kTestMaxRunTime = base::Seconds(30);
const std::u16string kDBName = u"TestDBName";
constexpr int64_t kDBId = 1;
constexpr int64_t kDBVersion = 2;
constexpr int64_t kDBMaxObjectStoreId = 29;

ACTION_P2(RunClosureThenReturn, closure, ret) {
  closure.Run();
  return ret;
}

class MockPreCloseTask : public PreCloseTask {
 public:
  MockPreCloseTask() : PreCloseTask(nullptr) {}
  ~MockPreCloseTask() override = default;

  bool RequiresMetadata() const override { return true; }

  MOCK_METHOD1(SetMetadata,
               void(const std::vector<IndexedDBDatabaseMetadata>* metadata));

  MOCK_METHOD0(RunRound, bool());
};

void SetBoolValue(bool* pointer, bool value_to_set) {
  DCHECK(pointer);
  *pointer = value_to_set;
}

Status MetadataFetcher(
    bool* called,
    Status return_status,
    std::vector<IndexedDBDatabaseMetadata>* metadata,
    std::vector<IndexedDBDatabaseMetadata>* output_metadata) {
  *called = true;
  *output_metadata = *metadata;
  return return_status;
}

class BackingStorePreCloseTaskQueueTest : public testing::Test {
 public:
  BackingStorePreCloseTaskQueueTest() {
    metadata_.emplace_back(kDBName, kDBId, kDBVersion, kDBMaxObjectStoreId);
  }
  ~BackingStorePreCloseTaskQueueTest() override = default;

 protected:
  std::vector<IndexedDBDatabaseMetadata> metadata_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(BackingStorePreCloseTaskQueueTest, NoTasks) {
  bool done_called = false;
  bool metadata_called = false;

  BackingStorePreCloseTaskQueue queue(
      std::list<std::unique_ptr<PreCloseTask>>(),
      base::BindOnce(&SetBoolValue, &done_called, true), kTestMaxRunTime,
      std::make_unique<base::MockOneShotTimer>());

  queue.Start(base::BindOnce(&MetadataFetcher, &metadata_called, Status::OK(),
                             &metadata_));

  EXPECT_FALSE(metadata_called);
  EXPECT_TRUE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_TRUE(queue.done());
}

TEST_F(BackingStorePreCloseTaskQueueTest, TaskOneRound) {
  bool done_called = false;
  bool metadata_called = false;

  auto task = std::make_unique<testing::StrictMock<MockPreCloseTask>>();
  MockPreCloseTask& task_ref = *task;

  EXPECT_CALL(task_ref,
              SetMetadata(testing::Pointee(testing::ContainerEq(metadata_))));

  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(std::move(task));
  BackingStorePreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime, std::make_unique<base::MockOneShotTimer>());

  queue.Start(base::BindOnce(&MetadataFetcher, &metadata_called, Status::OK(),
                             &metadata_));

  // Expect calls are posted as tasks.
  EXPECT_CALL(task_ref, RunRound()).WillOnce(testing::Return(true));

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(metadata_called);
  EXPECT_TRUE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_TRUE(queue.done());
}

TEST_F(BackingStorePreCloseTaskQueueTest, TaskTwoRounds) {
  bool done_called = false;
  bool metadata_called = false;

  auto task = std::make_unique<testing::StrictMock<MockPreCloseTask>>();
  MockPreCloseTask& task_ref = *task;

  EXPECT_CALL(task_ref,
              SetMetadata(testing::Pointee(testing::ContainerEq(metadata_))));

  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(std::move(task));
  BackingStorePreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime, std::make_unique<base::MockOneShotTimer>());

  queue.Start(base::BindOnce(&MetadataFetcher, &metadata_called, Status::OK(),
                             &metadata_));

  EXPECT_FALSE(queue.done());

  {
    base::RunLoop loop;

    EXPECT_CALL(task_ref, RunRound())
        .WillOnce(RunClosureThenReturn(loop.QuitClosure(), false));

    loop.Run();
  }

  EXPECT_FALSE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_FALSE(queue.done());

  EXPECT_CALL(task_ref, RunRound()).WillOnce(testing::Return(true));
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(metadata_called);
  EXPECT_TRUE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_TRUE(queue.done());
}

TEST_F(BackingStorePreCloseTaskQueueTest, TwoTasks) {
  bool done_called = false;
  bool metadata_called = false;

  auto task1 = std::make_unique<testing::StrictMock<MockPreCloseTask>>();
  MockPreCloseTask& task1_ref = *task1;
  auto task2 = std::make_unique<testing::StrictMock<MockPreCloseTask>>();
  MockPreCloseTask& task2_ref = *task2;

  EXPECT_CALL(task1_ref,
              SetMetadata(testing::Pointee(testing::ContainerEq(metadata_))));

  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(std::move(task1));
  tasks.push_back(std::move(task2));
  BackingStorePreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime, std::make_unique<base::MockOneShotTimer>());

  queue.Start(base::BindOnce(&MetadataFetcher, &metadata_called, Status::OK(),
                             &metadata_));

  EXPECT_FALSE(queue.done());

  {
    base::RunLoop loop;

    EXPECT_CALL(task1_ref, RunRound())
        .WillOnce(RunClosureThenReturn(loop.QuitClosure(), true));
    EXPECT_CALL(task2_ref,
                SetMetadata(testing::Pointee(testing::ContainerEq(metadata_))));

    loop.Run();
  }

  {
    base::RunLoop loop;

    EXPECT_CALL(task2_ref, RunRound())
        .WillOnce(RunClosureThenReturn(loop.QuitClosure(), true));

    loop.Run();
  }

  EXPECT_TRUE(metadata_called);
  EXPECT_TRUE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_TRUE(queue.done());
}

TEST_F(BackingStorePreCloseTaskQueueTest, StopForNewConnectionBeforeStart) {
  bool done_called = false;
  bool metadata_called = false;

  auto task1 = std::make_unique<testing::StrictMock<MockPreCloseTask>>();
  auto task2 = std::make_unique<testing::StrictMock<MockPreCloseTask>>();

  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(std::move(task1));
  tasks.push_back(std::move(task2));
  BackingStorePreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime, std::make_unique<base::MockOneShotTimer>());

  queue.Start(base::BindOnce(&MetadataFetcher, &metadata_called, Status::OK(),
                             &metadata_));

  queue.Stop();

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(metadata_called);
  EXPECT_TRUE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_TRUE(queue.done());
}

TEST_F(BackingStorePreCloseTaskQueueTest, StopForNewConnectionAfterRound) {
  bool done_called = false;
  bool metadata_called = false;

  auto task = std::make_unique<testing::StrictMock<MockPreCloseTask>>();
  MockPreCloseTask& task_ref = *task;

  EXPECT_CALL(task_ref,
              SetMetadata(testing::Pointee(testing::ContainerEq(metadata_))));

  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(std::move(task));
  BackingStorePreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime, std::make_unique<base::MockOneShotTimer>());

  queue.Start(base::BindOnce(&MetadataFetcher, &metadata_called, Status::OK(),
                             &metadata_));

  {
    base::RunLoop loop;

    EXPECT_CALL(task_ref, RunRound())
        .WillOnce(RunClosureThenReturn(loop.QuitClosure(), false));

    loop.Run();
  }

  queue.Stop();

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(metadata_called);
  EXPECT_TRUE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_TRUE(queue.done());
}

TEST_F(BackingStorePreCloseTaskQueueTest,
       StopForNewConnectionAfterTaskCompletes) {
  bool done_called = false;
  bool metadata_called = false;

  auto task1 = std::make_unique<testing::StrictMock<MockPreCloseTask>>();
  MockPreCloseTask& task1_ref = *task1;
  auto task2 = std::make_unique<testing::StrictMock<MockPreCloseTask>>();

  EXPECT_CALL(task1_ref,
              SetMetadata(testing::Pointee(testing::ContainerEq(metadata_))));

  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(std::move(task1));
  tasks.push_back(std::move(task2));
  BackingStorePreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime, std::make_unique<base::MockOneShotTimer>());

  queue.Start(base::BindOnce(&MetadataFetcher, &metadata_called, Status::OK(),
                             &metadata_));

  {
    base::RunLoop loop;

    EXPECT_CALL(task1_ref, RunRound())
        .WillOnce(RunClosureThenReturn(loop.QuitClosure(), true));

    loop.Run();
  }

  queue.Stop();

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(metadata_called);
  EXPECT_TRUE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_TRUE(queue.done());
}

TEST_F(BackingStorePreCloseTaskQueueTest, StopForTimout) {
  bool done_called = false;
  bool metadata_called = false;

  auto task1 = std::make_unique<testing::StrictMock<MockPreCloseTask>>();
  MockPreCloseTask& task1_ref = *task1;
  auto task2 = std::make_unique<testing::StrictMock<MockPreCloseTask>>();

  EXPECT_CALL(*task1,
              SetMetadata(testing::Pointee(testing::ContainerEq(metadata_))));

  auto fake_timer = std::make_unique<base::MockOneShotTimer>();
  base::MockOneShotTimer& fake_timer_ref = *fake_timer;

  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(std::move(task1));
  tasks.push_back(std::move(task2));
  BackingStorePreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime, std::move(fake_timer));

  queue.Start(base::BindOnce(&MetadataFetcher, &metadata_called, Status::OK(),
                             &metadata_));

  EXPECT_FALSE(queue.done());

  {
    base::RunLoop loop;

    EXPECT_CALL(task1_ref, RunRound())
        .WillOnce(RunClosureThenReturn(loop.QuitClosure(), true));

    loop.Run();
  }

  fake_timer_ref.Fire();

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(metadata_called);
  EXPECT_TRUE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_TRUE(queue.done());
}

TEST_F(BackingStorePreCloseTaskQueueTest, MetadataError) {
  bool done_called = false;
  bool metadata_called = false;

  auto task1 = std::make_unique<testing::StrictMock<MockPreCloseTask>>();
  auto task2 = std::make_unique<testing::StrictMock<MockPreCloseTask>>();

  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(std::move(task1));
  tasks.push_back(std::move(task2));
  BackingStorePreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime, std::make_unique<base::MockOneShotTimer>());

  queue.Start(base::BindOnce(&MetadataFetcher, &metadata_called,
                             Status::IOError(""), &metadata_));

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(metadata_called);
  EXPECT_TRUE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_TRUE(queue.done());
}

}  // namespace

}  // namespace content::indexed_db
