// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"

#include <stddef.h>

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "net/http/structured_headers.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom-blink.h"
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

class MockDataHost : public mojom::blink::AttributionDataHost {
 public:
  explicit MockDataHost(
      mojo::PendingReceiver<mojom::blink::AttributionDataHost> data_host) {
    receiver_.Bind(std::move(data_host));
    receiver_.set_disconnect_handler(
        base::BindOnce(&MockDataHost::OnDisconnect, base::Unretained(this)));
  }

  ~MockDataHost() override = default;

  size_t disconnects() const { return disconnects_; }

 private:
  void OnDisconnect() { disconnects_++; }

  // mojom::blink::AttributionDataHost:
  void SourceDataAvailable(
      mojom::blink::AttributionSourceDataPtr data) override {}
  void TriggerDataAvailable(
      mojom::blink::AttributionTriggerDataPtr data) override {}

  size_t disconnects_ = 0;
  mojo::Receiver<mojom::blink::AttributionDataHost> receiver_{this};
};

class MockAttributionHost : public mojom::blink::ConversionHost {
 public:
  explicit MockAttributionHost(blink::AssociatedInterfaceProvider* provider) {
    provider->OverrideBinderForTesting(
        mojom::blink::ConversionHost::Name_,
        base::BindRepeating(&MockAttributionHost::BindReceiver,
                            base::Unretained(this)));
  }

  ~MockAttributionHost() override = default;

  void WaitUntilBoundAndFlush() {
    if (receiver_.is_bound())
      return;
    base::RunLoop wait_loop;
    quit_ = wait_loop.QuitClosure();
    wait_loop.Run();
    receiver_.FlushForTesting();
  }

  MockDataHost* mock_data_host() { return mock_data_host_.get(); }

 private:
  void BindReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<mojom::blink::ConversionHost>(
            std::move(handle)));
    if (quit_)
      std::move(quit_).Run();
  }

  void RegisterDataHost(mojo::PendingReceiver<mojom::blink::AttributionDataHost>
                            data_host) override {
    mock_data_host_ = std::make_unique<MockDataHost>(std::move(data_host));
  }

  void RegisterNavigationDataHost(
      mojo::PendingReceiver<mojom::blink::AttributionDataHost> data_host,
      const blink::AttributionSrcToken& attribution_src_token) override {}

  mojo::AssociatedReceiver<mojom::blink::ConversionHost> receiver_{this};
  base::OnceClosure quit_;

  std::unique_ptr<MockDataHost> mock_data_host_;
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
    EXPECT_TRUE(attribution_src_loader_->RegisterNavigation(url));
  }

  EXPECT_FALSE(attribution_src_loader_->RegisterNavigation(url));

  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_TRUE(attribution_src_loader_->RegisterNavigation(url));
}

TEST_F(AttributionSrcLoaderTest, Referrer) {
  KURL url = ToKURL("https://example1.com/foo.html");
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->Register(url, /*element=*/nullptr);

  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_EQ(client_->request_head().GetReferrerPolicy(),
            network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin);
  EXPECT_EQ(client_->request_head().ReferrerString(), String());
}

TEST_F(AttributionSrcLoaderTest, EligibleHeader_Register) {
  KURL url = ToKURL("https://example1.com/foo.html");
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->Register(url, /*element=*/nullptr);

  url_test_helpers::ServeAsynchronousRequests();

  const AtomicString& eligible = client_->request_head().HttpHeaderField(
      http_names::kAttributionReportingEligible);
  EXPECT_EQ(eligible, "event-source, trigger");

  absl::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(eligible.Utf8());
  ASSERT_TRUE(dict);
  ASSERT_EQ(dict->size(), 2u);
  EXPECT_TRUE(dict->contains("event-source"));
  EXPECT_TRUE(dict->contains("trigger"));
}

TEST_F(AttributionSrcLoaderTest, EligibleHeader_RegisterNavigation) {
  KURL url = ToKURL("https://example1.com/foo.html");
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->RegisterNavigation(url, /*element=*/nullptr);

  url_test_helpers::ServeAsynchronousRequests();

  const AtomicString& eligible = client_->request_head().HttpHeaderField(
      http_names::kAttributionReportingEligible);
  EXPECT_EQ(eligible, "navigation-source");

  absl::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(eligible.Utf8());
  ASSERT_TRUE(dict);
  ASSERT_EQ(dict->size(), 1u);
  EXPECT_TRUE(dict->contains("navigation-source"));
}

// Regression test for crbug.com/1336797, where we didn't eagerly disconnect a
// source-eligible data host even if we knew there is no more data to be
// received on that channel. This test confirms the channel properly
// disconnects in this case.
TEST_F(AttributionSrcLoaderTest, EagerlyClosesRemote) {
  KURL url = ToKURL("https://example1.com/foo.html");
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  MockAttributionHost host(
      GetFrame().GetRemoteNavigationAssociatedInterfaces());
  attribution_src_loader_->Register(url, /*element=*/nullptr);
  host.WaitUntilBoundAndFlush();
  url_test_helpers::ServeAsynchronousRequests();

  auto* mock_data_host = host.mock_data_host();
  ASSERT_TRUE(mock_data_host);
  EXPECT_EQ(mock_data_host->disconnects(), 1u);
}

}  // namespace
}  // namespace blink
