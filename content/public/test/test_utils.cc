// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_utils.h"

#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/variations/variations_params_manager.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/common/url_schemes.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_plugin_guest_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_service_manager_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace content {

namespace {

// Number of times to repost a Quit task so that the MessageLoop finishes up
// pending tasks and tasks posted by those pending tasks without risking the
// potential hang behavior of MessageLoop::QuitWhenIdle.
// The criteria for choosing this number: it should be high enough to make the
// quit act like QuitWhenIdle, while taking into account that any page which is
// animating may be rendering another frame for each quit deferral. For an
// animating page, the potential delay to quitting the RunLoop would be
// kNumQuitDeferrals * frame_render_time. Some perf tests run slow, such as
// 200ms/frame.
constexpr int kNumQuitDeferrals = 10;

void DeferredQuitRunLoop(const base::Closure& quit_task,
                         int num_quit_deferrals) {
  if (num_quit_deferrals <= 0) {
    quit_task.Run();
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&DeferredQuitRunLoop, quit_task,
                                  num_quit_deferrals - 1));
  }
}

// Class used to handle result callbacks for ExecuteScriptAndGetValue.
class ScriptCallback {
 public:
  ScriptCallback() { }
  virtual ~ScriptCallback() { }
  void ResultCallback(const base::Value* result);

  std::unique_ptr<base::Value> result() { return std::move(result_); }

 private:
  std::unique_ptr<base::Value> result_;

  DISALLOW_COPY_AND_ASSIGN(ScriptCallback);
};

void ScriptCallback::ResultCallback(const base::Value* result) {
  if (result)
    result_.reset(result->DeepCopy());
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

// Monitors if any task is processed by the message loop.
class TaskObserver : public base::MessageLoop::TaskObserver {
 public:
  TaskObserver() : processed_(false) {}
  ~TaskObserver() override {}

  // MessageLoop::TaskObserver overrides.
  void WillProcessTask(const base::PendingTask& pending_task) override {}
  void DidProcessTask(const base::PendingTask& pending_task) override {
    processed_ = true;
  }

  // Returns true if any task was processed.
  bool processed() const { return processed_; }

 private:
  bool processed_;
  DISALLOW_COPY_AND_ASSIGN(TaskObserver);
};

// Adapter that makes a WindowedNotificationObserver::ConditionTestCallback from
// a WindowedNotificationObserver::ConditionTestCallbackWithoutSourceAndDetails
// by ignoring the notification source and details.
bool IgnoreSourceAndDetails(
    const WindowedNotificationObserver::
        ConditionTestCallbackWithoutSourceAndDetails& callback,
    const NotificationSource& source,
    const NotificationDetails& details) {
  return callback.Run();
}

}  // namespace

void RunMessageLoop() {
  base::RunLoop run_loop;
  RunThisRunLoop(&run_loop);
}

void RunThisRunLoop(base::RunLoop* run_loop) {
  base::MessageLoopCurrent::ScopedNestableTaskAllower allow;
  run_loop->Run();
}

void RunAllPendingInMessageLoop() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, GetDeferredQuitTaskForRunLoop(&run_loop));
  RunThisRunLoop(&run_loop);
}

void RunAllPendingInMessageLoop(BrowserThread::ID thread_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (thread_id == BrowserThread::UI) {
    RunAllPendingInMessageLoop();
    return;
  }

  // Post a DeferredQuitRunLoop() task to |thread_id|. Then, run a RunLoop on
  // this thread. When a few generations of pending tasks have run on
  // |thread_id|, a task will be posted to this thread to exit the RunLoop.
  base::RunLoop run_loop;
  const base::Closure post_quit_run_loop_to_ui_thread = base::Bind(
      base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
      base::ThreadTaskRunnerHandle::Get(), FROM_HERE, run_loop.QuitClosure());
  base::PostTaskWithTraits(
      FROM_HERE, {thread_id},
      base::BindOnce(&DeferredQuitRunLoop, post_quit_run_loop_to_ui_thread,
                     kNumQuitDeferrals));
  RunThisRunLoop(&run_loop);
}

void RunAllTasksUntilIdle() {
  while (true) {
    // Setup a task observer to determine if MessageLoop tasks run in the
    // current loop iteration and loop in case the MessageLoop posts tasks to
    // the Task Scheduler after the initial flush.
    TaskObserver task_observer;
    base::MessageLoopCurrent::Get()->AddTaskObserver(&task_observer);

    base::RunLoop run_loop;
    base::TaskScheduler::GetInstance()->FlushAsyncForTesting(
        run_loop.QuitWhenIdleClosure());
    run_loop.Run();

    base::MessageLoopCurrent::Get()->RemoveTaskObserver(&task_observer);

    if (!task_observer.processed())
      break;
  }
}

