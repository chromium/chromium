// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_pre_close_task_queue.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "base/timer/timer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"

using blink::IndexedDBDatabaseMetadata;

namespace content {

using PreCloseTask = IndexedDBPreCloseTaskQueue::PreCloseTask;
using StopReason = IndexedDBPreCloseTaskQueue::StopReason;

namespace {
constexpr base::TimeDelta kTestMaxRunTime = base::TimeDelta::FromSeconds(30);
const base::string16 kDBName = base::ASCIIToUTF16("TestDBName");
constexpr int64_t kDBId = 1;
constexpr int64_t kDBVersion = 2;
constexpr int64_t kDBMaxObjectStoreId = 29;

ACTION_P2(RunClosureThenReturn, closure, ret) {
  closure.Run();
  return ret;
}

class MockPreCloseTask : public PreCloseTask {
 public:
  MockPreCloseTask() {}
  ~MockPreCloseTask() override {}

  MOCK_METHOD1(SetMetadata,
               void(std::vector<IndexedDBDatabaseMetadata> const* metadata));

  MOCK_METHOD1(Stop, void(StopReason reason));

  MOCK_METHOD0(RunRound, bool());
};

void SetBoolValue(bool* pointer, bool value_to_set) {
  DCHECK(pointer);
  *pointer = value_to_set;
}

leveldb::Status MetadataFetcher(
    bool* called,
    leveldb::Status return_status,
    std::vector<IndexedDBDatabaseMetadata>* metadata,
    std::vector<IndexedDBDatabaseMetadata>* output_metadata) {
  *called = true;
  *output_metadata = *metadata;
  return return_status;
}

class IndexedDBPreCloseTaskQueueTest : public testing::Test {
 public:
  IndexedDBPreCloseTaskQueueTest() {
    metadata_.push_back(IndexedDBDatabaseMetadata(kDBName, kDBId, kDBVersion,
                                                  kDBMaxObjectStoreId));
  }
  ~IndexedDBPreCloseTaskQueueTest() override {}

 protected:
  std::vector<IndexedDBDatabaseMetadata> metadata_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(IndexedDBPreCloseTaskQueueTest, NoTasks) {
  bool done_called = false;
  bool metadata_called = false;

  base::MockOneShotTimer* fake_timer = new base::MockOneShotTimer;
  IndexedDBPreCloseTaskQueue queue(
      std::list<std::unique_ptr<PreCloseTask>>(),
      base::BindOnce(&SetBoolValue, &done_called, true), kTestMaxRunTime,
      base::WrapUnique(fake_timer));

  queue.Start(base::BindOnce(&MetadataFetcher, &metadata_called,
                             leveldb::Status::OK(), &metadata_));

  EXPECT_FALSE(metadata_called);
  EXPECT_TRUE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_TRUE(queue.done());
}

TEST_F(IndexedDBPreCloseTaskQueueTest, TaskOneRound) {
  bool done_called = false;
  bool metadata_called = false;

  MockPreCloseTask* task = new testing::StrictMock<MockPreCloseTask>();

  EXPECT_CALL(*task,
              SetMetadata(testing::Pointee(testing::ContainerEq(metadata_))));

  base::MockOneShotTimer* fake_timer = new base::MockOneShotTimer;
  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(base::WrapUnique(task));
  IndexedDBPreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime, base::WrapUnique(fake_timer));

  queue.Start(base::BindOnce(&MetadataFetcher, &metadata_called,
                             leveldb::Status::OK(), &metadata_));

  // Expect calls are posted as tasks.
  EXPECT_CALL(*task, RunRound()).WillOnce(testing::Return(true));

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(metadata_called);
  EXPECT_TRUE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_TRUE(queue.done());
}

TEST_F(IndexedDBPreCloseTaskQueueTest, TaskTwoRounds) {
  bool done_called = false;
  bool metadata_called = false;

  MockPreCloseTask* task = new testing::StrictMock<MockPreCloseTask>();

  EXPECT_CALL(*task,
              SetMetadata(testing::Pointee(testing::ContainerEq(metadata_))));

  base::MockOneShotTimer* fake_timer = new base::MockOneShotTimer;
  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(base::WrapUnique(task));
  IndexedDBPreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime, base::WrapUnique(fake_timer));

