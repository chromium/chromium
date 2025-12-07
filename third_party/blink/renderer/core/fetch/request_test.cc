// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/request.h"

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_request_destination.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_request_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_retry_options.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/core/url/url_search_params.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class RequestBodyTest : public testing::Test {
 protected:
  static const KURL RequestURL() {
    return KURL(AtomicString("http://www.example.com"));
  }

  static RequestInit* CreateRequestInit(
      V8TestingScope& scope,
      const v8::Local<v8::Value>& body_value) {
    auto* request_init = RequestInit::Create();
    request_init->setMethod("POST");
    request_init->setBody(blink::ScriptValue(scope.GetIsolate(), body_value));
    return request_init;
  }

 private:
  test::TaskEnvironment task_environment_;
};

TEST_F(RequestBodyTest, EmptyBody) {
  V8TestingScope scope;

  Request* request = Request::Create(scope.GetScriptState(), RequestURL(),
                                     scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());
  ASSERT_EQ(request->url(), RequestURL());

  EXPECT_EQ(request->BodyBufferByteLength(), 0u);
}

TEST_F(RequestBodyTest, InitWithBodyString) {
  V8TestingScope scope;
  String body = "test body!";
  auto* init = CreateRequestInit(
      scope, ToV8Traits<IDLString>::ToV8(scope.GetScriptState(), body));

  Request* request = Request::Create(scope.GetScriptState(), RequestURL(), init,
                                     scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());
  ASSERT_EQ(request->url(), RequestURL());

  EXPECT_EQ(request->BodyBufferByteLength(), body.length());
}

TEST_F(RequestBodyTest, InitWithBodyArrayBuffer) {
  V8TestingScope scope;
  String body = "test body!";
  auto* buffer = DOMArrayBuffer::Create(body.Span8());
  auto* init = CreateRequestInit(
      scope, ToV8Traits<DOMArrayBuffer>::ToV8(scope.GetScriptState(), buffer));

  Request* request = Request::Create(scope.GetScriptState(), RequestURL(), init,
                                     scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());
  ASSERT_EQ(request->url(), RequestURL());

  EXPECT_EQ(request->BodyBufferByteLength(), body.length());
}

TEST_F(RequestBodyTest, InitWithBodyArrayBufferView) {
  V8TestingScope scope;
  String body = "test body!";
  DOMArrayBufferView* buffer_view = DOMUint8Array::Create(body.Span8());
  auto* init =
      CreateRequestInit(scope, ToV8Traits<DOMArrayBufferView>::ToV8(
                                   scope.GetScriptState(), buffer_view));

  Request* request = Request::Create(scope.GetScriptState(), RequestURL(), init,
                                     scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());
  ASSERT_EQ(request->url(), RequestURL());

  EXPECT_EQ(request->BodyBufferByteLength(), body.length());
}

TEST_F(RequestBodyTest, InitWithBodyFormData) {
  V8TestingScope scope;
  auto* form = FormData::Create(scope.GetExceptionState());
  form->append("test-header", "test value!");
  auto* init = CreateRequestInit(
      scope, ToV8Traits<FormData>::ToV8(scope.GetScriptState(), form));

  Request* request = Request::Create(scope.GetScriptState(), RequestURL(), init,
                                     scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());
  ASSERT_EQ(request->url(), RequestURL());

  EXPECT_EQ(request->BodyBufferByteLength(),
            form->EncodeMultiPartFormData()->SizeInBytes());
}

TEST_F(RequestBodyTest, InitWithUrlSearchParams) {
  V8TestingScope scope;
  auto* params = URLSearchParams::Create(
      {std::make_pair("test-key", "test-value")}, scope.GetExceptionState());
  auto* init = CreateRequestInit(
      scope, ToV8Traits<URLSearchParams>::ToV8(scope.GetScriptState(), params));

  Request* request = Request::Create(scope.GetScriptState(), RequestURL(), init,
                                     scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());
  ASSERT_EQ(request->url(), RequestURL());

  EXPECT_EQ(request->BodyBufferByteLength(),
            params->ToEncodedFormData()->SizeInBytes());
}

