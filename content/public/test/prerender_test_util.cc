// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/prerender_test_util.h"

#include <tuple>

#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/trace_event/typed_macros.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"

namespace content {
namespace test {
namespace {

constexpr char kAddSpeculationRuleScript[] = R"({
    const script = document.createElement('script');
    script.type = 'speculationrules';
    script.text = `{
      "prerender": [{
        "source": "list",
        "urls": [$1]
      }]
    }`;
    document.head.appendChild(script);
  })";

constexpr char kAddSpeculationRuleWithTargetHintScript[] = R"({
    const script = document.createElement('script');
    script.type = 'speculationrules';
    script.text = `{
      "prerender": [{
        "source": "list",
        "urls": [$1],
        "target_hint": $2
      }]
    }`;
    document.head.appendChild(script);
  })";

PrerenderHostRegistry& GetPrerenderHostRegistry(WebContents* web_contents) {
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
  return *static_cast<WebContentsImpl*>(web_contents)
              ->GetPrerenderHostRegistry();
}

PrerenderHost* GetPrerenderHostById(WebContents* web_contents, int host_id) {
  auto& registry = GetPrerenderHostRegistry(web_contents);
  return registry.FindNonReservedHostById(host_id);
}

}  // namespace

class PrerenderHostRegistryObserverImpl
    : public PrerenderHostRegistry::Observer {
 public:
  explicit PrerenderHostRegistryObserverImpl(WebContents& web_contents) {
    observation_.Observe(&GetPrerenderHostRegistry(&web_contents));
  }

  void WaitForTrigger(const GURL& url) {
    if (triggered_.contains(url)) {
      return;
    }
    EXPECT_FALSE(waiting_.contains(url));
    base::RunLoop loop;
    waiting_[url] = loop.QuitClosure();
    loop.Run();
  }

  void NotifyOnTrigger(const GURL& url, base::OnceClosure callback) {
    if (triggered_.contains(url)) {
      std::move(callback).Run();
      return;
    }
    EXPECT_FALSE(waiting_.contains(url));
    waiting_[url] = std::move(callback);
  }

  void OnTrigger(const GURL& url) override {
    auto iter = waiting_.find(url);
    if (iter != waiting_.end()) {
      auto callback = std::move(iter->second);
      waiting_.erase(iter);
      std::move(callback).Run();
    } else {
      EXPECT_FALSE(triggered_.contains(url))
          << "this observer doesn't yet support multiple triggers";
      triggered_.insert(url);
    }
  }

  void OnRegistryDestroyed() override {
    EXPECT_TRUE(waiting_.empty());
    observation_.Reset();
  }

  base::ScopedObservation<PrerenderHostRegistry,
                          PrerenderHostRegistry::Observer>
      observation_{this};

  base::flat_map<GURL, base::OnceClosure> waiting_;
  base::flat_set<GURL> triggered_;
};

PrerenderHostRegistryObserver::PrerenderHostRegistryObserver(
    WebContents& web_contents)
    : impl_(std::make_unique<PrerenderHostRegistryObserverImpl>(web_contents)) {
}

PrerenderHostRegistryObserver::~PrerenderHostRegistryObserver() = default;

void PrerenderHostRegistryObserver::WaitForTrigger(const GURL& url) {
  TRACE_EVENT("test", "PrerenderHostRegistryObserver::WaitForTrigger", "url",
              url);
  impl_->WaitForTrigger(url);
}

void PrerenderHostRegistryObserver::NotifyOnTrigger(
    const GURL& url,
    base::OnceClosure callback) {
  TRACE_EVENT("test", "PrerenderHostRegistryObserver::NotifyOnTrigger", "url",
              url);
  impl_->NotifyOnTrigger(url, std::move(callback));
}

class PrerenderHostObserverImpl : public PrerenderHost::Observer {
 public:
  PrerenderHostObserverImpl(WebContents& web_contents, int host_id) {
    PrerenderHost* host = GetPrerenderHostById(&web_contents, host_id);
    DCHECK(host)
        << "A PrerenderHost with the given id does not, or no longer, exists.";
    StartObserving(*host);
  }

