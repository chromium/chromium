// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/back_forward_cache_browsertest.h"

#include <climits>
#include <unordered_map>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/strings/string_piece_forward.h"
#include "base/system/sys_info.h"
#include "base/task/common/task_annotator.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/back_forward_cache_can_store_document_result.h"
#include "content/browser/renderer_host/back_forward_cache_disable.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/should_swap_browsing_instance.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/commit_message_delayer.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/text_input_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/web_contents_observer_test_utils.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/render_accessibility.mojom.h"

// This file has too many tests.
//
// Before adding new tests to this file, consider if they will fit better into
// one of the other back_forward_cache_*_browsertest.cc files or if there are
// enough new tests to justify a new file.

using testing::_;
using testing::Each;
using testing::ElementsAre;
using testing::Not;
using testing::UnorderedElementsAreArray;

namespace content {

using NotRestoredReasons =
    BackForwardCacheCanStoreDocumentResult::NotRestoredReasons;
using NotRestoredReason = BackForwardCacheMetrics::NotRestoredReason;

namespace {

class DOMContentLoadedObserver : public WebContentsObserver {
 public:
  explicit DOMContentLoadedObserver(RenderFrameHostImpl* render_frame_host)
      : WebContentsObserver(
            WebContents::FromRenderFrameHost(render_frame_host)),
        render_frame_host_(render_frame_host) {}

  void DOMContentLoaded(RenderFrameHost* render_frame_host) override {
    if (render_frame_host_ == render_frame_host)
      run_loop_.Quit();
  }

  [[nodiscard]] bool Wait() {
    if (render_frame_host_->IsDOMContentLoaded())
      run_loop_.Quit();
    run_loop_.Run();
    return render_frame_host_->IsDOMContentLoaded();
  }

 private:
  raw_ptr<RenderFrameHostImpl> render_frame_host_;
  base::RunLoop run_loop_;
};

}  // namespace

bool WaitForDOMContentLoaded(RenderFrameHostImpl* rfh) {
  DOMContentLoadedObserver observer(rfh);
  return observer.Wait();
}

EvalJsResult GetLocalStorage(RenderFrameHostImpl* rfh, std::string key) {
  return EvalJs(rfh, JsReplace("localStorage.getItem($1)", key));
}

BackForwardCacheBrowserTest::BackForwardCacheBrowserTest() = default;

BackForwardCacheBrowserTest::~BackForwardCacheBrowserTest() {
  if (fail_for_unexpected_messages_while_cached_) {
    // If this is triggered, see MojoInterfaceName in
    // tools/metrics/histograms/enums.xml for which values correspond which
    // messages.
    std::vector<base::Bucket> samples = histogram_tester().GetAllSamples(
        "BackForwardCache.UnexpectedRendererToBrowserMessage."
        "InterfaceName");
    // TODO(https://crbug.com/1379490): Remove this.
    // This bucket corresponds to the LocalFrameHost interface. It is known to
    // be flaky due calls to `LocalFrameHost::DidFocusFrame()` after entering
    // BFCache. So we ignore it for now by removing it if it's present until we
    // can fix the root cause.
    std::erase_if(samples, [](base::Bucket bucket) {
      return bucket.min ==
             static_cast<base::HistogramBase::Sample>(
                 base::HashMetricName(blink::mojom::LocalFrameHost::Name_));
    });

    EXPECT_THAT(samples, testing::ElementsAre());
  }
}

void BackForwardCacheBrowserTest::NotifyNotRestoredReasons(
    std::unique_ptr<BackForwardCacheCanStoreTreeResult> tree_result) {
  tree_result_ = std::move(tree_result);
}

void BackForwardCacheBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  ContentBrowserTest::SetUpCommandLine(command_line);
  mock_cert_verifier_.SetUpCommandLine(command_line);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kUseFakeUIForMediaStream);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
  // TODO(sreejakshetty): Initialize ScopedFeatureLists from test constructor.
  EnableFeatureAndSetParams(features::kBackForwardCacheTimeToLiveControl,
                            "time_to_live_seconds", "3600");
  // Entry to the cache can be slow during testing and cause flakiness.
  DisableFeature(features::kBackForwardCacheEntryTimeout);
  EnableFeatureAndSetParams(features::kBackForwardCache,
                            "message_handling_when_cached", "log");
  EnableFeatureAndSetParams(
      blink::features::kLogUnexpectedIPCPostedToBackForwardCachedDocuments,
      "delay_before_tracking_ms", "0");
  // Allow unlimited network during tests. Override this if you want to test the
  // network limiting.
  EnableFeatureAndSetParams(blink::features::kLoadingTasksUnfreezable,
                            "max_buffered_bytes_per_process",
                            base::NumberToString(INT_MAX));
  EnableFeatureAndSetParams(blink::features::kLoadingTasksUnfreezable,
                            "grace_period_to_finish_loading_in_seconds",
                            base::NumberToString(INT_MAX));
  // Enable capturing not-restored-reasons tree.
  EnableFeatureAndSetParams(
      blink::features::kBackForwardCacheSendNotRestoredReasons, "", "");

  // Do not trigger DumpWithoutCrashing() for JavaScript execution.
  DisableFeature(blink::features::kBackForwardCacheDWCOnJavaScriptExecution);
#if BUILDFLAG(IS_ANDROID)
  EnableFeatureAndSetParams(features::kBackForwardCache,
                            "process_binding_strength", "NORMAL");
#endif
    // Allow BackForwardCache for all devices regardless of their memory.
    DisableFeature(features::kBackForwardCacheMemoryControls);
    // Disables BackForwardCache cache size overwritten by
    // `content::kBackForwardCacheSize`, as many browser tests here assume
    // specific or smaller cache size (e.g. 1) rather than 6.
    DisableFeature(kBackForwardCacheSize);

    SetupFeaturesAndParameters();

    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
    // Unfortunately needed for one test on slow bots, TextInputStateUpdated,
    // where deferred commits delays input too much.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
}

void BackForwardCacheBrowserTest::SetUpInProcessBrowserTestFixture() {
  ContentBrowserTest::SetUpInProcessBrowserTestFixture();
  mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
}

void BackForwardCacheBrowserTest::TearDownInProcessBrowserTestFixture() {
  ContentBrowserTest::TearDownInProcessBrowserTestFixture();
  mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
}

void BackForwardCacheBrowserTest::SetupFeaturesAndParameters() {
  std::vector<base::test::FeatureRefAndParams> enabled_features;

  for (const auto& [feature_ref, params] : features_with_params_) {
    enabled_features.emplace_back(*feature_ref, params);
  }

  feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                              disabled_features_);
  vmodule_switches_.InitWithSwitches("back_forward_cache_impl=1");
}

void BackForwardCacheBrowserTest::EnableFeatureAndSetParams(
    const base::Feature& feature,
    std::string param_name,
    std::string param_value) {
  features_with_params_[feature][param_name] = param_value;
}

void BackForwardCacheBrowserTest::DisableFeature(const base::Feature& feature) {
  disabled_features_.push_back(feature);
}

void BackForwardCacheBrowserTest::SetUpOnMainThread() {
  mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
  host_resolver()->AddRule("*", "127.0.0.1");
  // TestAutoSetUkmRecorder's constructor requires a sequenced context.
  ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  histogram_tester_ = std::make_unique<base::HistogramTester>();
  ContentBrowserTest::SetUpOnMainThread();
}

void BackForwardCacheBrowserTest::TearDownOnMainThread() {
  ukm_recorder_.reset();
  ContentBrowserTest::TearDownOnMainThread();
}

WebContentsImpl* BackForwardCacheBrowserTest::web_contents() const {
  return static_cast<WebContentsImpl*>(shell()->web_contents());
}

RenderFrameHostImpl* BackForwardCacheBrowserTest::current_frame_host() {
  return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
}

RenderFrameHostManager*
BackForwardCacheBrowserTest::render_frame_host_manager() {
  return web_contents()->GetPrimaryFrameTree().root()->render_manager();
}

std::string BackForwardCacheBrowserTest::DepictFrameTree(FrameTreeNode* node) {
  return visualizer_.DepictFrameTree(node);
}

bool BackForwardCacheBrowserTest::HistogramContainsIntValue(
    base::HistogramBase::Sample sample,
    std::vector<base::Bucket> histogram_values) {
  return base::Contains(histogram_values, static_cast<int>(sample),
                        &base::Bucket::min);
}

void BackForwardCacheBrowserTest::EvictByJavaScript(RenderFrameHostImpl* rfh) {
  // Run JavaScript on a page in the back-forward cache. The page should be
  // evicted. As the frame is deleted, ExecJs returns false without executing.
  // Run without user gesture to prevent UpdateUserActivationState message
  // being sent back to browser.
  EXPECT_FALSE(
      ExecJs(rfh, "console.log('hi');", EXECUTE_SCRIPT_NO_USER_GESTURE));
}

