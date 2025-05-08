// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/backing_store_pre_close_task_queue.h"

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/indexed_db/status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"

using blink::IndexedDBDatabaseMetadata;

namespace content::indexed_db {

using PreCloseTask = BackingStorePreCloseTaskQueue::PreCloseTask;

namespace {
constexpr base::TimeDelta kTestMaxRunTime = base::Seconds(30);
const std::u16string kDBName = u"TestDBName";
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
               void(const std::vector<
                    std::unique_ptr<IndexedDBDatabaseMetadata>>* metadata));

  MOCK_METHOD0(RunRound, bool());
};

void SetBoolValue(bool* pointer, bool value_to_set) {
  DCHECK(pointer);
  *pointer = value_to_set;
}

Status MetadataFetcher(
    bool* called,
    Status return_status,
    std::vector<std::unique_ptr<IndexedDBDatabaseMetadata>>* metadata,
    std::vector<std::unique_ptr<IndexedDBDatabaseMetadata>>* output_metadata) {
  *called = true;
  for (const auto& md : *metadata) {
    output_metadata->push_back(
        std::make_unique<IndexedDBDatabaseMetadata>(*md));
  }
  return return_status;
}

}  // namespace

class BackingStorePreCloseTaskQueueTest : public testing::Test {
 public:
  BackingStorePreCloseTaskQueueTest() {
    metadata_.emplace_back(
        std::make_unique<IndexedDBDatabaseMetadata>(kDBName));
    metadata_.back()->version = kDBVersion;
    metadata_.back()->max_object_store_id = kDBMaxObjectStoreId;
  }
  ~BackingStorePreCloseTaskQueueTest() override = default;

 protected:
  std::vector<std::unique_ptr<IndexedDBDatabaseMetadata>> metadata_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(BackingStorePreCloseTaskQueueTest, NoTasks) {
  bool done_called = false;
  bool metadata_called = false;

  BackingStorePreCloseTaskQueue queue(
      std::list<std::unique_ptr<PreCloseTask>>(),
      base::BindOnce(&SetBoolValue, &done_called, true), kTestMaxRunTime,
      base::BindOnce(&MetadataFetcher, &metadata_called, Status::OK(),
                     &metadata_));
  queue.Start();

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

  EXPECT_CALL(task_ref, SetMetadata(testing::_));

  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(std::move(task));
  BackingStorePreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime,
      base::BindOnce(&MetadataFetcher, &metadata_called, Status::OK(),
                     &metadata_));
  queue.Start();

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

  EXPECT_CALL(task_ref, SetMetadata(testing::_));

  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(std::move(task));
  BackingStorePreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime,
      base::BindOnce(&MetadataFetcher, &metadata_called, Status::OK(),
                     &metadata_));
  queue.Start();

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

  EXPECT_CALL(task1_ref, SetMetadata(testing::_));

  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(std::move(task1));
  tasks.push_back(std::move(task2));
  BackingStorePreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime,
      base::BindOnce(&MetadataFetcher, &metadata_called, Status::OK(),
                     &metadata_));
  queue.Start();

  EXPECT_FALSE(queue.done());

  {
    base::RunLoop loop;

    EXPECT_CALL(task1_ref, RunRound())
        .WillOnce(RunClosureThenReturn(loop.QuitClosure(), true));
    EXPECT_CALL(task2_ref, SetMetadata(testing::_));

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
      kTestMaxRunTime,
      base::BindOnce(&MetadataFetcher, &metadata_called, Status::OK(),
                     &metadata_));
  queue.Start();

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

  EXPECT_CALL(task_ref, SetMetadata(testing::_));

  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(std::move(task));
  BackingStorePreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime,
      base::BindOnce(&MetadataFetcher, &metadata_called, Status::OK(),
                     &metadata_));
  queue.Start();

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

  EXPECT_CALL(task1_ref, SetMetadata(testing::_));

  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(std::move(task1));
  tasks.push_back(std::move(task2));
  BackingStorePreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime,
      base::BindOnce(&MetadataFetcher, &metadata_called, Status::OK(),
                     &metadata_));
  queue.Start();

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

TEST_F(BackingStorePreCloseTaskQueueTest, StopForTimeout) {
  bool done_called = false;
  bool metadata_called = false;

  auto task1 = std::make_unique<testing::StrictMock<MockPreCloseTask>>();
  MockPreCloseTask& task1_ref = *task1;

  EXPECT_CALL(*task1,
              SetMetadata(testing::Pointee(testing::SizeIs(metadata_.size()))));

  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(std::move(task1));
  BackingStorePreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime,
      base::BindOnce(&MetadataFetcher, &metadata_called, Status::OK(),
                     &metadata_));
  queue.Start();

  EXPECT_FALSE(queue.done());
  EXPECT_CALL(task1_ref, RunRound())
      .WillRepeatedly(testing::Return(/*done=*/false));

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&base::OneShotTimer::FireNow,
                                base::Unretained(&queue.timeout_timer_)));
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
      kTestMaxRunTime,
      base::BindOnce(&MetadataFetcher, &metadata_called, Status::IOError(""),
                     &metadata_));
  queue.Start();

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(metadata_called);
  EXPECT_TRUE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_TRUE(queue.done());
}

}  // namespace content::indexed_db