  PrerenderHostObserverImpl(WebContents& web_contents, const GURL& gurl) {
    registry_observer_ =
        std::make_unique<PrerenderHostRegistryObserver>(web_contents);
    if (PrerenderHost* host = GetPrerenderHostRegistry(&web_contents)
                                  .FindHostByUrlForTesting(gurl)) {
      StartObserving(*host);
    } else {
      registry_observer_->NotifyOnTrigger(
          gurl,
          base::BindOnce(&PrerenderHostObserverImpl::OnTrigger,
                         base::Unretained(this), std::ref(web_contents), gurl));
    }
  }

  void OnActivated() override {
    was_activated_ = true;
    if (waiting_for_activation_)
      std::move(waiting_for_activation_).Run();
  }

  void OnHostDestroyed(PrerenderFinalStatus final_status) override {
    observation_.Reset();
    if (waiting_for_destruction_)
      std::move(waiting_for_destruction_).Run();
  }

  void WaitForActivation() {
    if (was_activated_)
      return;
    EXPECT_FALSE(waiting_for_activation_);
    base::RunLoop loop;
    waiting_for_activation_ = loop.QuitClosure();
    loop.Run();
  }

  void WaitForDestroyed() {
    if (did_observe_ && !observation_.IsObserving())
      return;
    EXPECT_FALSE(waiting_for_destruction_);
    base::RunLoop loop;
    waiting_for_destruction_ = loop.QuitClosure();
    loop.Run();
  }

  bool was_activated() const { return was_activated_; }

 private:
  void OnTrigger(WebContents& web_contents, const GURL& gurl) {
    PrerenderHost* host =
        GetPrerenderHostRegistry(&web_contents).FindHostByUrlForTesting(gurl);
    DCHECK(host) << "Attempted to trigger a prerender for [" << gurl << "] "
                 << "but canceled before a PrerenderHost was created.";
    StartObserving(*host);
  }
  void StartObserving(PrerenderHost& host) {
    did_observe_ = true;
    observation_.Observe(&host);

    // This method may be bound and called from |registry_observer_| so don't
    // add code below the reset.
    registry_observer_.reset();
  }

  base::ScopedObservation<PrerenderHost, PrerenderHost::Observer> observation_{
      this};
  base::OnceClosure waiting_for_activation_;
  base::OnceClosure waiting_for_destruction_;
  std::unique_ptr<PrerenderHostRegistryObserver> registry_observer_;
  bool was_activated_ = false;
  bool did_observe_ = false;
};

PrerenderHostObserver::PrerenderHostObserver(WebContents& web_contents,
                                             int prerender_host)
    : impl_(std::make_unique<PrerenderHostObserverImpl>(web_contents,
                                                        prerender_host)) {}

PrerenderHostObserver::PrerenderHostObserver(WebContents& web_contents,
                                             const GURL& gurl)
    : impl_(std::make_unique<PrerenderHostObserverImpl>(web_contents, gurl)) {}

PrerenderHostObserver::~PrerenderHostObserver() = default;

void PrerenderHostObserver::WaitForActivation() {
  TRACE_EVENT("test", "PrerenderHostObserver::WaitForActivation");
  impl_->WaitForActivation();
}

void PrerenderHostObserver::WaitForDestroyed() {
  TRACE_EVENT("test", "PrerenderHostObserver::WaitForDestroyed");
  impl_->WaitForDestroyed();
}

bool PrerenderHostObserver::was_activated() const {
  return impl_->was_activated();
}

ScopedPrerenderFeatureList::ScopedPrerenderFeatureList() {
  // Disable the memory requirement of Prerender2
  // so the test can run on any bot.
  feature_list_.InitAndDisableFeature(
      blink::features::kPrerender2MemoryControls);
}

PrerenderTestHelper::PrerenderTestHelper(const WebContents::Getter& fn)
    : get_web_contents_fn_(fn) {}

PrerenderTestHelper::~PrerenderTestHelper() = default;

void PrerenderTestHelper::SetUp(
    net::test_server::EmbeddedTestServer* http_server) {
  EXPECT_FALSE(http_server->Started());
  http_server->RegisterRequestMonitor(base::BindRepeating(
      &PrerenderTestHelper::MonitorResourceRequest, base::Unretained(this)));
}

int PrerenderTestHelper::GetHostForUrl(const GURL& gurl) {
  auto* host =
      GetPrerenderHostRegistry(GetWebContents()).FindHostByUrlForTesting(gurl);
  return host ? host->frame_tree_node_id()
              : RenderFrameHost::kNoFrameTreeNodeId;
}

