// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/debug_command_queue.h"

#include <string>
#include <vector>

#include "base/synchronization/lock.h"
#include "base/test/task_environment.h"
#include "base/thread_annotations.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace auction_worklet {

class DebugCommandQueueTest : public testing::Test {
 public:
  DebugCommandQueueTest() : v8_runner_(AuctionV8Helper::CreateTaskRunner()) {
    base::RunLoop run_loop;
    // Create `DebugCommandQueue on `v8_runner_`.
    v8_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<DebugCommandQueue>* out, base::OnceClosure done) {
              *out = base::MakeRefCounted<DebugCommandQueue>();
              std::move(done).Run();
            },
            &command_queue_, run_loop.QuitClosure()));
    run_loop.Run();
  }

  void QueueFromV8ThreadAndWait(base::OnceClosure to_post) {
    base::RunLoop run_loop;
    v8_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<DebugCommandQueue> queue,
               base::OnceClosure to_post, base::OnceClosure done) {
              queue->QueueTaskForV8Thread(std::move(to_post));
              std::move(done).Run();
            },
            command_queue_, std::move(to_post), run_loop.QuitClosure()));
    run_loop.Run();
  }

  std::vector<std::string> TakeLog() {
    base::AutoLock auto_lock(lock_);
    std::vector<std::string> result = std::move(log_);
    return result;
  }

  base::OnceClosure LogString(std::string msg) {
    return base::BindOnce(&DebugCommandQueueTest::DoLog, base::Unretained(this),
                          std::move(msg));
  }

  base::OnceClosure PauseForDebuggerAndRunCommands() {
    return base::BindOnce(
        &DebugCommandQueueTest::DoPauseForDebuggerAndRunCommands,
        base::Unretained(this));
  }

  base::OnceClosure QuitPauseForDebugger() {
    return base::BindOnce(&DebugCommandQueueTest::DoQuitPauseForDebugger,
                          base::Unretained(this));
  }

 protected:
  void DoLog(std::string message) {
    DCHECK(v8_runner_->RunsTasksInCurrentSequence());
    base::AutoLock auto_lock(lock_);
    log_.push_back(std::move(message));
  }

  void DoPauseForDebuggerAndRunCommands() {
    DoLog("Pause start");
    command_queue_->PauseForDebuggerAndRunCommands();
    DoLog("Pause end");
  }

  void DoQuitPauseForDebugger() {
    DoLog("Requested pause end");
    command_queue_->QuitPauseForDebugger();
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> v8_runner_;
  scoped_refptr<DebugCommandQueue> command_queue_;
  std::vector<std::string> log_ GUARDED_BY(lock_);
  base::Lock lock_;
};

TEST_F(DebugCommandQueueTest, TopLevel) {
  base::RunLoop run_loop;
  command_queue_->QueueTaskForV8Thread(LogString("1"));
  command_queue_->QueueTaskForV8Thread(LogString("2"));
  command_queue_->QueueTaskForV8Thread(LogString("3"));
  command_queue_->QueueTaskForV8Thread(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_THAT(TakeLog(), ElementsAre("1", "2", "3"));
}

TEST_F(DebugCommandQueueTest, Paused) {
  base::RunLoop run_loop;
  command_queue_->QueueTaskForV8Thread(LogString("1"));
  command_queue_->QueueTaskForV8Thread(LogString("2"));
  command_queue_->QueueTaskForV8Thread(PauseForDebuggerAndRunCommands());
  command_queue_->QueueTaskForV8Thread(LogString("3"));
  command_queue_->QueueTaskForV8Thread(LogString("4"));
  command_queue_->QueueTaskForV8Thread(QuitPauseForDebugger());
  command_queue_->QueueTaskForV8Thread(LogString("5"));
  command_queue_->QueueTaskForV8Thread(LogString("6"));
  command_queue_->QueueTaskForV8Thread(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_THAT(TakeLog(),
              ElementsAre("1", "2", "Pause start", "3", "4",
                          "Requested pause end", "Pause end", "5", "6"));
}

TEST_F(DebugCommandQueueTest, QueueFromV8Thread) {
  base::RunLoop run_loop;
  QueueFromV8ThreadAndWait(LogString("1"));
  QueueFromV8ThreadAndWait(LogString("2"));
  command_queue_->QueueTaskForV8Thread(LogString("3"));
  command_queue_->QueueTaskForV8Thread(LogString("4"));
  QueueFromV8ThreadAndWait(LogString("5"));
  QueueFromV8ThreadAndWait(LogString("6"));
  command_queue_->QueueTaskForV8Thread(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_THAT(TakeLog(), ElementsAre("1", "2", "3", "4", "5", "6"));
}

TEST_F(DebugCommandQueueTest, QueueFromTask) {
  // A task that itself queues more tasks.
  base::RunLoop run_loop;
  command_queue_->QueueTaskForV8Thread(base::BindOnce(
      [](scoped_refptr<DebugCommandQueue> command_queue, base::OnceClosure log1,
         base::OnceClosure log2, base::OnceClosure log3,
         base::OnceClosure quit_closure) {
        std::move(log1).Run();
        command_queue->QueueTaskForV8Thread(std::move(log2));
        command_queue->QueueTaskForV8Thread(std::move(log3));
        command_queue->QueueTaskForV8Thread(std::move(quit_closure));
      },
      command_queue_, LogString("1"), LogString("2"), LogString("3"),
      run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_THAT(TakeLog(), ElementsAre("1", "2", "3"));
}

TEST_F(DebugCommandQueueTest, QueueFromPauseTask) {
  // A task run from within PauseForDebuggerAndRunCommands() that itself queues
  // more tasks.
  base::RunLoop run_loop;
  command_queue_->QueueTaskForV8Thread(PauseForDebuggerAndRunCommands());
  command_queue_->QueueTaskForV8Thread(base::BindOnce(
      [](scoped_refptr<DebugCommandQueue> command_queue, base::OnceClosure log1,
         base::OnceClosure log2, base::OnceClosure log3,
         base::OnceClosure quit_closure) {
        std::move(log1).Run();
        command_queue->QueueTaskForV8Thread(std::move(log2));
        command_queue->QueueTaskForV8Thread(std::move(log3));
        command_queue->QueueTaskForV8Thread(std::move(quit_closure));
      },
      command_queue_, LogString("1"), LogString("2"), LogString("3"),
      run_loop.QuitClosure()));
  run_loop.Run();

  base::RunLoop run_loop2;
  command_queue_->QueueTaskForV8Thread(QuitPauseForDebugger());
  command_queue_->QueueTaskForV8Thread(run_loop2.QuitClosure());
  run_loop2.Run();

  EXPECT_THAT(TakeLog(), ElementsAre("Pause start", "1", "2", "3",
                                     "Requested pause end", "Pause end"));
}

}  // namespace auction_worklet