  queue.Start(base::BindOnce(&MetadataFetcher, &metadata_called,
                             leveldb::Status::OK(), &metadata_));

  EXPECT_FALSE(queue.done());

  {
    base::RunLoop loop;

    EXPECT_CALL(*task, RunRound())
        .WillOnce(RunClosureThenReturn(loop.QuitClosure(), false));

    loop.Run();
  }

  EXPECT_FALSE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_FALSE(queue.done());

  EXPECT_CALL(*task, RunRound()).WillOnce(testing::Return(true));
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(metadata_called);
  EXPECT_TRUE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_TRUE(queue.done());
}

TEST_F(IndexedDBPreCloseTaskQueueTest, TwoTasks) {
  bool done_called = false;
  bool metadata_called = false;

  MockPreCloseTask* task1 = new testing::StrictMock<MockPreCloseTask>();
  MockPreCloseTask* task2 = new testing::StrictMock<MockPreCloseTask>();

  EXPECT_CALL(*task1,
              SetMetadata(testing::Pointee(testing::ContainerEq(metadata_))));

  base::MockOneShotTimer* fake_timer = new base::MockOneShotTimer;
  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(base::WrapUnique(task1));
  tasks.push_back(base::WrapUnique(task2));
  IndexedDBPreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime, base::WrapUnique(fake_timer));

  queue.Start(base::BindOnce(&MetadataFetcher, &metadata_called,
                             leveldb::Status::OK(), &metadata_));

  EXPECT_FALSE(queue.done());

  {
    base::RunLoop loop;

    EXPECT_CALL(*task1, RunRound())
        .WillOnce(RunClosureThenReturn(loop.QuitClosure(), true));
    EXPECT_CALL(*task2,
                SetMetadata(testing::Pointee(testing::ContainerEq(metadata_))));

    loop.Run();
  }

  {
    base::RunLoop loop;

    EXPECT_CALL(*task2, RunRound())
        .WillOnce(RunClosureThenReturn(loop.QuitClosure(), true));

    loop.Run();
  }

  EXPECT_TRUE(metadata_called);
  EXPECT_TRUE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_TRUE(queue.done());
}

TEST_F(IndexedDBPreCloseTaskQueueTest, StopForNewConnectionBeforeStart) {
  bool done_called = false;
  bool metadata_called = false;

  MockPreCloseTask* task1 = new testing::StrictMock<MockPreCloseTask>();
  MockPreCloseTask* task2 = new testing::StrictMock<MockPreCloseTask>();

  EXPECT_CALL(*task1,
              SetMetadata(testing::Pointee(testing::ContainerEq(metadata_))));

  base::MockOneShotTimer* fake_timer = new base::MockOneShotTimer;
  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(base::WrapUnique(task1));
  tasks.push_back(base::WrapUnique(task2));
  IndexedDBPreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime, base::WrapUnique(fake_timer));

  queue.Start(base::BindOnce(&MetadataFetcher, &metadata_called,
                             leveldb::Status::OK(), &metadata_));

  EXPECT_CALL(*task1, Stop(StopReason::NEW_CONNECTION));
  EXPECT_CALL(*task2, Stop(StopReason::NEW_CONNECTION));

  queue.StopForNewConnection();

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(metadata_called);
  EXPECT_TRUE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_TRUE(queue.done());
}

TEST_F(IndexedDBPreCloseTaskQueueTest, StopForNewConnectionAfterRound) {
  bool done_called = false;
  bool metadata_called = false;

  MockPreCloseTask* task = new testing::StrictMock<MockPreCloseTask>();

  EXPECT_CALL(*task,
              SetMetadata(testing::Pointee(testing::ContainerEq(metadata_))));

  base::MockOneShotTimer* fake_timer = new base::MockOneShotTimer;
  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(base::WrapUnique(task));
  IndexedDBPreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime, base::WrapUnique(fake_timer));

  queue.Start(base::BindOnce(&MetadataFetcher, &metadata_called,
                             leveldb::Status::OK(), &metadata_));

  {
    base::RunLoop loop;

    EXPECT_CALL(*task, RunRound())
        .WillOnce(RunClosureThenReturn(loop.QuitClosure(), false));

    loop.Run();
  }

  EXPECT_CALL(*task, Stop(StopReason::NEW_CONNECTION));

  queue.StopForNewConnection();

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(metadata_called);
  EXPECT_TRUE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_TRUE(queue.done());
}

