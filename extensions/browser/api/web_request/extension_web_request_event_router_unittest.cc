// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/extension_web_request_event_router.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/browser/test_event_router.h"
#include "extensions/common/api/web_request/web_request_resource_type.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/url_pattern.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_database.mojom.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

using extension_web_request_api_helpers::ExtraInfoSpec;
using RequestFilter = WebRequestEventRouter::RequestFilter;

::testing::AssertionResult AreFiltersEqual(
    const RequestFilter& value,
    const RequestFilter& expected_value) {
  if (value.tab_id != expected_value.tab_id) {
    return ::testing::AssertionFailure()
           << "tab_id mismatch.\n  Expected: " << expected_value.tab_id
           << "\n  Actual:   " << value.tab_id;
  }

  if (value.window_id != expected_value.window_id) {
    return ::testing::AssertionFailure()
           << "window_id mismatch.\n  Expected: " << expected_value.window_id
           << "\n  Actual:   " << value.window_id;
  }

  if (value.types != expected_value.types) {
    return ::testing::AssertionFailure() << "types mismatch.";
  }

  if (value.urls != expected_value.urls) {
    return ::testing::AssertionFailure()
           << "URLPatternSet mismatch.\n  Expected: " << expected_value.urls
           << "\n  Actual:   " << value.urls;
  }

  return ::testing::AssertionSuccess();
}

class WebRequestEventRouterTest : public testing::Test {
 protected:
  // Helper to easily create a `URLPattern`.
  URLPattern CreatePattern(const std::string& pattern_str) {
    URLPattern pattern(kWebRequestFilterValidSchemes);
    EXPECT_EQ(URLPattern::ParseResult::kSuccess, pattern.Parse(pattern_str))
        << "Failed to parse pattern: " << pattern_str;
    return pattern;
  }

  // Helper to test the serialization and deserialization round trip.
  void TestRoundTrip(const RequestFilter& original) {
    base::DictValue serialized = original.ToValue();

    RequestFilter deserialized;
    std::string error;
    ASSERT_TRUE(deserialized.InitFromValue(serialized, &error))
        << "Deserialization failed with error: " << error;
    EXPECT_TRUE(error.empty());

    EXPECT_TRUE(AreFiltersEqual(deserialized, original));
  }
};

}  // namespace

// Test serialization and deserialization of an empty filter.
TEST_F(WebRequestEventRouterTest, RequestFilter_Empty) {
  RequestFilter filter;

  // An empty filter should serialize to {"urls": []} because `ToValue()`
  // ensures the "urls" key is present.
  base::DictValue expected = base::test::ParseJsonDict(R"({"urls": []})");
  EXPECT_EQ(expected, filter.ToValue());

  TestRoundTrip(filter);
}

// Test serialization and deserialization with only URLs.
TEST_F(WebRequestEventRouterTest, RequestFilter_UrlsOnly) {
  RequestFilter filter;
  filter.urls.AddPattern(CreatePattern("http://example.com/foo*"));
  filter.urls.AddPattern(CreatePattern("*://www.google.com/*"));

  // The order in the serialized list depends on the internal ordering
  // of `URLPattern` (lexicographical sort of the pattern string).
  base::DictValue expected = base::test::ParseJsonDict(R"({
    "urls": [
      "*://www.google.com/*",
      "http://example.com/foo*"
    ]
  })");

  EXPECT_EQ(expected, filter.ToValue());
  TestRoundTrip(filter);
}

// Test serialization and deserialization with resource types.
TEST_F(WebRequestEventRouterTest, RequestFilter_Types) {
  RequestFilter filter;
  filter.types.push_back(WebRequestResourceType::SCRIPT);
  filter.types.push_back(WebRequestResourceType::XHR);
  filter.types.push_back(WebRequestResourceType::MAIN_FRAME);
  filter.types.push_back(WebRequestResourceType::WEB_SOCKET);

  // NOTE: "urls": [] is always added by `ToValue()`.
  base::DictValue expected = base::test::ParseJsonDict(R"({
    "urls": [],
    "types": ["script", "xmlhttprequest", "main_frame", "websocket"]
  })");

  EXPECT_EQ(expected, filter.ToValue());
  TestRoundTrip(filter);
}

// Test serialization and deserialization with IDs.
TEST_F(WebRequestEventRouterTest, RequestFilter_Ids) {
  RequestFilter filter;
  filter.tab_id = 42;
  filter.window_id = 101;

  // NOTE: "urls": [] is always added by `ToValue()`.
  base::DictValue expected = base::test::ParseJsonDict(R"({
    "urls": [],
    "tabId": 42,
    "windowId": 101
  })");

  EXPECT_EQ(expected, filter.ToValue());
  TestRoundTrip(filter);
}

// Test a fully populated filter.
TEST_F(WebRequestEventRouterTest, RequestFilter_Full) {
  RequestFilter filter;
  filter.urls.AddPattern(CreatePattern("https://www.google.com/*"));
  filter.types.push_back(WebRequestResourceType::IMAGE);
  filter.types.push_back(WebRequestResourceType::STYLESHEET);
  filter.tab_id = 42;
  filter.window_id = 101;

  base::DictValue expected = base::test::ParseJsonDict(R"({
    "urls": ["https://www.google.com/*"],
    "types": ["image", "stylesheet"],
    "tabId": 42,
    "windowId": 101
  })");

  EXPECT_EQ(expected, filter.ToValue());
  TestRoundTrip(filter);
}

