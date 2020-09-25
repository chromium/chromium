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
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/task_observer.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
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

void DeferredQuitRunLoop(base::OnceClosure quit_task, int num_quit_deferrals) {
  if (num_quit_deferrals <= 0) {
    std::move(quit_task).Run();
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&DeferredQuitRunLoop, std::move(quit_task),
                                  num_quit_deferrals - 1));
  }
}

// Monitors if any task is processed by the message loop.
class TaskObserver : public base::TaskObserver {
 public:
  TaskObserver() : processed_(false) {}
  ~TaskObserver() override {}

  // TaskObserver overrides.
  void WillProcessTask(const base::PendingTask& pending_task,
                       bool was_blocked_or_low_priority) override {}
  void DidProcessTask(const base::PendingTask& pending_task) override {
    if (base::EndsWith(pending_task.posted_from.file_name(), "base/run_loop.cc",
                       base::CompareCase::SENSITIVE)) {
      // Don't consider RunLoop internal tasks (i.e. QuitClosure() reposted by
      // ProxyToTaskRunner() or RunLoop timeouts) as actual work.
      return;
    }
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
    WindowedNotificationObserver::ConditionTestCallbackWithoutSourceAndDetails
        callback,
    const NotificationSource& source,
    const NotificationDetails& details) {
  return std::move(callback).Run();
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
  base::CurrentThread::ScopedNestableTaskAllower allow;
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
    base::CurrentThread::Get()->AddTaskObserver(&task_observer);

    // This must use RunLoop::Type::kNestableTasksAllowed in case this
    // RunAllTasksUntilIdle() call is nested inside an existing Run(). Without
    // it, the QuitWhenIdleClosure() below would never run if it's posted from
    // another thread (i.e.. by run_loop.cc's ProxyToTaskRunner).
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);

    base::ThreadPoolInstance::Get()->FlushAsyncForTesting(
        run_loop.QuitWhenIdleClosure());

    run_loop.Run();

    base::CurrentThread::Get()->RemoveTaskObserver(&task_observer);

    if (!task_observer.processed())
      break;
  }
}

