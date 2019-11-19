// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_thread.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/browser_process_sub_thread.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/scheduler/browser_io_thread_delegate.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace content {

namespace {

using ::testing::Invoke;

using StrictMockTask =
    testing::StrictMock<base::MockCallback<base::Callback<void()>>>;

class SequenceManagerThreadDelegate : public base::Thread::Delegate {
 public:
  SequenceManagerThreadDelegate() {
    ui_sequence_manager_ =
        base::sequence_manager::internal::SequenceManagerImpl::CreateUnbound(
            base::sequence_manager::SequenceManager::Settings());
    auto browser_ui_thread_scheduler =
        BrowserUIThreadScheduler::CreateForTesting(
            ui_sequence_manager_.get(),
            ui_sequence_manager_->GetRealTimeDomain());

    default_task_runner_ =
        browser_ui_thread_scheduler->GetHandle()->GetDefaultTaskRunner();

    ui_sequence_manager_->SetDefaultTaskRunner(default_task_runner_);

    BrowserTaskExecutor::CreateForTesting(
        std::move(browser_ui_thread_scheduler),
        std::make_unique<BrowserIOThreadDelegate>());
    BrowserTaskExecutor::EnableAllQueues();
  }

  ~SequenceManagerThreadDelegate() override {
    BrowserTaskExecutor::ResetForTesting();
  }

  // Thread::Delegate:
  scoped_refptr<base::SingleThreadTaskRunner> GetDefaultTaskRunner() override {
    return default_task_runner_;
  }

  void BindToCurrentThread(base::TimerSlack timer_slack) override {
    ui_sequence_manager_->BindToMessagePump(
        base::MessagePump::Create(base::MessagePumpType::DEFAULT));
    ui_sequence_manager_->SetTimerSlack(timer_slack);
    BrowserTaskExecutor::BindToUIThreadForTesting();
  }

 private:
  std::unique_ptr<base::sequence_manager::SequenceManager> ui_sequence_manager_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SequenceManagerThreadDelegate);
};

}  // namespace

class BrowserThreadTest : public testing::Test {
 public:
  void Release() const {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    EXPECT_TRUE(on_release_);
    std::move(on_release_).Run();
  }

  void AddRef() {}

  void StopUIThread() { ui_thread_->Stop(); }

 protected:
  void SetUp() override {
    ui_thread_ = std::make_unique<BrowserProcessSubThread>(BrowserThread::UI);
    base::Thread::Options ui_options;
    ui_options.delegate = new SequenceManagerThreadDelegate();
    ui_thread_->StartWithOptions(ui_options);

    io_thread_ = BrowserTaskExecutor::CreateIOThread();

    ui_thread_->RegisterAsBrowserThread();
    io_thread_->RegisterAsBrowserThread();
  }

  void TearDown() override {
    io_thread_.reset();
    ui_thread_.reset();

    BrowserThreadImpl::ResetGlobalsForTesting(BrowserThread::UI);
    BrowserThreadImpl::ResetGlobalsForTesting(BrowserThread::IO);
    BrowserTaskExecutor::ResetForTesting();
  }

  // Prepares this BrowserThreadTest for Release() to be invoked. |on_release|
  // will be invoked when this occurs.
  void ExpectRelease(base::OnceClosure on_release) {
    on_release_ = std::move(on_release);
  }

  static void BasicFunction(base::OnceClosure continuation,
                            BrowserThread::ID target) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(target));
    std::move(continuation).Run();
  }

  class DeletedOnIO
      : public base::RefCountedThreadSafe<DeletedOnIO,
                                          BrowserThread::DeleteOnIOThread> {
   public:
    explicit DeletedOnIO(base::OnceClosure on_deletion)
        : on_deletion_(std::move(on_deletion)) {}

   private:
    friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
    friend class base::DeleteHelper<DeletedOnIO>;

    ~DeletedOnIO() {
      EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
      std::move(on_deletion_).Run();
    }

    base::OnceClosure on_deletion_;
  };

 private:
  std::unique_ptr<BrowserProcessSubThread> ui_thread_;
  std::unique_ptr<BrowserProcessSubThread> io_thread_;

  base::test::TaskEnvironment task_environment_;
  // Must be set before Release() to verify the deletion is intentional. Will be
  // run from the next call to Release(). mutable so it can be consumed from
  // Release().
  mutable base::OnceClosure on_release_;
};

class UIThreadDestructionObserver
    : public base::MessageLoopCurrent::DestructionObserver {
 public:
  explicit UIThreadDestructionObserver(bool* did_shutdown,
                                       const base::Closure& callback)
      : callback_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        callback_(callback),
        ui_task_runner_(
            base::CreateSingleThreadTaskRunner({BrowserThread::UI})),
        did_shutdown_(did_shutdown) {
    ui_task_runner_->PostTask(FROM_HERE, base::BindOnce(&Watch, this));
  }

 private:
  static void Watch(UIThreadDestructionObserver* observer) {
    base::MessageLoopCurrent::Get()->AddDestructionObserver(observer);
  }

  // base::MessageLoopCurrent::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    // Ensure that even during MessageLoop teardown the BrowserThread ID is
    // correctly associated with this thread and the BrowserThreadTaskRunner
    // knows it's on the right thread.
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    EXPECT_TRUE(ui_task_runner_->BelongsToCurrentThread());

    base::MessageLoopCurrent::Get()->RemoveDestructionObserver(this);
    *did_shutdown_ = true;
    callback_task_runner_->PostTask(FROM_HERE, callback_);
  }

  const scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner_;
  const base::Closure callback_;
  const scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  bool* did_shutdown_;
};