// Test deserialization failure due to an invalid URL pattern.
TEST_F(WebRequestEventRouterTest, RequestFilter_DeserializeInvalidUrl) {
  base::DictValue input = base::test::ParseJsonDict(R"({
    "urls": ["http://example.com/*", ":::invalid:::"]
  })");

  RequestFilter filter;
  std::string error;
  EXPECT_FALSE(filter.InitFromValue(input, &error));
  EXPECT_EQ("':::invalid:::' is not a valid URL pattern.", error);
}

// Test deserialization failure due to an invalid resource type string.
TEST_F(WebRequestEventRouterTest, RequestFilter_DeserializeInvalidType) {
  base::DictValue input = base::test::ParseJsonDict(R"({
    "urls": [],
    "types": ["script", "bad_type"]
  })");

  RequestFilter filter;
  std::string error;
  EXPECT_FALSE(filter.InitFromValue(input, &error));
}

// Test deserialization failure due to incorrect data types for fields.
TEST_F(WebRequestEventRouterTest, RequestFilter_DeserializeWrongDataType) {
  RequestFilter filter;
  std::string error;

  // Case 1: "urls" is not a list.
  base::DictValue input_urls =
      base::test::ParseJsonDict(R"({"urls": "string"})");
  EXPECT_FALSE(filter.InitFromValue(input_urls, &error));

  // Case 2: "types" is not a list.
  base::DictValue input_types =
      base::test::ParseJsonDict(R"({"urls":[], "types": 123})");
  EXPECT_FALSE(filter.InitFromValue(input_types, &error));

  // Case 3: "tabId" is not an int.
  base::DictValue input_tabid =
      base::test::ParseJsonDict(R"({"urls":[], "tabId": "string"})");
  EXPECT_FALSE(filter.InitFromValue(input_tabid, &error));

  // Case 4: "windowId" is not an int.
  base::DictValue input_windowid =
      base::test::ParseJsonDict(R"({"urls":[], "windowId": "string"})");
  EXPECT_FALSE(filter.InitFromValue(input_windowid, &error));
}

// Test deserialization behavior when "urls" is missing.
TEST_F(WebRequestEventRouterTest, RequestFilter_DeserializeMissingUrls) {
  RequestFilter filter;
  std::string error;

  // Case 1: Empty dictionary.
  base::DictValue input_empty = base::test::ParseJsonDict(R"({})");
  EXPECT_FALSE(filter.InitFromValue(input_empty, &error));

  // Case 2: Dictionary with other keys but missing "urls".
  base::DictValue input_missing =
      base::test::ParseJsonDict(R"({"types": ["script"]})");
  RequestFilter filter_missing;
  EXPECT_FALSE(filter_missing.InitFromValue(input_missing, &error));
}

namespace {

// Captures the events that per-context dispatch sends through the
// extension-scoped EventRouter, instead of delivering them to a renderer.
// Tests inspect the captured events and run their completion callbacks.
class CapturingEventRouter : public TestEventRouter {
 public:
  explicit CapturingEventRouter(content::BrowserContext* context)
      : TestEventRouter(context) {}

  // TestEventRouter:
  void DispatchEventToExtension(const ExtensionId& extension_id,
                                std::unique_ptr<Event> event) override {
    events_.push_back(std::move(event));
  }

  std::vector<std::unique_ptr<Event>>& events() { return events_; }

 private:
  std::vector<std::unique_ptr<Event>> events_;
};

// Builds a browser-initiated main-frame navigation request.
std::unique_ptr<WebRequestInfo> CreateRequest(uint64_t request_id) {
  WebRequestInfoInitParams params;
  params.id = request_id;
  params.url = GURL("http://example.com/");
  params.method = "GET";
  params.is_navigation_request = true;
  params.web_request_type = WebRequestResourceType::MAIN_FRAME;
  params.is_async = true;
  auto request = std::make_unique<WebRequestInfo>(std::move(params));
  request->dnr_actions.emplace();
  return request;
}

class WebRequestEventRouterContextDispatchTest : public ExtensionsTest {
 public:
  static constexpr const char* kExtensionName = "Test Extension";
  static constexpr const char* kEventName =
      extension_web_request_api_constants::kOnBeforeSendHeadersEvent;
  static constexpr int kWorkerThreadId = 12;
  static constexpr int64_t kServiceWorkerVersionId = 7;

  WebRequestEventRouterContextDispatchTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kWebRequestPerContextEventDispatch);
  }

  void SetUp() override {
    ExtensionsTest::SetUp();
    event_router_ =
        CreateAndUseTestEventRouter<CapturingEventRouter>(browser_context());
    render_process_host_ =
        std::make_unique<content::MockRenderProcessHost>(browser_context());

    extension_ = BuildExtension();
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension_);
    ProcessMap::Get(browser_context())
        ->Insert(extension_->id(), render_process_host_->GetID());
  }

  void TearDown() override {
    event_router_ = nullptr;
    render_process_host_.reset();
    ExtensionsTest::TearDown();
  }

 protected:
  // Builds the extension under test. Subclasses override this to change the
  // manifest.
  virtual scoped_refptr<const Extension> BuildExtension() {
    return ExtensionBuilder(kExtensionName)
        .AddHostPermission("<all_urls>")
        .Build();
  }

  WebRequestEventRouter* router() {
    return WebRequestEventRouter::Get(browser_context());
  }

  int process_id() { return render_process_host_->GetID().GetUnsafeValue(); }

  content::MockRenderProcessHost* process() {
    return render_process_host_.get();
  }

  const ExtensionId& extension_id() { return extension_->id(); }

  size_t GetListenerCount() {
    return router()->GetListenerCountForTesting(browser_context(), kEventName);
  }

  bool AddListener(const std::string& url_pattern,
                   int extra_info_spec = 0,
                   bool is_lazy = false) {
    int render_process_id = is_lazy ? -1 : process_id();
    int worker_thread_id = is_lazy ? kMainThreadId : kWorkerThreadId;
    int64_t service_worker_version_id =
        is_lazy ? blink::mojom::kInvalidServiceWorkerVersionId
                : kServiceWorkerVersionId;
    return router()->AddEventListener(
        browser_context(), extension_id(), kExtensionName, kEventName,
        /*sub_event_name=*/kEventName, MakeFilter(url_pattern), extra_info_spec,
        render_process_id, /*web_view_instance_id=*/0, worker_thread_id,
        service_worker_version_id, is_lazy);
  }

  void RemoveListener(const std::string& url_pattern, int extra_info_spec = 0) {
    RequestFilter filter = MakeFilter(url_pattern);
    router()->RemoveLazyListenerForTesting(browser_context(), extension_id(),
                                           kEventName, &filter,
                                           extra_info_spec);
  }

  bool AddWebViewListener(const std::string& url_pattern,
                          int web_view_instance_id,
                          int extra_info_spec = 0) {
    return router()->AddEventListener(
        browser_context(), extension_id(), kExtensionName, kEventName,
        /*sub_event_name=*/kEventName, MakeFilter(url_pattern), extra_info_spec,
        process_id(), web_view_instance_id, kMainThreadId,
        blink::mojom::kInvalidServiceWorkerVersionId, /*is_lazy=*/false);
  }

  void RemoveWebViewListener(const std::string& url_pattern,
                             int web_view_instance_id,
                             int extra_info_spec = 0) {
    RequestFilter filter = MakeFilter(url_pattern);
    router()->UpdateActiveListenerForTesting(
        browser_context(), WebRequestEventRouter::ListenerUpdateType::kRemove,
        extension_id(), kEventName, render_process_host_->GetID(),
        kMainThreadId, blink::mojom::kInvalidServiceWorkerVersionId, &filter,
        extra_info_spec, web_view_instance_id);
  }

  std::vector<std::unique_ptr<Event>>& dispatched_events() {
    return event_router_->events();
  }

  // The concrete identity of the active listener that `AddListener()`
  // registers.
  Event::DispatchTarget ActiveDispatchTarget() {
    return {render_process_host_->GetID(), kWorkerThreadId,
            kServiceWorkerVersionId};
  }

  // Starts the blocking "onBeforeSendHeaders" stage.
  int StartOnBeforeSendHeaders(const WebRequestInfo* request) {
    return router()->OnBeforeSendHeaders(
        browser_context(), request,
        base::BindLambdaForTesting(
            [this](const std::set<std::string>& removed_headers,
                   const std::set<std::string>& set_headers, int error_code) {
              removed_headers_ = removed_headers;
              set_headers_ = set_headers;
              result_ = error_code;
            }),
        &headers_);
  }

  // Simulates one blocking listener response from the target, delivered under
  // the target's concrete identity. A woken lazy context also responds with
  // concrete IDs.
  void RespondWithCancel(uint64_t request_id) {
    auto response = std::make_unique<WebRequestEventRouter::EventResponse>(
        extension_id(), base::Time::Now());
    response->cancel = true;
    router()->OnEventHandledForTarget(
        browser_context(), extension_id(), kEventName, request_id,
        render_process_host_->GetID(), /*web_view_instance_id=*/0,
        kWorkerThreadId, kServiceWorkerVersionId, ExtraInfoSpec::BLOCKING,
        std::move(response));
  }

  // Simulates the target's completion signal, sent after all matching
  // listeners in the context have run.
  void FinishHandling(uint64_t request_id) {
    router()->OnEventHandlingDone(browser_context(), extension_id(), kEventName,
                                  request_id, render_process_host_->GetID(),
                                  /*web_view_instance_id=*/0, kWorkerThreadId,
                                  kServiceWorkerVersionId);
  }

  // Fires the worker-stop teardown signal, as the ProcessManager would when the
  // worker stops.
  void SimulateWorkerStopped() {
    static_cast<ProcessManagerObserver*>(router())
        ->OnStoppedTrackingServiceWorkerInstance(
            *browser_context(),
            WorkerId(extension_id(), render_process_host_->GetID(),
                     kServiceWorkerVersionId, kWorkerThreadId));
  }

  net::HttpRequestHeaders headers_;
  std::optional<int> result_;
  std::set<std::string> removed_headers_;
  std::set<std::string> set_headers_;

 private:
  RequestFilter MakeFilter(const std::string& url_pattern) {
    RequestFilter filter;
    filter.urls.AddPattern(
        URLPattern(kWebRequestFilterValidSchemes, url_pattern));
    return filter;
  }

  base::test::ScopedFeatureList feature_list_;
  ExtensionsAPIClient api_client_;
  raw_ptr<CapturingEventRouter> event_router_ = nullptr;
  std::unique_ptr<content::MockRenderProcessHost> render_process_host_;
  scoped_refptr<const Extension> extension_;
};

}  // namespace

// Multiple parent named listeners from the same context are distinguished by
// their (filter, extra_info_spec) pair; an exact duplicate is rejected.
TEST_F(WebRequestEventRouterContextDispatchTest, RegistrationIdentity) {
  EXPECT_TRUE(AddListener("http://example.com/*"));
  EXPECT_EQ(1u, GetListenerCount());

  // An exact duplicate (same filter and spec) is rejected.
  EXPECT_FALSE(AddListener("http://example.com/*"));
  EXPECT_EQ(1u, GetListenerCount());

  // Same filter with a different spec is a distinct registration.
  EXPECT_TRUE(AddListener("http://example.com/*", ExtraInfoSpec::BLOCKING));
  EXPECT_EQ(2u, GetListenerCount());

  // A different filter is a distinct registration.
  EXPECT_TRUE(AddListener("http://other.example/*"));
  EXPECT_EQ(3u, GetListenerCount());
}