base::Closure GetDeferredQuitTaskForRunLoop(base::RunLoop* run_loop) {
  return base::Bind(&DeferredQuitRunLoop, run_loop->QuitClosure(),
                    kNumQuitDeferrals);
}

std::unique_ptr<base::Value> ExecuteScriptAndGetValue(
    RenderFrameHost* render_frame_host,
    const std::string& script) {
  ScriptCallback observer;

  render_frame_host->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(script),
      base::Bind(&ScriptCallback::ResultCallback, base::Unretained(&observer)));
  base::RunLoop().Run();
  return observer.result();
}

bool AreAllSitesIsolatedForTesting() {
  return SiteIsolationPolicy::UseDedicatedProcessesForAllSites();
}

void IsolateAllSitesForTesting(base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kSitePerProcess);
}

void ResetSchemesAndOriginsWhitelist() {
  url::Shutdown();
  RegisterContentSchemes(false);
  url::Initialize();
}

void DeprecatedEnableFeatureWithParam(const base::Feature& feature,
                                      const std::string& param_name,
                                      const std::string& param_value,
                                      base::CommandLine* command_line) {
  static const char kFakeTrialName[] = "TrialNameForTesting";
  static const char kFakeTrialGroupName[] = "TrialGroupForTesting";

  // Enable all the |feature|, associating them with |trial_name|.
  command_line->AppendSwitchASCII(
      switches::kEnableFeatures,
      std::string(feature.name) + "<" + kFakeTrialName);

  std::map<std::string, std::string> param_values = {{param_name, param_value}};
  variations::testing::VariationParamsManager::AppendVariationParams(
      kFakeTrialName, kFakeTrialGroupName, param_values, command_line);
}

namespace {

// Helper class for CreateAndAttachInnerContents.
//
// TODO(lfg): https://crbug.com/821187 Inner webcontentses currently require
// supplying a BrowserPluginGuestDelegate; however, the oopif architecture
// doesn't really require it. Refactor this so that we can create an inner
// contents without any of the guest machinery.
class InnerWebContentsHelper : public WebContentsObserver {
 public:
  explicit InnerWebContentsHelper() : WebContentsObserver() {}
  ~InnerWebContentsHelper() override = default;

  // WebContentsObserver:
  void WebContentsDestroyed() override { delete this; }

  void SetInnerWebContents(WebContents* inner_web_contents) {
    Observe(inner_web_contents);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InnerWebContentsHelper);
};

}  // namespace

WebContents* CreateAndAttachInnerContents(RenderFrameHost* rfh) {
  WebContents* outer_contents =
      static_cast<RenderFrameHostImpl*>(rfh)->delegate()->GetAsWebContents();
  if (!outer_contents)
    return nullptr;

  auto guest_delegate = std::make_unique<InnerWebContentsHelper>();

  WebContents::CreateParams inner_params(outer_contents->GetBrowserContext());

  std::unique_ptr<WebContents> inner_contents_ptr =
      WebContents::Create(inner_params);

  // Attach. |inner_contents| becomes owned by |outer_contents|.
  WebContents* inner_contents = inner_contents_ptr.get();
  inner_contents->AttachToOuterWebContentsFrame(std::move(inner_contents_ptr),
                                                rfh);

  // |guest_delegate| becomes owned by |inner_contents|.
  guest_delegate.release()->SetInnerWebContents(inner_contents);

  return inner_contents;
}

MessageLoopRunner::MessageLoopRunner(QuitMode quit_mode)
    : quit_mode_(quit_mode), loop_running_(false), quit_closure_called_(false) {
}

MessageLoopRunner::~MessageLoopRunner() = default;

void MessageLoopRunner::Run() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Do not run the message loop if our quit closure has already been called.
  // This helps in scenarios where the closure has a chance to run before
  // we Run explicitly.
  if (quit_closure_called_)
    return;

  loop_running_ = true;
  RunThisRunLoop(&run_loop_);
}

base::Closure MessageLoopRunner::QuitClosure() {
  return base::Bind(&MessageLoopRunner::Quit, this);
}

void MessageLoopRunner::Quit() {
  DCHECK(thread_checker_.CalledOnValidThread());

  quit_closure_called_ = true;

  // Only run the quit task if we are running the message loop.
  if (loop_running_) {
    switch (quit_mode_) {
      case QuitMode::DEFERRED:
        GetDeferredQuitTaskForRunLoop(&run_loop_).Run();
        break;
      case QuitMode::IMMEDIATE:
        run_loop_.Quit();
        break;
    }
    loop_running_ = false;
  }
}