void BackForwardCacheBrowserTest::StartRecordingEvents(
    RenderFrameHostImpl* rfh) {
  EXPECT_TRUE(ExecJs(rfh, R"(
      window.testObservedEvents = [];
      let event_list = [
        'visibilitychange',
        'pagehide',
        'pageshow',
        'freeze',
        'resume',
      ];
      for (event_name of event_list) {
        let result = event_name;
        window.addEventListener(event_name, event => {
          if (event.persisted)
            result += '.persisted';
          window.testObservedEvents.push('window.' + result);
        });
        document.addEventListener(event_name,
            () => window.testObservedEvents.push('document.' + result));
      }
    )"));
}

void BackForwardCacheBrowserTest::MatchEventList(RenderFrameHostImpl* rfh,
                                                 base::Value list,
                                                 base::Location location) {
  EXPECT_EQ(list, EvalJs(rfh, "window.testObservedEvents"))
      << location.ToString();
}

// Creates a minimal HTTPS server, accessible through https_server().
// Returns a pointer to the server.
net::EmbeddedTestServer* BackForwardCacheBrowserTest::CreateHttpsServer() {
  https_server_ = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_->AddDefaultHandlers(GetTestDataFilePath());
  https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  return https_server();
}

net::EmbeddedTestServer* BackForwardCacheBrowserTest::https_server() {
  return https_server_.get();
}

// Do not fail this test if a message from a renderer arrives at the browser
// for a cached page.
void BackForwardCacheBrowserTest::DoNotFailForUnexpectedMessagesWhileCached() {
  fail_for_unexpected_messages_while_cached_ = false;
}

  // Navigates to a page at |page_url| with an img element with src set to
  // "image.png".
RenderFrameHostImpl* BackForwardCacheBrowserTest::NavigateToPageWithImage(
    const GURL& page_url) {
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  RenderFrameHostImpl* rfh = current_frame_host();
  // Wait for the document to load DOM to ensure that kLoading is not
  // one of the reasons why the document wasn't cached.
  EXPECT_TRUE(WaitForDOMContentLoaded(rfh));

  EXPECT_TRUE(ExecJs(rfh, R"(
      var image = document.createElement("img");
      image.src = "image.png";
      document.body.appendChild(image);

      var image_load_status = new Promise((resolve, reject) => {
        image.onload = () => { resolve("loaded"); }
        image.onerror = () => { resolve("error"); }
      });
    )"));
  return rfh;
}

void BackForwardCacheBrowserTest::AcquireKeyboardLock(
    RenderFrameHostImpl* rfh) {
  EXPECT_TRUE(ExecJs(rfh, R"(
        new Promise(resolve => {
          navigator.keyboard.lock();
          resolve();
        });
      )"));
}

void BackForwardCacheBrowserTest::ReleaseKeyboardLock(
    RenderFrameHostImpl* rfh) {
  EXPECT_TRUE(ExecJs(rfh, R"(
        new Promise(resolve => {
          navigator.keyboard.unlock();
          resolve();
        });
      )"));
}

void BackForwardCacheBrowserTest::NavigateAndBlock(GURL url,
                                                   int history_offset) {
  // Block the navigation with an error.
  std::unique_ptr<URLLoaderInterceptor> url_interceptor =
      URLLoaderInterceptor::SetupRequestFailForURL(url,
                                                   net::ERR_BLOCKED_BY_CLIENT);
  if (history_offset) {
    shell()->GoBackOrForward(history_offset);
  } else {
    shell()->LoadURL(url);
  }
  WaitForLoadStop(web_contents());
  ASSERT_EQ(current_frame_host()->GetLastCommittedURL(), url);
  ASSERT_TRUE(current_frame_host()->IsErrorDocument());
}

ReasonsMatcher BackForwardCacheBrowserTest::MatchesNotRestoredReasons(
    const testing::Matcher<blink::mojom::BFCacheBlocked>& blocked,
    const absl::optional<testing::Matcher<std::string>>& id,
    const absl::optional<testing::Matcher<std::string>>& name,
    const absl::optional<testing::Matcher<std::string>>& src,
    const absl::optional<SameOriginMatcher>& same_origin_details) {
  return testing::Pointee(testing::AllOf(
      testing::Field("blocked",
                     &blink::mojom::BackForwardCacheNotRestoredReasons::blocked,
                     blocked),
      id.has_value()
          ? testing::Field(
                "id", &blink::mojom::BackForwardCacheNotRestoredReasons::id,
                testing::Optional(id.value()))
          : testing::Field(
                "id", &blink::mojom::BackForwardCacheNotRestoredReasons::id,
                absl::optional<std::string>(absl::nullopt)),
      name.has_value()
          ? testing::Field(
                "name", &blink::mojom::BackForwardCacheNotRestoredReasons::name,
                testing::Optional(name.value()))
          : testing::Field(
                "name", &blink::mojom::BackForwardCacheNotRestoredReasons::name,
                absl::optional<std::string>(absl::nullopt)),
      src.has_value()
          ? testing::Field(
                "src", &blink::mojom::BackForwardCacheNotRestoredReasons::src,
                testing::Optional(src.value()))
          : testing::Field(
                "src", &blink::mojom::BackForwardCacheNotRestoredReasons::src,
                absl::optional<std::string>(absl::nullopt)),
      testing::Field(
          "same_origin_details",
          &blink::mojom::BackForwardCacheNotRestoredReasons::
              same_origin_details,
          same_origin_details.has_value()
              ? same_origin_details.value()
              : testing::Property(
                    "is_null",
                    &blink::mojom::SameOriginBfcacheNotRestoredDetailsPtr::
                        is_null,
                    true))));
}

SameOriginMatcher BackForwardCacheBrowserTest::MatchesSameOriginDetails(
    const testing::Matcher<std::string>& url,
    const std::vector<testing::Matcher<std::string>>& reasons,
    const std::vector<ReasonsMatcher>& children) {
  return testing::Pointee(testing::AllOf(
      testing::Field(
          "url", &blink::mojom::SameOriginBfcacheNotRestoredDetails::url, url),
      testing::Field(
          "reasons",
          &blink::mojom::SameOriginBfcacheNotRestoredDetails::reasons,
          testing::UnorderedElementsAreArray(reasons)),
      testing::Field(
          "children",
          &blink::mojom::SameOriginBfcacheNotRestoredDetails::children,
          testing::ElementsAreArray(children))));
}

BlockingDetailsMatcher BackForwardCacheBrowserTest::MatchesBlockingDetails(
    const absl::optional<testing::Matcher<std::string>>& url,
    const absl::optional<testing::Matcher<std::string>>& function_name,
    const testing::Matcher<uint64_t>& line_number,
    const testing::Matcher<uint64_t>& column_number) {
  return testing::Pointee(testing::AllOf(
      url.has_value()
          ? testing::Field("url", &blink::mojom::BlockingDetails::url,
                           testing::Optional(url.value()))
          : testing::Field("url", &blink::mojom::BlockingDetails::url,
                           absl::optional<std::string>(absl::nullopt)),
      function_name.has_value()
          ? testing::Field("function_name",
                           &blink::mojom::BlockingDetails::function_name,
                           testing::Optional(function_name.value()))
          : testing::Field("function_name",
                           &blink::mojom::BlockingDetails::function_name,
                           absl::optional<std::string>(absl::nullopt)),
      testing::Field("line_number", &blink::mojom::BlockingDetails::line_number,
                     line_number),
      testing::Field("column_number",
                     &blink::mojom::BlockingDetails::column_number,
                     column_number)));
}

void BackForwardCacheUnloadBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
  scoped_feature_list_.InitAndEnableFeature(kBackForwardCacheUnloadAllowed);
}

std::initializer_list<RenderFrameHostImpl*> Elements(
    std::initializer_list<RenderFrameHostImpl*> t) {
  return t;
}

// Execute a custom callback when navigation is ready to commit. This is
// useful for simulating race conditions happening when a page enters the
// BackForwardCache and receive inflight messages sent when it wasn't frozen
// yet.
class ReadyToCommitNavigationCallback : public WebContentsObserver {
 public:
  ReadyToCommitNavigationCallback(
      WebContents* content,
      base::OnceCallback<void(NavigationHandle*)> callback)
      : WebContentsObserver(content), callback_(std::move(callback)) {}

  ReadyToCommitNavigationCallback(const ReadyToCommitNavigationCallback&) =
      delete;
  ReadyToCommitNavigationCallback& operator=(
      const ReadyToCommitNavigationCallback&) = delete;

 private:
  // WebContentsObserver:
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    if (callback_)
      std::move(callback_).Run(navigation_handle);
  }

  base::OnceCallback<void(NavigationHandle*)> callback_;
};

class FirstVisuallyNonEmptyPaintObserver : public WebContentsObserver {
 public:
  explicit FirstVisuallyNonEmptyPaintObserver(WebContents* contents)
      : WebContentsObserver(contents) {}
  void DidFirstVisuallyNonEmptyPaint() override {
    if (observed_)
      return;
    observed_ = true;
    run_loop_.Quit();
  }

  bool did_fire() const { return observed_; }

  void Wait() { run_loop_.Run(); }

 private:
  bool observed_ = false;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

void WaitForFirstVisuallyNonEmptyPaint(WebContents* contents) {
  if (contents->CompletedFirstVisuallyNonEmptyPaint())
    return;
  FirstVisuallyNonEmptyPaintObserver observer(contents);
  observer.Wait();
}

class ThemeColorObserver : public WebContentsObserver {
 public:
  explicit ThemeColorObserver(WebContents* contents)
      : WebContentsObserver(contents) {}

  // Can only be called once.
  [[nodiscard]] bool WaitUntilThemeColorChange() {
    CHECK(!loop_);
    loop_ = std::make_unique<base::RunLoop>();
    if (observed_) {
      return true;
    }
    loop_->Run();
    return observed_;
  }

  void DidChangeThemeColor() override {
    observed_ = true;
    if (loop_) {
      loop_->Quit();
    }
  }

  bool did_fire() const { return observed_; }

 private:
  std::unique_ptr<base::RunLoop> loop_;
  bool observed_ = false;
};

PageLifecycleStateManagerTestDelegate::PageLifecycleStateManagerTestDelegate(
    PageLifecycleStateManager* manager)
    : manager_(manager) {
  manager->SetDelegateForTesting(this);
}

PageLifecycleStateManagerTestDelegate::
    ~PageLifecycleStateManagerTestDelegate() {
  if (manager_)
    manager_->SetDelegateForTesting(nullptr);
}

bool PageLifecycleStateManagerTestDelegate::WaitForInBackForwardCacheAck() {
  DCHECK(manager_);
  if (manager_->last_acknowledged_state().is_in_back_forward_cache) {
    return true;
  }
  base::RunLoop loop;
  store_in_back_forward_cache_ack_received_ = loop.QuitClosure();
  loop.Run();
  return manager_->last_acknowledged_state().is_in_back_forward_cache;
}

void PageLifecycleStateManagerTestDelegate::OnStoreInBackForwardCacheSent(
    base::OnceClosure cb) {
  store_in_back_forward_cache_sent_ = std::move(cb);
}

void PageLifecycleStateManagerTestDelegate::OnDisableJsEvictionSent(
    base::OnceClosure cb) {
  disable_eviction_sent_ = std::move(cb);
}

void PageLifecycleStateManagerTestDelegate::OnRestoreFromBackForwardCacheSent(
    base::OnceClosure cb) {
  restore_from_back_forward_cache_sent_ = std::move(cb);
}

void PageLifecycleStateManagerTestDelegate::OnLastAcknowledgedStateChanged(
    const blink::mojom::PageLifecycleState& old_state,
    const blink::mojom::PageLifecycleState& new_state) {
  if (store_in_back_forward_cache_ack_received_ &&
      new_state.is_in_back_forward_cache)
    std::move(store_in_back_forward_cache_ack_received_).Run();
}

void PageLifecycleStateManagerTestDelegate::OnUpdateSentToRenderer(
    const blink::mojom::PageLifecycleState& new_state) {
  if (store_in_back_forward_cache_sent_ && new_state.is_in_back_forward_cache) {
    std::move(store_in_back_forward_cache_sent_).Run();
  }

  if (disable_eviction_sent_ && new_state.eviction_enabled == false) {
    std::move(disable_eviction_sent_).Run();
  }

  if (restore_from_back_forward_cache_sent_ &&
      !new_state.is_in_back_forward_cache) {
    std::move(restore_from_back_forward_cache_sent_).Run();
  }
}

void PageLifecycleStateManagerTestDelegate::OnDeleted() {
  manager_ = nullptr;
}

// Check the visible URL in the omnibox is properly updated when restoring a
// document from the BackForwardCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, VisibleURL) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Go to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  // 2) Go to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(url_a, web_contents()->GetVisibleURL());

  // 4) Go forward to B.
  ASSERT_TRUE(HistoryGoForward(web_contents()));
  EXPECT_EQ(url_b, web_contents()->GetVisibleURL());
}

// Test only 1 document is kept in the at a time BackForwardCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CacheSizeLimitedToOneDocumentPerTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  // BackForwardCache is empty.
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  // BackForwardCache contains only rfh_a.
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  // BackForwardCache contains only rfh_b.
  delete_observer_rfh_a.WaitUntilDeleted();
  EXPECT_FALSE(delete_observer_rfh_b.deleted());

  // If/when the cache size is increased, this can be tested iteratively, see
  // deleted code in: https://crrev.com/c/1782902.

  ASSERT_TRUE(HistoryGoToOffset(web_contents(), -2));
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::kCacheLimit},
                    {}, {}, {}, {}, FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, ResponseHeaders) {
  CreateHttpsServer();
  ASSERT_TRUE(https_server()->Start());

  GURL url_a(https_server()->GetURL("a.test", "/set-header?X-Foo: bar"));
  GURL url_b(https_server()->GetURL("b.test", "/title1.html"));

  // 1) Navigate to A.
  NavigationHandleObserver observer1(web_contents(), url_a);
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  EXPECT_TRUE(observer1.has_committed());
  EXPECT_EQ("bar", observer1.GetNormalizedResponseHeader("x-foo"));

  // 2) Navigate to B.
  NavigationHandleObserver observer2(web_contents(), url_b);
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(rfh_b->IsInBackForwardCache());
  EXPECT_TRUE(observer2.has_committed());

  // 3) Go back to A.
  NavigationHandleObserver observer3(web_contents(), url_a);
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
  EXPECT_TRUE(observer3.has_committed());
  EXPECT_EQ("bar", observer3.GetNormalizedResponseHeader("x-foo"));

  ExpectRestored(FROM_HERE);
}

void HighCacheSizeBackForwardCacheBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  EnableFeatureAndSetParams(features::kBackForwardCache, "cache_size",
                            base::NumberToString(kBackForwardCacheSize));
  BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
}

// Test documents are evicted from the BackForwardCache at some point.
IN_PROC_BROWSER_TEST_F(HighCacheSizeBackForwardCacheBrowserTest,
                       CacheEvictionWithIncreasedCacheSize) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a));  // BackForwardCache size is 0.
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  EXPECT_TRUE(NavigateToURL(shell(), url_b));  // BackForwardCache size is 1.
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  for (size_t i = 2; i < kBackForwardCacheSize; ++i) {
    EXPECT_TRUE(NavigateToURL(shell(), i % 2 ? url_b : url_a));
    // After |i+1| navigations, |i| documents went into the BackForwardCache.
    // When |i| is greater than the BackForwardCache size limit, they are
    // evicted:
    EXPECT_EQ(i >= kBackForwardCacheSize + 1, delete_observer_rfh_a.deleted());
    EXPECT_EQ(i >= kBackForwardCacheSize + 2, delete_observer_rfh_b.deleted());
  }
}

