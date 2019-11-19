// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_EXECUTOR_H_
#define CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_EXECUTOR_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_executor.h"
#include "build/build_config.h"
#include "content/browser/scheduler/browser_io_thread_delegate.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

// The BrowserTaskExecutor's job is to map base::TaskTraits to actual task
// queues for the browser process.
//
// We actually have three TaskExecutors:
// * BrowserTaskExecutor registered for BrowserTaskTraitsExtension.
// * BrowserTaskExecutor::UIThreadExecutor registered with UI thread TLS.
// * BrowserTaskExecutor::IOThreadExecutor registered with IO thread TLS.
//
// This lets us efficiently implement base::CurrentThread on UI and IO threads.
namespace content {

class BrowserTaskExecutorTest;
class BrowserProcessSubThread;

class CONTENT_EXPORT BaseBrowserTaskExecutor : public base::TaskExecutor {
 public:
  BaseBrowserTaskExecutor();
  ~BaseBrowserTaskExecutor() override;

  // base::TaskExecutor implementation.
  bool PostDelayedTask(const base::Location& from_here,
                       const base::TaskTraits& traits,
                       base::OnceClosure task,
                       base::TimeDelta delay) override;

  scoped_refptr<base::TaskRunner> CreateTaskRunner(
      const base::TaskTraits& traits) override;

  scoped_refptr<base::SequencedTaskRunner> CreateSequencedTaskRunner(
      const base::TaskTraits& traits) override;

  scoped_refptr<base::SingleThreadTaskRunner> CreateSingleThreadTaskRunner(
      const base::TaskTraits& traits,
      base::SingleThreadTaskRunnerThreadMode thread_mode) override;

#if defined(OS_WIN)
  scoped_refptr<base::SingleThreadTaskRunner> CreateCOMSTATaskRunner(
      const base::TaskTraits& traits,
      base::SingleThreadTaskRunnerThreadMode thread_mode) override;
#endif  // defined(OS_WIN)

  struct ThreadIdAndQueueType {
    BrowserThread::ID thread_id;
    BrowserTaskQueues::QueueType queue_type;
  };

  ThreadIdAndQueueType GetThreadIdAndQueueType(
      const base::TaskTraits& traits) const;

 protected:
  virtual BrowserThread::ID GetCurrentThreadID() const = 0;

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      const base::TaskTraits& traits) const;

  scoped_refptr<BrowserUIThreadScheduler::Handle> browser_ui_thread_handle_;
  scoped_refptr<BrowserIOThreadDelegate::Handle> browser_io_thread_handle_;
};

class CONTENT_EXPORT BrowserTaskExecutor : public BaseBrowserTaskExecutor {
 public:
  // Creates and registers a BrowserTaskExecutor on the current thread which
  // owns a BrowserUIThreadScheduler. This facilitates posting tasks to a
  // BrowserThread via //base/task/post_task.h.
  // All BrowserThread::UI task queues except best effort ones are also enabled.
  // TODO(carlscab): These queues should be enabled in
  // BrowserMainLoop::InitializeMainThread() but some Android tests fail if we
  // do so.
  static void Create();

  // Creates the IO thread using the scheduling infrastructure set up in the
  // Create() method. That means that clients have access to TaskRunners
  // associated with the IO thread before that thread is even created. In order
  // to do so this class will own the Thread::Delegate for the IO thread
  // (BrowserIOThreadDelegate) until the thread is created, at which point
  // ownership will be transferred and the |BrowserTaskExecutor| will only
  // communicate with it via TaskRunner instances.
  //
  // Browser task queues will initially be disabled, that is tasks posted to
  // them will not run. But the default task runner of the thread (the one you
  // get via ThreadTaskRunnerHandle::Get()) will be active. This is the same
  // task runner you get by calling BrowserProcessSubThread::task_runner(). The
  // queues can be initialized by calling InitializeIOThread which is done
  // during Chromium starup in BrowserMainLoop::CreateThreads.
  //
  // Early on during Chromium startup we initialize the ServiceManager and it
  // needs to run tasks immediately. The ServiceManager itself does not know
  // about the IO thread (it does not use the browser task traits), it only uses
  // the task runner provided to it during initialization and possibly
  // ThreadTaskRunnerHandle::Get() from tasks it posts. But we currently run it
  // on the IO thread so we need the default task runner to be active for its
  // tasks to run. Note that since tasks posted via the browser task traits will
  // not run they won't be able to access the default task runner either, so for
  // those tasks the default task queue is also "disabled".
  //
  // Attention: This method can only be called once (as there must be only one
  // IO thread).
  // Attention: Must be called after Create()
  // Attention: Can not be called after Shutdown() or ResetForTesting()
  static std::unique_ptr<BrowserProcessSubThread> CreateIOThread();

  // Enables non best effort queues on the IO thread. Usually called from
  // BrowserMainLoop::CreateThreads.
  static void InitializeIOThread();

  // Enables all queues on all threads.
  // Can be called multiple times.
  static void EnableAllQueues();

  // As Create but with the user provided objects. Must call
  // BindToUIThreadForTesting before tasks can be run on the UI thread.
  static void CreateForTesting(
      std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler,
      std::unique_ptr<BrowserIOThreadDelegate> browser_io_thread_delegate);

  // Completes ui-thread set up. Must be called on the UI thread.
  static void BindToUIThreadForTesting();

