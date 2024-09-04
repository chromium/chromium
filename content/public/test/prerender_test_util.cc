// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/prerender_test_util.h"

#include <tuple>

#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/trace_event/typed_macros.h"
#include "base/types/cxx23_to_underlying.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/isolated_world_ids.h"
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
      "prerender": [{ $1 }]
    }`;
    document.head.appendChild(script);
  })";

std::string ConvertEagernessToString(
    blink::mojom::SpeculationEagerness eagerness) {
  switch (eagerness) {
    case blink::mojom::SpeculationEagerness::kEager:
      return "eager";
    case blink::mojom::SpeculationEagerness::kModerate:
      return "moderate";
    case blink::mojom::SpeculationEagerness::kConservative:
      return "conservative";
  }
}

// Builds <script type="speculationrules"> element for prerendering.
std::string BuildScriptElementSpeculationRules(
    const std::vector<GURL>& prerendering_urls,
    std::optional<blink::mojom::SpeculationEagerness> eagerness,
    std::optional<std::string> no_vary_search_hint,
    const std::string& target_hint) {
  std::stringstream ss;

  // Add source filed.
  ss << R"("source": "list", )";

  // Concatenate the given URLs with a comma separator.
  std::stringstream urls_ss;
  for (size_t i = 0; i < prerendering_urls.size(); i++) {
    // Wrap the url with double quotes.
    urls_ss << base::StringPrintf(R"("%s")",
                                  prerendering_urls[i].spec().c_str());
    if (i + 1 < prerendering_urls.size()) {
      urls_ss << ", ";
    }
  }
  // Add urls fields.
  ss << base::StringPrintf(R"("urls": [ %s ])", urls_ss.str().c_str());

  // Add eagerness field.
  if (eagerness.has_value()) {
    ss << base::StringPrintf(
        R"(, "eagerness": "%s")",
        ConvertEagernessToString(eagerness.value()).c_str());
  }
  if (no_vary_search_hint.has_value()) {
    ss << base::StringPrintf(R"(, "expects_no_vary_search": "%s")",
                             no_vary_search_hint.value().c_str());
  }

  // Add target_hint field.
  if (!target_hint.empty()) {
    ss << base::StringPrintf(R"(, "target_hint": "%s")", target_hint.c_str());
  }

  return base::ReplaceStringPlaceholders(kAddSpeculationRuleScript, {ss.str()},
                                         nullptr);
}

constexpr char kAddSpeculationRulePrefetchScript[] = R"({
    const script = document.createElement('script');
    script.type = 'speculationrules';
    script.text = `{
      "prefetch": [{
        "source": "list",
        "urls": [$1]
      }]
    }`;
    document.head.appendChild(script);
  })";

PrerenderHostRegistry& GetPrerenderHostRegistry(WebContents* web_contents) {
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
  return *static_cast<WebContentsImpl*>(web_contents)
              ->GetPrerenderHostRegistry();
}

PrerenderHost* GetPrerenderHostById(WebContents* web_contents,
                                    FrameTreeNodeId host_id) {
  auto& registry = GetPrerenderHostRegistry(web_contents);
  return registry.FindNonReservedHostById(host_id);
}

PrerenderHost* GetPrerenderHostByUrl(WebContents* web_contents,
                                     const GURL& url) {
  auto& registry = GetPrerenderHostRegistry(web_contents);
  return registry.FindHostByUrlForTesting(url);
}

}  // namespace

class PrerenderHostRegistryObserverImpl
    : public PrerenderHostRegistry::Observer {
 public:
  explicit PrerenderHostRegistryObserverImpl(WebContents& web_contents) {
    observation_.Observe(&GetPrerenderHostRegistry(&web_contents));
  }

  void WaitForTrigger(const GURL& url) {
    ASSERT_FALSE(waiting_.contains(url));
    if (triggered_.contains(url)) {
      return;
    }
    base::RunLoop loop;
    waiting_[url] = loop.QuitClosure();
    loop.Run();
  }

  GURL WaitForNextTrigger() {
    EXPECT_FALSE(waiting_next_);
    GURL triggered_url;
    base::RunLoop loop;
    waiting_next_ =
        base::BindLambdaForTesting([&triggered_url, &loop](const GURL& url) {
          triggered_url = url;
          loop.Quit();
        });
    loop.Run();
    return triggered_url;
  }

  void NotifyOnTrigger(const GURL& url, base::OnceClosure callback) {
    ASSERT_FALSE(waiting_.contains(url));
    if (triggered_.contains(url)) {
      std::move(callback).Run();
      return;
    }
    waiting_[url] = std::move(callback);
  }

  base::flat_set<GURL> GetTriggeredUrls() const { return triggered_; }

  void OnTrigger(const GURL& url) override {
    if (triggered_.contains(url)) {
      ASSERT_FALSE(waiting_.contains(url));
      return;
    }
    triggered_.insert(url);

    if (waiting_next_) {
      std::move(waiting_next_).Run(url);
    }

    auto iter = waiting_.find(url);
    if (iter != waiting_.end()) {
      auto callback = std::move(iter->second);
      waiting_.erase(iter);
      std::move(callback).Run();
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
  base::OnceCallback<void(const GURL&)> waiting_next_;

  // Set when prerendering is triggered. Doesn't yet support the case where
  // prerendering is triggered, canceled, and then re-triggered for the same
  // URL.
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

GURL PrerenderHostRegistryObserver::WaitForNextTrigger() {
  TRACE_EVENT("test", "PrerenderHostRegistryObserver::WaitForNextTrigger");
  return impl_->WaitForNextTrigger();
}

void PrerenderHostRegistryObserver::NotifyOnTrigger(
    const GURL& url,
    base::OnceClosure callback) {
  TRACE_EVENT("test", "PrerenderHostRegistryObserver::NotifyOnTrigger", "url",
              url);
  impl_->NotifyOnTrigger(url, std::move(callback));
}

base::flat_set<GURL> PrerenderHostRegistryObserver::GetTriggeredUrls() const {
  return impl_->GetTriggeredUrls();
}

class PrerenderHostObserverImpl : public PrerenderHost::Observer {
 public:
  PrerenderHostObserverImpl(WebContents& web_contents,
                            FrameTreeNodeId host_id) {
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

  void OnHeadersReceived() override {
    received_headers_ = true;
    if (waiting_for_headers_) {
      std::move(waiting_for_headers_).Run();
    }
  }

  void OnHostDestroyed(PrerenderFinalStatus final_status) override {
    observation_.Reset();
    last_status_ = final_status;
    if (waiting_for_destruction_) {
      std::move(waiting_for_destruction_).Run();
    }
    EXPECT_FALSE(waiting_for_activation_)
        << "A prerender was destroyed, with status "
        << base::to_underlying(final_status)
        << ", while waiting for activation.";
  }

  void WaitForActivation() {
    if (was_activated_)
      return;
    EXPECT_FALSE(waiting_for_activation_);

    EXPECT_FALSE(did_observe_ && !observation_.IsObserving())
        << "A prerender was destroyed, with status "
        << base::to_underlying(
               last_status_.value_or(PrerenderFinalStatus::kDestroyed))
        << ", before waiting for activation.";

    base::RunLoop loop;
    waiting_for_activation_ = loop.QuitClosure();
    loop.Run();

    EXPECT_TRUE(did_observe_) << "No prerender was triggered.";
  }

  void WaitForHeaders() {
    if (received_headers_) {
      return;
    }
    EXPECT_FALSE(waiting_for_headers_);

    EXPECT_FALSE(did_observe_ && !observation_.IsObserving())
        << "A prerender was destroyed, with status "
        << base::to_underlying(
               last_status_.value_or(PrerenderFinalStatus::kDestroyed))
        << ", before waiting for headers.";

    base::RunLoop loop;
    waiting_for_headers_ = loop.QuitClosure();
    loop.Run();

    EXPECT_TRUE(did_observe_) << "No prerender was triggered.";
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
  base::OnceClosure waiting_for_headers_;
  base::OnceClosure waiting_for_destruction_;
  std::unique_ptr<PrerenderHostRegistryObserver> registry_observer_;
  bool was_activated_ = false;
  bool received_headers_ = false;
  bool did_observe_ = false;
  std::optional<PrerenderFinalStatus> last_status_;
};

PrerenderHostObserver::PrerenderHostObserver(WebContents& web_contents,
                                             FrameTreeNodeId prerender_host)
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

void PrerenderHostObserver::WaitForHeaders() {
  TRACE_EVENT("test", "PrerenderHostObserver::WaitForHeaders");
  impl_->WaitForHeaders();
}

void PrerenderHostObserver::WaitForDestroyed() {
  TRACE_EVENT("test", "PrerenderHostObserver::WaitForDestroyed");
  impl_->WaitForDestroyed();
}

bool PrerenderHostObserver::was_activated() const {
  return impl_->was_activated();
}

PrerenderHostCreationWaiter::PrerenderHostCreationWaiter() {
  PrerenderHost::SetHostCreationCallbackForTesting(
      base::BindLambdaForTesting([&](FrameTreeNodeId host_id) {
        created_host_id_ = host_id;
        run_loop_.QuitClosure().Run();
      }));
}

FrameTreeNodeId PrerenderHostCreationWaiter::Wait() {
  EXPECT_TRUE(created_host_id_.is_null());
  run_loop_.Run();
  EXPECT_TRUE(created_host_id_);
  return created_host_id_;
}

ScopedPrerenderFeatureList::ScopedPrerenderFeatureList() {
  // Disable the memory requirement of Prerender2
  // so the test can run on any bot.
  feature_list_.InitWithFeatures({blink::features::kPrerender2InNewTab},
                                 {blink::features::kPrerender2MemoryControls});
}

PrerenderTestHelper::PrerenderTestHelper(const WebContents::Getter& fn)
    : get_web_contents_fn_(fn) {}

PrerenderTestHelper::~PrerenderTestHelper() = default;

void PrerenderTestHelper::RegisterServerRequestMonitor(
    net::test_server::EmbeddedTestServer* http_server) {
  EXPECT_FALSE(http_server->Started());
  http_server->RegisterRequestMonitor(base::BindRepeating(
      &PrerenderTestHelper::MonitorResourceRequest, base::Unretained(this)));
}
void PrerenderTestHelper::RegisterServerRequestMonitor(
    net::test_server::EmbeddedTestServer& test_server) {
  EXPECT_FALSE(test_server.Started());
  test_server.RegisterRequestMonitor(base::BindRepeating(
      &PrerenderTestHelper::MonitorResourceRequest, base::Unretained(this)));
}

// static
FrameTreeNodeId PrerenderTestHelper::GetHostForUrl(WebContents& web_contents,
                                                   const GURL& gurl) {
  auto* host =
      GetPrerenderHostRegistry(&web_contents).FindHostByUrlForTesting(gurl);
  return host ? host->frame_tree_node_id() : FrameTreeNodeId();
}

FrameTreeNodeId PrerenderTestHelper::GetHostForUrl(const GURL& gurl) {
  return GetHostForUrl(*GetWebContents(), gurl);
}

bool PrerenderTestHelper::HasNewTabHandle(FrameTreeNodeId host_id) {
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry(GetWebContents());
  return registry.HasNewTabHandleByIdForTesting(host_id);
}

void PrerenderTestHelper::WaitForPrerenderLoadCompletion(
    FrameTreeNodeId host_id) {
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

FrameTreeNodeId PrerenderTestHelper::AddPrerender(const GURL& prerendering_url,
                                                  int32_t world_id) {
  return AddPrerender(prerendering_url, /*eagerness=*/std::nullopt,
                      /*target_hint=*/"", world_id);
}

FrameTreeNodeId PrerenderTestHelper::AddPrerender(
    const GURL& prerendering_url,
    std::optional<blink::mojom::SpeculationEagerness> eagerness,
    const std::string& target_hint,
    int32_t world_id) {
  return AddPrerender(prerendering_url, eagerness,
                      /*no_vary_search_hint=*/std::nullopt, target_hint,
                      world_id);
}

FrameTreeNodeId PrerenderTestHelper::AddPrerender(
    const GURL& prerendering_url,
    std::optional<blink::mojom::SpeculationEagerness> eagerness,
    std::optional<std::string> no_vary_search_hint,
    const std::string& target_hint,
    int32_t world_id) {
  TRACE_EVENT("test", "PrerenderTestHelper::AddPrerender", "prerendering_url",
              prerendering_url);
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));

  WebContents* prerender_web_contents = nullptr;
  if (target_hint == "_blank") {
    // Wait until AddPrerendersAsync() creates a new WebContents for
    // prerendering.
    base::RunLoop run_loop;
    auto creation_subscription = content::RegisterWebContentsCreationCallback(
        base::BindLambdaForTesting([&](content::WebContents* web_contents) {
          prerender_web_contents = web_contents;
          run_loop.QuitClosure().Run();
        }));
    AddPrerendersAsync({prerendering_url}, eagerness, no_vary_search_hint,
                       target_hint, world_id);
    run_loop.Run();
  } else {
    // For other target hints, the initiator's WebContents will host a
    // prerendered page.
    prerender_web_contents = GetWebContents();
    AddPrerendersAsync({prerendering_url}, eagerness, no_vary_search_hint,
                       target_hint, world_id);
  }

  WaitForPrerenderLoadCompletion(*prerender_web_contents, prerendering_url);
  FrameTreeNodeId host_id =
      GetHostForUrl(*prerender_web_contents, prerendering_url);
  EXPECT_TRUE(host_id);
  return host_id;
}

void PrerenderTestHelper::AddPrerenderAsync(const GURL& prerendering_url,
                                            int32_t world_id) {
  AddPrerendersAsync({prerendering_url}, std::nullopt, std::string(), world_id);
}

void PrerenderTestHelper::AddPrerendersAsync(
    const std::vector<GURL>& prerendering_urls,
    std::optional<blink::mojom::SpeculationEagerness> eagerness,
    const std::string& target_hint,
    int32_t world_id) {
  AddPrerendersAsync(prerendering_urls, eagerness,
                     /*no_vary_search_hint=*/std::nullopt, target_hint,
                     world_id);
}

void PrerenderTestHelper::AddPrerendersAsync(
    const std::vector<GURL>& prerendering_urls,
    std::optional<blink::mojom::SpeculationEagerness> eagerness,
    std::optional<std::string> no_vary_search_hint,
    const std::string& target_hint,
    int32_t world_id) {
  TRACE_EVENT(
      "test", "PrerenderTestHelper::AddPrerendersAsync", "prerendering_urls",
      prerendering_urls, "eagerness",
      eagerness.has_value() ? ConvertEagernessToString(eagerness.value())
                            : "(empty)",
      "expected_no_vary_search",
      no_vary_search_hint.has_value() ? no_vary_search_hint.value() : "(empty)",
      "target_hint", target_hint.empty() ? "(empty)" : target_hint);
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string script = BuildScriptElementSpeculationRules(
      prerendering_urls, eagerness, no_vary_search_hint, target_hint);

  if (world_id == ISOLATED_WORLD_ID_GLOBAL) {
    // Have to use ExecuteJavaScriptForTests instead of ExecJs/EvalJs here,
    // because some test pages have ContentSecurityPolicy and EvalJs cannot work
    // with it. See the quick migration guide for EvalJs for more information.
    GetWebContents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        base::UTF8ToUTF16(script), base::NullCallback(), world_id);
  } else {
    GetWebContents()->GetPrimaryMainFrame()->ExecuteJavaScriptInIsolatedWorld(
        base::UTF8ToUTF16(script), base::NullCallback(), world_id);
  }
}

void PrerenderTestHelper::AddPrefetchAsync(const GURL& prefetch_url) {
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string script =
      JsReplace(kAddSpeculationRulePrefetchScript, prefetch_url);

  // Have to use ExecuteJavaScriptForTests instead of ExecJs/EvalJs here,
  // because some test pages have ContentSecurityPolicy and EvalJs cannot work
  // with it. See the quick migration guide for EvalJs for more information.
  GetWebContents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(script), base::NullCallback(),
      ISOLATED_WORLD_ID_GLOBAL);
}

std::unique_ptr<PrerenderHandle>
PrerenderTestHelper::AddEmbedderTriggeredPrerenderAsync(
    const GURL& prerendering_url,
    PreloadingTriggerType trigger_type,
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
      page_transition, /*should_warm_up_compositor=*/false,
      PreloadingHoldbackStatus::kUnspecified,
      /*preloading_attempt=*/nullptr, /*url_match_predicate=*/{},
      /*prerender_navigation_handle_callback=*/{});
}

void PrerenderTestHelper::NavigatePrerenderedPage(FrameTreeNodeId host_id,
                                                  const GURL& gurl) {
  TRACE_EVENT("test", "PrerenderTestHelper::NavigatePrerenderedPage", "host_id",
              host_id, "url", gurl);

  // Take RenderFrameHost corresponding to the main frame of the prerendered
  // page.
  auto* prerender_web_contents = WebContents::FromFrameTreeNodeId(host_id);
  auto* prerender_host = GetPrerenderHostById(prerender_web_contents, host_id);
  ASSERT_NE(prerender_host, nullptr);
  RenderFrameHostImpl* prerender_render_frame_host =
      prerender_host->GetPrerenderedMainFrameHost();

  // Navigate the RenderFrameHost to the URL.
  //
  // Ignore the result of ExecJs() to avoid unexpected execution failure.
  // Navigation from the prerendered page could cancel prerendering and destroy
  // the prerendered frame before ExecJs() gets a result from that. This results
  // in execution failure even when the execution succeeded.
  // See https://crbug.com/1186584 for details.
  std::ignore =
      ExecJs(prerender_render_frame_host, JsReplace("location = $1", gurl));
}

void PrerenderTestHelper::CancelPrerenderedPage(FrameTreeNodeId host_id) {
  PrerenderHostRegistry& registry = GetPrerenderHostRegistry(GetWebContents());
  registry.CancelHost(host_id, PrerenderFinalStatus::kDestroyed);
}

// static
std::unique_ptr<content::TestNavigationObserver>
PrerenderTestHelper::NavigatePrimaryPageAsync(WebContents& web_contents,
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
  std::unique_ptr<content::TestNavigationObserver> observer =
      std::make_unique<content::TestNavigationObserver>(&web_contents);
  observer->set_wait_event(
      content::TestNavigationObserver::WaitEvent::kLoadStopped);
  // Ignore the result of ExecJs().
  //
  // Depending on timing, activation could destroy a navigating frame before
  // ExecJs() gets a result from the frame. This results in execution failure
  // even when the navigation succeeded.
  std::ignore = ExecJs(web_contents.GetPrimaryMainFrame(),
                       JsReplace("location = $1", gurl));
  return observer;
}

std::unique_ptr<content::TestNavigationObserver>
PrerenderTestHelper::NavigatePrimaryPageAsync(const GURL& gurl) {
  return NavigatePrimaryPageAsync(*GetWebContents(), gurl);
}

// static
void PrerenderTestHelper::NavigatePrimaryPage(WebContents& web_contents,
                                              const GURL& gurl) {
  NavigatePrimaryPageAsync(web_contents, gurl)->Wait();
}

void PrerenderTestHelper::NavigatePrimaryPage(const GURL& gurl) {
  NavigatePrimaryPage(*GetWebContents(), gurl);
}

void PrerenderTestHelper::OpenNewWindowWithoutOpener(WebContents& web_contents,
                                                     const GURL& url) {
  std::string script = R"(window.open($1, "_blank", "noopener");)";
  EXPECT_TRUE(ExecJs(&web_contents, JsReplace(script, url.spec())));
}

void PrerenderTestHelper::SetHoldback(PreloadingType preloading_type,
                                      PreloadingPredictor predictor,
                                      bool holdback) {
  preloading_config_override_.SetHoldback(preloading_type, predictor, holdback);
}

void PrerenderTestHelper::SetHoldback(std::string_view preloading_type,
                                      std::string_view predictor,
                                      bool holdback) {
  preloading_config_override_.SetHoldback(preloading_type, predictor, holdback);
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

// static
RenderFrameHost* PrerenderTestHelper::GetPrerenderedMainFrameHost(
    WebContents& web_contents,
    FrameTreeNodeId host_id) {
  auto* prerender_host = GetPrerenderHostById(&web_contents, host_id);
  EXPECT_NE(prerender_host, nullptr);
  return prerender_host->GetPrerenderedMainFrameHost();
}

// static
RenderFrameHost* PrerenderTestHelper::GetPrerenderedMainFrameHost(
    WebContents& web_contents,
    const GURL& url) {
  auto* prerender_host = GetPrerenderHostByUrl(&web_contents, url);
  EXPECT_NE(prerender_host, nullptr);
  return prerender_host->GetPrerenderedMainFrameHost();
}

RenderFrameHost* PrerenderTestHelper::GetPrerenderedMainFrameHost(
    FrameTreeNodeId host_id) {
  return GetPrerenderedMainFrameHost(*GetWebContents(), host_id);
}

RenderFrameHost* PrerenderTestHelper::GetPrerenderedMainFrameHost(
    const GURL& url) {
  return GetPrerenderedMainFrameHost(*GetWebContents(), url);
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
    content::PreloadingTriggerType trigger_type,
    const std::string& embedder_suffix) {
  switch (trigger_type) {
    case content::PreloadingTriggerType::kSpeculationRule:
      DCHECK(embedder_suffix.empty());
      return std::string(histogram_base_name) + ".SpeculationRule";
    case content::PreloadingTriggerType::kSpeculationRuleFromIsolatedWorld:
      DCHECK(embedder_suffix.empty());
      return std::string(histogram_base_name) +
             ".SpeculationRuleFromIsolatedWorld";
    case content::PreloadingTriggerType::
        kSpeculationRuleFromAutoSpeculationRules:
      DCHECK(embedder_suffix.empty());
      return std::string(histogram_base_name) +
             ".SpeculationRuleFromAutoSpeculationRules";
    case content::PreloadingTriggerType::kEmbedder:
      DCHECK(!embedder_suffix.empty());
      return std::string(histogram_base_name) + ".Embedder_" + embedder_suffix;
  }
  NOTREACHED_IN_MIGRATION();
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

MockLinkPreviewWebContentsDelegate::MockLinkPreviewWebContentsDelegate() =
    default;

MockLinkPreviewWebContentsDelegate::~MockLinkPreviewWebContentsDelegate() =
    default;

}  // namespace test

}  // namespace content
