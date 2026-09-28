// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource/script_resource.h"

#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/core/script/classic_pending_script.h"
#include "third_party/blink/renderer/core/script/mock_script_element_base.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "url/gurl.h"

namespace blink {
namespace {

TEST(ScriptResourceTest, SuccessfulRevalidation) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const KURL url("https://www.example.com/script.js");
  ScriptResource* resource =
      ScriptResource::CreateForTest(scope.GetIsolate(), url, Utf8Encoding());
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  resource->ResponseReceived(response);
  constexpr std::string_view kData = "abcd";
  resource->AppendData(kData);
  resource->FinishForTest();

  auto* original_handler = resource->CacheHandler();
  EXPECT_TRUE(original_handler);
  EXPECT_EQ(Utf8Encoding().GetName(), original_handler->Encoding());

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
      ScriptResource::CreateForTest(scope.GetIsolate(), url, Utf8Encoding());
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
  SetIsBeforeThreadCreatedForTest();  // Required for next operation:
#endif
  SchemeRegistry::RegisterURLSchemeAsCodeCacheWithHashing(
      "codecachewithhashing");

  V8TestingScope scope;
  const KURL url("codecachewithhashing://www.example.com/script.js");
  ScriptResource* resource =
      ScriptResource::CreateForTest(scope.GetIsolate(), url, Utf8Encoding());
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  resource->ResponseReceived(response);
  constexpr std::string_view kData = "abcd";
  resource->AppendData(kData);
  resource->FinishForTest();

  auto* handler = resource->CacheHandler();
  EXPECT_TRUE(handler);
  EXPECT_TRUE(handler->HashRequired());
  EXPECT_EQ(Utf8Encoding().GetName(), handler->Encoding());

#if DCHECK_IS_ON()
  SetIsBeforeThreadCreatedForTest();  // Required for next operation:
#endif
  SchemeRegistry::RemoveURLSchemeAsCodeCacheWithHashingForTest(
      "codecachewithhashing");
}

TEST(ScriptResourceTest, WebUICodeCacheDisabled) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const KURL url("nocodecachewithhashing://www.example.com/script.js");
  ScriptResource* resource =
      ScriptResource::CreateForTest(scope.GetIsolate(), url, Utf8Encoding());
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
      ScriptResource::CreateForTest(scope.GetIsolate(), url, Utf8Encoding());
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
  EXPECT_EQ(Utf8Encoding().GetName(), handler->Encoding());
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
        ScriptResource::CreateForTest(scope.GetIsolate(), url, Utf8Encoding());
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
    EXPECT_EQ(Utf8Encoding().GetName(), handler->Encoding());
  }

  {
    // Assert the cache handler is not created when code caching is restricted
    // by the platform.
    platform->set_should_use_code_cache_with_hashing(false);
    ScriptResource* resource = create_resource();

    auto* handler = resource->CacheHandler();
    EXPECT_FALSE(handler);
  }

  SchemeRegistry::RemoveURLSchemeAsCodeCacheWithHashingForTest(
      "codecachewithhashing");
}

class TestingPlatformForWebUIBundledCodeCache : public TestingPlatformSupport {
 public:
  // TestingPlatformSupport:
  std::optional<int> GetWebUIBundledCodeCacheResourceId(
      const GURL& resource_url) override {
    return resource_url == webui_bundled_code_cache_url_ ? std::optional<int>(1)
                                                         : std::nullopt;
  }

  void set_webui_bundled_code_cache_url(
      const GURL& webui_bundled_code_cache_url) {
    webui_bundled_code_cache_url_ = webui_bundled_code_cache_url;
  }

 private:
  GURL webui_bundled_code_cache_url_;
};