void PrerenderTestHelper::WaitForPrerenderLoadCompletion(int host_id) {
  TRACE_EVENT("test", "PrerenderTestHelper::WaitForPrerenderLoadCompletion",
              "host_id", host_id);
  auto* host = GetPrerenderHostById(GetWebContents(), host_id);
  ASSERT_NE(host, nullptr);
  auto status = host->WaitForLoadStopForTesting();
  EXPECT_EQ(status, PrerenderHost::LoadingOutcome::kLoadingCompleted);
}

// static
void PrerenderTestHelper::WaitForPrerenderLoadCompletion(
    WebContents& web_contents,
    const GURL& gurl) {
  TRACE_EVENT("test", "PrerenderTestHelper::WaitForPrerenderLoadCompletion",
              "web_contents", web_contents, "url", gurl);
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry(&web_contents);
  PrerenderHost* host = registry.FindHostByUrlForTesting(gurl);
  // Wait for the host to be created if it hasn't yet.
  if (!host) {
    PrerenderHostRegistryObserver observer(web_contents);
    observer.WaitForTrigger(gurl);
    host = registry.FindHostByUrlForTesting(gurl);
    ASSERT_NE(host, nullptr);
  }
  auto status = host->WaitForLoadStopForTesting();
  EXPECT_EQ(status, PrerenderHost::LoadingOutcome::kLoadingCompleted);
}

void PrerenderTestHelper::WaitForPrerenderLoadCompletion(const GURL& gurl) {
  TRACE_EVENT("test", "PrerenderTestHelper::WaitForPrerenderLoadCompletion",
              "url", gurl);
  WaitForPrerenderLoadCompletion(*GetWebContents(), gurl);
}

int PrerenderTestHelper::AddPrerender(const GURL& prerendering_url,
                                      int32_t world_id) {
  TRACE_EVENT("test", "PrerenderTestHelper::AddPrerender", "prerendering_url",
              prerendering_url);
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
  AddPrerenderAsync(prerendering_url, world_id);

  WaitForPrerenderLoadCompletion(prerendering_url);
  int host_id = GetHostForUrl(prerendering_url);
  EXPECT_NE(host_id, RenderFrameHost::kNoFrameTreeNodeId);
  return host_id;
}

void PrerenderTestHelper::AddPrerenderAsync(const GURL& prerendering_url,
                                            int32_t world_id) {
  TRACE_EVENT("test", "PrerenderTestHelper::AddPrerenderAsync",
              "prerendering_url", prerendering_url);
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string script = JsReplace(kAddSpeculationRuleScript, prerendering_url);

  if (world_id == ISOLATED_WORLD_ID_GLOBAL) {
    // Have to use ExecuteJavaScriptForTests instead of ExecJs/EvalJs here,
    // because some test pages have ContentSecurityPolicy and EvalJs cannot work
    // with it. See the quick migration guide for EvalJs for more information.
    GetWebContents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        base::UTF8ToUTF16(script), base::NullCallback());
  } else {
    GetWebContents()->GetPrimaryMainFrame()->ExecuteJavaScriptInIsolatedWorld(
        base::UTF8ToUTF16(script), base::NullCallback(), world_id);
  }
}

void PrerenderTestHelper::AddMultiplePrerenderAsync(
    const std::vector<GURL>& prerendering_urls) {
  TRACE_EVENT("test", "PrerenderTestHelper::AddMultiplePrerenderAsync",
              "prerendering_urls", prerendering_urls);
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Concatenate the given URLs with a comma separator.
  std::string urls_str;
  for (size_t i = 0; i < prerendering_urls.size(); i++) {
    // Wrap the url with double quotes.
    urls_str +=
        base::StringPrintf(R"("%s")", prerendering_urls[i].spec().c_str());
    if (i + 1 < prerendering_urls.size()) {
      urls_str += ", ";
    }
  }

  std::string script = base::ReplaceStringPlaceholders(
      kAddSpeculationRuleScript, {urls_str}, nullptr);

  GetWebContents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(script), base::NullCallback());
}

void PrerenderTestHelper::AddPrerenderWithTargetHintAsync(
    const GURL& prerendering_url,
    const std::string& target_hint) {
  TRACE_EVENT("test", "PrerenderTestHelper::AddPrerenderWithTargetHintAsync",
              "prerendering_url", prerendering_url, "target_hint", target_hint);
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string script = JsReplace(kAddSpeculationRuleWithTargetHintScript,
                                 prerendering_url, target_hint);

  // Have to use ExecuteJavaScriptForTests instead of ExecJs/EvalJs here,
  // because some test pages have ContentSecurityPolicy and EvalJs cannot work
  // with it. See the quick migration guide for EvalJs for more information.
  GetWebContents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(script), base::NullCallback());
}

