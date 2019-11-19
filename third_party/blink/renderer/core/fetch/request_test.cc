// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/request.h"

#include <memory>
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

TEST(ServiceWorkerRequestTest, FromString) {
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
  V8TestingScope scope;
  auto fetch_api_request = mojom::blink::FetchAPIRequest::New();

  const KURL url("http://www.example.com/");
  const String method = "GET";
  struct {
    const char* key;
    const char* value;
  } headers[] = {{"X-Foo", "bar"}, {"X-Quux", "foop"}, {nullptr, nullptr}};
  const String referrer = "http://www.referrer.com/";
  const network::mojom::ReferrerPolicy kReferrerPolicy =
      network::mojom::ReferrerPolicy::kAlways;
  const mojom::RequestContextType kContext = mojom::RequestContextType::AUDIO;
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
  fetch_api_request->request_context_type = kContext;
  for (int i = 0; headers[i].key; ++i) {
    fetch_api_request->headers.insert(String(headers[i].key),
                                      String(headers[i].value));
  }
  fetch_api_request->referrer =
      mojom::blink::Referrer::New(KURL(NullURL(), referrer), kReferrerPolicy);

  Request* request =
      Request::Create(scope.GetScriptState(), *fetch_api_request,
                      Request::ForServiceWorkerFetchEvent::kFalse);
  DCHECK(request);
  EXPECT_EQ(url, request->url());
  EXPECT_EQ(method, request->method());
  EXPECT_EQ("audio", request->destination());
  EXPECT_EQ(referrer, request->referrer());
  EXPECT_EQ("navigate", request->mode());

  Headers* request_headers = request->getHeaders();

  WTF::HashMap<String, String> headers_map;
  for (int i = 0; headers[i].key; ++i)
    headers_map.insert(headers[i].key, headers[i].value);
  EXPECT_EQ(headers_map.size(), request_headers->HeaderList()->size());
  for (WTF::HashMap<String, String>::iterator iter = headers_map.begin();
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
  EXPECT_EQ(kContext, second_fetch_api_request->request_context_type);
  EXPECT_EQ(referrer, second_fetch_api_request->referrer->url);
  EXPECT_EQ(network::mojom::ReferrerPolicy::kAlways,
            second_fetch_api_request->referrer->policy);
  EXPECT_EQ(fetch_api_request->headers, second_fetch_api_request->headers);
}

TEST(ServiceWorkerRequestTest, ToFetchAPIRequestStripsURLFragment) {
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  String url_without_fragment = "http://www.example.com/";
  String url = url_without_fragment + "#fragment";
  Request* request =
      Request::Create(scope.GetScriptState(), url, exception_state);
  DCHECK(request);

  mojom::blink::FetchAPIRequestPtr fetch_api_request =
      request->CreateFetchAPIRequest();
  EXPECT_EQ(url_without_fragment, fetch_api_request->url);
}

}  // namespace
}  // namespace blink
