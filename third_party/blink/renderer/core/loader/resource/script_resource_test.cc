// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource/script_resource.h"

#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {
namespace {

TEST(ScriptResourceTest, SuccessfulRevalidation) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const KURL url("https://www.example.com/script.js");
  ScriptResource* resource =
      ScriptResource::CreateForTest(scope.GetIsolate(), url, UTF8Encoding());
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  resource->ResponseReceived(response);
  constexpr std::string_view kData = "abcd";
  resource->AppendData(kData);
  resource->FinishForTest();

  auto* original_handler = resource->CacheHandler();
  EXPECT_TRUE(original_handler);
  EXPECT_EQ(UTF8Encoding().GetName(), original_handler->Encoding());

  resource->SetRevalidatingRequest(ResourceRequestHead(url));
  ResourceResponse revalidation_response(url);
  revalidation_response.SetHttpStatusCode(304);
  resource->ResponseReceived(revalidation_response);

  EXPECT_EQ(original_handler, resource->CacheHandler());
}

TEST(ScriptResourceTest, FailedRevalidation) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const KURL url("https://www.example.com/script.js");
  ScriptResource* resource =
      ScriptResource::CreateForTest(scope.GetIsolate(), url, Latin1Encoding());
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  resource->ResponseReceived(response);
  constexpr std::string_view kData = "abcd";
  resource->AppendData(kData);
  resource->FinishForTest();

  auto* original_handler = resource->CacheHandler();
  EXPECT_TRUE(original_handler);
  EXPECT_EQ(Latin1Encoding().GetName(), original_handler->Encoding());

  resource->SetRevalidatingRequest(ResourceRequestHead(url));
  ResourceResponse revalidation_response(url);
  revalidation_response.SetHttpStatusCode(200);
  resource->ResponseReceived(revalidation_response);

  auto* new_handler = resource->CacheHandler();
  EXPECT_TRUE(new_handler);
  EXPECT_NE(original_handler, new_handler);
}

TEST(ScriptResourceTest, RedirectDuringRevalidation) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const KURL url("https://www.example.com/script.js");
  ScriptResource* resource =
      ScriptResource::CreateForTest(scope.GetIsolate(), url, UTF8Encoding());
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  resource->ResponseReceived(response);
  constexpr std::string_view kData = "abcd";
  resource->AppendData(kData);
  resource->FinishForTest();

  auto* original_handler = resource->CacheHandler();
  EXPECT_TRUE(original_handler);

  resource->SetRevalidatingRequest(ResourceRequestHead(url));
  const KURL destination("https://www.example.com/another-script.js");
  ResourceResponse revalidation_response(url);
  revalidation_response.SetHttpStatusCode(302);
  revalidation_response.SetHttpHeaderField(
      http_names::kLocation, AtomicString(destination.GetString()));
  ResourceRequest redirect_request(destination);
  resource->WillFollowRedirect(redirect_request, revalidation_response);

  auto* new_handler = resource->CacheHandler();
  EXPECT_FALSE(new_handler);
}

TEST(ScriptResourceTest, WebUICodeCacheEnabled) {
  test::TaskEnvironment task_environment;
#if DCHECK_IS_ON()
  WTF::SetIsBeforeThreadCreatedForTest();  // Required for next operation:
#endif
  SchemeRegistry::RegisterURLSchemeAsCodeCacheWithHashing(
      "codecachewithhashing");

  V8TestingScope scope;
  const KURL url("codecachewithhashing://www.example.com/script.js");
  ScriptResource* resource =
      ScriptResource::CreateForTest(scope.GetIsolate(), url, UTF8Encoding());
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  resource->ResponseReceived(response);
  constexpr std::string_view kData = "abcd";
  resource->AppendData(kData);
  resource->FinishForTest();

  auto* handler = resource->CacheHandler();
  EXPECT_TRUE(handler);
  EXPECT_TRUE(handler->HashRequired());
  EXPECT_EQ(UTF8Encoding().GetName(), handler->Encoding());

#if DCHECK_IS_ON()
  WTF::SetIsBeforeThreadCreatedForTest();  // Required for next operation:
#endif
  SchemeRegistry::RemoveURLSchemeAsCodeCacheWithHashing("codecachewithhashing");
}

