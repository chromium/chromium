// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource/script_resource.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {
namespace {

TEST(ScriptResourceTest, SuccessfulRevalidation) {
  const KURL url("https://www.example.com/script.js");
  ScriptResource* resource = ScriptResource::CreateForTest(url, UTF8Encoding());
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  resource->ResponseReceived(response);
  constexpr char kData[5] = "abcd";
  resource->AppendData(kData, strlen(kData));
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
  const KURL url("https://www.example.com/script.js");
  ScriptResource* resource =
      ScriptResource::CreateForTest(url, Latin1Encoding());
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  resource->ResponseReceived(response);
  constexpr char kData[5] = "abcd";
  resource->AppendData(kData, strlen(kData));
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
  const KURL url("https://www.example.com/script.js");
  ScriptResource* resource = ScriptResource::CreateForTest(url, UTF8Encoding());
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  resource->ResponseReceived(response);
  constexpr char kData[5] = "abcd";
  resource->AppendData(kData, strlen(kData));
  resource->FinishForTest();

  auto* original_handler = resource->CacheHandler();
  EXPECT_TRUE(original_handler);

  resource->SetRevalidatingRequest(ResourceRequestHead(url));
  const KURL destination("https://www.example.com/another-script.js");
  ResourceResponse revalidation_response(url);
  revalidation_response.SetHttpStatusCode(302);
  revalidation_response.SetHttpHeaderField(
      "location", AtomicString(destination.GetString()));
  ResourceRequest redirect_request(destination);
  resource->WillFollowRedirect(redirect_request, revalidation_response);

  auto* new_handler = resource->CacheHandler();
  EXPECT_FALSE(new_handler);
}

TEST(ScriptResourceTest, WebUICodeCacheEnabled) {
  SchemeRegistry::RegisterURLSchemeAsCodeCacheWithHashing(
      "codecachewithhashing");

  const KURL url("codecachewithhashing://www.example.com/script.js");
  ScriptResource* resource = ScriptResource::CreateForTest(url, UTF8Encoding());
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  resource->ResponseReceived(response);
  constexpr char kData[5] = "abcd";
  resource->AppendData(kData, strlen(kData));
  resource->FinishForTest();

  auto* handler = resource->CacheHandler();
  EXPECT_TRUE(handler);
  EXPECT_TRUE(handler->HashRequired());
  EXPECT_EQ(UTF8Encoding().GetName(), handler->Encoding());

  SchemeRegistry::RemoveURLSchemeAsCodeCacheWithHashing("codecachewithhashing");
}

TEST(ScriptResourceTest, WebUICodeCacheDisabled) {
  const KURL url("nocodecachewithhashing://www.example.com/script.js");
  ScriptResource* resource = ScriptResource::CreateForTest(url, UTF8Encoding());
  ResourceResponse response(url);
  response.SetHttpStatusCode(200);

  resource->ResponseReceived(response);
  constexpr char kData[5] = "abcd";
  resource->AppendData(kData, strlen(kData));
  resource->FinishForTest();

  auto* handler = resource->CacheHandler();
  EXPECT_FALSE(handler);
}

}  // namespace
}  // namespace blink