  // This must be called after the FeatureList has been initialized in order
  // for scheduling experiments to function.
  static void PostFeatureListSetup();

  // Winds down the BrowserTaskExecutor, after this no tasks can be executed
  // and the base::TaskExecutor APIs are non-functional but won't crash if
  // called. In unittests however we need to clean up, so
  // BrowserTaskExecutor::ResetForTesting should be
  // called (~BrowserTaskEnvironment() takes care of this).
  static void Shutdown();

  // Unregister and delete the TaskExecutor after a test.
  static void ResetForTesting();

  // Runs all pending tasks for the given thread. Tasks posted after this method
  // is called (in particular any task posted from within any of the pending
  // tasks) will be queued but not run. Conceptually this call will disable all
  // queues, run any pending tasks, and re-enable all the queues.
  //
  // If any of the pending tasks posted a task, these could be run by calling
  // this method again or running a regular RunLoop. But if that were the case
  // you should probably rewrite you tests to wait for a specific event instead.
  static void RunAllPendingTasksOnThreadForTesting(
      BrowserThread::ID identifier);

#if DCHECK_IS_ON()
  // Adds a Validator for |traits|. It is assumed the lifetime of |validator| is
  // is longer than that of the BrowserTaskExecutor unless RemoveValidator
  // is called. Does nothing if the BrowserTaskExecutor is not registered.
  static void AddValidator(const base::TaskTraits& traits,
                           BrowserTaskQueues::Validator* validator);

  // Removes a Validator previously added by AddValidator. Does nothing if the
  // BrowserTaskExecutor is not registered.
  static void RemoveValidator(const base::TaskTraits& traits,
                              BrowserTaskQueues::Validator* validator);
#endif  // DCHECK_IS_ON()

  // base::TaskExecutor implementation.
  const scoped_refptr<base::SequencedTaskRunner>& GetContinuationTaskRunner()
      override;

 private:
  friend class BrowserIOThreadDelegate;
  friend class BrowserTaskExecutorTest;

  // Constructed on UI thread and registered with UI thread TLS. This backs the
  // implementation of base::CurrentThread for the browser UI thread.
  class UIThreadExecutor : public BaseBrowserTaskExecutor {
   public:
    explicit UIThreadExecutor(
        std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler);

    ~UIThreadExecutor() override;

    // base::TaskExecutor implementation.
    const scoped_refptr<base::SequencedTaskRunner>& GetContinuationTaskRunner()
        override;

    scoped_refptr<BrowserUIThreadScheduler::Handle> GetUIThreadHandle();

    void SetIOThreadHandle(
        scoped_refptr<BrowserUIThreadScheduler::Handle> io_thread_handle);

    void BindToCurrentThread();

   private:
    BrowserThread::ID GetCurrentThreadID() const override;

    std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler_;
    bool bound_to_thread_ = false;
  };

  // Constructed on UI thread and later registered with IO thread TLS. This
  // backs the implementation of base::CurrentThread for the browser IO thread.
  class IOThreadExecutor : public BaseBrowserTaskExecutor {
   public:
    explicit IOThreadExecutor(
        std::unique_ptr<BrowserIOThreadDelegate> browser_io_thread_delegate);

    ~IOThreadExecutor() override;

    // base::TaskExecutor implementation.
    const scoped_refptr<base::SequencedTaskRunner>& GetContinuationTaskRunner()
        override;

    scoped_refptr<BrowserUIThreadScheduler::Handle> GetIOThreadHandle();

    void SetUIThreadHandle(
        scoped_refptr<BrowserUIThreadScheduler::Handle> ui_thread_handle);

    std::unique_ptr<BrowserIOThreadDelegate> TakeDelegate() {
      return std::move(browser_io_thread_delegate_);
    }

    bool HasDelegateForTesting() const { return !!browser_io_thread_delegate_; }

   private:
    BrowserThread::ID GetCurrentThreadID() const override;

    std::unique_ptr<BrowserIOThreadDelegate> browser_io_thread_delegate_;
  };

  static void CreateInternal(
      std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler,
      std::unique_ptr<BrowserIOThreadDelegate> browser_io_thread_delegate);

  // For GetProxyTaskRunnerForThread().
  FRIEND_TEST_ALL_PREFIXES(BrowserTaskExecutorTest,
                           EnsureUIThreadTraitPointsToExpectedQueue);
  FRIEND_TEST_ALL_PREFIXES(BrowserTaskExecutorTest,
                           EnsureIOThreadTraitPointsToExpectedQueue);
  FRIEND_TEST_ALL_PREFIXES(BrowserTaskExecutorTest,
                           BestEffortTasksRunAfterStartup);

  // For Get();
  FRIEND_TEST_ALL_PREFIXES(BrowserTaskExecutorTest,
                           RegisterExecutorForBothThreads);
  explicit BrowserTaskExecutor(
      std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler,
      std::unique_ptr<BrowserIOThreadDelegate> browser_io_thread_delegate);
  ~BrowserTaskExecutor() override;

  BrowserThread::ID GetCurrentThreadID() const override;

  static BrowserTaskExecutor* Get();

  std::unique_ptr<UIThreadExecutor> ui_thread_executor_;
  std::unique_ptr<IOThreadExecutor> io_thread_executor_;

  DISALLOW_COPY_AND_ASSIGN(BrowserTaskExecutor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_EXECUTOR_H_