std::unique_ptr<PrerenderHandle>
PrerenderTestHelper::AddEmbedderTriggeredPrerenderAsync(
    const GURL& prerendering_url,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix,
    ui::PageTransition page_transition) {
  TRACE_EVENT("test", "PrerenderTestHelper::AddEmbedderTriggeredPrerenderAsync",
              "prerendering_url", prerendering_url, "trigger_type",
              trigger_type, "embedder_histogram_suffix",
              embedder_histogram_suffix, "page_transition", page_transition);
  if (!content::BrowserThread::CurrentlyOn(BrowserThread::UI))
    return nullptr;

  WebContents* web_contents = GetWebContents();
  return web_contents->StartPrerendering(
      prerendering_url, trigger_type, embedder_histogram_suffix,
      page_transition, PreloadingHoldbackStatus::kUnspecified, nullptr);
}

void PrerenderTestHelper::NavigatePrerenderedPage(int host_id,
                                                  const GURL& gurl) {
  TRACE_EVENT("test", "PrerenderTestHelper::NavigatePrerenderedPage", "host_id",
              host_id, "url", gurl);
  auto* prerender_host = GetPrerenderHostById(GetWebContents(), host_id);
  ASSERT_NE(prerender_host, nullptr);
  RenderFrameHostImpl* prerender_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHost();
  // Ignore the result of ExecJs().
  //
  // Navigation from the prerendered page could cancel prerendering and
  // destroy the prerendered frame before ExecJs() gets a result from that.
  // This results in execution failure even when the execution succeeded. See
  // https://crbug.com/1186584 for details.
  //
  // This part will drastically be modified by the MPArch, so we take the
  // approach just to ignore it instead of fixing the timing issue. When
  // ExecJs() actually fails, the remaining test steps should fail, so it
  // should be safe to ignore it.
  std::ignore =
      ExecJs(prerender_render_frame_host, JsReplace("location = $1", gurl));
}

void PrerenderTestHelper::CancelPrerenderedPage(int host_id) {
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry(GetWebContents());
  registry.CancelHost(host_id, PrerenderFinalStatus::kDestroyed);
}

// static
void PrerenderTestHelper::NavigatePrimaryPage(WebContents& web_contents,
                                              const GURL& gurl) {
  TRACE_EVENT("test", "PrerenderTestHelper::NavigatePrimaryPage",
              "web_contents", web_contents, "url", gurl);
  if (web_contents.IsLoading()) {
    // Ensure that any ongoing navigation is complete prior to the construction
    // of |observer| below (this navigation may complete while executing ExecJs
    // machinery).
    content::TestNavigationObserver initial_observer(&web_contents);
    initial_observer.set_wait_event(
        content::TestNavigationObserver::WaitEvent::kLoadStopped);
    initial_observer.Wait();
  }

  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::TestNavigationObserver observer(&web_contents);
  observer.set_wait_event(
      content::TestNavigationObserver::WaitEvent::kLoadStopped);
  // Ignore the result of ExecJs().
  //
  // Depending on timing, activation could destroy the current WebContents
  // before ExecJs() gets a result from the frame that executed scripts. This
  // results in execution failure even when the execution succeeded. See
  // https://crbug.com/1156141 for details.
  //
  // This part will drastically be modified by the MPArch, so we take the
  // approach just to ignore it instead of fixing the timing issue. When
  // ExecJs() actually fails, the remaining test steps should fail, so it
  // should be safe to ignore it.
  std::ignore = ExecJs(web_contents.GetPrimaryMainFrame(),
                       JsReplace("location = $1", gurl));
  observer.Wait();
}

void PrerenderTestHelper::NavigatePrimaryPage(const GURL& gurl) {
  NavigatePrimaryPage(*GetWebContents(), gurl);
}

