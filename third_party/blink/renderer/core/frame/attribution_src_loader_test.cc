// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"

#include <stddef.h>

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/testing/web_url_loader_factory_with_mock.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

using blink::url_test_helpers::RegisterMockedErrorURLLoad;
using blink::url_test_helpers::RegisterMockedURLLoad;
using blink::url_test_helpers::ToKURL;

class AttributionSrcLocalFrameClient : public EmptyLocalFrameClient {
 public:
  AttributionSrcLocalFrameClient() = default;

  std::unique_ptr<WebURLLoaderFactory> CreateURLLoaderFactory() override {
    return std::make_unique<WebURLLoaderFactoryWithMock>(
        WebURLLoaderMockFactory::GetSingletonInstance());
  }

  void DispatchWillSendRequest(ResourceRequest& request) override {
    if (request.GetRequestContext() ==
        mojom::blink::RequestContextType::ATTRIBUTION_SRC) {
      request_head_ = request;
    }
  }

  const ResourceRequestHead& request_head() const { return request_head_; }

 private:
  ResourceRequestHead request_head_;
};

class AttributionSrcLoaderTest : public PageTestBase {
 public:
  void SetUp() override {
    client_ = MakeGarbageCollected<AttributionSrcLocalFrameClient>();
    PageTestBase::SetupPageWithClients(nullptr, client_);

    SecurityContext& security_context =
        GetDocument().GetFrame()->DomWindow()->GetSecurityContext();
    security_context.SetSecurityOriginForTesting(nullptr);
    security_context.SetSecurityOrigin(
        SecurityOrigin::CreateFromString("https://example.com"));

    attribution_src_loader_ =
        MakeGarbageCollected<AttributionSrcLoader>(GetDocument().GetFrame());
  }

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
    PageTestBase::TearDown();
  }

 protected:
  Persistent<AttributionSrcLocalFrameClient> client_;
  Persistent<AttributionSrcLoader> attribution_src_loader_;
};

TEST_F(AttributionSrcLoaderTest, AttributionSrcRequestStatusHistogram) {
  base::HistogramTester histograms;

  KURL url1 = ToKURL("https://example1.com/foo.html");
  RegisterMockedURLLoad(url1, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->Register(url1, /*element=*/nullptr);

  KURL url2 = ToKURL("https://example2.com/foo.html");
  RegisterMockedErrorURLLoad(url2);

  attribution_src_loader_->Register(url2, /*element=*/nullptr);

  // kRequested = 0.
  histograms.ExpectUniqueSample("Conversions.AttributionSrcRequestStatus", 0,
                                2);

  url_test_helpers::ServeAsynchronousRequests();

  // kReceived = 1.
  histograms.ExpectBucketCount("Conversions.AttributionSrcRequestStatus", 1, 1);

  // kFailed = 2.
  histograms.ExpectBucketCount("Conversions.AttributionSrcRequestStatus", 2, 1);
}

TEST_F(AttributionSrcLoaderTest, TooManyConcurrentRequests_NewRequestDropped) {
  KURL url = ToKURL("https://example1.com/foo.html");
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  for (size_t i = 0; i < AttributionSrcLoader::kMaxConcurrentRequests; ++i) {
    EXPECT_EQ(attribution_src_loader_->RegisterSources(url),
              AttributionSrcLoader::RegisterResult::kSuccess);
  }

  EXPECT_EQ(attribution_src_loader_->RegisterSources(url),
            AttributionSrcLoader::RegisterResult::kFailedToRegister);

  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_EQ(attribution_src_loader_->RegisterSources(url),
            AttributionSrcLoader::RegisterResult::kSuccess);
}

TEST_F(AttributionSrcLoaderTest, Referrer) {
  KURL url = ToKURL("https://example1.com/foo.html");
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  EXPECT_EQ(attribution_src_loader_->RegisterSources(url),
            AttributionSrcLoader::RegisterResult::kSuccess);

  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_EQ(client_->request_head().GetReferrerPolicy(),
            network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin);
  EXPECT_EQ(client_->request_head().ReferrerString(), String());
}

}  // namespace
}  // namespace blink