TEST_F(RequestBodyTest, InitWithBlob) {
  V8TestingScope scope;
  String body = "test body!";
  auto* blob = Blob::Create(body.Span8(), "text/html");
  auto* init = CreateRequestInit(
      scope, ToV8Traits<Blob>::ToV8(scope.GetScriptState(), blob));

  Request* request = Request::Create(scope.GetScriptState(), RequestURL(), init,
                                     scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());
  ASSERT_EQ(request->url(), RequestURL());

  EXPECT_EQ(request->BodyBufferByteLength(), body.length());
}

TEST(ServiceWorkerRequestTest, FromString) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;

  KURL url("http://www.example.com/");
  Request* request =
      Request::Create(scope.GetScriptState(), url, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  DCHECK(request);
  EXPECT_EQ(url, request->url());
}

TEST(ServiceWorkerRequestTest, FromRequest) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;

  KURL url("http://www.example.com/");
  Request* request1 =
      Request::Create(scope.GetScriptState(), url, exception_state);
  DCHECK(request1);

  Request* request2 =
      Request::Create(scope.GetScriptState(), request1, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  DCHECK(request2);
  EXPECT_EQ(url, request2->url());
}

TEST(ServiceWorkerRequestTest, FromAndToFetchAPIRequest) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto fetch_api_request = mojom::blink::FetchAPIRequest::New();

  const KURL url("http://www.example.com/");
  const String method = "GET";
  struct KeyValueCStringPair {
    const char* key;
    const char* value;
  };
  constexpr auto headers = std::to_array<KeyValueCStringPair>({
      {"X-Foo", "bar"},
      {"X-Quux", "foop"},
  });
  const String referrer = "http://www.referrer.com/";
  const network::mojom::ReferrerPolicy kReferrerPolicy =
      network::mojom::ReferrerPolicy::kAlways;
  const network::mojom::RequestDestination kDestination =
      network::mojom::RequestDestination::kAudio;
  const network::mojom::RequestMode kMode =
      network::mojom::RequestMode::kNavigate;
  const network::mojom::CredentialsMode kCredentialsMode =
      network::mojom::CredentialsMode::kInclude;
  const auto kCacheMode = mojom::FetchCacheMode::kValidateCache;
  const network::mojom::RedirectMode kRedirectMode =
      network::mojom::RedirectMode::kError;

  fetch_api_request->url = url;
  fetch_api_request->method = method;
  fetch_api_request->mode = kMode;
  fetch_api_request->credentials_mode = kCredentialsMode;
  fetch_api_request->cache_mode = kCacheMode;
  fetch_api_request->redirect_mode = kRedirectMode;
  fetch_api_request->destination = kDestination;
  for (const auto& header : headers) {
    fetch_api_request->headers.insert(String(header.key), String(header.value));
  }
  fetch_api_request->referrer =
      mojom::blink::Referrer::New(KURL(NullURL(), referrer), kReferrerPolicy);
  const auto fetch_api_request_headers = fetch_api_request->headers;

  Request* request =
      Request::Create(scope.GetScriptState(), std::move(fetch_api_request),
                      Request::ForServiceWorkerFetchEvent::kFalse);
  DCHECK(request);
  EXPECT_EQ(url, request->url());
  EXPECT_EQ(method, request->method());
  EXPECT_EQ(V8RequestDestination::Enum::kAudio, request->destination());
  EXPECT_EQ(referrer, request->referrer());
  EXPECT_EQ(V8RequestMode::Enum::kNavigate, request->mode());

  Headers* request_headers = request->getHeaders();

  HashMap<String, String> headers_map;
  for (const auto& header : headers) {
    headers_map.insert(header.key, header.value);
  }
  EXPECT_EQ(headers_map.size(), request_headers->HeaderList()->size());
  for (HashMap<String, String>::iterator iter = headers_map.begin();
       iter != headers_map.end(); ++iter) {
    DummyExceptionStateForTesting exception_state;
    EXPECT_EQ(iter->value, request_headers->get(iter->key, exception_state));
    EXPECT_FALSE(exception_state.HadException());
  }

  mojom::blink::FetchAPIRequestPtr second_fetch_api_request =
      request->CreateFetchAPIRequest();
  EXPECT_EQ(url, second_fetch_api_request->url);
  EXPECT_EQ(method, second_fetch_api_request->method);
  EXPECT_EQ(kMode, second_fetch_api_request->mode);
  EXPECT_EQ(kCredentialsMode, second_fetch_api_request->credentials_mode);
  EXPECT_EQ(kCacheMode, second_fetch_api_request->cache_mode);
  EXPECT_EQ(kRedirectMode, second_fetch_api_request->redirect_mode);
  EXPECT_EQ(kDestination, second_fetch_api_request->destination);
  EXPECT_EQ(referrer, second_fetch_api_request->referrer->url);
  EXPECT_EQ(network::mojom::ReferrerPolicy::kAlways,
            second_fetch_api_request->referrer->policy);
  EXPECT_EQ(fetch_api_request_headers, second_fetch_api_request->headers);
}