// Tests that evicting a page in between the time the back/forward cache
// NavigationRequest restore was created and when the NavigationRequest actually
// starts after finishing beforeunload won't result in a crash.
// See https://crbug.com/1218114.
IN_PROC_BROWSER_TEST_F(HighCacheSizeBackForwardCacheBrowserTest,
                       EvictedWhileWaitingForBeforeUnload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title3.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImplWrapper rfh_b(current_frame_host());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Navigate to C, which has a beforeunload handler that never finishes.
  EXPECT_TRUE(NavigateToURL(shell(), url_c));
  RenderFrameHostImplWrapper rfh_c(current_frame_host());
  EXPECT_TRUE(ExecJs(rfh_c.get(), R"(
    window.onbeforeunload = () => {
      while (true) {}
    }
  )"));
  // Both A & B are in the back/forward cache.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());

  // 4) Evict entry A. This will post a task that destroys all evicted entries
  // when it runs (task #1).
  DisableBFCacheForRFHForTesting(rfh_a->GetGlobalId());
  EXPECT_FALSE(rfh_a.IsDestroyed());
  EXPECT_TRUE(rfh_a->is_evicted_from_back_forward_cache());

  // 5) Trigger a back navigation to B. This will create a BFCache restore
  // navigation to B, but will wait for C's beforeunload handler to finish
  // running before continuing.
  // The BFCache entry will be evicted before the back navigation completes, so
  // the old navigation will be reset and a new navigation will be restarted.
  // This observer is waiting for the two navigation requests to complete.
  TestNavigationObserver observer(web_contents(),
                                  /* expected_number_of_navigations= */ 2,
                                  MessageLoopRunner::QuitMode::IMMEDIATE,
                                  /* ignore_uncommitted_navigations= */ false);
  web_contents()->GetController().GoBack();

  // 6) Post a task to run BeforeUnloadCompleted (task #2). This will continue
  // the BFCache restore navigation to B from step 5, which is currently waiting
  // for a BeforeUnloadCompleted call.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        root->navigator().BeforeUnloadCompleted(root, true /* proceed */,
                                                base::TimeTicks::Now());
      }));

  // 7) Evict entry B. This will post a task (task #3) to restart the navigation
  // to B, and also another task (task #4) to destroy all evicted entries.
  DisableBFCacheForRFHForTesting(rfh_b->GetGlobalId());
  EXPECT_FALSE(rfh_b.IsDestroyed());
  EXPECT_TRUE(rfh_b->is_evicted_from_back_forward_cache());

  // 8) Wait until the back navigation to B finishes. This will run posted tasks
  // in order. So:
  // - Task #1 from step 4 will run and destroy all evicted entries. As both the
  // entries for A & B have been evicted, they are both destroyed.
  // - Task #2 from step 6 will run and continue the back/forward cache restore
  // NavigationRequest to B. However, it would notice that the entry for B is
  // now gone, and should handle it gracefully.
  // - Task #3 from step 7 to restart navigation to B runs, and should create a
  // NavigationRequest to replace the previous NavigationRequest to B.
  // - Task #4 from step 7 to destroy evicted entries runs and won't destroy
  // any entry since there's no longer any entry in the back/forward cache.
  observer.Wait();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url_b);
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    {}, {}, {RenderFrameHostDisabledForTestingReason()}, {},
                    FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       SubframeWithOngoingNavigationNotCached) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/hung");
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a page with an iframe.
  TestNavigationObserver navigation_observer1(web_contents());
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/back_forward_cache/page_with_hung_iframe.html"));
  shell()->LoadURL(main_url);
  navigation_observer1.WaitForNavigationFinished();

  RenderFrameHostImpl* main_frame = current_frame_host();
  RenderFrameDeletedObserver frame_deleted_observer(main_frame);
  response.WaitForRequest();

  // Navigate away.
  TestNavigationObserver navigation_observer2(web_contents());
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));
  navigation_observer2.WaitForNavigationFinished();

  // The page with the unsupported feature should be deleted (not cached).
  frame_deleted_observer.WaitUntilDeleted();
}

// Only HTTP/HTTPS main document can enter the BackForwardCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, CacheHTTPDocumentOnly) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(CreateHttpsServer()->Start());

  GURL http_url(embedded_test_server()->GetURL("a.test", "/title1.html"));
  GURL https_url(https_server()->GetURL("a.test", "/title1.html"));
  GURL file_url = net::FilePathToFileURL(GetTestFilePath("", "title1.html"));
  GURL data_url = GURL("data:text/html,");
  GURL blank_url = GURL(url::kAboutBlankURL);
  GURL webui_url = GetWebUIURL("gpu");

  enum { STORED, DELETED };
  struct {
    int expectation;
    GURL url;
  } test_cases[] = {
      // Only document with HTTP/HTTPS URLs are allowed to enter the
      // BackForwardCache.
      {STORED, http_url},
      {STORED, https_url},

      // Others aren't allowed.
      {DELETED, file_url},
      {DELETED, data_url},
      {DELETED, webui_url},
      {DELETED, blank_url},
  };

  char hostname[] = "a.unique";
  for (auto& test_case : test_cases) {
    SCOPED_TRACE(testing::Message()
                 << std::endl
                 << "expectation = " << test_case.expectation << std::endl
                 << "url = " << test_case.url << std::endl);

    // 1) Navigate to.
    EXPECT_TRUE(NavigateToURL(shell(), test_case.url));
    RenderFrameHostImplWrapper rfh(current_frame_host());

    // 2) Navigate away.
    hostname[0]++;
    GURL reset_url(embedded_test_server()->GetURL(hostname, "/title1.html"));
    EXPECT_TRUE(NavigateToURL(shell(), reset_url));

    if (test_case.expectation == STORED) {
      EXPECT_FALSE(rfh.IsRenderFrameDeleted());
      EXPECT_TRUE(rfh->IsInBackForwardCache());
      continue;
    }

    if (rfh.get() == current_frame_host()) {
      // If the RenderFrameHost is reused, it won't be deleted, so don't wait
      // for deletion. Just check that it's not saved in the back-forward cache.
      EXPECT_FALSE(rfh.IsRenderFrameDeleted());
      EXPECT_FALSE(rfh->IsInBackForwardCache());
      continue;
    }

    // When the RenderFrameHost is not reused and it's not stored in the
    // back-forward cache, it will eventually be deleted.
    ASSERT_TRUE(rfh.WaitUntilRenderFrameDeleted());
  }
}

// Regression test for https://crbug.com/993337.
//
// A note about sharing BrowsingInstances and the BackForwardCache:
//
// We should never keep around more than one main frame that belongs to the same
// BrowsingInstance. When swapping two pages, when one is stored in the
// back-forward cache or one is restored from it, the current code expects the
// two to live in different BrowsingInstances.
//
// History navigation can recreate a page with the same BrowsingInstance as the
// one stored in the back-forward cache. This case must to be handled. When it
// happens, the back-forward cache page is evicted.
//
// Since cache eviction is asynchronous, it's is possible for two main frames
// belonging to the same BrowsingInstance to be alive for a brief period of time
// (the new page being navigated to, and a page in the cache, until it is
// destroyed asynchronously via eviction).
//
// The test below tests that the brief period of time where two main frames are
// alive in the same BrowsingInstance does not cause anything to blow up.

// TODO(crbug.com/1127979, crbug.com/1446206): Flaky on Linux, Windows and
// ChromeOS, iOS, and Mac.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC) || BUILDFLAG(IS_IOS)
#define MAYBE_NavigateToTwoPagesOnSameSite DISABLED_NavigateToTwoPagesOnSameSite
#else
#define MAYBE_NavigateToTwoPagesOnSameSite NavigateToTwoPagesOnSameSite
#endif
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MAYBE_NavigateToTwoPagesOnSameSite) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_b3(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A1.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));

  // 2) Navigate to A2.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  RenderFrameHostImpl* rfh_a2 = current_frame_host();
  RenderFrameDeletedObserver delete_rfh_a2(current_frame_host());

  // 3) Navigate to B3.
  EXPECT_TRUE(NavigateToURL(shell(), url_b3));
  EXPECT_TRUE(rfh_a2->IsInBackForwardCache());
  RenderFrameHostImpl* rfh_b3 = current_frame_host();

  // 4) Do a history navigation back to A1.
  ASSERT_TRUE(HistoryGoToIndex(web_contents(), 0));
  EXPECT_TRUE(rfh_b3->IsInBackForwardCache());

  // Note that the frame for A1 gets created before A2 is deleted from the
  // cache, so there will be a brief period where two the main frames (A1 and
  // A2) are alive in the same BrowsingInstance/SiteInstance, at the same time.
  // That is the scenario this test is covering. This used to cause a CHECK,
  // because the two main frames shared a single RenderViewHost (no longer the
  // case after https://crrev.com/c/1833616).

  // A2 should be evicted from the cache and asynchronously deleted, due to the
  // cache size limit (B3 took its place in the cache).
  delete_rfh_a2.WaitUntilDeleted();
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       NavigateToTwoPagesOnSameSiteWithSubframes) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // This test covers the same scenario as NavigateToTwoPagesOnSameSite, except
  // the pages contain subframes:
  // A1(B) -> A2(B(C)) -> D3 -> A1(B)
  //
  // The subframes shouldn't make a difference, so the expected behavior is the
  // same as NavigateToTwoPagesOnSameSite.
  GURL url_a1(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_a2(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  GURL url_d3(embedded_test_server()->GetURL("d.com", "/title1.html"));

  // 1) Navigate to A1(B).
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));

  // 2) Navigate to A2(B(C)).
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  RenderFrameHostImpl* rfh_a2 = current_frame_host();
  RenderFrameDeletedObserver delete_rfh_a2(current_frame_host());

  // 3) Navigate to D3.
  EXPECT_TRUE(NavigateToURL(shell(), url_d3));
  EXPECT_TRUE(rfh_a2->IsInBackForwardCache());
  RenderFrameHostImpl* rfh_d3 = current_frame_host();

  // 4) Do a history navigation back to A1(B).
  ASSERT_TRUE(HistoryGoToIndex(web_contents(), 0));

  // D3 takes A2(B(C))'s place in the cache.
  EXPECT_TRUE(rfh_d3->IsInBackForwardCache());
  delete_rfh_a2.WaitUntilDeleted();
}

// Sub-frame doesn't transition from LifecycleStateImpl::kInBackForwardCache to
// LifecycleStateImpl::kRunningUnloadHandlers even when the sub-frame having
// unload handlers is being evicted from BackForwardCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheUnloadBrowserTest,
                       SubframeWithUnloadHandler) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a.com(a.com)"));
  GURL child_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a.com()");
  GURL url_2(embedded_test_server()->GetURL("a.com", "/title1.html"));

  // 1) Navigate to |main_url|.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImpl* main_rfh = current_frame_host();
  ASSERT_EQ(1U, main_rfh->child_count());
  RenderFrameHostImpl* child_rfh = main_rfh->child_at(0)->current_frame_host();
  RenderFrameDeletedObserver main_rfh_observer(main_rfh),
      child_rfh_observer(child_rfh);

  // 2) Add an unload handler to the child RFH.
  EXPECT_TRUE(ExecJs(child_rfh, "window.onunload = () => {} "));

  // 3) Navigate to |url_2|.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  // 4) The previous main RFH and child RFH should be in the back-forward
  // cache.
  EXPECT_FALSE(main_rfh_observer.deleted());
  EXPECT_FALSE(child_rfh_observer.deleted());
  EXPECT_TRUE(main_rfh->IsInBackForwardCache());
  EXPECT_TRUE(child_rfh->IsInBackForwardCache());

  // Destruction of bfcached page happens after shutdown and it should not
  // trigger unload handlers and be destroyed directly.
}