// Removing a listener and re-adding an identical one. Parent named removals
// are processed synchronously (see `WebRequestAPI::OnListenerRemoved()`), so
// the re-add is processed after the removal and registers fresh.
TEST_F(WebRequestEventRouterContextDispatchTest, RemoveThenReAddListener) {
  ASSERT_TRUE(AddListener("http://example.com/*"));
  ASSERT_EQ(1u, GetListenerCount());

  // The removal runs synchronously at notification time...
  RemoveListener("http://example.com/*");
  EXPECT_EQ(0u, GetListenerCount());

  // ...so the re-add of an identical listener registers fresh.
  ASSERT_TRUE(AddListener("http://example.com/*"));
  EXPECT_EQ(1u, GetListenerCount());
}

// Removing a parent-named listener only removes the registration matching the
// provided (filter, extra_info_spec) pair.
TEST_F(WebRequestEventRouterContextDispatchTest, RemovalNarrowedByFilter) {
  ASSERT_TRUE(AddListener("http://example.com/*"));
  ASSERT_TRUE(AddListener("http://other.example/*", ExtraInfoSpec::BLOCKING));
  ASSERT_EQ(2u, GetListenerCount());

  // Removing with a non-matching spec is a no-op.
  RemoveListener("http://example.com/*", ExtraInfoSpec::BLOCKING);
  EXPECT_EQ(2u, GetListenerCount());

  // Removing with the matching (filter, spec) removes exactly one.
  RemoveListener("http://example.com/*");
  EXPECT_EQ(1u, GetListenerCount());

  RemoveListener("http://other.example/*", ExtraInfoSpec::BLOCKING);
  EXPECT_EQ(0u, GetListenerCount());
}

// Under the same parent event name, two webviews of one embedder process
// can hold identical registrations, distinguished only by their
// `web_view_instance_id`.
TEST_F(WebRequestEventRouterContextDispatchTest,
       WebViewRemovalNarrowedByInstanceId) {
  ASSERT_TRUE(
      AddWebViewListener("http://example.com/*", /*web_view_instance_id=*/1));
  ASSERT_TRUE(
      AddWebViewListener("http://example.com/*", /*web_view_instance_id=*/2));
  EXPECT_EQ(2u, GetListenerCount());

  // An exact duplicate for the same webview is rejected.
  EXPECT_FALSE(
      AddWebViewListener("http://example.com/*", /*web_view_instance_id=*/1));
  EXPECT_EQ(2u, GetListenerCount());

  // Removing webview 1's registration keeps webview 2's identical one.
  RemoveWebViewListener("http://example.com/*", /*web_view_instance_id=*/1);
  EXPECT_EQ(1u, GetListenerCount());

  // Repeating the removal is a no-op.
  RemoveWebViewListener("http://example.com/*", /*web_view_instance_id=*/1);
  EXPECT_EQ(1u, GetListenerCount());

  RemoveWebViewListener("http://example.com/*", /*web_view_instance_id=*/2);
  EXPECT_EQ(0u, GetListenerCount());
}

// Verifies that when a service worker wakes up and activates one of its lazy
// listeners, sibling lazy registrations for the same parent event
// ("onBeforeSendHeaders") are preserved. In legacy sub-event naming, leftover
// inactive records under the same name were purged as stale; under parent
// naming, those records represent distinct, valid sibling listeners.
TEST_F(WebRequestEventRouterContextDispatchTest,
       LazyActivationKeepsSiblingRegistrations) {
  // Simulate an extension with two distinct lazy registrations.
  ASSERT_TRUE(AddListener("http://example.com/*", 0, /*is_lazy=*/true));
  ASSERT_TRUE(AddListener("http://other.example/*", ExtraInfoSpec::BLOCKING,
                          /*is_lazy=*/true));
  EXPECT_EQ(2u,
            router()->GetInactiveListenerCount(browser_context(), kEventName));

  // Simulate a service worker activating only one of its listeners.
  ASSERT_TRUE(AddListener("http://example.com/*"));

  // Verify that one listener transitioned to active, while its sibling remains
  // lazy.
  EXPECT_EQ(1u, GetListenerCount());
  EXPECT_EQ(1u,
            router()->GetInactiveListenerCount(browser_context(), kEventName));
}

// A response resolves an active dispatch target.
TEST_F(WebRequestEventRouterContextDispatchTest,
       ActiveTargetResolvedByResponse) {
  ASSERT_TRUE(AddListener("http://example.com/*", ExtraInfoSpec::BLOCKING));

  // The blocking listener makes its target a blocking source, so the request
  // must wait for the target's resolution.
  std::unique_ptr<WebRequestInfo> request = CreateRequest(1);
  ASSERT_EQ(net::ERR_IO_PENDING, StartOnBeforeSendHeaders(request.get()));

  // The target receives one event, addressed to the listener's concrete
  // renderer identity.
  ASSERT_EQ(1u, dispatched_events().size());
  const Event& event = *dispatched_events()[0];
  EXPECT_EQ(kEventName, event.event_name);
  ASSERT_TRUE(event.restrict_to_dispatch_target.has_value());
  EXPECT_EQ(ActiveDispatchTarget(), *event.restrict_to_dispatch_target);

  // A listener response only records a delta. Other matching listeners in the
  // context may still be running, so the target stays pending and the request
  // stays blocked.
  RespondWithCancel(request->id);
  EXPECT_FALSE(result_.has_value());

  // The completion signal resolves the target: the recorded cancel applies
  // and the request unblocks.
  FinishHandling(request->id);
  ASSERT_TRUE(result_.has_value());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, *result_);
}