TEST(ScriptResourceTest, CreatesHandlerForWebUIBundledCodeCaching) {
  test::TaskEnvironment task_environment;
  ScopedTestingPlatformSupport<TestingPlatformForWebUIBundledCodeCache>
      platform;
  V8TestingScope scope;

  // Define two URLs with the supported scheme.
  constexpr char kBundledUrl1[] = "chrome://example/script_1.js";
  constexpr char kBundledUrl2[] = "chrome://example/script_2.js";

  // Define lambda to enable / disable bundled code caching for the URL scheme.
  const auto enable_webui_bundled_code_caching = [&](bool enable) {
#if DCHECK_IS_ON()
    SetIsBeforeThreadCreatedForTest();  // Required for next operation:
#endif
    if (enable) {
      SchemeRegistry::RegisterURLSchemeAsWebUIBundledBytecode("chrome");
    } else {
      SchemeRegistry::RemoveURLSchemeAsWebUIBundledBytecodeForTest("chrome");
    }
  };

  // Define a lambda that loads a URL and runs the appropriate cached metadata
  // handler checks.
  const auto load_resource_and_check = [&](const char* url_str,
                                           bool expect_handler) {
    const KURL url(url_str);
    ScriptResource* resource =
        ScriptResource::CreateForTest(scope.GetIsolate(), url, Utf8Encoding());
    ResourceResponse response(url);
    response.SetHttpStatusCode(200);

    resource->ResponseReceived(response);
    constexpr std::string_view kData = "abcd";
    resource->AppendData(kData);
    resource->FinishForTest();

    auto* handler = resource->CacheHandler();
    EXPECT_EQ(expect_handler, !!handler);
    if (handler) {
      EXPECT_EQ(CachedMetadataHandler::ServingSource::kWebUIBundledCache,
                handler->GetServingSource());
      EXPECT_EQ(Utf8Encoding().GetName(), handler->Encoding());
    }
  };

  // With code caching disabled, the handler should not be created.
  enable_webui_bundled_code_caching(false);
  load_resource_and_check(kBundledUrl1, /*expect_handler=*/false);
  load_resource_and_check(kBundledUrl2, /*expect_handler=*/false);

  // With code caching enabled, but no bundled data, the handler should not be
  // created.
  enable_webui_bundled_code_caching(true);
  load_resource_and_check(kBundledUrl1, /*expect_handler=*/false);
  load_resource_and_check(kBundledUrl2, /*expect_handler=*/false);

  // With code caching enabled, and bundled data, the handler should be created.
  platform->set_webui_bundled_code_cache_url(GURL(kBundledUrl1));
  load_resource_and_check(kBundledUrl1, /*expect_handler=*/true);
  load_resource_and_check(kBundledUrl2, /*expect_handler=*/false);

  enable_webui_bundled_code_caching(false);
}

TEST(ScriptResourceTest, ServiceWorkerCodeCacheFlag) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const KURL url("https://www.example.com/sw-script.js");

  {
    ScopedServiceWorkerCodeCacheForTest scoped_feature(false);
    ScriptResource* resource =
        ScriptResource::CreateForTest(scope.GetIsolate(), url, Utf8Encoding());
    ResourceResponse response(url);
    response.SetHttpStatusCode(200);
    response.SetWasFetchedViaServiceWorker(true);

    resource->ResponseReceived(response);
    EXPECT_FALSE(resource->CacheHandler());
  }

  {
    ScopedServiceWorkerCodeCacheForTest scoped_feature(true);
    ScriptResource* resource =
        ScriptResource::CreateForTest(scope.GetIsolate(), url, Utf8Encoding());
    ResourceResponse response(url);
    response.SetHttpStatusCode(200);
    response.SetWasFetchedViaServiceWorker(true);

    resource->ResponseReceived(response);
    ASSERT_TRUE(resource->CacheHandler());
    EXPECT_TRUE(resource->CacheHandler()->HashRequired());
  }
}