// Do a same document navigation and make sure we do not fire the
// DidFirstVisuallyNonEmptyPaint again
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    DoesNotFireDidFirstVisuallyNonEmptyPaintForSameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a_1(embedded_test_server()->GetURL(
      "a.com", "/accessibility/html/a-name.html"));
  GURL url_a_2(embedded_test_server()->GetURL(
      "a.com", "/accessibility/html/a-name.html#id"));

  EXPECT_TRUE(NavigateToURL(shell(), url_a_1));
  WaitForFirstVisuallyNonEmptyPaint(shell()->web_contents());

  FirstVisuallyNonEmptyPaintObserver observer(web_contents());
  EXPECT_TRUE(NavigateToURL(shell(), url_a_2));
  // Make sure the bfcache restore code does not fire the event during commit
  // navigation.
  EXPECT_FALSE(observer.did_fire());
  EXPECT_TRUE(web_contents()->CompletedFirstVisuallyNonEmptyPaint());
}

// Make sure we fire DidFirstVisuallyNonEmptyPaint when restoring from bf-cache.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    FiresDidFirstVisuallyNonEmptyPaintWhenRestoredFromCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  WaitForFirstVisuallyNonEmptyPaint(shell()->web_contents());
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  ASSERT_TRUE(rfh_a->IsInBackForwardCache());
  WaitForFirstVisuallyNonEmptyPaint(shell()->web_contents());

  // 3) Navigate to back to A.
  FirstVisuallyNonEmptyPaintObserver observer(web_contents());
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  // Make sure the bfcache restore code does fire the event during commit
  // navigation.
  EXPECT_TRUE(web_contents()->CompletedFirstVisuallyNonEmptyPaint());
  EXPECT_TRUE(observer.did_fire());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       SetsThemeColorWhenRestoredFromCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/theme_color.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  WaitForFirstVisuallyNonEmptyPaint(web_contents());
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  EXPECT_EQ(web_contents()->GetThemeColor(), 0xFFFF0000u);

  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  WaitForFirstVisuallyNonEmptyPaint(web_contents());
  ASSERT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_EQ(web_contents()->GetThemeColor(), absl::nullopt);

  ThemeColorObserver observer(web_contents());
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ASSERT_TRUE(observer.WaitUntilThemeColorChange());
  EXPECT_EQ(web_contents()->GetThemeColor(), 0xFFFF0000u);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       ContentsMimeTypeWhenRestoredFromCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  EXPECT_EQ(web_contents()->GetContentsMimeType(), "text/html");

  // Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Go back to A, which restores A from bfcache. ContentsMimeType should be
  // restored as well.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());
  ExpectRestored(FROM_HERE);
  EXPECT_EQ(web_contents()->GetContentsMimeType(), "text/html");
}

// Check BackForwardCache is enabled and works for devices with very low memory.
// Navigate from A -> B and go back.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       BackForwardCacheEnabledOnLowMemoryDevices) {
  // Set device physical memory to 10 MB.
  blink::ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(10);
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameDeletedObserver delete_observer_rfh_a(current_frame_host());
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate to B. A should be in BackForwardCache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // 3) Go back to A. B should be in BackForwardCache.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_b.deleted());
  EXPECT_TRUE(rfh_b->IsInBackForwardCache());
}

// Test for functionality of memory controls in back-forward cache for low
// memory devices.
class BackForwardCacheBrowserTestForLowMemoryDevices
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);

    // Set the value of memory threshold more than the physical memory and check
    // if back-forward cache is disabled or not.
    std::string memory_threshold =
        base::NumberToString(base::SysInfo::AmountOfPhysicalMemoryMB() + 1);
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackForwardCacheMemoryControls,
          {{"memory_threshold_for_back_forward_cache_in_mb",
            memory_threshold}}},
         {features::kBackForwardCache_NoMemoryLimit_Trial, {}},
         {blink::features::kLoadingTasksUnfreezable, {}}},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensure that the BackForwardCache trial is not activated and the
// BackForwardCache_NoMemoryLimit_Trial trial got activated as expected on
// low-memory devices.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestForLowMemoryDevices,
                       DisableBFCacheForLowEndDevices) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Ensure that the BackForwardCache trial starts inactive, and the
  // BackForwardCache_NoMemoryLimit_Trial trial starts active.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          features::kBackForwardCache_NoMemoryLimit_Trial)
          ->trial_name()));

  EXPECT_FALSE(IsBackForwardCacheEnabled());

  // Ensure that we do not activate the BackForwardCache trial when querying
  // bfcache status, and the BackForwardCache_NoMemoryLimit_Trial trial stays
  // active.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          features::kBackForwardCache_NoMemoryLimit_Trial)
          ->trial_name()));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) A shouldn't be stored in back-forward cache because the physical
  // memory is less than the memory threshold.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 4) Go back to check the
  // NotRestoredReasons.kBackForwardCacheDisabledByLowMemory is recorded when
  // the memory is less than the threshold value.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  ExpectNotRestored(
      {
          BackForwardCacheMetrics::NotRestoredReason::kBackForwardCacheDisabled,
          BackForwardCacheMetrics::NotRestoredReason::
              kBackForwardCacheDisabledByLowMemory,
      },
      {}, {}, {}, {}, FROM_HERE);

  // Ensure that the BackForwardCache trial still hasn't been activated, and the
  // BackForwardCache_NoMemoryLimit_Trial trial stays active.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          features::kBackForwardCache_NoMemoryLimit_Trial)
          ->trial_name()));
}

// Trigger network reqeuests, then navigate from A to B, then go back.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestForLowMemoryDevices,
                       DisableBFCacheForLowEndDevices_NetworkRequests) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Ensure that the trials starts inactive.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          blink::features::kLoadingTasksUnfreezable)
          ->trial_name()));

  EXPECT_FALSE(IsBackForwardCacheEnabled());

  // Ensure that we do not activate the trials for kBackForwardCache and
  // kLoadingTasksUnfreezable when querying bfcache or unfreezable loading tasks
  // status.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          blink::features::kLoadingTasksUnfreezable)
          ->trial_name()));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Request for an image and send a response to trigger loading code. This is
  // to ensure kLoadingTasksUnfreezable won't trigger bfcache activation.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
      var image = document.createElement("img");
      image.src = "image.png";
      document.body.appendChild(image);
    )"));
  image_response.WaitForRequest();
  image_response.Send(net::HTTP_OK, "image/png");
  image_response.Send("image_body");
  image_response.Done();

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) A shouldn't be stored in back-forward cache because the physical
  // memory is less than the memory threshold.
  delete_observer_rfh_a.WaitUntilDeleted();

  // Nothing is recorded when the memory is less than the threshold value.
  ExpectOutcomeDidNotChange(FROM_HERE);
  ExpectNotRestoredDidNotChange(FROM_HERE);

  // Ensure that the trials still haven't been activated.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          blink::features::kLoadingTasksUnfreezable)
          ->trial_name()));
}

// Test for functionality of memory controls in back-forward cache for high
// memory devices.
class BackForwardCacheBrowserTestForHighMemoryDevices
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);

    // Set the value of memory threshold less than the physical memory and check
    // if back-forward cache is enabled or not.
    std::string memory_threshold =
        base::NumberToString(base::SysInfo::AmountOfPhysicalMemoryMB() - 1);
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackForwardCacheMemoryControls,
          {{"memory_threshold_for_back_forward_cache_in_mb",
            memory_threshold}}},
         {features::kBackForwardCache_NoMemoryLimit_Trial, {}},
         {blink::features::kLoadingTasksUnfreezable, {}}},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensure that the BackForwardCache_NoMemoryLimit_Trial and the
// BackForwardCache trials got activated as expected on high-memory devices
// when the BackForwardCache feature is enabled.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestForHighMemoryDevices,
                       EnableBFCacheForHighMemoryDevices) {
  // Ensure that the BackForwardCache and the
  // BackForwardCache_NoMemoryLimit_Trial trials start active
  // on high-memory devices when the BackForwardCache feature is enabled,
  // because IsBackForwardCacheEnabled() got queried already before the test
  // starts.
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          features::kBackForwardCache_NoMemoryLimit_Trial)
          ->trial_name()));

  EXPECT_TRUE(IsBackForwardCacheEnabled());

  // Ensure that the BackForwardCache and the
  // BackForwardCache_NoMemoryLimit_Trial trial stays active after
  // querying IsBackForwardCacheEnabled().
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          features::kBackForwardCache_NoMemoryLimit_Trial)
          ->trial_name()));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) A should be stored in back-forward cache because the physical memory is
  // greater than the memory threshold.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Ensure that the BackForwardCache and the
  // BackForwardCache_NoMemoryLimit_Trial trial stays active.
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          features::kBackForwardCache_NoMemoryLimit_Trial)
          ->trial_name()));
}

// Trigger network reqeuests, then navigate from A to B, then go back.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTestForHighMemoryDevices,
                       EnableBFCacheForHighMemoryDevices_NetworkRequests) {
  net::test_server::ControllableHttpResponse image_response(
      embedded_test_server(), "/image.png");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Ensure that back-forward cache flag is enabled and the trial is active.
  EXPECT_TRUE(IsBackForwardCacheEnabled());
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));

  // Ensure that the LoadingTasksUnfreezable trials starts as inactive.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          blink::features::kLoadingTasksUnfreezable)
          ->trial_name()));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Request for an image and send a response to trigger loading code.
  EXPECT_TRUE(ExecJs(rfh_a, R"(
      var image = document.createElement("img");
      image.src = "image.png";
      document.body.appendChild(image);
    )"));
  image_response.WaitForRequest();
  image_response.Send(net::HTTP_OK, "image/png");
  image_response.Send("image_body");
  image_response.Done();

  // The loading code activates the LoadingTasksUnfreezable trial.
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          blink::features::kLoadingTasksUnfreezable)
          ->trial_name()));

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) A should be stored in back-forward cache because the physical memory is
  // greater than the memory threshold.
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());

  // Ensure that the trials stay activated.
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(features::kBackForwardCache)
          ->trial_name()));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          blink::features::kLoadingTasksUnfreezable)
          ->trial_name()));
}

// Tests for high memory devices that have the BackForwardCache feature flag
// disabled.
class BackForwardCacheBrowserTestForHighMemoryDevicesWithBFCacheDisabled
    : public BackForwardCacheBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);

    // Set the value of memory threshold less than the physical memory and check
    // if back-forward cache is enabled or not.
    std::string memory_threshold =
        base::NumberToString(base::SysInfo::AmountOfPhysicalMemoryMB() - 1);
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kBackForwardCacheMemoryControls,
          {{"memory_threshold_for_back_forward_cache_in_mb",
            memory_threshold}}},
         {features::kBackForwardCache_NoMemoryLimit_Trial, {}},
         {blink::features::kLoadingTasksUnfreezable, {}}},
        /*disabled_features=*/
        {features::kBackForwardCache});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensure that the BackForwardCache_NoMemoryLimit_Trial does not get activated
// on high-memory devices that have the BackForwardCache feature disabled.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTestForHighMemoryDevicesWithBFCacheDisabled,
    HighMemoryDevicesWithBFacheDisabled) {
  // Ensure that BackForwardCache_NoMemoryLimit_Trial trials starts inactive
  // on high-memory devices that have the BackForwardCache feature disabled.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          features::kBackForwardCache_NoMemoryLimit_Trial)
          ->trial_name()));

  // Ensure that IsBackForwardCacheEnabled() returns false, because the
  // BackForwardCache feature is disabled.
  EXPECT_FALSE(IsBackForwardCacheEnabled());

  // Ensure that the BackForwardCache_NoMemoryLimit_Trial trial stays inactive
  // after querying IsBackForwardCacheEnabled().
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          features::kBackForwardCache_NoMemoryLimit_Trial)
          ->trial_name()));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) A shouldn't be stored in back-forward cache because the BackForwardCache
  // feature is disabled.
  delete_observer_rfh_a.WaitUntilDeleted();

  // 4) Go back to check that only kBackForwardCacheDisabled is recorded.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  ExpectNotRestored(
      {
          BackForwardCacheMetrics::NotRestoredReason::kBackForwardCacheDisabled,
      },
      {}, {}, {}, {}, FROM_HERE);

  // Ensure that the BackForwardCache_NoMemoryLimit_Trial trial stays inactive.
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      base::FeatureList::GetFieldTrial(
          features::kBackForwardCache_NoMemoryLimit_Trial)
          ->trial_name()));
}