TEST_F(BrowserThreadTest, PostTask) {
  base::RunLoop run_loop;
  EXPECT_TRUE(base::PostTask(
      FROM_HERE, {BrowserThread::IO, NonNestable()},
      base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                     BrowserThread::IO)));
  run_loop.Run();
}

TEST_F(BrowserThreadTest, Release) {
  base::RunLoop run_loop;
  ExpectRelease(run_loop.QuitWhenIdleClosure());
  BrowserThread::ReleaseSoon(BrowserThread::UI, FROM_HERE,
                             base::WrapRefCounted(this));
  run_loop.Run();
}

TEST_F(BrowserThreadTest, ReleasedOnCorrectThread) {
  base::RunLoop run_loop;
  {
    scoped_refptr<DeletedOnIO> test(
        new DeletedOnIO(run_loop.QuitWhenIdleClosure()));
  }
  run_loop.Run();
}

TEST_F(BrowserThreadTest, PostTaskViaTaskRunner) {
  scoped_refptr<base::TaskRunner> task_runner =
      base::CreateTaskRunner({BrowserThread::IO});
  base::RunLoop run_loop;
  EXPECT_TRUE(task_runner->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                                BrowserThread::IO)));
  run_loop.Run();
}

TEST_F(BrowserThreadTest, PostTaskViaSequencedTaskRunner) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::CreateSequencedTaskRunner({BrowserThread::IO});
  base::RunLoop run_loop;
  EXPECT_TRUE(task_runner->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                                BrowserThread::IO)));
  run_loop.Run();
}

TEST_F(BrowserThreadTest, PostTaskViaSingleThreadTaskRunner) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::CreateSingleThreadTaskRunner({BrowserThread::IO});
  base::RunLoop run_loop;
  EXPECT_TRUE(task_runner->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                                BrowserThread::IO)));
  run_loop.Run();
}

#if defined(OS_WIN)
TEST_F(BrowserThreadTest, PostTaskViaCOMSTATaskRunner) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::CreateCOMSTATaskRunner({BrowserThread::UI});
  base::RunLoop run_loop;
  EXPECT_TRUE(task_runner->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                                BrowserThread::UI)));
  run_loop.Run();
}
#endif  // defined(OS_WIN)

TEST_F(BrowserThreadTest, ReleaseViaTaskRunner) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::CreateSingleThreadTaskRunner({BrowserThread::UI});
  base::RunLoop run_loop;
  ExpectRelease(run_loop.QuitWhenIdleClosure());
  task_runner->ReleaseSoon(FROM_HERE, base::WrapRefCounted(this));
  run_loop.Run();
}

TEST_F(BrowserThreadTest, PostTaskAndReply) {
  // Most of the heavy testing for PostTaskAndReply() is done inside the
  // task runner test.  This just makes sure we get piped through at all.
  base::RunLoop run_loop;
  ASSERT_TRUE(base::PostTaskAndReply(FROM_HERE, {BrowserThread::IO},
                                     base::DoNothing(),
                                     run_loop.QuitWhenIdleClosure()));
  run_loop.Run();
}

TEST_F(BrowserThreadTest, RunsTasksInCurrentSequenceDuringShutdown) {
  bool did_shutdown = false;
  base::RunLoop loop;
  UIThreadDestructionObserver observer(&did_shutdown, loop.QuitClosure());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserThreadTest::StopUIThread, base::Unretained(this)));
  loop.Run();

  EXPECT_TRUE(did_shutdown);
}

class BrowserThreadWithCustomSchedulerTest : public testing::Test {
 private:
  class TaskEnvironmentWithCustomScheduler
      : public base::test::TaskEnvironment {
   public:
    TaskEnvironmentWithCustomScheduler()
        : base::test::TaskEnvironment(SubclassCreatesDefaultTaskRunner{}) {
      std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler =
          BrowserUIThreadScheduler::CreateForTesting(sequence_manager(),
                                                     GetTimeDomain());
      DeferredInitFromSubclass(
          browser_ui_thread_scheduler->GetHandle()->GetBrowserTaskRunner(
              QueueType::kDefault));
      BrowserTaskExecutor::CreateForTesting(
          std::move(browser_ui_thread_scheduler),
          std::make_unique<BrowserIOThreadDelegate>());

      ui_thread_ = BrowserTaskExecutor::CreateIOThread();
      BrowserTaskExecutor::InitializeIOThread();
      ui_thread_->RegisterAsBrowserThread();
    }

    ~TaskEnvironmentWithCustomScheduler() override {
      ui_thread_.reset();
      BrowserThreadImpl::ResetGlobalsForTesting(BrowserThread::IO);
      BrowserTaskExecutor::ResetForTesting();
    }

   private:
    std::unique_ptr<BrowserProcessSubThread> ui_thread_;
  };

 public:
  using QueueType = BrowserTaskQueues::QueueType;

 protected:
  TaskEnvironmentWithCustomScheduler task_environment_;
};

TEST_F(BrowserThreadWithCustomSchedulerTest, PostBestEffortTask) {
  StrictMockTask best_effort_task;
  StrictMockTask regular_task;

  auto task_runner =
      base::CreateTaskRunner({BrowserThread::UI, base::TaskPriority::HIGHEST});

  task_runner->PostTask(FROM_HERE, regular_task.Get());
  BrowserThread::PostBestEffortTask(FROM_HERE, task_runner,
                                    best_effort_task.Get());

  EXPECT_CALL(regular_task, Run);
  task_environment_.RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&regular_task);

  BrowserTaskExecutor::EnableAllQueues();
  base::RunLoop run_loop;
  EXPECT_CALL(best_effort_task, Run).WillOnce(Invoke([&]() {
    run_loop.Quit();
  }));
  run_loop.Run();
}

}  // namespace content