TEST(ScriptResourceTest, CodeCacheForServiceWorkerResponseUsesHashing) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const KURL url("https://www.example.com/script.js");
  ScriptResource* resource =
      ScriptResource::CreateForTest(scope.GetIsolate(), url, Utf8Encoding());
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.SetWasFetchedViaServiceWorker(true);

  resource->ResponseReceived(response);
  constexpr std::string_view kData = "abcd";
  resource->AppendData(kData);
  resource->FinishForTest();

  auto* handler = resource->CacheHandler();
  ASSERT_TRUE(handler);
  EXPECT_TRUE(handler->HashRequired());
  EXPECT_EQ(Utf8Encoding().GetName(), handler->Encoding());
}

TEST(ScriptResourceTest, ServiceWorkerCodeCacheWithHashLoadedSuccessfully) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const KURL url("https://www.example.com/script.js");
  ScriptResource* resource =
      ScriptResource::CreateForTest(scope.GetIsolate(), url, Utf8Encoding());
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.SetWasFetchedViaServiceWorker(true);

  resource->ResponseReceived(response);

  constexpr std::string_view kScriptContent = "console.log('test');";
  ParkableString source_text(String(kScriptContent).Impl());
  const ParkableString::DigestHolder digest_holder = source_text.Digest();
  const SecureStringDigest& digest = digest_holder.Get();

  constexpr uint32_t kDataTypeId = 1;
  constexpr uint8_t kDummyData[] = {1, 2, 3, 4};
  scoped_refptr<CachedMetadata> metadata =
      CachedMetadata::Create(kDataTypeId, base::span(kDummyData));
  Vector<uint8_t> serialized_with_hash =
      ScriptCachedMetadataHandlerWithHashing::AddHashHeader(
          base::span(digest), metadata->SerializedData());

  resource->SetSerializedCachedMetadata(
      mojo_base::BigBuffer(base::span(serialized_with_hash)));

  auto* handler = resource->CacheHandler();
  ASSERT_TRUE(handler);
  EXPECT_TRUE(handler->HashRequired());
  EXPECT_TRUE(handler->GetCachedMetadata(
      kDataTypeId, CachedMetadataHandler::kAllowUnchecked));

  resource->AppendData(kScriptContent);
  resource->FinishForTest();
  handler->Check(nullptr, source_text);
  EXPECT_TRUE(handler->GetCachedMetadata(
      kDataTypeId, CachedMetadataHandler::kCrashIfUnchecked));
}

TEST(ScriptResourceTest, ServiceWorkerLegacyCodeCacheWithoutHashRejected) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const KURL url("https://www.example.com/script.js");
  ScriptResource* resource =
      ScriptResource::CreateForTest(scope.GetIsolate(), url, Utf8Encoding());
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.SetWasFetchedViaServiceWorker(true);

  resource->ResponseReceived(response);

  constexpr uint32_t kDataTypeId = 1;
  constexpr uint8_t kDummyData[] = {1, 2, 3, 4};
  scoped_refptr<CachedMetadata> metadata =
      CachedMetadata::Create(kDataTypeId, base::span(kDummyData));

  resource->SetSerializedCachedMetadata(
      mojo_base::BigBuffer(metadata->SerializedData()));

  auto* handler = resource->CacheHandler();
  ASSERT_TRUE(handler);
  EXPECT_TRUE(handler->HashRequired());
  EXPECT_FALSE(handler->GetCachedMetadata(
      kDataTypeId, CachedMetadataHandler::kAllowUnchecked));
}

