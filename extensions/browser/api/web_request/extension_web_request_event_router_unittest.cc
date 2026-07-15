// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/extension_web_request_event_router.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/values_test_util.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/api/web_request.h"
#include "extensions/common/api/web_request/web_request_resource_type.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/url_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_database.mojom.h"

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

class WebRequestEventRouterContextDispatchTest : public ExtensionsTest {
 public:
  static constexpr const char* kExtensionName = "Test Extension";
  static constexpr const char* kEventName =
      api::web_request::OnBeforeRequest::kEventName;
  static constexpr int kWorkerThreadId = 12;
  static constexpr int64_t kServiceWorkerVersionId = 7;

  void SetUp() override {
    ExtensionsTest::SetUp();

    render_process_host_ =
        std::make_unique<content::MockRenderProcessHost>(browser_context());

    extension_ = ExtensionBuilder(kExtensionName)
                     .AddHostPermission("<all_urls>")
                     .Build();
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension_);
    ProcessMap::Get(browser_context())
        ->Insert(extension_->id(), render_process_host_->GetID());
  }

  void TearDown() override {
    render_process_host_.reset();
    ExtensionsTest::TearDown();
  }

 protected:
  WebRequestEventRouter* router() {
    return WebRequestEventRouter::Get(browser_context());
  }

  int process_id() { return render_process_host_->GetID().GetUnsafeValue(); }

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

 private:
  RequestFilter MakeFilter(const std::string& url_pattern) {
    RequestFilter filter;
    filter.urls.AddPattern(
        URLPattern(kWebRequestFilterValidSchemes, url_pattern));
    return filter;
  }

  ExtensionsAPIClient api_client_;
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
// ("onBeforeRequest") are preserved. In legacy sub-event naming, leftover
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

}  // namespace extensions