// Start an inifite dialogs in JS, yielding after each. The first dialog should
// be dismissed by navigation. The later dialogs should be handled gracefully
// and not appear while in BFCache. Finally, when the page comes out of BFCache,
// dialogs should appear again.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       CanUseCacheWhenPageAlertsInTimeoutLoop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  AppModalDialogWaiter dialog_waiter(shell());

  EXPECT_TRUE(ExecJs(rfh_a, R"(
    function alertLoop() {
      setTimeout(alertLoop, 0);
      window.alert("alert");
    }
    // Don't block this script.
    setTimeout(alertLoop, 0);
  )"));

  dialog_waiter.Wait();

  // Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();

  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  ASSERT_THAT(rfh_a, InBackForwardCache());
  ASSERT_NE(rfh_a, rfh_b);

  dialog_waiter.Restart();

  // Go back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());

  // The page should still be requesting dialogs in a loop. Wait for one to be
  // requested.
  dialog_waiter.Wait();
}

// UnloadOldFrame will clear all dialogs. We test that further requests for
// dialogs coming from JS do not result in the creation of a dialog. This test
// posts some dialog creation JS to the render from inside the
// CommitNavigationCallback task. This JS is then able to post a task back to
// the renders to show a dialog. By the time this task runs, we the
// RenderFrameHostImpl's is_active() should be false.
//
// This test is not perfect, it can pass simply because the renderer thread does
// not run the JS in time. Ideally it would block until the renderer posts the
// request for a dialog but it's possible to do that without creating a nested
// message loop and if we do that, we risk processing the dialog request.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       DialogsCancelledAndSuppressedWhenCached) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // Let's us know whether the following callback ran. Not strictly necessary
  // since it really should run.
  bool posted_dialog_js = false;
  // Create a callback that will be called during the DidCommitNavigation task.
  WillEnterBackForwardCacheCallbackForTesting
      will_enter_back_forward_cache_callback =
          base::BindLambdaForTesting([&]() {
            // Post a dialog, it should not result in a dialog being created.
            ExecuteScriptAsync(rfh_a, R"(window.alert("alert");)");
            posted_dialog_js = true;
          });
  rfh_a->render_view_host()->SetWillEnterBackForwardCacheCallbackForTesting(
      will_enter_back_forward_cache_callback);

  AppModalDialogWaiter dialog_waiter(shell());

  // Try show another dialog. It should work.
  ExecuteScriptAsync(rfh_a, R"(window.alert("alert");)");
  dialog_waiter.Wait();

  dialog_waiter.Restart();

  // Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImpl* rfh_b = current_frame_host();

  ASSERT_FALSE(delete_observer_rfh_a.deleted());
  ASSERT_THAT(rfh_a, InBackForwardCache());
  ASSERT_NE(rfh_a, rfh_b);
  // Test that the JS was run and that it didn't result in a dialog.
  ASSERT_TRUE(posted_dialog_js);
  ASSERT_FALSE(dialog_waiter.WasDialogRequestedCallbackCalled());

  // Go back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  EXPECT_EQ(rfh_a, current_frame_host());
  EXPECT_FALSE(rfh_a->IsInBackForwardCache());

  // Try show another dialog. It should work.
  ExecuteScriptAsync(rfh_a, R"(window.alert("alert");)");
  dialog_waiter.Wait();
}

// Tests that pagehide handlers of the old RFH are run for bfcached pages even
// if the page is already hidden (and visibilitychange won't run).
// Disabled on Linux and Win because of flakiness, see crbug.com/1170802.
// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       PagehideRunsWhenPageIsHidden) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL url_3(embedded_test_server()->GetURL("a.com", "/title2.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // 1) Navigate to |url_1| and hide the tab.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  RenderFrameHostImplWrapper main_frame_1(web_contents->GetPrimaryMainFrame());
  // We need to set it to Visibility::VISIBLE first in case this is the first
  // time the visibility is updated.
  web_contents->UpdateWebContentsVisibility(Visibility::VISIBLE);
  web_contents->UpdateWebContentsVisibility(Visibility::HIDDEN);
  EXPECT_EQ(Visibility::HIDDEN, web_contents->GetVisibility());

  // Create a pagehide handler that sets item "pagehide_storage" and a
  // visibilitychange handler that sets item "visibilitychange_storage" in
  // localStorage.
  EXPECT_TRUE(ExecJs(main_frame_1.get(),
                     R"(
    localStorage.setItem('pagehide_storage', 'not_dispatched');
    var dispatched_pagehide = false;
    window.onpagehide = function(e) {
      if (dispatched_pagehide) {
        // We shouldn't dispatch pagehide more than once.
        localStorage.setItem('pagehide_storage', 'dispatched_more_than_once');
      } else if (!e.persisted) {
        localStorage.setItem('pagehide_storage', 'wrong_persisted');
      } else {
        localStorage.setItem('pagehide_storage', 'dispatched_once');
      }
      dispatched_pagehide = true;
    }
    localStorage.setItem('visibilitychange_storage', 'not_dispatched');
    document.onvisibilitychange = function(e) {
      localStorage.setItem('visibilitychange_storage',
        'should_not_be_dispatched');
    }
  )"));
  // |visibilitychange_storage| should be set to its initial correct value.
  EXPECT_EQ("not_dispatched",
            GetLocalStorage(main_frame_1.get(), "visibilitychange_storage"));

  // 2) Navigate cross-site to |url_2|. We need to navigate cross-site to make
  // sure we won't run pagehide and visibilitychange during new page's commit,
  // which is tested in ProactivelySwapBrowsingInstancesSameSiteTest.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  // |main_frame_1| should be in the back-forward cache.
  EXPECT_TRUE(main_frame_1->IsInBackForwardCache());

  // 3) Navigate to |url_3| which is same-origin with |url_1|, so we can check
  // the localStorage values.
  EXPECT_TRUE(NavigateToURL(shell(), url_3));
  RenderFrameHostImpl* main_frame_3 = web_contents->GetPrimaryMainFrame();

  // Check that the value for 'pagehide_storage' and 'visibilitychange_storage'
  // are set correctly.
  EXPECT_EQ("dispatched_once",
            GetLocalStorage(main_frame_3, "pagehide_storage"));
  EXPECT_EQ("not_dispatched",
            GetLocalStorage(main_frame_3, "visibilitychange_storage"));
}

// Tests that we're getting the correct TextInputState and focus updates when a
// page enters the back-forward cache and when it gets restored.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, TextInputStateUpdated) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to |url_1| and add a text input with "foo" as the value.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  RenderFrameHostImpl* rfh_1 = current_frame_host();
  EXPECT_TRUE(ExecJs(rfh_1,
                     "document.title='bfcached';"
                     "var input = document.createElement('input');"
                     "input.setAttribute('type', 'text');"
                     "input.setAttribute('value', 'foo');"
                     "document.body.appendChild(input);"
                     "var focusCount = 0;"
                     "var blurCount = 0;"
                     "input.onfocus = () => { focusCount++;};"
                     "input.onblur = () => { blurCount++; };"));

  {
    TextInputManagerTypeObserver type_observer(web_contents(),
                                               ui::TEXT_INPUT_TYPE_TEXT);
    TextInputManagerValueObserver value_observer(web_contents(), "foo");
    // 2) Press tab key to focus the <input>, and verify the type & value.
    SimulateKeyPress(web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, false, false, false);
    type_observer.Wait();
    value_observer.Wait();

    EXPECT_EQ(rfh_1, web_contents()->GetFocusedFrame());
    EXPECT_EQ(EvalJs(rfh_1, "focusCount").ExtractInt(), 1);
    EXPECT_EQ(EvalJs(rfh_1, "blurCount").ExtractInt(), 0);
  }

  {
    TextInputManagerTester tester(web_contents());
    TextInputManagerValueObserver value_observer(web_contents(), "A");
    // 3) Press the "A" key to change the text input value. This should notify
    // the browser that the text input value has changed.
    SimulateKeyPress(web_contents(), ui::DomKey::FromCharacter('A'),
                     ui::DomCode::US_A, ui::VKEY_A, false, false, false, false);
    value_observer.Wait();

    EXPECT_EQ(rfh_1, web_contents()->GetFocusedFrame());
    EXPECT_EQ(EvalJs(rfh_1, "focusCount").ExtractInt(), 1);
    EXPECT_EQ(EvalJs(rfh_1, "blurCount").ExtractInt(), 0);
  }

  {
    TextInputManagerTypeObserver type_observer(web_contents(),
                                               ui::TEXT_INPUT_TYPE_NONE);
    // 4) Navigating to |url_2| should reset type to TEXT_INPUT_TYPE_NONE.
    EXPECT_TRUE(NavigateToURL(shell(), url_2));
    type_observer.Wait();
    // |rfh_1| should get into the back-forward cache.
    EXPECT_TRUE(rfh_1->IsInBackForwardCache());
    EXPECT_EQ(current_frame_host(), web_contents()->GetFocusedFrame());
    EXPECT_NE(rfh_1, web_contents()->GetFocusedFrame());
  }

  {
    // 5) Navigating back to |url_1|, we shouldn't restore the focus to the
    // text input, but |rfh_1| will be focused again as we will restore focus
    // to main frame after navigation.
    ASSERT_TRUE(HistoryGoBack(web_contents()));

    EXPECT_EQ(rfh_1, web_contents()->GetFocusedFrame());
    EXPECT_EQ(EvalJs(rfh_1, "focusCount").ExtractInt(), 1);
    EXPECT_EQ(EvalJs(rfh_1, "blurCount").ExtractInt(), 1);
  }

  {
    TextInputManagerTypeObserver type_observer(web_contents(),
                                               ui::TEXT_INPUT_TYPE_TEXT);
    TextInputManagerValueObserver value_observer(web_contents(), "A");
    // 6) Press tab key to focus the <input> again. Note that we need to press
    // the tab key twice here, because the last "tab focus" point was the
    // <input> element. The first tab key press would focus on the UI/url bar,
    // then the second tab key would go back to the <input>.
    SimulateKeyPress(web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, false, false, false);
    SimulateKeyPress(web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, false, false, false);
    type_observer.Wait();
    value_observer.Wait();

    EXPECT_EQ(rfh_1, web_contents()->GetFocusedFrame());
    EXPECT_EQ(EvalJs(rfh_1, "focusCount").ExtractInt(), 2);
    EXPECT_EQ(EvalJs(rfh_1, "blurCount").ExtractInt(), 1);
  }
}