TEST(ScriptResourceTest,
     NonServiceWorkerResponseLoadsHashedCodeCacheGracefully) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const KURL url("https://www.example.com/script.js");
  ScriptResource* resource =
      ScriptResource::CreateForTest(scope.GetIsolate(), url, Utf8Encoding());
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.SetWasFetchedViaServiceWorker(false);

  resource->ResponseReceived(response);

  constexpr std::string_view kScriptContent = "console.log('test');";
  ParkableString source_text(String(kScriptContent).Impl());
  const ParkableString::DigestHolder digest_holder = source_text.Digest();
  const SecureStringDigest& digest = digest_holder.Get();

  constexpr uint32_t kDataTypeId = 1;
  constexpr uint8_t kDummyData[] = {1, 2, 3, 4};
  scoped_refptr<CachedMetadata> metadata =
      CachedMetadata::Create(kDataTypeId, base::span(kDummyData));
  Vector<uint8_t> serialized_with_hash =
      ScriptCachedMetadataHandlerWithHashing::AddHashHeader(
          base::span(digest), metadata->SerializedData());

  resource->SetSerializedCachedMetadata(
      mojo_base::BigBuffer(base::span(serialized_with_hash)));

  auto* handler = resource->CacheHandler();
  ASSERT_TRUE(handler);
  EXPECT_FALSE(handler->HashRequired());

  // Verify Check() succeeds when the script source matches the stored hash.
  handler->Check(nullptr, source_text);
  scoped_refptr<CachedMetadata> loaded_metadata = handler->GetCachedMetadata(
      kDataTypeId, CachedMetadataHandler::kCrashIfUnchecked);
  ASSERT_TRUE(loaded_metadata);
  EXPECT_EQ(loaded_metadata->Data(), base::span(kDummyData));

  // Verify Check() rejects and clears the metadata when a non-Service-Worker
  // ScriptResource loads a hashed code cache entry whose SHA-256 hash
  // mismatches the loaded script source.
  ScriptResource* resource_mismatch =
      ScriptResource::CreateForTest(scope.GetIsolate(), url, Utf8Encoding());
  resource_mismatch->ResponseReceived(response);
  resource_mismatch->SetSerializedCachedMetadata(
      mojo_base::BigBuffer(base::span(serialized_with_hash)));
  auto* mismatch_handler = resource_mismatch->CacheHandler();
  ASSERT_TRUE(mismatch_handler);
  EXPECT_TRUE(mismatch_handler->GetCachedMetadata(
      kDataTypeId, CachedMetadataHandler::kAllowUnchecked));
  mismatch_handler->Check(
      nullptr, ParkableString(String("console.log('different');").Impl()));
  EXPECT_FALSE(mismatch_handler->GetCachedMetadata(
      kDataTypeId, CachedMetadataHandler::kAllowUnchecked));
}

TEST(ScriptResourceTest, ServiceWorkerCodeCachePlatformOverride) {
  test::TaskEnvironment task_environment;
  ScopedTestingPlatformSupport<MockTestingPlatformForCodeCache> platform;
  V8TestingScope scope;
  const auto create_resource = [&scope]() {
    const KURL url("https://www.example.com/script.js");
    ScriptResource* resource =
        ScriptResource::CreateForTest(scope.GetIsolate(), url, Utf8Encoding());
    ResourceResponse response(url);
    response.SetHttpStatusCode(200);
    response.SetWasFetchedViaServiceWorker(true);

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
    ASSERT_TRUE(handler);
    EXPECT_TRUE(handler->HashRequired());
    EXPECT_EQ(Utf8Encoding().GetName(), handler->Encoding());
  }

  {
    // Assert the cache handler is not created when code caching is restricted
    // by the platform and does not fall back to unhashed caching.
    platform->set_should_use_code_cache_with_hashing(false);
    ScriptResource* resource = create_resource();

    auto* handler = resource->CacheHandler();
    EXPECT_FALSE(handler);
  }
}