// A lazy dispatch target wakes up, and its concrete response resolves it.
TEST_F(WebRequestEventRouterContextDispatchTest,
       LazyTargetResolvedByConcreteResponse) {
  ASSERT_TRUE(AddListener("http://example.com/*", ExtraInfoSpec::BLOCKING,
                          /*is_lazy=*/true));

  std::unique_ptr<WebRequestInfo> request = CreateRequest(1);
  ASSERT_EQ(net::ERR_IO_PENDING, StartOnBeforeSendHeaders(request.get()));

  // A stopped context has no renderer identity at dispatch time, so the event
  // goes out under the target's lazy key. This dispatch is what wakes the
  // context.
  ASSERT_EQ(1u, dispatched_events().size());
  const Event& event = *dispatched_events()[0];
  ASSERT_TRUE(event.restrict_to_dispatch_target.has_value());
  EXPECT_TRUE(event.restrict_to_dispatch_target->IsLazy());

  // The woken context responds under its concrete identity, which never
  // equals the lazy key the pending target was recorded under. The response
  // lookup must fall back to the pending lazy entry, and must not resolve it
  // yet.
  RespondWithCancel(request->id);
  EXPECT_FALSE(result_.has_value());

  // The completion signal, also under the concrete identity, resolves the
  // lazy target and applies the recorded cancel.
  FinishHandling(request->id);
  ASSERT_TRUE(result_.has_value());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, *result_);
}

// A lazy dispatch target whose context cannot start resolves empty.
TEST_F(WebRequestEventRouterContextDispatchTest,
       LazyTargetCannotStartResolvesEmpty) {
  ASSERT_TRUE(AddListener("http://example.com/*", ExtraInfoSpec::BLOCKING,
                          /*is_lazy=*/true));

  std::unique_ptr<WebRequestInfo> request = CreateRequest(1);
  ASSERT_EQ(net::ERR_IO_PENDING, StartOnBeforeSendHeaders(request.get()));

  // A blocking target's event must carry a cannot-dispatch callback; without
  // it, a failed dispatch would block the request forever.
  ASSERT_EQ(1u, dispatched_events().size());
  const Event& event = *dispatched_events()[0];
  ASSERT_TRUE(event.cannot_dispatch_callback);

  // Simulate EventRouter reporting the failed dispatch.
  event.cannot_dispatch_callback.Run();

  // The target resolves with no response deltas: the request unblocks and
  // proceeds unmodified.
  ASSERT_TRUE(result_.has_value());
  EXPECT_EQ(net::OK, *result_);
  EXPECT_TRUE(removed_headers_.empty());
  EXPECT_TRUE(set_headers_.empty());
}

// Stopping the extension's service worker resolves the pending targets
// recorded under the worker's concrete identity.
TEST_F(WebRequestEventRouterContextDispatchTest, WorkerStopResolvesTarget) {
  ASSERT_TRUE(AddListener("http://example.com/*", ExtraInfoSpec::BLOCKING));

  std::unique_ptr<WebRequestInfo> request = CreateRequest(1);
  ASSERT_EQ(net::ERR_IO_PENDING, StartOnBeforeSendHeaders(request.get()));

  // The worker stops before responding; the target resolves with no responses
  // and the request proceeds unmodified.
  SimulateWorkerStopped();
  ASSERT_TRUE(result_.has_value());
  EXPECT_EQ(net::OK, *result_);

  // A late response from the stopped worker (e.g. still in flight when the
  // stop was processed) is ignored.
  RespondWithCancel(request->id);
  FinishHandling(request->id);
  EXPECT_EQ(net::OK, *result_);
}

// Stopping the extension's service worker also resolves a pending target
// still recorded under the extension's lazy key.
TEST_F(WebRequestEventRouterContextDispatchTest, WorkerStopResolvesLazyTarget) {
  ASSERT_TRUE(AddListener("http://example.com/*", ExtraInfoSpec::BLOCKING,
                          /*is_lazy=*/true));

  std::unique_ptr<WebRequestInfo> request = CreateRequest(1);
  ASSERT_EQ(net::ERR_IO_PENDING, StartOnBeforeSendHeaders(request.get()));

  // The woken worker dies before re-registering or responding. The stop
  // signal resolves the lazy target synchronously, before any queued
  // dispatch-failure fallback gets a chance to run.
  SimulateWorkerStopped();
  ASSERT_TRUE(result_.has_value());
  EXPECT_EQ(net::OK, *result_);
}

// A render process going away resolves every pending target it hosts.
TEST_F(WebRequestEventRouterContextDispatchTest, ProcessExitResolvesTarget) {
  ASSERT_TRUE(AddListener("http://example.com/*", ExtraInfoSpec::BLOCKING));

  std::unique_ptr<WebRequestInfo> request = CreateRequest(1);
  ASSERT_EQ(net::ERR_IO_PENDING, StartOnBeforeSendHeaders(request.get()));

  process()->SimulateCrash();
  ASSERT_TRUE(result_.has_value());
  EXPECT_EQ(net::OK, *result_);
}

