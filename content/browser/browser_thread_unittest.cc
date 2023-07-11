// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_thread.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/browser_process_io_thread.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/scheduler/browser_io_thread_delegate.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/browser/scheduler/browser_task_priority.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace content {

namespace {

using ::testing::Invoke;

class SequenceManagerThreadDelegate : public base::Thread::Delegate {
 public:
  SequenceManagerThreadDelegate() {
    ui_sequence_manager_ = base::sequence_manager::CreateUnboundSequenceManager(
        base::sequence_manager::SequenceManager::Settings::Builder()
            .SetPrioritySettings(internal::CreateBrowserTaskPrioritySettings())
            .Build());
    auto browser_ui_thread_scheduler =
        BrowserUIThreadScheduler::CreateForTesting(ui_sequence_manager_.get());

    default_task_runner_ =
        browser_ui_thread_scheduler->GetHandle()->GetDefaultTaskRunner();

    ui_sequence_manager_->SetDefaultTaskRunner(default_task_runner_);

    BrowserTaskExecutor::CreateForTesting(
        std::move(browser_ui_thread_scheduler),
        std::make_unique<BrowserIOThreadDelegate>());
    BrowserTaskExecutor::OnStartupComplete();
  }

  SequenceManagerThreadDelegate(const SequenceManagerThreadDelegate&) = delete;
  SequenceManagerThreadDelegate& operator=(
      const SequenceManagerThreadDelegate&) = delete;

  ~SequenceManagerThreadDelegate() override {
    BrowserTaskExecutor::ResetForTesting();
  }

  // Thread::Delegate:
  scoped_refptr<base::SingleThreadTaskRunner> GetDefaultTaskRunner() override {
    return default_task_runner_;
  }

  void BindToCurrentThread() override {
    ui_sequence_manager_->BindToMessagePump(
        base::MessagePump::Create(base::MessagePumpType::DEFAULT));
  }

 private:
  std::unique_ptr<base::sequence_manager::SequenceManager> ui_sequence_manager_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
};

}  // namespace

class BrowserThreadTest : public testing::Test {
 public:
  void Release() const {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    EXPECT_TRUE(on_release_);
    std::move(on_release_).Run();
  }

  void AddRef() {}

 protected:
  void SetUp() override {
    ui_thread_ = std::make_unique<base::Thread>(
        BrowserThreadImpl::GetThreadName(BrowserThread::UI));
    base::Thread::Options ui_options;
    ui_options.delegate = std::make_unique<SequenceManagerThreadDelegate>();
    ui_thread_->StartWithOptions(std::move(ui_options));

    io_thread_ = BrowserTaskExecutor::CreateIOThread();
    io_thread_->RegisterAsBrowserThread();
  }

  void TearDown() override {
    io_thread_.reset();
    ui_thread_.reset();

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
  std::unique_ptr<base::Thread> ui_thread_;
  std::unique_ptr<BrowserProcessIOThread> io_thread_;

  base::test::TaskEnvironment task_environment_;
  // Must be set before Release() to verify the deletion is intentional. Will be
  // run from the next call to Release(). mutable so it can be consumed from
  // Release().
  mutable base::OnceClosure on_release_;
};

class UIThreadDestructionObserver
    : public base::CurrentThread::DestructionObserver {
 public:
  explicit UIThreadDestructionObserver(bool* did_shutdown,
                                       base::OnceClosure callback)
      : callback_task_runner_(
            base::SingleThreadTaskRunner::GetCurrentDefault()),
        ui_task_runner_(GetUIThreadTaskRunner({})),
        callback_(std::move(callback)),
        did_shutdown_(did_shutdown) {
    ui_task_runner_->PostTask(FROM_HERE, base::BindOnce(&Watch, this));
  }

 private:
  static void Watch(UIThreadDestructionObserver* observer) {
    base::CurrentThread::Get()->AddDestructionObserver(observer);
  }

  // base::CurrentThread::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    // Ensure that even during MessageLoop teardown the BrowserThread ID is
    // correctly associated with this thread and the BrowserThreadTaskRunner
    // knows it's on the right thread.
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    EXPECT_TRUE(ui_task_runner_->BelongsToCurrentThread());

    base::CurrentThread::Get()->RemoveDestructionObserver(this);
    *did_shutdown_ = true;
    callback_task_runner_->PostTask(FROM_HERE, std::move(callback_));
  }

  const scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  base::OnceClosure callback_;
  raw_ptr<bool> did_shutdown_;
};