TEST(ScriptResourceTest,
     ServiceWorkerCodeCacheMismatchClearedBeforeCacheConsumer) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const KURL url("https://www.example.com/script.js");
  url_test_helpers::RegisterMockedErrorURLLoad(url);

  MockScriptElementBase* element =
      MakeGarbageCollected<testing::NiceMock<MockScriptElementBase>>();
  ON_CALL(*element, GetDocument())
      .WillByDefault(testing::ReturnRef(scope.GetDocument()));
  ON_CALL(*element, GetExecutionContext())
      .WillByDefault(testing::Return(scope.GetExecutionContext()));

  ClassicPendingScript* pending_script = ClassicPendingScript::Fetch(
      url, scope.GetDocument(), ScriptFetchOptions(),
      CrossOriginAttributeValue::kCrossOriginAttributeNotSet, Utf8Encoding(),
      element, FetchParameters::kNoDefer, /*task_state=*/nullptr);
  ASSERT_TRUE(pending_script);
  ScriptResource* resource = To<ScriptResource>(pending_script->GetResource());
  ASSERT_TRUE(resource);

  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.SetWasFetchedViaServiceWorker(true);
  resource->ResponseReceived(response);

  auto* handler = resource->CacheHandler();
  ASSERT_TRUE(handler);
  EXPECT_TRUE(handler->HashRequired());

  // Construct serialized metadata with a hash for script A.
  ParkableString source_a(String("console.log('script A');").Impl());
  const ParkableString::DigestHolder digest_a = source_a.Digest();
  const uint32_t code_cache_tag = V8CodeCache::TagForCodeCache(handler);
  constexpr uint8_t kDummyData[] = {1, 2, 3, 4};
  scoped_refptr<CachedMetadata> metadata =
      CachedMetadata::Create(code_cache_tag, base::span(kDummyData));
  Vector<uint8_t> serialized_with_hash_a =
      ScriptCachedMetadataHandlerWithHashing::AddHashHeader(
          base::span(digest_a.Get()), metadata->SerializedData());

  resource->SetSerializedCachedMetadata(
      mojo_base::BigBuffer(base::span(serialized_with_hash_a)));
  EXPECT_TRUE(handler->GetCachedMetadata(
      code_cache_tag, CachedMetadataHandler::kAllowUnchecked));

  // Finish loading with script B (mismatched source).
  resource->AppendData("console.log('script B');");
  resource->Loader()->DidFinishLoading(base::TimeTicks(), 0, 0, 0);

  // ClassicPendingScript::NotifyFinished must have validated the hash via
  // Check() and cleared the mismatched metadata before notifying
  // ScriptCacheConsumer, advancing directly to kReady.
  EXPECT_FALSE(handler->GetCachedMetadata(
      code_cache_tag, CachedMetadataHandler::kAllowUnchecked));
  EXPECT_TRUE(pending_script->IsReady());
  url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
}

TEST(ScriptResourceTest, ServiceWorkerNonHttpUrlDoesNotCreateCacheHandler) {
  test::TaskEnvironment task_environment;
  KURL request_url("https://www.example.com/sw_script.js");
  ScriptResource* resource = ScriptResource::CreateForTest(
      nullptr, request_url, Utf8Encoding(), mojom::blink::ScriptType::kClassic);

  // If a Service Worker responds with a non-HTTP CurrentRequestUrl() (not in
  // SchemeRegistry::SchemeSupportsCodeCacheWithHashing), no
  // CachedMetadataHandler should be created because CodeCacheHost only accepts
  // HTTP(S) or registered code-cache schemes.
  ResourceResponse response(KURL("data:text/javascript,console.log(1);"));
  response.SetHttpStatusCode(200);
  response.SetWasFetchedViaServiceWorker(true);

  resource->ResponseReceived(response);
  EXPECT_FALSE(resource->CacheHandler());
}

TEST(ScriptResourceTest,
     ServiceWorkerCodeCacheHashingDisabledFallsBackToUnhashedHandler) {
  test::TaskEnvironment task_environment;
  ScopedServiceWorkerCodeCacheHashingForTest hashing_disabled(false);
  KURL request_url("https://www.example.com/sw_script.js");
  ScriptResource* resource = ScriptResource::CreateForTest(
      nullptr, request_url, Utf8Encoding(), mojom::blink::ScriptType::kClassic);

  ResourceResponse response(request_url);
  response.SetHttpStatusCode(200);
  response.SetWasFetchedViaServiceWorker(true);

  resource->ResponseReceived(response);
  ASSERT_TRUE(resource->CacheHandler());
  EXPECT_FALSE(resource->CacheHandler()->HashRequired());
}

}  // namespace
}  // namespace blink