// Tests that when a request matches both a started worker's concrete identity
// and a lazy listener for the same extension (e.g. while a service worker is
// in the middle of executing its top-level script), the targets are folded
// into a single dispatch.
TEST_F(WebRequestEventRouterContextDispatchTest,
       LazyGroupFoldsIntoStartedWorker) {
  // Register a concrete listener for the active worker.
  ASSERT_TRUE(AddListener("http://example.com/*", ExtraInfoSpec::BLOCKING));
  // Simulate mid-startup: a second listener remains under the lazy key.
  ASSERT_TRUE(AddListener("http://*/*", ExtraInfoSpec::BLOCKING,
                          /*is_lazy=*/true));

  std::unique_ptr<WebRequestInfo> request = CreateRequest(1);
  ASSERT_EQ(net::ERR_IO_PENDING, StartOnBeforeSendHeaders(request.get()));

  // Verify only 1 event was dispatched to the concrete target.
  ASSERT_EQ(1u, dispatched_events().size());
  const Event& event = *dispatched_events()[0];
  ASSERT_TRUE(event.restrict_to_dispatch_target.has_value());
  EXPECT_EQ(ActiveDispatchTarget(), *event.restrict_to_dispatch_target);

  RespondWithCancel(request->id);
  EXPECT_FALSE(result_.has_value());

  // Verify one completion signal unblocks the request. Without folding,
  // a dangling lazy target would leave the request blocked after this call.
  FinishHandling(request->id);
  ASSERT_TRUE(result_.has_value());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, *result_);
}

// Tests for blocked requests that cross between the regular context and its
// off-the-record context. A spanning-mode extension has its listeners in the
// regular context but observes and blocks requests from both.
// TODO(andreaorru): the level of simulation we need here in these unittests may
// be fragile. Rewrite these as browser tests with "real" extensions.
class WebRequestEventRouterCrossBrowserContextTest : public ExtensionsTest {
 public:
  static constexpr int kRenderProcessId = 1;

  WebRequestEventRouterCrossBrowserContextTest() {
    feature_list_.InitAndDisableFeature(
        extensions_features::kWebRequestPerContextEventDispatch);
  }

 protected:
  void SetUp() override {
    ExtensionsTest::SetUp();

    user_prefs::UserPrefs::Set(browser_context(), pref_service());
    user_prefs::UserPrefs::Set(incognito_context(), pref_service());

    extension_ = ExtensionBuilder("Spanning Extension")
                     .AddHostPermission("<all_urls>")
                     .Build();
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension_);
    ExtensionPrefs::Get(browser_context())
        ->SetIsIncognitoEnabled(extension_->id(), true);

    WebRequestEventRouter::OnOTRBrowserContextCreated(browser_context(),
                                                      incognito_context());
  }

  void TearDown() override {
    if (!otr_destroyed_) {
      DestroyOTRContext();
    }
    ExtensionsTest::TearDown();
  }

  // Simulates the destruction notification for the off-the-record context.
  void DestroyOTRContext() {
    WebRequestEventRouter::OnOTRBrowserContextDestroyed(browser_context(),
                                                        incognito_context());
    otr_destroyed_ = true;
  }

  WebRequestEventRouter* router() {
    return WebRequestEventRouter::Get(browser_context());
  }

  std::string event_name() {
    return extension_web_request_api_constants::kOnBeforeSendHeadersEvent;
  }

  std::string sub_event_name() { return event_name() + "/1"; }

  // Registers a blocking listener for the extension in `listener_context`.
  void AddBlockingListener(content::BrowserContext* listener_context) {
    ASSERT_TRUE(router()->AddEventListener(
        listener_context, extension_->id(), extension_->name(), event_name(),
        sub_event_name(), WebRequestEventRouter::RequestFilter(),
        ExtraInfoSpec::BLOCKING, kRenderProcessId, /*web_view_instance_id=*/0,
        kMainThreadId, blink::mojom::kInvalidServiceWorkerVersionId,
        /*is_lazy=*/false));
  }

  // Removes the listener that `AddBlockingListener()` registered.
  void RemoveBlockingListener(content::BrowserContext* listener_context) {
    router()->UpdateActiveListenerForTesting(
        listener_context, WebRequestEventRouter::ListenerUpdateType::kRemove,
        extension_->id(), sub_event_name(),
        content::ChildProcessId(kRenderProcessId), kMainThreadId,
        blink::mojom::kInvalidServiceWorkerVersionId);
  }

  // Starts the blocking "onBeforeSendHeaders" stage for `request` in
  // `context`. The completion callback stores its error code in `result_`.
  int StartOnBeforeSendHeaders(content::BrowserContext* context,
                               const WebRequestInfo* request) {
    return router()->OnBeforeSendHeaders(
        context, request,
        base::BindLambdaForTesting(
            [this](const std::set<std::string>& removed_headers,
                   const std::set<std::string>& set_headers,
                   int error_code) { result_ = error_code; }),
        &headers_);
  }

  // Simulates the extension's response from the regular context, where the
  // spanning extension's renderer lives.
  void Respond(uint64_t request_id, bool cancel) {
    auto response = std::make_unique<WebRequestEventRouter::EventResponse>(
        extension_->id(), base::Time::Now());
    response->cancel = cancel;
    router()->OnEventHandled(browser_context(), extension_->id(), event_name(),
                             sub_event_name(), request_id, kRenderProcessId,
                             /*web_view_instance_id=*/0, kMainThreadId,
                             blink::mojom::kInvalidServiceWorkerVersionId,
                             std::move(response));
  }

  std::optional<int> result_;

 private:
  base::test::ScopedFeatureList feature_list_;
  ExtensionsAPIClient api_client_;
  scoped_refptr<const Extension> extension_;
  net::HttpRequestHeaders headers_;
  bool otr_destroyed_ = false;
};