#if (BUILDFLAG(IS_MAC))
#define MAYBE_SubframeTextInputStateUpdated DISABLED_SubframeTextInputStateUpdated
#else
#define MAYBE_SubframeTextInputStateUpdated SubframeTextInputStateUpdated
#endif
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MAYBE_SubframeTextInputStateUpdated) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_1(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(a))"));
  GURL url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to |url_1| and add a text input with "foo" as the value in the
  // a.com subframe.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameHostImpl* rfh_b = rfh_a->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_subframe_a =
      rfh_b->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(rfh_subframe_a,
                     "var input = document.createElement('input');"
                     "input.setAttribute('type', 'text');"
                     "input.setAttribute('value', 'foo');"
                     "document.body.appendChild(input);"
                     "var focusCount = 0;"
                     "var blurCount = 0;"
                     "input.onfocus = () => { focusCount++;};"
                     "input.onblur = () => { blurCount++; };"));

  {
    TextInputManagerTypeObserver type_observer(web_contents(),
                                               ui::TEXT_INPUT_TYPE_TEXT);
    TextInputManagerValueObserver value_observer(web_contents(), "foo");
    // 2) Press tab key to focus the <input>, and verify the type & value.
    SimulateKeyPress(web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, false, false, false);
    type_observer.Wait();
    value_observer.Wait();

    EXPECT_EQ(rfh_subframe_a, web_contents()->GetFocusedFrame());
    EXPECT_EQ(EvalJs(rfh_subframe_a, "focusCount").ExtractInt(), 1);
    EXPECT_EQ(EvalJs(rfh_subframe_a, "blurCount").ExtractInt(), 0);
  }

  {
    TextInputManagerTester tester(web_contents());
    TextInputManagerValueObserver value_observer(web_contents(), "A");
    // 3) Press the "A" key to change the text input value. This should notify
    // the browser that the text input value has changed.
    SimulateKeyPress(web_contents(), ui::DomKey::FromCharacter('A'),
                     ui::DomCode::US_A, ui::VKEY_A, false, false, false, false);
    value_observer.Wait();

    EXPECT_EQ(rfh_subframe_a, web_contents()->GetFocusedFrame());
    EXPECT_EQ(EvalJs(rfh_subframe_a, "focusCount").ExtractInt(), 1);
    EXPECT_EQ(EvalJs(rfh_subframe_a, "blurCount").ExtractInt(), 0);
  }

  {
    TextInputManagerTypeObserver type_observer(web_contents(),
                                               ui::TEXT_INPUT_TYPE_NONE);
    // 4) Navigating to |url_2| should reset type to TEXT_INPUT_TYPE_NONE and
    // changed focus to the new page's main frame.
    EXPECT_TRUE(NavigateToURL(shell(), url_2));
    type_observer.Wait();

    // |rfh_a| and its subframes should get into the back-forward cache.
    EXPECT_TRUE(rfh_a->IsInBackForwardCache());
    EXPECT_TRUE(rfh_b->IsInBackForwardCache());
    EXPECT_TRUE(rfh_subframe_a->IsInBackForwardCache());
    EXPECT_NE(rfh_subframe_a, web_contents()->GetFocusedFrame());
  }

  {
    // 5) Navigating back to |url_1|, we shouldn't restore the focus to the
    // text input in the subframe (we will focus on the main frame |rfh_a|
    // instead).
    ASSERT_TRUE(HistoryGoBack(web_contents()));

    EXPECT_EQ(rfh_a, web_contents()->GetFocusedFrame());
    EXPECT_EQ(EvalJs(rfh_subframe_a, "focusCount").ExtractInt(), 1);
    EXPECT_EQ(EvalJs(rfh_subframe_a, "blurCount").ExtractInt(), 1);
  }

  {
    TextInputManagerTypeObserver type_observer(web_contents(),
                                               ui::TEXT_INPUT_TYPE_TEXT);
    TextInputManagerValueObserver value_observer(web_contents(), "A");
    // 6) Press tab key to focus the <input> again.
    SimulateKeyPress(web_contents(), ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, false, false, false);
    type_observer.Wait();
    value_observer.Wait();

    EXPECT_EQ(rfh_subframe_a, web_contents()->GetFocusedFrame());
    EXPECT_EQ(EvalJs(rfh_subframe_a, "focusCount").ExtractInt(), 2);
    EXPECT_EQ(EvalJs(rfh_subframe_a, "blurCount").ExtractInt(), 1);
  }
}

// Tests that trying to focus on a BFCached cross-site iframe won't crash.
// See https://crbug.com/1250218.
// TODO(crbug.com/1349657): Flaky on linux tsan
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_FocusSameSiteSubframeOnPagehide \
  DISABLED_FocusSameSiteSubframeOnPagehide
#else
#define MAYBE_FocusSameSiteSubframeOnPagehide FocusSameSiteSubframeOnPagehide
#endif
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MAYBE_FocusSameSiteSubframeOnPagehide) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  GURL main_url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to a page with a same-site iframe.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImplWrapper rfh_1(current_frame_host());
  EXPECT_EQ(rfh_1.get(), web_contents()->GetFocusedFrame());

  // 2) Navigate away from the page while trying to focus the subframe on
  // pagehide. The DidFocusFrame IPC should arrive after the page gets into
  // BFCache and should be ignored by the browser. The focus after navigation
  // should go to the new main frame.
  EXPECT_TRUE(ExecJs(rfh_1.get(), R"(
    window.onpagehide = function(e) {
      document.getElementById("test_iframe").focus();
  })"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url_2));
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());
  EXPECT_NE(rfh_1.get(), web_contents()->GetFocusedFrame());
  EXPECT_EQ(current_frame_host(), web_contents()->GetFocusedFrame());

  // 3) Navigate back to the page. The focus should be on the main frame.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh_1.get(), web_contents()->GetFocusedFrame());
  ExpectRestored(FROM_HERE);
}

// Tests that trying to focus on a BFCached cross-site iframe won't crash.
// See https://crbug.com/1250218.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       FocusCrossSiteSubframeOnPagehide) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL main_url_2(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to a page with a cross-site iframe.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImplWrapper rfh_1(current_frame_host());
  EXPECT_EQ(rfh_1.get(), web_contents()->GetFocusedFrame());

  // 2) Navigate away from the page while trying to focus the subframe on
  // pagehide. The DidFocusFrame IPC should arrive after the page gets into
  // BFCache and should be ignored by the browser. The focus after navigation
  // should go to the new main frame.
  EXPECT_TRUE(ExecJs(rfh_1.get(), R"(
    window.onpagehide = function(e) {
      document.getElementById("child-0").focus();
    })"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url_2));
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());
  EXPECT_NE(rfh_1.get(), web_contents()->GetFocusedFrame());

  // 3) Navigate back to the page. The focus should be on the original page's
  // main frame.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh_1.get(), current_frame_host());
  ExpectRestored(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       MainDocumentCSPHeadersAreRestored) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL(
      "a.com",
      "/set-header?"
      "Content-Security-Policy: frame-src 'none'"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A, which should set CSP.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // Check that CSP was set.
  {
    const std::vector<network::mojom::ContentSecurityPolicyPtr>& root_csp =
        current_frame_host()
            ->policy_container_host()
            ->policies()
            .content_security_policies;
    EXPECT_EQ(1u, root_csp.size());
    EXPECT_EQ("frame-src 'none'", root_csp[0]->header->header_value);
  }

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Navigate back and expect that the CSP headers are present on the main
  // frame.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(rfh_a, current_frame_host());
  ExpectRestored(FROM_HERE);

  // Check that CSP was restored.
  {
    const std::vector<network::mojom::ContentSecurityPolicyPtr>& root_csp =
        current_frame_host()
            ->policy_container_host()
            ->policies()
            .content_security_policies;
    EXPECT_EQ(1u, root_csp.size());
    EXPECT_EQ("frame-src 'none'", root_csp[0]->header->header_value);
  }
}

// Check that sandboxed documents are cached and won't lose their sandbox flags
// after restoration.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, CspSandbox) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(
      embedded_test_server()->GetURL("a.com",
                                     "/set-header?"
                                     "Content-Security-Policy: sandbox"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A, which should set CSP.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  {
    const std::vector<network::mojom::ContentSecurityPolicyPtr>& root_csp =
        current_frame_host()
            ->policy_container_host()
            ->policies()
            .content_security_policies;
    ASSERT_EQ(1u, root_csp.size());
    ASSERT_EQ("sandbox", root_csp[0]->header->header_value);
    ASSERT_EQ(network::mojom::WebSandboxFlags::kAll,
              current_frame_host()->active_sandbox_flags());
  };

  // 2) Navigate to B. Expect the previous RenderFrameHost to enter the bfcache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  {
    const std::vector<network::mojom::ContentSecurityPolicyPtr>& root_csp =
        current_frame_host()
            ->policy_container_host()
            ->policies()
            .content_security_policies;
    ASSERT_EQ(0u, root_csp.size());
    ASSERT_EQ(network::mojom::WebSandboxFlags::kNone,
              current_frame_host()->active_sandbox_flags());
  };

  // 3) Navigate back and expect the page to be restored, with the correct
  // CSP and sandbox flags.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_EQ(current_frame_host(), rfh_a);
  {
    const std::vector<network::mojom::ContentSecurityPolicyPtr>& root_csp =
        current_frame_host()
            ->policy_container_host()
            ->policies()
            .content_security_policies;
    ASSERT_EQ(1u, root_csp.size());
    ASSERT_EQ("sandbox", root_csp[0]->header->header_value);
    ASSERT_EQ(network::mojom::WebSandboxFlags::kAll,
              current_frame_host()->active_sandbox_flags());
  };
}