WindowedNotificationObserver::WindowedNotificationObserver(
    int notification_type,
    const NotificationSource& source)
    : source_(NotificationService::AllSources()) {
  AddNotificationType(notification_type, source);
}

WindowedNotificationObserver::WindowedNotificationObserver(
    int notification_type,
    const ConditionTestCallback& callback)
    : callback_(callback), source_(NotificationService::AllSources()) {
  AddNotificationType(notification_type, source_);
}

WindowedNotificationObserver::WindowedNotificationObserver(
    int notification_type,
    const ConditionTestCallbackWithoutSourceAndDetails& callback)
    : callback_(base::Bind(&IgnoreSourceAndDetails, callback)),
      source_(NotificationService::AllSources()) {
  registrar_.Add(this, notification_type, source_);
}

WindowedNotificationObserver::~WindowedNotificationObserver() = default;

void WindowedNotificationObserver::AddNotificationType(
    int notification_type,
    const NotificationSource& source) {
  registrar_.Add(this, notification_type, source);
}

void WindowedNotificationObserver::Wait() {
  if (!seen_)
    run_loop_.Run();
  EXPECT_TRUE(seen_);
}

void WindowedNotificationObserver::Observe(int type,
                                           const NotificationSource& source,
                                           const NotificationDetails& details) {
  source_ = source;
  details_ = details;
  if (!callback_.is_null() && !callback_.Run(source, details))
    return;

  seen_ = true;
  run_loop_.Quit();
}

InProcessUtilityThreadHelper::InProcessUtilityThreadHelper()
    : child_thread_count_(0), shell_context_(new TestServiceManagerContext) {
  RenderProcessHost::SetRunRendererInProcess(true);
  BrowserChildProcessObserver::Add(this);
}

InProcessUtilityThreadHelper::~InProcessUtilityThreadHelper() {
  if (child_thread_count_) {
    DCHECK(BrowserThread::IsThreadInitialized(BrowserThread::UI));
    DCHECK(BrowserThread::IsThreadInitialized(BrowserThread::IO));
    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
  }
  BrowserChildProcessObserver::Remove(this);
  RenderProcessHost::SetRunRendererInProcess(false);
}

void InProcessUtilityThreadHelper::BrowserChildProcessHostConnected(
    const ChildProcessData& data) {
  child_thread_count_++;
}

void InProcessUtilityThreadHelper::BrowserChildProcessHostDisconnected(
    const ChildProcessData& data) {
  if (--child_thread_count_)
    return;

  if (run_loop_)
    run_loop_->Quit();
}

RenderFrameDeletedObserver::RenderFrameDeletedObserver(RenderFrameHost* rfh)
    : WebContentsObserver(WebContents::FromRenderFrameHost(rfh)),
      process_id_(rfh->GetProcess()->GetID()),
      routing_id_(rfh->GetRoutingID()),
      deleted_(false) {}

RenderFrameDeletedObserver::~RenderFrameDeletedObserver() {}

void RenderFrameDeletedObserver::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  if (render_frame_host->GetProcess()->GetID() == process_id_ &&
      render_frame_host->GetRoutingID() == routing_id_) {
    deleted_ = true;

    if (runner_.get())
      runner_->Quit();
  }
}

bool RenderFrameDeletedObserver::deleted() {
  return deleted_;
}

void RenderFrameDeletedObserver::WaitUntilDeleted() {
  if (deleted_)
    return;

  runner_.reset(new base::RunLoop());
  runner_->Run();
  runner_.reset();
}

WebContentsDestroyedWatcher::WebContentsDestroyedWatcher(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  EXPECT_TRUE(web_contents != nullptr);
}

WebContentsDestroyedWatcher::~WebContentsDestroyedWatcher() {
}

void WebContentsDestroyedWatcher::Wait() {
  run_loop_.Run();
}

void WebContentsDestroyedWatcher::WebContentsDestroyed() {
  run_loop_.Quit();
}

TestPageScaleObserver::TestPageScaleObserver(WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

TestPageScaleObserver::~TestPageScaleObserver() {}

void TestPageScaleObserver::OnPageScaleFactorChanged(float page_scale_factor) {
  last_scale_ = page_scale_factor;
  seen_page_scale_change_ = true;
  if (done_callback_)
    std::move(done_callback_).Run();
}

float TestPageScaleObserver::WaitForPageScaleUpdate() {
  if (!seen_page_scale_change_) {
    base::RunLoop run_loop;
    done_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  seen_page_scale_change_ = false;
  return last_scale_;
}

GURL EffectiveURLContentBrowserClient::GetEffectiveURL(
    BrowserContext* browser_context,
    const GURL& url) {
  if (url == url_to_modify_)
    return url_to_return_;
  return url;
}

}  // namespace content
