// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_EXECUTOR_H_
#define CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_EXECUTOR_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/browser/scheduler/browser_io_thread_delegate.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

// The BrowserTaskExecutor's job is to map base::TaskTraits to actual task
// queues for the browser process.
//
// We actually have two TaskExecutors:
// * BrowserTaskExecutor::UIThreadExecutor registered with UI thread TLS.
// * BrowserTaskExecutor::IOThreadExecutor registered with IO thread TLS.
//
// This lets us efficiently implement base::CurrentThread on UI and IO threads.
namespace content {

class BrowserTaskExecutorTest;
class BrowserProcessIOThread;

class CONTENT_EXPORT BaseBrowserTaskExecutor {
 public:
  BaseBrowserTaskExecutor();
  virtual ~BaseBrowserTaskExecutor();

  // Returns the task runner for |traits| under |identifier|. Note: during the
  // migration away from task traits extension, |traits| may also contain a
  // browser thread id, if so, it should match |identifier| (|identifier| has to
  // be provided explicitly because in the new source of tasks it's not part of
  // |traits|) -- ref. crbug.com/1026641.
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      BrowserThread::ID identifier,
      const BrowserTaskTraits& traits) const;

  // Helper to match a QueueType from TaskTraits.
  // TODO(crbug.com/40108370): Take BrowserTaskTraits as a parameter when
  // getting off the need to support base::TaskTraits currently passed to this
  // class in its role as a base::TaskExecutor.
  static BrowserTaskQueues::QueueType GetQueueType(
      const BrowserTaskTraits& traits);

 protected:
  scoped_refptr<BrowserUIThreadScheduler::Handle> browser_ui_thread_handle_;
  scoped_refptr<BrowserIOThreadDelegate::Handle> browser_io_thread_handle_;
};

class CONTENT_EXPORT BrowserTaskExecutor : public BaseBrowserTaskExecutor {
 public:
  // Creates and registers a BrowserTaskExecutor on the current thread which
  // owns a BrowserUIThreadScheduler. This facilitates posting tasks to a
  // BrowserThread via //base/task/post_task.h.
  // TODO(crbug.com/40108370): Clean this up now that post_task.h is deprecated.
  // All BrowserThread::UI task queues except best effort ones are also enabled.
  // TODO(carlscab): These queues should be enabled in
  // BrowserMainLoop::InitializeMainThread() but some Android tests fail if we
  // do so.
  static void Create();

  BrowserTaskExecutor(const BrowserTaskExecutor&) = delete;
  BrowserTaskExecutor& operator=(const BrowserTaskExecutor&) = delete;

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
  // get via SingleThreadTaskRunner::GetCurrentDefault()) will be active. This
  // is the same task runner you get by calling
  // BrowserProcessIOThread::task_runner(). The queues can be initialized by
  // calling InitializeIOThread which is done during Chromium startup in
  // BrowserMainLoop::CreateThreads.
  //
  // Early on during Chromium startup we initialize the ServiceManager and it
  // needs to run tasks immediately. The ServiceManager itself does not know
  // about the IO thread (it does not use the browser task traits), it only uses
  // the task runner provided to it during initialization and possibly
  // SingleThreadTaskRunner::GetCurrentDefault() from tasks it posts. But we
  // currently run it on the IO thread so we need the default task runner to be
  // active for its tasks to run. Note that since tasks posted via the browser
  // task traits will not run they won't be able to access the default task
  // runner either, so for those tasks the default task queue is also
  // "disabled".
  //
  // Attention: This method can only be called once (as there must be only one
  // IO thread).
  // Attention: Must be called after Create()
  // Attention: Can not be called after Shutdown() or ResetForTesting()
  static std::unique_ptr<BrowserProcessIOThread> CreateIOThread();

  // Enables non best effort queues on the IO thread. Usually called from
  // BrowserMainLoop::CreateThreads.
  static void InitializeIOThread();

  // Informs BrowserTaskExecutor that startup is complete.
  // It will communicate that to UI and IO thread BrowserTaskQueues.
  // Can be called multiple times.
  static void OnStartupComplete();

  // Helpers to statically call into BaseBrowserTaskExecutor::GetTaskRunner()
  // from browser_thread_impl.cc. Callers should use browser_thread.h's
  // GetUIThreadTaskRunner over this.
  // TODO(crbug.com/40108370): Clean up this indirection after the migration
  // (once registering a base::BrowserTaskExecutor is no longer necessary).
  static scoped_refptr<base::SingleThreadTaskRunner> GetUIThreadTaskRunner(
      const BrowserTaskTraits& traits);
  static scoped_refptr<base::SingleThreadTaskRunner> GetIOThreadTaskRunner(
      const BrowserTaskTraits& traits);

  // As Create but with the user provided objects.
  static void CreateForTesting(
      std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler,
      std::unique_ptr<BrowserIOThreadDelegate> browser_io_thread_delegate);

  // This must be called after the FeatureList has been initialized in order
  // for scheduling experiments to function.
  static void PostFeatureListSetup();

  // Called when some part of the browser begins handling input. Must be called
  // from the browser UI thread and the value must be reset once input is
  // finished.
  static std::optional<BrowserUIThreadScheduler::UserInputActiveHandle>
  OnUserInputStart();

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

    scoped_refptr<BrowserUIThreadScheduler::Handle> GetUIThreadHandle();

    void SetIOThreadHandle(
        scoped_refptr<BrowserUIThreadScheduler::Handle> io_thread_handle);

    void BindToCurrentThread();

    std::optional<BrowserUIThreadScheduler::UserInputActiveHandle>
    OnUserInputStart();

    void PostFeatureListSetup();

   private:
    std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler_;
  };

  // Constructed on UI thread and later registered with IO thread TLS. This
  // backs the implementation of base::CurrentThread for the browser IO thread.
  class IOThreadExecutor : public BaseBrowserTaskExecutor {
   public:
    explicit IOThreadExecutor(
        std::unique_ptr<BrowserIOThreadDelegate> browser_io_thread_delegate);

    ~IOThreadExecutor() override;

    scoped_refptr<BrowserUIThreadScheduler::Handle> GetIOThreadHandle();

    void SetUIThreadHandle(
        scoped_refptr<BrowserUIThreadScheduler::Handle> ui_thread_handle);

    std::unique_ptr<BrowserIOThreadDelegate> TakeDelegate() {
      return std::move(browser_io_thread_delegate_);
    }

   private:
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

  static BrowserTaskExecutor* Get();

  std::unique_ptr<UIThreadExecutor> ui_thread_executor_;
  std::unique_ptr<IOThreadExecutor> io_thread_executor_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_EXECUTOR_H_