TEST_F(IndexedDBPreCloseTaskQueueTest, StopForNewConnectionAfterTaskCompletes) {
  bool done_called = false;
  bool metadata_called = false;

  MockPreCloseTask* task1 = new testing::StrictMock<MockPreCloseTask>();
  MockPreCloseTask* task2 = new testing::StrictMock<MockPreCloseTask>();

  EXPECT_CALL(*task1,
              SetMetadata(testing::Pointee(testing::ContainerEq(metadata_))));

  base::MockOneShotTimer* fake_timer = new base::MockOneShotTimer;
  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(base::WrapUnique(task1));
  tasks.push_back(base::WrapUnique(task2));
  IndexedDBPreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime, base::WrapUnique(fake_timer));

  queue.Start(base::BindOnce(&MetadataFetcher, &metadata_called,
                             leveldb::Status::OK(), &metadata_));

  {
    base::RunLoop loop;

    EXPECT_CALL(*task1, RunRound())
        .WillOnce(RunClosureThenReturn(loop.QuitClosure(), true));
    EXPECT_CALL(*task2,
                SetMetadata(testing::Pointee(testing::ContainerEq(metadata_))));

    loop.Run();
  }

  EXPECT_CALL(*task2, Stop(StopReason::NEW_CONNECTION));

  queue.StopForNewConnection();

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(metadata_called);
  EXPECT_TRUE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_TRUE(queue.done());
}

TEST_F(IndexedDBPreCloseTaskQueueTest, StopForTimout) {
  bool done_called = false;
  bool metadata_called = false;

  MockPreCloseTask* task1 = new testing::StrictMock<MockPreCloseTask>();
  MockPreCloseTask* task2 = new testing::StrictMock<MockPreCloseTask>();

  EXPECT_CALL(*task1,
              SetMetadata(testing::Pointee(testing::ContainerEq(metadata_))));

  base::MockOneShotTimer* fake_timer = new base::MockOneShotTimer;
  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(base::WrapUnique(task1));
  tasks.push_back(base::WrapUnique(task2));
  IndexedDBPreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime, base::WrapUnique(fake_timer));

  queue.Start(base::BindOnce(&MetadataFetcher, &metadata_called,
                             leveldb::Status::OK(), &metadata_));

  EXPECT_FALSE(queue.done());

  {
    base::RunLoop loop;

    EXPECT_CALL(*task1, RunRound())
        .WillOnce(RunClosureThenReturn(loop.QuitClosure(), true));
    EXPECT_CALL(*task2,
                SetMetadata(testing::Pointee(testing::ContainerEq(metadata_))));

    loop.Run();
  }
  EXPECT_CALL(*task2, Stop(StopReason::TIMEOUT));

  fake_timer->Fire();

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(metadata_called);
  EXPECT_TRUE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_TRUE(queue.done());
}

TEST_F(IndexedDBPreCloseTaskQueueTest, MetadataError) {
  bool done_called = false;
  bool metadata_called = false;

  MockPreCloseTask* task1 = new testing::StrictMock<MockPreCloseTask>();
  MockPreCloseTask* task2 = new testing::StrictMock<MockPreCloseTask>();

  base::MockOneShotTimer* fake_timer = new base::MockOneShotTimer;
  std::list<std::unique_ptr<PreCloseTask>> tasks;
  tasks.push_back(base::WrapUnique(task1));
  tasks.push_back(base::WrapUnique(task2));
  IndexedDBPreCloseTaskQueue queue(
      std::move(tasks), base::BindOnce(&SetBoolValue, &done_called, true),
      kTestMaxRunTime, base::WrapUnique(fake_timer));

  EXPECT_CALL(*task1, Stop(StopReason::METADATA_ERROR));
  EXPECT_CALL(*task2, Stop(StopReason::METADATA_ERROR));

  queue.Start(base::BindOnce(&MetadataFetcher, &metadata_called,
                             leveldb::Status::IOError(""), &metadata_));

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(metadata_called);
  EXPECT_TRUE(done_called);
  EXPECT_TRUE(queue.started());
  EXPECT_TRUE(queue.done());
}

}  // namespace

}  // namespace content
