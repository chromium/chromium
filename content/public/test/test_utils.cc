// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_utils.h"

#include <memory>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_observer.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/font_access/font_enumeration_cache.h"
#include "content/browser/origin_agent_cluster_isolation_state.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/site_info.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/features.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&DeferredQuitRunLoop, std::move(quit_task),
                                  num_quit_deferrals - 1));
  }
}

// Monitors if any task is processed by the message loop.
class TaskObserver : public base::TaskObserver {
 public:
  TaskObserver() = default;

  TaskObserver(const TaskObserver&) = delete;
  TaskObserver& operator=(const TaskObserver&) = delete;

  ~TaskObserver() override = default;

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
  bool processed_ = false;
};

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
  request->headers = headers;
  request->referrer = std::move(referrer);
  request->is_reload = is_reload;
  return request;
}

void RunMessageLoop() {
  base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).Run();
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

bool AreAllSitesIsolatedForTesting() {
  return SiteIsolationPolicy::UseDedicatedProcessesForAllSites();
}

bool IsOriginAgentClusterEnabledForOrigin(SiteInstance* site_instance,
                                          const url::Origin& origin) {
  OriginAgentClusterIsolationState origin_requests_isolation(
      OriginAgentClusterIsolationState::CreateNonIsolated());

  return static_cast<ChildProcessSecurityPolicyImpl*>(
             ChildProcessSecurityPolicy::GetInstance())
      ->DetermineOriginAgentClusterIsolation(
          static_cast<SiteInstanceImpl*>(site_instance)->GetIsolationContext(),
          origin, origin_requests_isolation)
      .is_origin_agent_cluster();
}

bool AreDefaultSiteInstancesEnabled() {
  return !AreAllSitesIsolatedForTesting() &&
         base::FeatureList::IsEnabled(
             features::kProcessSharingWithDefaultSiteInstances);
}

bool AreStrictSiteInstancesEnabled() {
  return AreAllSitesIsolatedForTesting() ||
         base::FeatureList::IsEnabled(
             features::kProcessSharingWithStrictSiteInstances);
}

bool IsIsolatedOriginRequiredToGuaranteeDedicatedProcess() {
  return AreDefaultSiteInstancesEnabled() ||
         base::FeatureList::IsEnabled(
             features::kProcessSharingWithStrictSiteInstances);
}

void IsolateAllSitesForTesting(base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kSitePerProcess);
}

bool CanSameSiteMainFrameNavigationsChangeRenderFrameHosts() {
  return ShouldCreateNewRenderFrameHostOnSameSiteNavigation(
             /*is_main_frame=*/true, /*is_local_root=*/true) ||
         CanSameSiteMainFrameNavigationsChangeSiteInstances();
}

bool WillSameSiteNavigationChangeRenderFrameHosts(bool is_main_frame,
                                                  bool is_local_root) {
  return ShouldCreateNewRenderFrameHostOnSameSiteNavigation(is_main_frame,
                                                            is_local_root);
}

bool CanSameSiteMainFrameNavigationsChangeSiteInstances() {
  return IsBackForwardCacheEnabled();
}

bool IsNavigationQueueingEnabled() {
  return ShouldQueueNavigationsWhenPendingCommitRFHExists();
}

void DisableProactiveBrowsingInstanceSwapFor(RenderFrameHost* rfh) {
  if (!CanSameSiteMainFrameNavigationsChangeSiteInstances())
    return;
  // If the RFH is not a primary main frame, navigations on it will never result
  // in a proactive BrowsingInstance swap, so we shouldn't call this function on
  // subframes.
  DCHECK(rfh->IsInPrimaryMainFrame());
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
  auto* outer_contents = WebContents::FromRenderFrameHost(rfh);
  if (!outer_contents)
    return nullptr;

  WebContents::CreateParams inner_params(outer_contents->GetBrowserContext());

  std::unique_ptr<WebContents> inner_contents_ptr =
      WebContents::Create(inner_params);

  // Attach. |inner_contents| becomes owned by |outer_contents|.
  WebContents* inner_contents = inner_contents_ptr.get();
  outer_contents->AttachInnerWebContents(std::move(inner_contents_ptr), rfh,
                                         /*is_full_page=*/false);

  return inner_contents;
}