TEST(ScriptResourceTest, WebUICodeCacheDisabled) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const KURL url("nocodecachewithhashing://www.example.com/script.js");
  ScriptResource* resource =
      ScriptResource::CreateForTest(scope.GetIsolate(), url, UTF8Encoding());
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  resource->ResponseReceived(response);
  constexpr std::string_view kData = "abcd";
  resource->AppendData(kData);
  resource->FinishForTest();

  auto* handler = resource->CacheHandler();
  EXPECT_FALSE(handler);
}

TEST(ScriptResourceTest, CodeCacheEnabledByResponseFlag) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const KURL url("https://www.example.com/script.js");
  ScriptResource* resource =
      ScriptResource::CreateForTest(scope.GetIsolate(), url, UTF8Encoding());
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.SetShouldUseSourceHashForJSCodeCache(true);

  resource->ResponseReceived(response);
  constexpr std::string_view kData = "abcd";
  resource->AppendData(kData);
  resource->FinishForTest();

  auto* handler = resource->CacheHandler();
  EXPECT_TRUE(handler);
  EXPECT_TRUE(handler->HashRequired());
  EXPECT_EQ(UTF8Encoding().GetName(), handler->Encoding());
}

class MockTestingPlatformForCodeCache : public TestingPlatformSupport {
 public:
  MockTestingPlatformForCodeCache() = default;
  ~MockTestingPlatformForCodeCache() override = default;

  // TestingPlatformSupport:
  bool ShouldUseCodeCacheWithHashing(const WebURL& request_url) const override {
    return should_use_code_cache_with_hashing_;
  }

  void set_should_use_code_cache_with_hashing(
      bool should_use_code_cache_with_hashing) {
    should_use_code_cache_with_hashing_ = should_use_code_cache_with_hashing;
  }

 private:
  bool should_use_code_cache_with_hashing_ = true;
};

TEST(ScriptResourceTest, WebUICodeCachePlatformOverride) {
  test::TaskEnvironment task_environment;
  SchemeRegistry::RegisterURLSchemeAsCodeCacheWithHashing(
      "codecachewithhashing");
  ScopedTestingPlatformSupport<MockTestingPlatformForCodeCache> platform;
  V8TestingScope scope;
  const auto create_resource = [&scope]() {
    const KURL url("codecachewithhashing://www.example.com/script.js");
    ScriptResource* resource =
        ScriptResource::CreateForTest(scope.GetIsolate(), url, UTF8Encoding());
    ResourceResponse response(url);
    response.SetHttpStatusCode(200);

    resource->ResponseReceived(response);
    constexpr std::string_view kData = "abcd";
    resource->AppendData(kData);
    resource->FinishForTest();

    return resource;
  };

  {
    // Assert the cache handler is created when code caching is allowed by the
    // platform.
    platform->set_should_use_code_cache_with_hashing(true);
    ScriptResource* resource = create_resource();

    auto* handler = resource->CacheHandler();
    EXPECT_TRUE(handler);
    EXPECT_TRUE(handler->HashRequired());
    EXPECT_EQ(UTF8Encoding().GetName(), handler->Encoding());
  }

  {
    // Assert the cache handler is not created when code caching is restricted
    // by the platform.
    platform->set_should_use_code_cache_with_hashing(false);
    ScriptResource* resource = create_resource();

    auto* handler = resource->CacheHandler();
    EXPECT_FALSE(handler);
  }

  SchemeRegistry::RemoveURLSchemeAsCodeCacheWithHashing("codecachewithhashing");
}

}  // namespace
}  // namespace blink