// Check that about:blank is not cached.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, AboutBlankWillNotBeCached) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to about:blank.
  GURL blank_url(url::kAboutBlankURL);
  EXPECT_TRUE(NavigateToURL(shell(), blank_url));

  // 2) Navigate to a.com.
  GURL url_a(embedded_test_server()->GetURL("a.com", "/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  // 3) Navigate back to about:blank.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // This about:blank document does not have a SiteInstance and then loading a
  // page on it doesn't swap the browsing instance.
  ExpectNotRestored(
      {
          BackForwardCacheMetrics::NotRestoredReason::
              kBrowsingInstanceNotSwapped,
      },
      {}, {ShouldSwapBrowsingInstance::kNo_DoesNotHaveSite}, {}, {}, FROM_HERE);
}

// Check that browsing instances are not swapped when a navigation redirects
// toward the last committed URL and the reasons are recorded correctly.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, RedirectToSelf) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigationControllerImpl& controller = web_contents()->GetController();

  // 1) Navigate to a.com/empty.html.
  GURL url_a(embedded_test_server()->GetURL("a.com", "/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* rfh_a = current_frame_host();
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(url_a, controller.GetLastCommittedEntry()->GetURL());

  // 2) Navigate to the same page by redirection.
  GURL url_a2(embedded_test_server()->GetURL(
      "a.com", "/server-redirect-301?" + url_a.spec()));
  EXPECT_TRUE(NavigateToURL(shell(), url_a2, url_a));
  RenderFrameHostImpl* rfh_b = current_frame_host();
  EXPECT_EQ(2, controller.GetEntryCount());

  EXPECT_FALSE(rfh_a->IsInBackForwardCache());
  EXPECT_TRUE(rfh_a->GetSiteInstance()->IsRelatedSiteInstance(
      rfh_b->GetSiteInstance()));
  EXPECT_EQ(url_a, controller.GetLastCommittedEntry()->GetURL());

  // 3) Navigate back to the previous page.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(url_a, controller.GetLastCommittedEntry()->GetURL());

  // TODO(crbug.com/1198030): Investigate whether these navigation results are
  // expected.

  ExpectNotRestored(
      {
          BackForwardCacheMetrics::NotRestoredReason::
              kBrowsingInstanceNotSwapped,
      },
      {}, {ShouldSwapBrowsingInstance::kNo_SameUrlNavigation}, {}, {},
      FROM_HERE);
}

// Check that reloading doesn't affect the back-forward cache usage.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, ReloadDoesntAffectCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigationControllerImpl& controller = web_contents()->GetController();

  // 1) Navigate to a.com.
  GURL url_a(embedded_test_server()->GetURL("a.com", "/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(url_a, controller.GetLastCommittedEntry()->GetURL());

  // 2) Navigate to b.com.
  GURL url_b(embedded_test_server()->GetURL("b.com", "/empty.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(url_b, controller.GetLastCommittedEntry()->GetURL());

  // 3) Go back to a.com and reload.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(url_a, controller.GetLastCommittedEntry()->GetURL());

  ExpectRestored(FROM_HERE);

  // 4) Reload the tab.
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(url_a, controller.GetLastCommittedEntry()->GetURL());

  // By reloading the tab, ShouldSwapBrowsingInstance::
  // kNo_AlreadyHasMatchingBrowsingInstance is set once. This should be reset
  // when the navigation 4)'s commit finishes and should not prevent putting the
  // page into the back-forward cache.
  //
  // Note that SetBrowsingInstanceSwapResult might not be called for every
  // navigation because we might not get to this point for some navigations,
  // e.g. if the navigation uses a pre-existing RenderFrameHost and SiteInstance
  // for navigation.
  //
  // TODO(crbug.com/1176061): Tie BrowsingInstanceSwapResult to
  // NavigationRequest instead and move the SetBrowsingInstanceSwapResult call
  // for navigations to happen at commit time instead.

  // 5) Go forward to b.com and reload.
  ASSERT_TRUE(HistoryGoForward(web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(url_b, controller.GetLastCommittedEntry()->GetURL());

  // The page loaded at B) is correctly cached and restored. Reloading doesn't
  // affect the cache usage.
  ExpectRestored(FROM_HERE);

  // 6) Go back to a.com.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(url_a, controller.GetLastCommittedEntry()->GetURL());

  // The page loaded at 3) is correctly cached and restored. Reloading doesn't
  // affect the cache usage.
  ExpectRestored(FROM_HERE);
}

// Regression test for crbug.com/1183313. Checks that CommitNavigationParam's
// |has_user_gesture| value reflects the gesture from the latest navigation
// after the commit finished.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheBrowserTest,
    SameDocumentNavAfterRestoringDocumentLoadedWithUserGesture) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a_foo(embedded_test_server()->GetURL("a.com", "/title1.html#foo"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  NavigationControllerImpl& controller = web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Initial navigation (so that we can initiate a navigation from renderer).
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  // 1) Navigate to A with user gesture.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    EXPECT_TRUE(NavigateToURLFromRenderer(shell(), url_a));
    params_capturer.Wait();
    EXPECT_TRUE(params_capturer.has_user_gesture());
    EXPECT_TRUE(root->current_frame_host()
                    ->last_committed_common_params_has_user_gesture());
  }
  RenderFrameHostImpl* rfh_a = current_frame_host();

  // 2) Navigate to B. A should be stored in the back-forward cache.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
  EXPECT_FALSE(root->current_frame_host()
                   ->last_committed_common_params_has_user_gesture());

  // 3) GoBack to A. RenderFrameHost of A should be restored from the
  // back-forward cache, and "has_user_gesture" is set to false correctly.
  // Note that since this is a back-forward cache restore we create the
  // DidCommitProvisionalLoadParams completely in the browser, so we got the
  // correct value from the latest navigation. However, we did not update the
  // renderer's navigation-related values, so the renderer's DocumentLoader
  // still thinks the last "gesture" value is "true", which will get corrected
  // on the next navigation.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    controller.GoBack();
    params_capturer.Wait();
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
    EXPECT_EQ(rfh_a, current_frame_host());
    // The navigation doesn't have user gesture.
    EXPECT_FALSE(params_capturer.has_user_gesture());
    EXPECT_FALSE(root->current_frame_host()
                     ->last_committed_common_params_has_user_gesture());
  }

  // 4) Same-document navigation to A#foo without user gesture. At this point
  // we will update the renderer's DocumentLoader's latest gesture value to
  // "no user gesture", and we'll get the correct gesture value in
  // DidCommitProvisionalLoadParams.
  {
    FrameNavigateParamsCapturer params_capturer(root);
    EXPECT_TRUE(
        NavigateToURLFromRendererWithoutUserGesture(shell(), url_a_foo));
    params_capturer.Wait();
    // The navigation doesn't have user gesture.
    EXPECT_FALSE(params_capturer.has_user_gesture());
    EXPECT_FALSE(root->current_frame_host()
                     ->last_committed_common_params_has_user_gesture());
  }
}

testing::Matcher<BackForwardCacheCanStoreTreeResult> MatchesTreeResult(
    testing::Matcher<bool> same_origin,
    GURL url) {
  return testing::AllOf(
      testing::Property("IsSameOrigin",
                        &BackForwardCacheCanStoreTreeResult::IsSameOrigin,
                        same_origin),
      testing::Property("GetUrl", &BackForwardCacheCanStoreTreeResult::GetUrl,
                        url));
}

RenderFrameHostImpl* ChildFrame(RenderFrameHostImpl* rfh, int child_index) {
  return rfh->child_at(child_index)->current_frame_host();
}

// Verifies that the reasons match those given and no others.
testing::Matcher<BackForwardCacheCanStoreDocumentResult>
BackForwardCacheBrowserTest::MatchesDocumentResult(
    testing::Matcher<NotRestoredReasons> not_stored,
    BlockListedFeatures block_listed) {
  return testing::AllOf(
      testing::Property(
          "not_restored_reasons",
          &BackForwardCacheCanStoreDocumentResult::not_restored_reasons,
          not_stored),
      testing::Property(
          "blocklisted_features",
          &BackForwardCacheCanStoreDocumentResult::blocklisted_features,
          block_listed),
      testing::Property(
          "disabled_reasons",
          &BackForwardCacheCanStoreDocumentResult::disabled_reasons,
          BackForwardCacheCanStoreDocumentResult::DisabledReasonsMap()),
      testing::Property(
          "disallow_activation_reasons",
          &BackForwardCacheCanStoreDocumentResult::disallow_activation_reasons,
          std::set<uint64_t>()));
}

// Check the contents of the BackForwardCacheCanStoreTreeResult of a page.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, TreeResultFeatureUsage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a, b, c)"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to a(a, b, c).
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh(current_frame_host());

  // 2) Add a blocking feature to the main frame A and the sub frame B.
  current_frame_host()
      ->UseDummyStickyBackForwardCacheDisablingFeatureForTesting();
  current_frame_host()
      ->child_at(1)
      ->current_frame_host()
      ->UseDummyStickyBackForwardCacheDisablingFeatureForTesting();

  GURL url_subframe_a = ChildFrame(rfh.get(), 0)->GetLastCommittedURL();
  GURL url_subframe_b = ChildFrame(rfh.get(), 1)->GetLastCommittedURL();
  GURL url_subframe_c = ChildFrame(rfh.get(), 2)->GetLastCommittedURL();

  // 3) Initialize the reasons tree and navigate away to ensure that everything
  // from the old frame has been destroyed.
  BackForwardCacheCanStoreDocumentResultWithTree can_store_result =
      web_contents()
          ->GetController()
          .GetBackForwardCache()
          .GetCurrentBackForwardCacheEligibility(rfh.get());
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  ASSERT_TRUE(rfh.WaitUntilRenderFrameDeleted());

  // 4) Check IsSameOrigin() and GetUrl().
  // a
  EXPECT_THAT(*can_store_result.tree_reasons,
              MatchesTreeResult(/*same_origin=*/true,
                                /*url=*/url_a));
  // a->a
  EXPECT_THAT(*can_store_result.tree_reasons->GetChildren().at(0),
              MatchesTreeResult(/*same_origin=*/true,
                                /*url=*/url_subframe_a));
  // a->b
  EXPECT_THAT(*can_store_result.tree_reasons->GetChildren().at(1),
              MatchesTreeResult(/*same_origin=*/false,
                                /*url=*/url_subframe_b));
  // a->c
  EXPECT_THAT(*can_store_result.tree_reasons->GetChildren().at(2),
              MatchesTreeResult(/*same_origin=*/false,
                                /*url=*/url_subframe_c));

  // 5) Check that the blocking reasons match.
  // a
  EXPECT_THAT(can_store_result.tree_reasons->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons({NotRestoredReason::kBlocklistedFeatures}),
                  BlockListedFeatures(
                      {blink::scheduler::WebSchedulerTrackedFeature::kDummy})));
  // a->a
  EXPECT_THAT(
      can_store_result.tree_reasons->GetChildren().at(0)->GetDocumentResult(),
      MatchesDocumentResult(NotRestoredReasons(),
                            BlockListedFeatures(BlockListedFeatures())));
  // a->b
  EXPECT_THAT(
      can_store_result.tree_reasons->GetChildren().at(1)->GetDocumentResult(),
      MatchesDocumentResult(
          NotRestoredReasons({NotRestoredReason::kBlocklistedFeatures}),
          BlockListedFeatures(
              {blink::scheduler::WebSchedulerTrackedFeature::kDummy})));
  // a->c
  EXPECT_THAT(
      can_store_result.tree_reasons->GetChildren().at(2)->GetDocumentResult(),
      MatchesDocumentResult(NotRestoredReasons(),
                            BlockListedFeatures(BlockListedFeatures())));
}

// Check the contents of the BackForwardCacheCanStoreTreeResult of a page when
// it is evicted.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       TreeResultEvictionMainFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to a.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Navigate to B and evict A by JavaScript execution.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  EvictByJavaScript(rfh_a.get());
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kJavaScriptExecution}, {}, {}, {}, {},
                    FROM_HERE);
  EXPECT_THAT(GetTreeResult()->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons({NotRestoredReason::kJavaScriptExecution}),
                  BlockListedFeatures()));
}

// Check the contents of the BackForwardCacheCanStoreTreeResult of a page when
// its subframe is evicted.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       TreeResultEvictionSubFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  RenderFrameHostImplWrapper rfh_b(
      current_frame_host()->child_at(0)->current_frame_host());
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Navigate to C and evict A's subframe B by JavaScript execution.
  ASSERT_TRUE(NavigateToURL(shell(), url_c));
  EvictByJavaScript(rfh_b.get());
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kJavaScriptExecution}, {}, {}, {}, {},
                    FROM_HERE);
  // Main frame result in the tree is empty.
  EXPECT_THAT(
      GetTreeResult()->GetDocumentResult(),
      MatchesDocumentResult(NotRestoredReasons(), BlockListedFeatures()));
  // Subframe result in the tree contains the reason.
  EXPECT_THAT(GetTreeResult()->GetChildren().at(0)->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons({NotRestoredReason::kJavaScriptExecution}),
                  BlockListedFeatures()));
}

// Check the contents of the BackForwardCacheCanStoreTreeResult of a page when
// its subframe's subframe is evicted.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       TreeResultEvictionSubFramesSubframe) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  GURL url_d(embedded_test_server()->GetURL("d.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());
  RenderFrameHostImplWrapper rfh_c(current_frame_host()
                                       ->child_at(0)
                                       ->current_frame_host()
                                       ->child_at(0)
                                       ->current_frame_host());
  rfh_a->GetBackForwardCacheMetrics()->SetObserverForTesting(this);

  // 2) Navigate to D and evict C by JavaScript execution.
  ASSERT_TRUE(NavigateToURL(shell(), url_d));
  EvictByJavaScript(rfh_c.get());
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // 3) Go back to A.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestored({NotRestoredReason::kJavaScriptExecution}, {}, {}, {}, {},
                    FROM_HERE);
  // Main frame result in the tree is empty.
  EXPECT_THAT(
      GetTreeResult()->GetDocumentResult(),
      MatchesDocumentResult(NotRestoredReasons(), BlockListedFeatures()));
  // The first level subframe result in the tree is empty.
  EXPECT_THAT(
      GetTreeResult()->GetChildren().at(0)->GetDocumentResult(),
      MatchesDocumentResult(NotRestoredReasons(), BlockListedFeatures()));
  // The second level subframe result in the tree contains the reason.
  EXPECT_THAT(GetTreeResult()
                  ->GetChildren()
                  .at(0)
                  ->GetChildren()
                  .at(0)
                  ->GetDocumentResult(),
              MatchesDocumentResult(
                  NotRestoredReasons({NotRestoredReason::kJavaScriptExecution}),
                  BlockListedFeatures()));
}