TEST(ServiceWorkerRequestTest, ToFetchAPIRequestDoesNotStripURLFragment) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  String url_with_fragment = "http://www.example.com/#fragment";
  Request* request = Request::Create(scope.GetScriptState(), url_with_fragment,
                                     exception_state);
  DCHECK(request);

  mojom::blink::FetchAPIRequestPtr fetch_api_request =
      request->CreateFetchAPIRequest();
  EXPECT_EQ(url_with_fragment, fetch_api_request->url);
}

TEST(RequestRetryOptionsTest, RetryOptionsEnabledNoOptionsSet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(blink::features::kFetchRetry);
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  Request* request = Request::Create(
      scope.GetScriptState(), "https://example.com", scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());

  EXPECT_EQ(request->getRetryOptions(), nullptr);
}

TEST(RequestRetryOptionsTest, RetryOptionsEnabledWithOptionsSet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(blink::features::kFetchRetry);
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  RequestInit* init = RequestInit::Create();
  RetryOptions* retry_options = RetryOptions::Create();
  retry_options->setMaxAttempts(5);
  retry_options->setInitialDelay(100);
  retry_options->setBackoffFactor(2.5);
  retry_options->setMaxAge(5000);
  retry_options->setRetryAfterUnload(true);
  retry_options->setRetryNonIdempotent(true);
  retry_options->setRetryOnlyIfServerUnreached(true);
  init->setRetryOptions(retry_options);

  Request* request =
      Request::Create(scope.GetScriptState(), "https://example.com", init,
                      scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());

  RetryOptions* result = request->getRetryOptions();
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->maxAttempts(), 5u);
  EXPECT_TRUE(result->hasInitialDelay());
  EXPECT_EQ(result->initialDelay(), 100u);
  EXPECT_TRUE(result->hasBackoffFactor());
  EXPECT_EQ(result->backoffFactor(), 2.5);
  EXPECT_EQ(result->maxAge(), 5000u);
  EXPECT_TRUE(result->retryAfterUnload());
  EXPECT_TRUE(result->retryNonIdempotent());
  EXPECT_TRUE(result->retryOnlyIfServerUnreached());
}

TEST(RequestRetryOptionsTest, RetryOptionsEnabledWithPartialOptionsSet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(blink::features::kFetchRetry);
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  RequestInit* init = RequestInit::Create();
  RetryOptions* retry_options = RetryOptions::Create();
  retry_options->setMaxAttempts(2);
  retry_options->setMaxAge(2000);
  init->setRetryOptions(retry_options);

  Request* request =
      Request::Create(scope.GetScriptState(), "https://example.com", init,
                      scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());

  RetryOptions* result = request->getRetryOptions();
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->maxAttempts(), 2u);
  EXPECT_FALSE(result->hasInitialDelay());
  EXPECT_FALSE(result->hasBackoffFactor());
  EXPECT_EQ(result->maxAge(), 2000u);
  EXPECT_FALSE(result->retryAfterUnload());
  EXPECT_FALSE(result->retryNonIdempotent());
  EXPECT_FALSE(result->retryOnlyIfServerUnreached());
}

}  // namespace blink