// A blocked request from the off-the-record context must be resolved by the
// spanning extension's response, which arrives with the regular context.
TEST_F(WebRequestEventRouterCrossBrowserContextTest,
       CrossContextBlockedRequestHandled) {
  // Block an incognito request on a listener in the regular context.
  AddBlockingListener(browser_context());
  std::unique_ptr<WebRequestInfo> request = CreateRequest(1);
  EXPECT_EQ(net::ERR_IO_PENDING,
            StartOnBeforeSendHeaders(incognito_context(), request.get()));

  // The blocked request belongs to the off-the-record context.
  EXPECT_EQ(1u,
            router()->GetBlockedRequestCountForTesting(incognito_context()));
  EXPECT_EQ(0u, router()->GetBlockedRequestCountForTesting(browser_context()));

  // The extension cancels the request; its response arrives with the regular
  // context and must resolve the entry owned by incognito.
  Respond(/*request_id=*/1, /*cancel=*/true);
  ASSERT_TRUE(result_.has_value());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, *result_);
  EXPECT_EQ(0u,
            router()->GetBlockedRequestCountForTesting(incognito_context()));

  router()->OnRequestWillBeDestroyed(incognito_context(), request.get());
}

// Removal of the blocking listener (in the regular context) must unblock the
// pending off-the-record request.
TEST_F(WebRequestEventRouterCrossBrowserContextTest,
       CrossContextBlockedRequestListenerRemoved) {
  AddBlockingListener(browser_context());
  std::unique_ptr<WebRequestInfo> request = CreateRequest(1);
  EXPECT_EQ(net::ERR_IO_PENDING,
            StartOnBeforeSendHeaders(incognito_context(), request.get()));

  // Removing the only blocking listener must resolve the request, with no
  // changes applied.
  RemoveBlockingListener(browser_context());

  ASSERT_TRUE(result_.has_value());
  EXPECT_EQ(net::OK, *result_);
  EXPECT_EQ(0u,
            router()->GetBlockedRequestCountForTesting(incognito_context()));

  router()->OnRequestWillBeDestroyed(incognito_context(), request.get());
}

// Each blocked request is owned by exactly the BrowserContext its request
// came from.
TEST_F(WebRequestEventRouterCrossBrowserContextTest,
       BlockedRequestsAreIsolatedPerContext) {
  // Block one request from each context on the same listener.
  AddBlockingListener(browser_context());
  std::unique_ptr<WebRequestInfo> regular_request = CreateRequest(1);
  std::unique_ptr<WebRequestInfo> otr_request = CreateRequest(2);
  EXPECT_EQ(net::ERR_IO_PENDING,
            StartOnBeforeSendHeaders(browser_context(), regular_request.get()));
  EXPECT_EQ(net::ERR_IO_PENDING,
            StartOnBeforeSendHeaders(incognito_context(), otr_request.get()));

  // Each context owns exactly its own blocked request.
  EXPECT_EQ(1u, router()->GetBlockedRequestCountForTesting(browser_context()));
  EXPECT_EQ(1u,
            router()->GetBlockedRequestCountForTesting(incognito_context()));

  // Resolving the regular request must not touch the incognito one.
  Respond(/*request_id=*/1, /*cancel=*/false);
  ASSERT_TRUE(result_.has_value());
  EXPECT_EQ(net::OK, *result_);
  EXPECT_EQ(0u, router()->GetBlockedRequestCountForTesting(browser_context()));
  EXPECT_EQ(1u,
            router()->GetBlockedRequestCountForTesting(incognito_context()));

  // Resolving the incognito request removes its entry.
  result_.reset();
  Respond(/*request_id=*/2, /*cancel=*/false);
  ASSERT_TRUE(result_.has_value());
  EXPECT_EQ(net::OK, *result_);
  EXPECT_EQ(0u,
            router()->GetBlockedRequestCountForTesting(incognito_context()));

  router()->OnRequestWillBeDestroyed(browser_context(), regular_request.get());
  router()->OnRequestWillBeDestroyed(incognito_context(), otr_request.get());
}

// Shutdown of the off-the-record context must drop exactly its own blocked
// requests. A late response for a dropped request is a no-op.
TEST_F(WebRequestEventRouterCrossBrowserContextTest,
       BlockedRequestClearedOnOTRShutdown) {
  AddBlockingListener(browser_context());
  std::unique_ptr<WebRequestInfo> regular_request = CreateRequest(1);
  std::unique_ptr<WebRequestInfo> otr_request = CreateRequest(2);
  EXPECT_EQ(net::ERR_IO_PENDING,
            StartOnBeforeSendHeaders(browser_context(), regular_request.get()));
  EXPECT_EQ(net::ERR_IO_PENDING,
            StartOnBeforeSendHeaders(incognito_context(), otr_request.get()));

  // Destroying the off-the-record context drops its blocked request and only
  // that one.
  DestroyOTRContext();
  EXPECT_EQ(0u,
            router()->GetBlockedRequestCountForTesting(incognito_context()));
  EXPECT_EQ(1u, router()->GetBlockedRequestCountForTesting(browser_context()));

  // A late response for the dropped off-the-record request must not run its
  // callback.
  Respond(/*request_id=*/2, /*cancel=*/false);
  EXPECT_FALSE(result_.has_value());

  // The regular context's blocked request is unaffected.
  Respond(/*request_id=*/1, /*cancel=*/false);
  ASSERT_TRUE(result_.has_value());
  EXPECT_EQ(net::OK, *result_);

  router()->OnRequestWillBeDestroyed(browser_context(), regular_request.get());
}