void BackForwardCacheBrowserTest::InstallUnloadHandlerOnMainFrame() {
  EXPECT_TRUE(ExecJs(current_frame_host(), R"(
      localStorage["unload_run_count"] = 0;
      window.onunload = () => {
        localStorage["unload_run_count"] =
            1 + parseInt(localStorage["unload_run_count"]);
      };
    )"));
  EXPECT_EQ("0", GetUnloadRunCount());
}

void BackForwardCacheBrowserTest::InstallUnloadHandlerOnSubFrame() {
  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecJs(current_frame_host(), R"(
      const iframeElement = document.createElement("iframe");
      iframeElement.src = "%s";
      document.body.appendChild(iframeElement);
    )"));
  navigation_observer.Wait();
  RenderFrameHostImpl* subframe_render_frame_host =
      current_frame_host()->child_at(0)->current_frame_host();
  EXPECT_TRUE(ExecJs(subframe_render_frame_host, R"(
      localStorage["unload_run_count"] = 0;
      window.onunload = () => {
        localStorage["unload_run_count"] =
            1 + parseInt(localStorage["unload_run_count"]);
      };
    )"));
  EXPECT_EQ("0", GetUnloadRunCount());
}

EvalJsResult BackForwardCacheBrowserTest::GetUnloadRunCount() {
  return GetLocalStorage(current_frame_host(), "unload_run_count");
}

bool BackForwardCacheBrowserTest::AddBlocklistedFeature(RenderFrameHost* rfh) {
  // Add kDummy as blocking feature.
  RenderFrameHostImplWrapper rfh_a(rfh);
  rfh_a->UseDummyStickyBackForwardCacheDisablingFeatureForTesting();
  return true;
}

void BackForwardCacheBrowserTest::ExpectNotRestoredDueToBlocklistedFeature(
    base::Location location) {
  ExpectNotRestored({NotRestoredReason::kBlocklistedFeatures},
                    {blink::scheduler::WebSchedulerTrackedFeature::kDummy}, {},
                    {}, {}, location);
}

const ukm::TestAutoSetUkmRecorder& BackForwardCacheBrowserTest::ukm_recorder() {
  return *ukm_recorder_;
}

const base::HistogramTester& BackForwardCacheBrowserTest::histogram_tester() {
  return *histogram_tester_;
}

// Ensure that psges with unload are only allowed to enter back/forward cache by
// default on Android.
IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, UnloadAllowedFlag) {
#if BUILDFLAG(IS_ANDROID)
  ASSERT_TRUE(BackForwardCacheImpl::IsUnloadAllowed());
#else
  ASSERT_FALSE(BackForwardCacheImpl::IsUnloadAllowed());
#endif
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       FrameWithBlocklistedFeatureNotCached) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a page that contains a blocklisted feature.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  RenderFrameHostWrapper rfh(current_frame_host());

  ASSERT_TRUE(AddBlocklistedFeature(rfh.get()));

  // Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // The page with the unsupported feature should be deleted (not cached).
  ASSERT_TRUE(rfh.WaitUntilRenderFrameDeleted());

  // Go back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestoredDueToBlocklistedFeature(FROM_HERE);
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       SubframeWithBlocklistedFeatureNotCached) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a page with an iframe that contains a blocklisted feature.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(b)")));

  RenderFrameHostWrapper rfh(
      current_frame_host()->child_at(0)->current_frame_host());

  ASSERT_TRUE(AddBlocklistedFeature(rfh.get()));

  // Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // The page with the unsupported feature should be deleted (not cached).
  ASSERT_TRUE(rfh.WaitUntilRenderFrameDeleted());

  // Go back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ExpectNotRestoredDueToBlocklistedFeature(FROM_HERE);
}

class BackForwardCacheBrowserUnloadHandlerTest
    : public BackForwardCacheBrowserTest,
      public ::testing::WithParamInterface<std::tuple<bool, TestFrameType>> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
    if (IsUnloadAllowed()) {
      scoped_feature_list_.InitAndEnableFeature(kBackForwardCacheUnloadAllowed);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          kBackForwardCacheUnloadAllowed);
    }
  }

  bool IsUnloadAllowed() { return std::get<0>(GetParam()); }

  TestFrameType GetTestFrameType() { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensure that unload handlers in main frames and subframes block caching or
// not, depending on the flag setting.
IN_PROC_BROWSER_TEST_P(BackForwardCacheBrowserUnloadHandlerTest,
                       UnloadHandlerPresent) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  BackForwardCacheMetrics::NotRestoredReason expected_blocking_reason;
  switch (GetTestFrameType()) {
    case content::TestFrameType::kMainFrame:
      InstallUnloadHandlerOnMainFrame();
      expected_blocking_reason = BackForwardCacheMetrics::NotRestoredReason::
          kUnloadHandlerExistsInMainFrame;
      break;
    case content::TestFrameType::kSubFrame:
      InstallUnloadHandlerOnSubFrame();
      expected_blocking_reason = BackForwardCacheMetrics::NotRestoredReason::
          kUnloadHandlerExistsInSubFrame;
      break;
    default:
      NOTREACHED();
  }

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));

  // 3) Go back.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // Pages with unload handlers are eligible for bfcache only on Android.
  if (BackForwardCacheImpl::IsUnloadAllowed()) {
    ExpectRestored(FROM_HERE);
    EXPECT_EQ("0", GetUnloadRunCount());
  } else {
    ExpectNotRestored({expected_blocking_reason}, {}, {}, {}, {}, FROM_HERE);
    EXPECT_EQ("1", GetUnloadRunCount());
  }

  // 4) Go forward.
  ASSERT_TRUE(HistoryGoForward(web_contents()));

  ExpectRestored(FROM_HERE);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BackForwardCacheBrowserUnloadHandlerTest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(TestFrameType::kMainFrame,
                                         TestFrameType::kSubFrame)));

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest, DisableForRenderFrameHost) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostWrapper rfh_wrapper_a(current_frame_host());

  // 2) Navigate to B.
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostWrapper rfh_wrapper_b(current_frame_host());

  // Regardless of whether the source Id is set or not, it shouldn't affect the
  // result of the BFCache eviction.
  BackForwardCache::DisabledReason test_reason =
      BackForwardCacheDisable::DisabledReason(
          BackForwardCacheDisable::DisabledReasonId::kUnknown);

  // 3) Disable BFCache for A with UKM source Id and go back.
  BackForwardCache::DisableForRenderFrameHost(
      rfh_wrapper_a.get(), test_reason, ukm::UkmRecorder::GetNewSourceID());
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ASSERT_TRUE(rfh_wrapper_a.WaitUntilRenderFrameDeleted());
  // Page A should be evicted properly.
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    {}, {}, {test_reason}, {}, FROM_HERE);

  // 4) Disable BFCache for B without UKM source Id and go forward.
  BackForwardCache::DisableForRenderFrameHost(rfh_wrapper_b.get(), test_reason);
  ASSERT_TRUE(HistoryGoForward(web_contents()));
  ASSERT_TRUE(rfh_wrapper_b.WaitUntilRenderFrameDeleted());
  // Page B should be evicted properly.
  ExpectNotRestored({BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled},
                    {}, {}, {test_reason}, {}, FROM_HERE);
}

namespace {
enum class SubframeType { SameSite, CrossSite };
}

class BackForwardCacheEvictionDueToSubframeNavigationBrowserTest
    : public BackForwardCacheBrowserTest,
      public ::testing::WithParamInterface<SubframeType> {
 public:
  // Provides meaningful param names instead of /0 and /1.
  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    switch (info.param) {
      case SubframeType::SameSite:
        return "SameSite";
      case SubframeType::CrossSite:
        return "CrossSite";
    }
  }
};

IN_PROC_BROWSER_TEST_P(
    BackForwardCacheEvictionDueToSubframeNavigationBrowserTest,
    SubframePendingCommitShouldPreventCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  bool use_cross_origin_subframe = GetParam() == SubframeType::CrossSite;
  GURL subframe_url = embedded_test_server()->GetURL(
      use_cross_origin_subframe ? "b.com" : "a.com", "/title1.html");

  IsolateOriginsForTesting(embedded_test_server(), web_contents(),
                           {"a.com", "b.com"});

  // 1) Navigate to a.com.
  EXPECT_TRUE(NavigateToURL(shell(), a_url));
  RenderFrameHostImpl* main_frame = current_frame_host();

  // 2) Add subframe and wait for empty document to commit.
  CreateSubframe(web_contents(), "child", GURL(""), true);

  CommitMessageDelayer commit_message_delayer(
      web_contents(), subframe_url,
      base::BindLambdaForTesting([&](RenderFrameHost*) {
        // 5) Test that page cannot be stored in bfcache when subframe is
        // pending commit.
        BackForwardCacheCanStoreDocumentResultWithTree can_store_result =
            web_contents()
                ->GetController()
                .GetBackForwardCache()
                .GetCurrentBackForwardCacheEligibility(
                    static_cast<RenderFrameHostImpl*>(main_frame));
        EXPECT_TRUE(can_store_result.flattened_reasons.HasNotRestoredReason(
            BackForwardCacheMetrics::NotRestoredReason::kSubframeIsNavigating));
      }));

  // 3) Start navigation in subframe to |subframe_url|.
  ExecuteScriptAsync(
      main_frame,
      JsReplace("document.querySelector('#child').src = $1;", subframe_url));
  // 4) Wait until subframe navigation is pending commit.
  commit_message_delayer.Wait();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BackForwardCacheEvictionDueToSubframeNavigationBrowserTest,
    ::testing::Values(SubframeType::SameSite, SubframeType::CrossSite),
    &BackForwardCacheEvictionDueToSubframeNavigationBrowserTest::
        DescribeParams);

class BackForwardCacheFencedFrameBrowserTest
    : public BackForwardCacheBrowserTest {
 public:
  BackForwardCacheFencedFrameBrowserTest() = default;
  ~BackForwardCacheFencedFrameBrowserTest() override = default;
  BackForwardCacheFencedFrameBrowserTest(
      const BackForwardCacheFencedFrameBrowserTest&) = delete;

  BackForwardCacheFencedFrameBrowserTest& operator=(
      const BackForwardCacheFencedFrameBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    BackForwardCacheBrowserTest::SetUpCommandLine(command_line);
    fenced_frame_helper_ = std::make_unique<test::FencedFrameTestHelper>();
  }

  test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return *fenced_frame_helper_;
  }

 private:
  std::unique_ptr<test::FencedFrameTestHelper> fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(BackForwardCacheFencedFrameBrowserTest,
                       FencedFramePageNotStoredInBackForwardCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(
      embedded_test_server()->GetURL("b.com", "/fenced_frames/title1.html"));
  GURL url_c(
      embedded_test_server()->GetURL("c.com", "/fenced_frames/title1.html"));

  // 1) Navigate to A.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  // 2) Create a fenced frame.
  content::RenderFrameHostImpl* fenced_frame_host =
      static_cast<content::RenderFrameHostImpl*>(
          fenced_frame_test_helper().CreateFencedFrame(
              web_contents()->GetPrimaryMainFrame(), url_b));
  RenderFrameHostWrapper fenced_frame_host_wrapper(fenced_frame_host);

  // 3) Navigate to C on the fenced frame host.
  fenced_frame_test_helper().NavigateFrameInFencedFrameTree(fenced_frame_host,
                                                            url_c);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  if (!fenced_frame_host_wrapper.IsRenderFrameDeleted())
    EXPECT_FALSE(fenced_frame_host->IsInBackForwardCache());
}

IN_PROC_BROWSER_TEST_F(BackForwardCacheBrowserTest,
                       RendererInitiatedNavigateToSameUrl) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImplWrapper rfh_a(current_frame_host());

  // 2) Navigate to B.
  ASSERT_TRUE(NavigateToURL(shell(), url_b));
  RenderFrameHostImplWrapper rfh_b(current_frame_host());

  // 3) Navigate to B again, renderer initiated.
  ASSERT_TRUE(NavigateToURLFromRenderer(rfh_b.get(), url_b));
  // This is treated as replacement, and RenderFrameHost does not change.
  EXPECT_EQ(rfh_b.get(), current_frame_host());

  // 4) Go back. Make sure we go back to A instead of B and restore from
  // bfcache.
  ASSERT_TRUE(HistoryGoBack(shell()->web_contents()));
  EXPECT_EQ(current_frame_host(), rfh_a.get());
  EXPECT_TRUE(rfh_b.get()->IsInBackForwardCache());
  ExpectRestored(FROM_HERE);

  // 5) Go forward and restore from bfcache.
  ASSERT_TRUE(HistoryGoForward(shell()->web_contents()));
  EXPECT_EQ(current_frame_host(), rfh_b.get());
  ExpectRestored(FROM_HERE);
}

// BEFORE ADDING A NEW TEST HERE
// Read the note at the top about the other files you could add it to.
}  // namespace content