base::OnceClosure GetDeferredQuitTaskForRunLoop(base::RunLoop* run_loop) {
  return base::BindOnce(&DeferredQuitRunLoop, run_loop->QuitClosure(),
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

bool CanSameSiteMainFrameNavigationsChangeRenderFrameHosts() {
  // TODO(crbug.com/936696): Also return true when RenderDocument for main frame
  // is enabled.
  return CanSameSiteMainFrameNavigationsChangeSiteInstances();
}

bool CanSameSiteMainFrameNavigationsChangeSiteInstances() {
  return IsProactivelySwapBrowsingInstanceOnSameSiteNavigationEnabled() ||
         IsSameSiteBackForwardCacheEnabled();
}

void DisableProactiveBrowsingInstanceSwapFor(RenderFrameHost* rfh) {
  if (!CanSameSiteMainFrameNavigationsChangeSiteInstances())
    return;
  // If the RFH is not a main frame, navigations on it will never result in a
  // proactive BrowsingInstance swap, so we shouldn't really call it on main
  // frames.
  DCHECK(!rfh->GetParent());
  static_cast<RenderFrameHostImpl*>(rfh)
      ->DisableProactiveBrowsingInstanceSwapForTesting();
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

void AwaitDocumentOnLoadCompleted(WebContents* web_contents) {
  class Awaiter : public WebContentsObserver {
   public:
    explicit Awaiter(content::WebContents* web_contents)
        : content::WebContentsObserver(web_contents),
          observed_(web_contents->IsDocumentOnLoadCompletedInMainFrame()) {}

    Awaiter(const Awaiter&) = delete;
    Awaiter& operator=(const Awaiter&) = delete;

    ~Awaiter() override = default;

    void Await() {
      if (!observed_)
        run_loop_.Run();
      DCHECK(web_contents()->IsDocumentOnLoadCompletedInMainFrame());
    }

    // WebContentsObserver:
    void DocumentOnLoadCompletedInMainFrame() override {
      observed_ = true;
      if (run_loop_.running())
        run_loop_.Quit();
    }

   private:
    bool observed_ = false;
    base::RunLoop run_loop_;
  };

  Awaiter(web_contents).Await();
}

MessageLoopRunner::MessageLoopRunner(QuitMode quit_mode)
    : quit_mode_(quit_mode) {}

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

base::OnceClosure MessageLoopRunner::QuitClosure() {
  return base::BindOnce(&MessageLoopRunner::Quit, this);
}

void MessageLoopRunner::Quit() {
  DCHECK(thread_checker_.CalledOnValidThread());

  quit_closure_called_ = true;

  // Only run the quit task if we are running the message loop.
  if (loop_running_) {
    switch (quit_mode_) {
      case QuitMode::DEFERRED:
        DeferredQuitRunLoop(run_loop_.QuitClosure(), kNumQuitDeferrals);
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
    ConditionTestCallback callback)
    : callback_(std::move(callback)),
      source_(NotificationService::AllSources()) {
  AddNotificationType(notification_type, source_);
}

WindowedNotificationObserver::WindowedNotificationObserver(
    int notification_type,
    ConditionTestCallbackWithoutSourceAndDetails callback)
    : callback_(
          base::BindRepeating(&IgnoreSourceAndDetails, std::move(callback))),
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

InProcessUtilityThreadHelper::InProcessUtilityThreadHelper() {
  RenderProcessHost::SetRunRendererInProcess(true);
}

InProcessUtilityThreadHelper::~InProcessUtilityThreadHelper() {
  JoinAllUtilityThreads();
  RenderProcessHost::SetRunRendererInProcess(false);
}

void InProcessUtilityThreadHelper::JoinAllUtilityThreads() {
  ASSERT_FALSE(run_loop_);
  run_loop_.emplace();
  BrowserChildProcessObserver::Add(this);
  CheckHasRunningChildProcess();
  run_loop_->Run();
  run_loop_.reset();
  BrowserChildProcessObserver::Remove(this);
}

void InProcessUtilityThreadHelper::CheckHasRunningChildProcess() {
  ASSERT_TRUE(run_loop_);

  auto check_has_running_child_process_on_io =
      [](base::OnceClosure quit_closure) {
        BrowserChildProcessHostIterator it;
        // If not Done(), we have some running child processes and need to wait.
        if (it.Done())
          std::move(quit_closure).Run();
      };

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(check_has_running_child_process_on_io,
                                run_loop_->QuitClosure()));
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
  destroyed_ = true;
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
    bool requires_dedicated_process)
    : requires_dedicated_process_(requires_dedicated_process) {}

EffectiveURLContentBrowserClient::EffectiveURLContentBrowserClient(
    const GURL& url_to_modify,
    const GURL& url_to_return,
    bool requires_dedicated_process)
    : requires_dedicated_process_(requires_dedicated_process) {
  AddTranslation(url_to_modify, url_to_return);
}

EffectiveURLContentBrowserClient::~EffectiveURLContentBrowserClient() {}

void EffectiveURLContentBrowserClient::AddTranslation(
    const GURL& url_to_modify,
    const GURL& url_to_return) {
  urls_to_modify_[url_to_modify] = url_to_return;
}

GURL EffectiveURLContentBrowserClient::GetEffectiveURL(
    BrowserContext* browser_context,
    const GURL& url) {
  auto it = urls_to_modify_.find(url);
  if (it != urls_to_modify_.end())
    return it->second;
  return url;
}

bool EffectiveURLContentBrowserClient::DoesSiteRequireDedicatedProcess(
    BrowserContext* browser_context,
    const GURL& effective_site_url) {
  if (!requires_dedicated_process_)
    return false;

  for (const auto& pair : urls_to_modify_) {
    if (SiteInstance::GetSiteForURL(browser_context, pair.first) ==
        effective_site_url)
      return true;
  }
  return false;
}

}  // namespace content