// Per-context dispatch across regular contexts and off-the-record contexts,
// with the default spanning incognito mode.
class WebRequestEventRouterContextDispatchIncognitoTest
    : public WebRequestEventRouterContextDispatchTest {
 protected:
  void SetUp() override {
    WebRequestEventRouterContextDispatchTest::SetUp();
    user_prefs::UserPrefs::Set(browser_context(), pref_service());
    user_prefs::UserPrefs::Set(incognito_context(), pref_service());
    incognito_event_router_ =
        CreateAndUseTestEventRouter<CapturingEventRouter>(incognito_context());
    ExtensionPrefs::Get(browser_context())
        ->SetIsIncognitoEnabled(extension_id(), true);
    WebRequestEventRouter::OnOTRBrowserContextCreated(browser_context(),
                                                      incognito_context());
  }

  void TearDown() override {
    incognito_event_router_ = nullptr;
    WebRequestEventRouter::OnOTRBrowserContextDestroyed(browser_context(),
                                                        incognito_context());
    WebRequestEventRouterContextDispatchTest::TearDown();
  }

  // Starts the blocking "onBeforeSendHeaders" stage for `request` in the
  // incognito context. The completion callback stores its error code in
  // `incognito_result_`.
  int StartIncognitoOnBeforeSendHeaders(const WebRequestInfo* request) {
    return router()->OnBeforeSendHeaders(
        incognito_context(), request,
        base::BindLambdaForTesting(
            [this](const std::set<std::string>& removed_headers,
                   const std::set<std::string>& set_headers,
                   int error_code) { incognito_result_ = error_code; }),
        &incognito_headers_);
  }

  std::optional<int> incognito_result_;

 private:
  net::HttpRequestHeaders incognito_headers_;
  raw_ptr<CapturingEventRouter> incognito_event_router_ = nullptr;
};

// In spanning-mode incognito, a regular-context target can block a request
// that the off-the-record context owns; the target's teardown must resolve
// that request.
TEST_F(WebRequestEventRouterContextDispatchIncognitoTest,
       WorkerStopResolvesCrossContextTarget) {
  ASSERT_TRUE(AddListener("http://example.com/*", ExtraInfoSpec::BLOCKING));

  std::unique_ptr<WebRequestInfo> request = CreateRequest(1);
  ASSERT_EQ(net::ERR_IO_PENDING,
            StartIncognitoOnBeforeSendHeaders(request.get()));
  EXPECT_EQ(1u,
            router()->GetBlockedRequestCountForTesting(incognito_context()));
  EXPECT_EQ(0u, router()->GetBlockedRequestCountForTesting(browser_context()));

  // The spanning extension's worker stops in the regular context.
  SimulateWorkerStopped();
  ASSERT_TRUE(incognito_result_.has_value());
  EXPECT_EQ(net::OK, *incognito_result_);
  EXPECT_EQ(0u,
            router()->GetBlockedRequestCountForTesting(incognito_context()));
}

// Same as above, but with the extension in split incognito mode.
class WebRequestEventRouterContextDispatchSplitIncognitoTest
    : public WebRequestEventRouterContextDispatchIncognitoTest {
 protected:
  scoped_refptr<const Extension> BuildExtension() override {
    return ExtensionBuilder(kExtensionName)
        .AddHostPermission("<all_urls>")
        .SetManifestKey("incognito", "split")
        .Build();
  }

  // Registers a lazy blocking listener in the incognito context, as the
  // extension's incognito instance would.
  bool AddIncognitoLazyListener(const std::string& url_pattern) {
    WebRequestEventRouter::RequestFilter filter;
    filter.urls.AddPattern(
        URLPattern(kWebRequestFilterValidSchemes, url_pattern));
    return router()->AddEventListener(
        incognito_context(), extension_id(), kExtensionName, kEventName,
        /*sub_event_name=*/kEventName, std::move(filter),
        ExtraInfoSpec::BLOCKING, /*render_process_id=*/-1,
        /*web_view_instance_id=*/0, kMainThreadId,
        blink::mojom::kInvalidServiceWorkerVersionId, /*is_lazy=*/true);
  }
};

// In split-mode incognito, a worker stop must resolve only its own context's
// lazy targets; the other context's lazy targets for the same extension stay
// pending.
TEST_F(WebRequestEventRouterContextDispatchSplitIncognitoTest,
       WorkerStopKeepsOtherContextLazyTarget) {
  ASSERT_TRUE(AddListener("http://example.com/*", ExtraInfoSpec::BLOCKING,
                          /*is_lazy=*/true));
  ASSERT_TRUE(AddIncognitoLazyListener("http://example.com/*"));

  // Both listeners match both requests, but split-mode keeps each request on
  // its own context's listener.
  std::unique_ptr<WebRequestInfo> regular_request = CreateRequest(1);
  std::unique_ptr<WebRequestInfo> incognito_request = CreateRequest(2);
  ASSERT_EQ(net::ERR_IO_PENDING,
            StartOnBeforeSendHeaders(regular_request.get()));
  ASSERT_EQ(net::ERR_IO_PENDING,
            StartIncognitoOnBeforeSendHeaders(incognito_request.get()));

  // The regular context's worker stops. Only the regular context's lazy
  // target resolves; the incognito one keeps blocking its request.
  SimulateWorkerStopped();
  ASSERT_TRUE(result_.has_value());
  EXPECT_EQ(net::OK, *result_);
  EXPECT_FALSE(incognito_result_.has_value());
  EXPECT_EQ(1u,
            router()->GetBlockedRequestCountForTesting(incognito_context()));

  router()->OnRequestWillBeDestroyed(incognito_context(),
                                     incognito_request.get());
}

}  // namespace extensions
