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
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/task_observer.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/common/url_schemes.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_plugin_guest_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_service_manager_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/fetch/fetch_api_request_headers_map.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
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

// Monitors if any task is processed by the message loop.
class TaskObserver : public base::TaskObserver {
 public:
  TaskObserver() : processed_(false) {}
  ~TaskObserver() override {}

  // TaskObserver overrides.
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

blink::mojom::FetchAPIRequestPtr CreateFetchAPIRequest(
    const GURL& url,
    const std::string& method,
    const blink::FetchAPIRequestHeadersMap& headers,
    blink::mojom::ReferrerPtr referrer,
    bool is_reload) {
  auto request = blink::mojom::FetchAPIRequest::New();
  request->url = url;
  request->method = method;
  request->headers = {headers.begin(), headers.end()};
  request->referrer = std::move(referrer);
  request->is_reload = is_reload;
  return request;
}

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
  RunAllPendingInMessageLoop(BrowserThread::UI);
}

void RunAllPendingInMessageLoop(BrowserThread::ID thread_id) {
  // See comment for |kNumQuitDeferrals| for why this is needed.
  for (int i = 0; i <= kNumQuitDeferrals; ++i) {
    BrowserThread::RunAllPendingTasksOnThreadForTesting(thread_id);
  }
}

void RunAllTasksUntilIdle() {
  while (true) {
    // Setup a task observer to determine if MessageLoop tasks run in the
    // current loop iteration and loop in case the MessageLoop posts tasks to
    // the Task Scheduler after the initial flush.
    TaskObserver task_observer;
    base::MessageLoopCurrent::Get()->AddTaskObserver(&task_observer);

    base::RunLoop run_loop;
    base::ThreadPoolInstance::Get()->FlushAsyncForTesting(
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

base::Value ExecuteScriptAndGetValue(RenderFrameHost* render_frame_host,
                                     const std::string& script) {
  base::RunLoop run_loop;
  base::Value result;

  render_frame_host->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(script),
      base::BindOnce(
          [](base::OnceClosure quit_closure, base::Value* out_result,
             base::Value value) {
            *out_result = std::move(value);
            std::move(quit_closure).Run();
          },
          run_loop.QuitWhenIdleClosure(), &result));
  run_loop.Run();

  return result;
}

bool AreAllSitesIsolatedForTesting() {
  return SiteIsolationPolicy::UseDedicatedProcessesForAllSites();
}

bool AreDefaultSiteInstancesEnabled() {
  return !AreAllSitesIsolatedForTesting() &&
         base::FeatureList::IsEnabled(
             features::kProcessSharingWithDefaultSiteInstances);
}

void IsolateAllSitesForTesting(base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kSitePerProcess);
}

void ResetSchemesAndOriginsWhitelist() {
  url::ResetForTests();
  RegisterContentSchemes(false);
}

GURL GetWebUIURL(const std::string& host) {
  return GURL(GetWebUIURLString(host));
}

std::string GetWebUIURLString(const std::string& host) {
  return std::string(content::kChromeUIScheme) + url::kStandardSchemeSeparator +
         host;
}

WebContents* CreateAndAttachInnerContents(RenderFrameHost* rfh) {
  WebContents* outer_contents =
      static_cast<RenderFrameHostImpl*>(rfh)->delegate()->GetAsWebContents();
  if (!outer_contents)
    return nullptr;

  WebContents::CreateParams inner_params(outer_contents->GetBrowserContext());

  std::unique_ptr<WebContents> inner_contents_ptr =
      WebContents::Create(inner_params);

  // Attach. |inner_contents| becomes owned by |outer_contents|.
  WebContents* inner_contents = inner_contents_ptr.get();
  outer_contents->AttachInnerWebContents(std::move(inner_contents_ptr), rfh,
                                         false /* is_full_page */);

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
    : shell_context_(new TestServiceManagerContext) {
  RenderProcessHost::SetRunRendererInProcess(true);
}

InProcessUtilityThreadHelper::~InProcessUtilityThreadHelper() {
  JoinAllUtilityThreads();
  RenderProcessHost::SetRunRendererInProcess(false);
}

void InProcessUtilityThreadHelper::JoinAllUtilityThreads() {
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  BrowserChildProcessObserver::Add(this);
  CheckHasRunningChildProcess();
  run_loop.Run();
  BrowserChildProcessObserver::Remove(this);
}

void InProcessUtilityThreadHelper::CheckHasRunningChildProcess() {
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &InProcessUtilityThreadHelper::CheckHasRunningChildProcessOnIO,
          quit_closure_));
}

// static
void InProcessUtilityThreadHelper::CheckHasRunningChildProcessOnIO(
    const base::RepeatingClosure& quit_closure) {
  BrowserChildProcessHostIterator it;
  if (!it.Done()) {
    // Have some running child processes -> need to wait.
    return;
  }

  DCHECK(quit_closure);
  quit_closure.Run();
}

void InProcessUtilityThreadHelper::BrowserChildProcessHostDisconnected(
    const ChildProcessData& data) {
  CheckHasRunningChildProcess();
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

EffectiveURLContentBrowserClient::EffectiveURLContentBrowserClient(
    const GURL& url_to_modify,
    const GURL& url_to_return,
    bool requires_dedicated_process)
    : url_to_modify_(url_to_modify),
      url_to_return_(url_to_return),
      requires_dedicated_process_(requires_dedicated_process) {}

EffectiveURLContentBrowserClient::~EffectiveURLContentBrowserClient() {}

GURL EffectiveURLContentBrowserClient::GetEffectiveURL(
    BrowserContext* browser_context,
    const GURL& url) {
  if (url == url_to_modify_)
    return url_to_return_;
  return url;
}

bool EffectiveURLContentBrowserClient::DoesSiteRequireDedicatedProcess(
    BrowserContext* browser_context,
    const GURL& effective_site_url) {
  GURL expected_effective_site_url =
      SiteInstance::GetSiteForURL(browser_context, url_to_modify_);

  return requires_dedicated_process_ &&
         expected_effective_site_url == effective_site_url;
}

}  // namespace content
