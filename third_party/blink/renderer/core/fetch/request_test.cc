// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/request.h"

#include <memory>
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_request.h"
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

TEST(ServiceWorkerRequestTest, FromAndToWebRequest) {
  V8TestingScope scope;
  WebServiceWorkerRequest web_request;

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
  const network::mojom::FetchRequestMode kMode =
      network::mojom::FetchRequestMode::kNavigate;
  const network::mojom::FetchCredentialsMode kCredentialsMode =
      network::mojom::FetchCredentialsMode::kInclude;
  const auto kCacheMode = mojom::FetchCacheMode::kValidateCache;
  const network::mojom::FetchRedirectMode kRedirectMode =
      network::mojom::FetchRedirectMode::kError;

  web_request.SetURL(url);
  web_request.SetMethod(method);
  web_request.SetMode(kMode);
  web_request.SetCredentialsMode(kCredentialsMode);
  web_request.SetCacheMode(kCacheMode);
  web_request.SetRedirectMode(kRedirectMode);
  web_request.SetRequestContext(kContext);
  for (int i = 0; headers[i].key; ++i) {
    web_request.SetHeader(WebString::FromUTF8(headers[i].key),
                          WebString::FromUTF8(headers[i].value));
  }
  web_request.SetReferrer(referrer, kReferrerPolicy);

  Request* request = Request::Create(scope.GetScriptState(), web_request);
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

  WebServiceWorkerRequest second_web_request;
  request->PopulateWebServiceWorkerRequest(second_web_request);
  EXPECT_EQ(url, KURL(second_web_request.Url()));
  EXPECT_EQ(method, String(second_web_request.Method()));
  EXPECT_EQ(kMode, second_web_request.Mode());
  EXPECT_EQ(kCredentialsMode, second_web_request.CredentialsMode());
  EXPECT_EQ(kCacheMode, second_web_request.CacheMode());
  EXPECT_EQ(kRedirectMode, second_web_request.RedirectMode());
  EXPECT_EQ(kContext, second_web_request.GetRequestContext());
  EXPECT_EQ(referrer, KURL(second_web_request.ReferrerUrl()));
  EXPECT_EQ(network::mojom::ReferrerPolicy::kAlways,
            second_web_request.GetReferrerPolicy());
  EXPECT_EQ(web_request.Headers(), second_web_request.Headers());
}

TEST(ServiceWorkerRequestTest, ToWebRequestStripsURLFragment) {
  V8TestingScope scope;
  DummyExceptionStateForTesting exception_state;
  String url_without_fragment = "http://www.example.com/";
  String url = url_without_fragment + "#fragment";
  Request* request =
      Request::Create(scope.GetScriptState(), url, exception_state);
  DCHECK(request);

  WebServiceWorkerRequest web_request;
  request->PopulateWebServiceWorkerRequest(web_request);
  EXPECT_EQ(url_without_fragment, KURL(web_request.Url()));
}

}  // namespace
}  // namespace blink