::testing::AssertionResult PrerenderTestHelper::VerifyPrerenderingState(
    const GURL& gurl) {
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry(GetWebContents());
  PrerenderHost* prerender_host = registry.FindHostByUrlForTesting(gurl);
  RenderFrameHostImpl* prerendered_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHost();
  std::vector<RenderFrameHost*> frames =
      CollectAllRenderFrameHosts(prerendered_render_frame_host);
  for (auto* frame : frames) {
    auto* rfhi = static_cast<RenderFrameHostImpl*>(frame);
    // All the subframes should be in LifecycleStateImpl::kPrerendering state
    // before activation.
    if (rfhi->lifecycle_state() !=
        RenderFrameHostImpl::LifecycleStateImpl::kPrerendering) {
      return ::testing::AssertionFailure() << "subframe in incorrect state";
    }
  }

  // Make sure that all the PrerenderHost frame trees are prerendering.
  const std::vector<FrameTree*> prerender_frame_trees =
      registry.GetPrerenderFrameTrees();
  std::for_each(std::begin(prerender_frame_trees),
                std::end(prerender_frame_trees), [](auto const& frame_tree) {
                  ASSERT_TRUE(frame_tree->is_prerendering());
                });

  return ::testing::AssertionSuccess();
}

RenderFrameHost* PrerenderTestHelper::GetPrerenderedMainFrameHost(int host_id) {
  auto* prerender_host = GetPrerenderHostById(GetWebContents(), host_id);
  EXPECT_NE(prerender_host, nullptr);
  return prerender_host->GetPrerenderedMainFrameHost();
}

int PrerenderTestHelper::GetRequestCount(const GURL& url) {
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock auto_lock(lock_);
  return request_count_by_path_[url.PathForRequest()];
}

net::test_server::HttpRequest::HeaderMap PrerenderTestHelper::GetRequestHeaders(
    const GURL& url) {
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock auto_lock(lock_);
  std::string path = url.PathForRequest();
  DCHECK(base::Contains(request_headers_by_path_, path)) << path;
  return request_headers_by_path_[path];
}

void PrerenderTestHelper::WaitForRequest(const GURL& url, int count) {
  TRACE_EVENT("test", "PrerenderTestHelper::WaitForRequest", "url", url,
              "count", count);
  for (;;) {
    base::RunLoop run_loop;
    {
      base::AutoLock auto_lock(lock_);
      if (request_count_by_path_[url.PathForRequest()] >= count)
        return;
      monitor_callback_ = run_loop.QuitClosure();
    }
    run_loop.Run();
  }
}

void PrerenderTestHelper::MonitorResourceRequest(
    const net::test_server::HttpRequest& request) {
  // This should be called on `EmbeddedTestServer::io_thread_`.
  EXPECT_FALSE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::AutoLock auto_lock(lock_);
  request_count_by_path_[request.GetURL().PathForRequest()]++;
  request_headers_by_path_.emplace(request.GetURL().PathForRequest(),
                                   request.headers);
  if (monitor_callback_)
    std::move(monitor_callback_).Run();
}

WebContents* PrerenderTestHelper::GetWebContents() {
  return get_web_contents_fn_.Run();
}

std::string PrerenderTestHelper::GenerateHistogramName(
    const std::string& histogram_base_name,
    content::PrerenderTriggerType trigger_type,
    const std::string& embedder_suffix) {
  switch (trigger_type) {
    case content::PrerenderTriggerType::kSpeculationRule:
      DCHECK(embedder_suffix.empty());
      return std::string(histogram_base_name) + ".SpeculationRule";
    case content::PrerenderTriggerType::kSpeculationRuleFromIsolatedWorld:
      DCHECK(embedder_suffix.empty());
      return std::string(histogram_base_name) +
             ".SpeculationRuleFromIsolatedWorld";
    case content::PrerenderTriggerType::kEmbedder:
      DCHECK(!embedder_suffix.empty());
      return std::string(histogram_base_name) + ".Embedder_" + embedder_suffix;
  }
  NOTREACHED();
}

ScopedPrerenderWebContentsDelegate::ScopedPrerenderWebContentsDelegate(
    WebContents& web_contents)
    : web_contents_(web_contents.GetWeakPtr()) {
  web_contents_->SetDelegate(this);
}

ScopedPrerenderWebContentsDelegate::~ScopedPrerenderWebContentsDelegate() {
  if (web_contents_)
    web_contents_.get()->SetDelegate(nullptr);
}

PreloadingEligibility ScopedPrerenderWebContentsDelegate::IsPrerender2Supported(
    WebContents& web_contents) {
  return PreloadingEligibility::kEligible;
}

}  // namespace test

}  // namespace content