void AwaitDocumentOnLoadCompleted(WebContents* web_contents) {
  class Awaiter : public WebContentsObserver {
   public:
    explicit Awaiter(content::WebContents* web_contents)
        : content::WebContentsObserver(web_contents),
          observed_(
              web_contents->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {}

    Awaiter(const Awaiter&) = delete;
    Awaiter& operator=(const Awaiter&) = delete;

    ~Awaiter() override = default;

    void Await() {
      if (!observed_)
        run_loop_.Run();
      DCHECK(web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame());
    }

    // WebContentsObserver:
    void DocumentOnLoadCompletedInPrimaryMainFrame() override {
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

void FocusWebContentsOnFrame(WebContents* web_contents, RenderFrameHost* rfh) {
  WebContentsImpl* contents = static_cast<WebContentsImpl*>(web_contents);
  FrameTreeNode* node =
      contents->GetPrimaryFrameTree().FindByID(rfh->GetFrameTreeNodeId());
  CHECK(node);
  CHECK_EQ(node->current_frame_host(), rfh);
  contents->GetPrimaryFrameTree().SetFocusedFrame(
      node, node->current_frame_host()->GetSiteInstance()->group());
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
  run_loop_.Run();
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

LoadStopObserver::LoadStopObserver(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      run_loop_(base::RunLoop::Type::kNestableTasksAllowed) {}

void LoadStopObserver::Wait() {
  if (!seen_)
    run_loop_.Run();

  EXPECT_TRUE(seen_);
}

void LoadStopObserver::DidStopLoading() {
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

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(check_has_running_child_process_on_io,
                                run_loop_->QuitClosure()));
}

void InProcessUtilityThreadHelper::BrowserChildProcessHostDisconnected(
    const ChildProcessData& data) {
  CheckHasRunningChildProcess();
}

RenderFrameDeletedObserver::RenderFrameDeletedObserver(RenderFrameHost* rfh)
    : WebContentsObserver(WebContents::FromRenderFrameHost(rfh)),
      rfh_id_(rfh->GetGlobalId()) {
  DCHECK(rfh);
}

RenderFrameDeletedObserver::~RenderFrameDeletedObserver() = default;

void RenderFrameDeletedObserver::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  if (render_frame_host->GetGlobalId() == rfh_id_) {
    rfh_id_ = GlobalRenderFrameHostId();

    if (runner_.get())
      runner_->Quit();
  }
}

bool RenderFrameDeletedObserver::deleted() const {
  return rfh_id_ == GlobalRenderFrameHostId();
}

bool RenderFrameDeletedObserver::WaitUntilDeleted() {
  if (deleted())
    return true;

  runner_ = std::make_unique<base::RunLoop>();
  runner_->Run();
  runner_.reset();
  return deleted();
}

RenderFrameHostWrapper::RenderFrameHostWrapper(RenderFrameHost* rfh)
    : rfh_id_(rfh ? rfh->GetGlobalId() : GlobalRenderFrameHostId()),
      deleted_observer_(rfh ? std::make_unique<RenderFrameDeletedObserver>(rfh)
                            : nullptr) {}

RenderFrameHostWrapper::RenderFrameHostWrapper(RenderFrameHostWrapper&& rfhft) =
    default;
RenderFrameHostWrapper::~RenderFrameHostWrapper() = default;

RenderFrameHost* RenderFrameHostWrapper::get() const {
  return RenderFrameHost::FromID(rfh_id_);
}

bool RenderFrameHostWrapper::IsDestroyed() const {
  return get() == nullptr;
}

// See RenderFrameDeletedObserver for notes on the difference between
// RenderFrame being deleted and RenderFrameHost being destroyed.
bool RenderFrameHostWrapper::WaitUntilRenderFrameDeleted() const {
  CHECK(deleted_observer_);
  return deleted_observer_->WaitUntilDeleted();
}

bool RenderFrameHostWrapper::IsRenderFrameDeleted() const {
  CHECK(deleted_observer_);
  return deleted_observer_->deleted();
}

RenderFrameHost& RenderFrameHostWrapper::operator*() const {
  DCHECK(get());
  return *get();
}

RenderFrameHost* RenderFrameHostWrapper::operator->() const {
  DCHECK(get());
  return get();
}

WebContentsDestroyedWatcher::WebContentsDestroyedWatcher(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  EXPECT_TRUE(web_contents != nullptr);
}

WebContentsDestroyedWatcher::~WebContentsDestroyedWatcher() = default;

void WebContentsDestroyedWatcher::Wait() {
  run_loop_.Run();
}

void WebContentsDestroyedWatcher::WebContentsDestroyed() {
  destroyed_ = true;
  run_loop_.Quit();
}

TestPageScaleObserver::TestPageScaleObserver(WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

TestPageScaleObserver::~TestPageScaleObserver() = default;

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

EffectiveURLContentBrowserClientHelper::EffectiveURLContentBrowserClientHelper(
    bool requires_dedicated_process)
    : requires_dedicated_process_(requires_dedicated_process) {}

EffectiveURLContentBrowserClientHelper::
    ~EffectiveURLContentBrowserClientHelper() = default;

void EffectiveURLContentBrowserClientHelper::AddTranslation(
    const GURL& url_to_modify,
    const GURL& url_to_return) {
  urls_to_modify_[url_to_modify] = url_to_return;
}

GURL EffectiveURLContentBrowserClientHelper::GetEffectiveURL(const GURL& url) {
  auto it = urls_to_modify_.find(url);
  if (it != urls_to_modify_.end()) {
    return it->second;
  }
  return url;
}

bool EffectiveURLContentBrowserClientHelper::DoesSiteRequireDedicatedProcess(
    BrowserContext* browser_context,
    const GURL& effective_site_url) {
  if (!requires_dedicated_process_) {
    return false;
  }

  for (const auto& pair : urls_to_modify_) {
    auto site_info = SiteInfo::CreateForTesting(
        IsolationContext(browser_context), pair.first);
    if (site_info.site_url() == effective_site_url) {
      return true;
    }
  }
  return false;
}

EffectiveURLContentBrowserClient::EffectiveURLContentBrowserClient(
    bool requires_dedicated_process)
    : helper_(requires_dedicated_process) {}

EffectiveURLContentBrowserClient::EffectiveURLContentBrowserClient(
    const GURL& url_to_modify,
    const GURL& url_to_return,
    bool requires_dedicated_process)
    : helper_(requires_dedicated_process) {
  AddTranslation(url_to_modify, url_to_return);
}

EffectiveURLContentBrowserClient::~EffectiveURLContentBrowserClient() = default;

void EffectiveURLContentBrowserClient::AddTranslation(
    const GURL& url_to_modify,
    const GURL& url_to_return) {
  helper_.AddTranslation(url_to_modify, url_to_return);
}

GURL EffectiveURLContentBrowserClient::GetEffectiveURL(
    BrowserContext* browser_context,
    const GURL& url) {
  return helper_.GetEffectiveURL(url);
}

bool EffectiveURLContentBrowserClient::DoesSiteRequireDedicatedProcess(
    BrowserContext* browser_context,
    const GURL& effective_site_url) {
  return helper_.DoesSiteRequireDedicatedProcess(browser_context,
                                                 effective_site_url);
}

ScopedContentBrowserClientSetting::ScopedContentBrowserClientSetting(
    ContentBrowserClient* new_client)
    : old_client_(SetBrowserClientForTesting(new_client)) {}

ScopedContentBrowserClientSetting::~ScopedContentBrowserClientSetting() {
  SetBrowserClientForTesting(old_client_);
}

}  // namespace content
