// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_task_executor.h"

#include <atomic>

#include "base/functional/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/threading_features.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/browser_process_io_thread.h"
#include "content/browser/browser_thread_impl.h"
#include "content/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/common/content_features.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/task_scheduler/post_task_android.h"
#include "base/android/task_scheduler/task_runner_android.h"
#include "base/android/task_scheduler/task_traits_android.h"
#endif

using QueueType = content::BrowserTaskQueues::QueueType;

namespace content {

namespace {

// |g_browser_task_executor| is intentionally leaked on shutdown.
BrowserTaskExecutor* g_browser_task_executor = nullptr;

#if BUILDFLAG(IS_ANDROID)
scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForAndroidMainThread(
    ::TaskTraits android_traits) {
  BrowserTaskTraits traits;
  switch (android_traits) {
    case ::TaskTraits::UI_BEST_EFFORT:
      traits = {base::TaskPriority::BEST_EFFORT};
      break;
    case ::TaskTraits::UI_USER_VISIBLE:
      traits = {base::TaskPriority::USER_VISIBLE};
      break;
    case ::TaskTraits::UI_USER_BLOCKING:
      traits = {base::TaskPriority::USER_BLOCKING};
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return g_browser_task_executor->GetUIThreadTaskRunner(traits);
}
#endif

}  // namespace

BaseBrowserTaskExecutor::BaseBrowserTaskExecutor() = default;

BaseBrowserTaskExecutor::~BaseBrowserTaskExecutor() = default;

scoped_refptr<base::SingleThreadTaskRunner>
BaseBrowserTaskExecutor::GetTaskRunner(BrowserThread::ID identifier,
                                       const BrowserTaskTraits& traits) const {
  const QueueType queue_type = GetQueueType(traits);

  switch (identifier) {
    case BrowserThread::UI: {
      return browser_ui_thread_handle_->GetBrowserTaskRunner(queue_type);
    }
    case BrowserThread::IO:
      return browser_io_thread_handle_->GetBrowserTaskRunner(queue_type);
    case BrowserThread::ID_COUNT:
      NOTREACHED_IN_MIGRATION();
  }
  return nullptr;
}

// static
QueueType BaseBrowserTaskExecutor::GetQueueType(
    const BrowserTaskTraits& traits) {
  switch (traits.task_type()) {
    case BrowserTaskType::kUserInput:
      return QueueType::kUserInput;

    case BrowserTaskType::kNavigationNetworkResponse:
      if (base::FeatureList::IsEnabled(
              features::kNavigationNetworkResponseQueue)) {
        return QueueType::kNavigationNetworkResponse;
      }
      // Defer to traits.priority() below.
      break;

    case BrowserTaskType::kServiceWorkerStorageControlResponse:
      return QueueType::kServiceWorkerStorageControlResponse;

    case BrowserTaskType::kBeforeUnloadBrowserResponse:
      if (base::FeatureList::IsEnabled(
              features::kBeforeUnloadBrowserResponseQueue)) {
        return QueueType::kBeforeUnloadBrowserResponse;
      }
      break;

    case BrowserTaskType::kDefault:
      // Defer to traits.priority() below.
      break;
  }

  switch (traits.priority()) {
    case base::TaskPriority::BEST_EFFORT:
      return QueueType::kBestEffort;

    case base::TaskPriority::USER_VISIBLE:
      return QueueType::kUserVisible;

    case base::TaskPriority::USER_BLOCKING:
      return QueueType::kUserBlocking;
  }
}

BrowserTaskExecutor::BrowserTaskExecutor(
    std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler,
    std::unique_ptr<BrowserIOThreadDelegate> browser_io_thread_delegate)
    : ui_thread_executor_(std::make_unique<UIThreadExecutor>(
          std::move(browser_ui_thread_scheduler))),
      io_thread_executor_(std::make_unique<IOThreadExecutor>(
          std::move(browser_io_thread_delegate))) {
  browser_ui_thread_handle_ = ui_thread_executor_->GetUIThreadHandle();
  browser_io_thread_handle_ = io_thread_executor_->GetIOThreadHandle();
  ui_thread_executor_->SetIOThreadHandle(browser_io_thread_handle_);
  io_thread_executor_->SetUIThreadHandle(browser_ui_thread_handle_);
}

BrowserTaskExecutor::~BrowserTaskExecutor() = default;

// static
void BrowserTaskExecutor::Create() {
  DCHECK(!base::SingleThreadTaskRunner::HasCurrentDefault());
  CreateInternal(std::make_unique<BrowserUIThreadScheduler>(),
                 std::make_unique<BrowserIOThreadDelegate>());
}

// static
void BrowserTaskExecutor::CreateForTesting(
    std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler,
    std::unique_ptr<BrowserIOThreadDelegate> browser_io_thread_delegate) {
  CreateInternal(std::move(browser_ui_thread_scheduler),
                 std::move(browser_io_thread_delegate));
}

// static
void BrowserTaskExecutor::CreateInternal(
    std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler,
    std::unique_ptr<BrowserIOThreadDelegate> browser_io_thread_delegate) {
  DCHECK(!g_browser_task_executor);
  g_browser_task_executor =
      new BrowserTaskExecutor(std::move(browser_ui_thread_scheduler),
                              std::move(browser_io_thread_delegate));

  g_browser_task_executor->browser_ui_thread_handle_
      ->EnableAllExceptBestEffortQueues();

#if BUILDFLAG(IS_ANDROID)
  // In Android Java, UI thread is a base/ concept, but needs to know how that
  // maps onto the BrowserThread::UI in C++.
  base::TaskRunnerAndroid::SetUiThreadTaskRunnerCallback(
      base::BindRepeating(&GetTaskRunnerForAndroidMainThread));
  base::PostTaskAndroid::SignalNativeSchedulerReady();
#endif
}

// static
BrowserTaskExecutor* BrowserTaskExecutor::Get() {
  DCHECK(g_browser_task_executor)
      << "No browser task executor created.\nHint: if this is in a unit test, "
         "you're likely missing a content::BrowserTaskEnvironment member in "
         "your fixture.";
  return g_browser_task_executor;
}

// static
void BrowserTaskExecutor::ResetForTesting() {
  if (g_browser_task_executor) {
    RunAllPendingTasksOnThreadForTesting(BrowserThread::UI);
    RunAllPendingTasksOnThreadForTesting(BrowserThread::IO);
    delete g_browser_task_executor;
    g_browser_task_executor = nullptr;
  }
}

// static
void BrowserTaskExecutor::PostFeatureListSetup() {
  DCHECK(Get()->ui_thread_executor_);
  Get()->ui_thread_executor_->PostFeatureListSetup();
}

// static
std::optional<BrowserUIThreadScheduler::UserInputActiveHandle>
BrowserTaskExecutor::OnUserInputStart() {
  DCHECK(Get()->ui_thread_executor_);
  return std::optional<BrowserUIThreadScheduler::UserInputActiveHandle>(
      Get()->ui_thread_executor_->OnUserInputStart());
}

// static
void BrowserTaskExecutor::Shutdown() {
  if (!g_browser_task_executor)
    return;

  DCHECK(Get()->ui_thread_executor_);
  DCHECK(Get()->io_thread_executor_);
  // We don't delete |g_browser_task_executor| because other threads may
  // PostTask or call BrowserTaskExecutor::GetTaskRunner while we're tearing
  // things down. We don't want to add locks so we just leak instead of dealing
  // with that. For similar reasons we don't need to call
  // PostTaskAndroid::SignalNativeSchedulerShutdown on Android. In tests however
  // we need to clean up, so BrowserTaskExecutor::ResetForTesting should be
  // called.
  Get()->ui_thread_executor_.reset();
  Get()->io_thread_executor_.reset();
}

// static
void BrowserTaskExecutor::RunAllPendingTasksOnThreadForTesting(
    BrowserThread::ID identifier) {
  DCHECK(Get());

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  switch (identifier) {
    case BrowserThread::UI:
      Get()->browser_ui_thread_handle_->ScheduleRunAllPendingTasksForTesting(
          run_loop.QuitClosure());
      break;
    case BrowserThread::IO: {
      Get()->browser_io_thread_handle_->ScheduleRunAllPendingTasksForTesting(
          run_loop.QuitClosure());
      break;
    }
    case BrowserThread::ID_COUNT:
      NOTREACHED_IN_MIGRATION();
  }

  run_loop.Run();
}

// static
void BrowserTaskExecutor::OnStartupComplete() {
  Get()->browser_ui_thread_handle_->OnStartupComplete();
  Get()->browser_io_thread_handle_->OnStartupComplete();
}

// static
scoped_refptr<base::SingleThreadTaskRunner>
BrowserTaskExecutor::GetUIThreadTaskRunner(const BrowserTaskTraits& traits) {
  return Get()->GetTaskRunner(BrowserThread::UI, traits);
}

// static
scoped_refptr<base::SingleThreadTaskRunner>
BrowserTaskExecutor::GetIOThreadTaskRunner(const BrowserTaskTraits& traits) {
  return Get()->GetTaskRunner(BrowserThread::IO, traits);
}

// static
void BrowserTaskExecutor::InitializeIOThread() {
  Get()->browser_io_thread_handle_->EnableAllExceptBestEffortQueues();
}

std::unique_ptr<BrowserProcessIOThread> BrowserTaskExecutor::CreateIOThread() {
  DCHECK(Get()->io_thread_executor_);

  std::unique_ptr<BrowserIOThreadDelegate> browser_io_thread_delegate =
      Get()->io_thread_executor_->TakeDelegate();

  DCHECK(browser_io_thread_delegate);
  TRACE_EVENT0("startup", "BrowserTaskExecutor::CreateIOThread");

  auto io_thread = std::make_unique<BrowserProcessIOThread>();

  if (browser_io_thread_delegate->allow_blocking_for_testing()) {
    io_thread->AllowBlockingForTesting();
  }

  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  options.delegate = std::move(browser_io_thread_delegate);
// TODO(crbug.com/40226692): Align Win ThreadType with other platforms. The
// platform discrepancy stems from organic evolution of the thread priorities on
// each platform and while it might make sense not to bump the priority of the
// IO thread per Windows' priority boosts capabilities on MessagePumpForIO, this
// should at least be aligned with what platform_thread_win.cc does for
// ThreadType::kDisplayCritical (IO pumps in other processes) and it currently
// does not.
#if BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(base::kAboveNormalCompositingBrowserWin)) {
    options.thread_type = base::ThreadType::kDisplayCritical;
  }
#else
  // Up the priority of the |io_thread_| as some of its IPCs relate to
  // display tasks.
  options.thread_type = base::ThreadType::kDisplayCritical;
#endif
  if (!io_thread->StartWithOptions(std::move(options)))
    LOG(FATAL) << "Failed to start BrowserThread:IO";
  return io_thread;
}

BrowserTaskExecutor::UIThreadExecutor::UIThreadExecutor(
    std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler)
    : browser_ui_thread_scheduler_(std::move(browser_ui_thread_scheduler)) {
  browser_ui_thread_handle_ = browser_ui_thread_scheduler_->GetHandle();
}

BrowserTaskExecutor::UIThreadExecutor::~UIThreadExecutor() = default;

std::optional<BrowserUIThreadScheduler::UserInputActiveHandle>
BrowserTaskExecutor::UIThreadExecutor::OnUserInputStart() {
  DCHECK(browser_ui_thread_scheduler_);
  return browser_ui_thread_scheduler_->OnUserInputStart();
}

void BrowserTaskExecutor::UIThreadExecutor::PostFeatureListSetup() {
  DCHECK(browser_ui_thread_scheduler_);
  browser_ui_thread_scheduler_->PostFeatureListSetup();
}

scoped_refptr<BrowserUIThreadScheduler::Handle>
BrowserTaskExecutor::UIThreadExecutor::GetUIThreadHandle() {
  return browser_ui_thread_handle_;
}

void BrowserTaskExecutor::UIThreadExecutor::SetIOThreadHandle(
    scoped_refptr<BrowserUIThreadScheduler::Handle> io_thread_handle) {
  browser_io_thread_handle_ = std::move(io_thread_handle);
}

BrowserTaskExecutor::IOThreadExecutor::IOThreadExecutor(
    std::unique_ptr<BrowserIOThreadDelegate> browser_io_thread_delegate)
    : browser_io_thread_delegate_(std::move(browser_io_thread_delegate)) {
  // |browser_io_thread_delegate_| can be null in tests.
  if (!browser_io_thread_delegate_)
    return;
  browser_io_thread_handle_ = browser_io_thread_delegate_->GetHandle();
}

BrowserTaskExecutor::IOThreadExecutor::~IOThreadExecutor() = default;

scoped_refptr<BrowserUIThreadScheduler::Handle>
BrowserTaskExecutor::IOThreadExecutor::GetIOThreadHandle() {
  return browser_io_thread_handle_;
}

void BrowserTaskExecutor::IOThreadExecutor::SetUIThreadHandle(
    scoped_refptr<BrowserUIThreadScheduler::Handle> ui_thread_handle) {
  browser_ui_thread_handle_ = std::move(ui_thread_handle);
}

}  // namespace content