TEST_F(BrowserThreadTest, PostTask) {
  base::RunLoop run_loop;
  EXPECT_TRUE(GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                                BrowserThread::IO)));
  run_loop.Run();
}

TEST_F(BrowserThreadTest, Release) {
  base::RunLoop run_loop;
  ExpectRelease(run_loop.QuitWhenIdleClosure());
  BrowserThread::ReleaseSoon(BrowserThread::IO, FROM_HERE,
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
  scoped_refptr<base::TaskRunner> task_runner = GetIOThreadTaskRunner({});
  base::RunLoop run_loop;
  EXPECT_TRUE(task_runner->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                                BrowserThread::IO)));
  run_loop.Run();
}

TEST_F(BrowserThreadTest, PostTaskViaSequencedTaskRunner) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      GetIOThreadTaskRunner({});
  base::RunLoop run_loop;
  EXPECT_TRUE(task_runner->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                                BrowserThread::IO)));
  run_loop.Run();
}

TEST_F(BrowserThreadTest, PostTaskViaSingleThreadTaskRunner) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetIOThreadTaskRunner({});
  base::RunLoop run_loop;
  EXPECT_TRUE(task_runner->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, run_loop.QuitWhenIdleClosure(),
                                BrowserThread::IO)));
  run_loop.Run();
}


TEST_F(BrowserThreadTest, ReleaseViaTaskRunner) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetIOThreadTaskRunner({});
  base::RunLoop run_loop;
  ExpectRelease(run_loop.QuitWhenIdleClosure());
  task_runner->ReleaseSoon(FROM_HERE, base::WrapRefCounted(this));
  run_loop.Run();
}

TEST_F(BrowserThreadTest, PostTaskAndReply) {
  // Most of the heavy testing for PostTaskAndReply() is done inside the
  // task runner test.  This just makes sure we get piped through at all.
  base::RunLoop run_loop;
  ASSERT_TRUE(GetIOThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE, base::DoNothing(), run_loop.QuitWhenIdleClosure()));
  run_loop.Run();
}

class BrowserThreadWithCustomSchedulerTest : public testing::Test {
 private:
  class TaskEnvironmentWithCustomScheduler
      : public base::test::TaskEnvironment {
   public:
    TaskEnvironmentWithCustomScheduler()
        : base::test::TaskEnvironment(
              internal::CreateBrowserTaskPrioritySettings(),
              SubclassCreatesDefaultTaskRunner{}) {
      std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler =
          BrowserUIThreadScheduler::CreateForTesting(sequence_manager());
      DeferredInitFromSubclass(
          browser_ui_thread_scheduler->GetHandle()->GetBrowserTaskRunner(
              QueueType::kDefault));
      BrowserTaskExecutor::CreateForTesting(
          std::move(browser_ui_thread_scheduler),
          std::make_unique<BrowserIOThreadDelegate>());

      io_thread_ = BrowserTaskExecutor::CreateIOThread();
      BrowserTaskExecutor::InitializeIOThread();
      io_thread_->RegisterAsBrowserThread();
    }

    ~TaskEnvironmentWithCustomScheduler() override {
      io_thread_.reset();
      BrowserThreadImpl::ResetGlobalsForTesting(BrowserThread::IO);
      BrowserTaskExecutor::ResetForTesting();
    }

   private:
    std::unique_ptr<BrowserProcessIOThread> io_thread_;
  };

 public:
  using QueueType = BrowserTaskQueues::QueueType;

 protected:
  TaskEnvironmentWithCustomScheduler task_environment_;
};

TEST_F(BrowserThreadWithCustomSchedulerTest, PostBestEffortTask) {
  base::MockOnceClosure best_effort_task;
  base::MockOnceClosure regular_task;

  auto task_runner = GetUIThreadTaskRunner({base::TaskPriority::HIGHEST});

  task_runner->PostTask(FROM_HERE, regular_task.Get());
  BrowserThread::PostBestEffortTask(FROM_HERE, task_runner,
                                    best_effort_task.Get());

  EXPECT_CALL(regular_task, Run).Times(1);
  EXPECT_CALL(best_effort_task, Run).Times(0);
  task_environment_.RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&regular_task);

  BrowserTaskExecutor::OnStartupComplete();
  base::RunLoop run_loop;
  EXPECT_CALL(best_effort_task, Run).WillOnce(Invoke([&]() {
    run_loop.Quit();
  }));
  run_loop.Run();
}

}  // namespace content
