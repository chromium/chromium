// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"

#include <stddef.h>

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "net/http/structured_headers.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
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
#include "third_party/blink/renderer/core/testing/fake_local_frame_host.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource.h"
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

  const Vector<attribution_reporting::SourceRegistration>& source_data() const {
    return source_data_;
  }

  const Vector<attribution_reporting::TriggerRegistration>& trigger_data()
      const {
    return trigger_data_;
  }

  size_t disconnects() const { return disconnects_; }

  void Flush() { receiver_.FlushForTesting(); }

 private:
  void OnDisconnect() { disconnects_++; }

  // mojom::blink::AttributionDataHost:
  void SourceDataAvailable(
      attribution_reporting::SuitableOrigin reporting_origin,
      attribution_reporting::SourceRegistration data) override {
    source_data_.push_back(std::move(data));
  }

  void TriggerDataAvailable(
      attribution_reporting::SuitableOrigin reporting_origin,
      attribution_reporting::TriggerRegistration data) override {
    trigger_data_.push_back(std::move(data));
  }

  Vector<attribution_reporting::SourceRegistration> source_data_;

  Vector<attribution_reporting::TriggerRegistration> trigger_data_;

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

  void RegisterDataHost(
      mojo::PendingReceiver<mojom::blink::AttributionDataHost> data_host,
      blink::mojom::AttributionRegistrationType) override {
    mock_data_host_ = std::make_unique<MockDataHost>(std::move(data_host));
  }

  void RegisterNavigationDataHost(
      mojo::PendingReceiver<mojom::blink::AttributionDataHost> data_host,
      const blink::AttributionSrcToken& attribution_src_token,
      blink::mojom::AttributionNavigationType type) override {}

  mojo::AssociatedReceiver<mojom::blink::ConversionHost> receiver_{this};
  base::OnceClosure quit_;

  std::unique_ptr<MockDataHost> mock_data_host_;
};

class AttributionSrcLoaderTest : public PageTestBase {
 public:
  AttributionSrcLoaderTest() = default;

  ~AttributionSrcLoaderTest() override = default;

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
    GetFrame()
        .GetRemoteNavigationAssociatedInterfaces()
        ->OverrideBinderForTesting(
            mojom::blink::ConversionHost::Name_,
            base::BindRepeating([](mojo::ScopedInterfaceEndpointHandle) {}));
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
    PageTestBase::TearDown();
  }

 protected:
  Persistent<AttributionSrcLocalFrameClient> client_;
  Persistent<AttributionSrcLoader> attribution_src_loader_;
};

TEST_F(AttributionSrcLoaderTest, RegisterTriggerWithoutEligibleHeader) {
  KURL test_url = ToKURL("https://example1.com/foo.html");

  ResourceRequest request(test_url);
  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterTrigger,
      R"({"event_trigger_data":[{"trigger_data": "7"}]})");

  MockAttributionHost host(
      GetFrame().GetRemoteNavigationAssociatedInterfaces());
  EXPECT_TRUE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
      request, response, resource));
  host.WaitUntilBoundAndFlush();

  auto* mock_data_host = host.mock_data_host();
  ASSERT_TRUE(mock_data_host);

  mock_data_host->Flush();
  EXPECT_EQ(mock_data_host->trigger_data().size(), 1u);
}

TEST_F(AttributionSrcLoaderTest, RegisterTriggerWithTriggerHeader) {
  KURL test_url = ToKURL("https://example1.com/foo.html");

  ResourceRequest request(test_url);
  request.SetHttpHeaderField(http_names::kAttributionReportingEligible,
                             "trigger");
  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterTrigger,
      R"({"event_trigger_data":[{"trigger_data": "7"}]})");

  MockAttributionHost host(
      GetFrame().GetRemoteNavigationAssociatedInterfaces());
  attribution_src_loader_->MaybeRegisterAttributionHeaders(request, response,
                                                           resource);
  host.WaitUntilBoundAndFlush();

  auto* mock_data_host = host.mock_data_host();
  ASSERT_TRUE(mock_data_host);

  mock_data_host->Flush();
  EXPECT_EQ(mock_data_host->trigger_data().size(), 1u);
}

TEST_F(AttributionSrcLoaderTest, RegisterTriggerWithSourceTriggerHeader) {
  KURL test_url = ToKURL("https://example1.com/foo.html");

  ResourceRequest request(test_url);
  request.SetHttpHeaderField(http_names::kAttributionReportingEligible,
                             "event-source, trigger");
  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterTrigger,
      R"({"event_trigger_data":[{"trigger_data": "7"}]})");

  MockAttributionHost host(
      GetFrame().GetRemoteNavigationAssociatedInterfaces());
  attribution_src_loader_->MaybeRegisterAttributionHeaders(request, response,
                                                           resource);
  host.WaitUntilBoundAndFlush();

  auto* mock_data_host = host.mock_data_host();
  ASSERT_TRUE(mock_data_host);

  mock_data_host->Flush();
  EXPECT_EQ(mock_data_host->trigger_data().size(), 1u);
}

TEST_F(AttributionSrcLoaderTest, AttributionSrcRequestsIgnored) {
  KURL test_url = ToKURL("https://example1.com/foo.html");
  ResourceRequest request(test_url);
  request.SetRequestContext(mojom::blink::RequestContextType::ATTRIBUTION_SRC);

  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterTrigger,
      R"({"event_trigger_data":[{"trigger_data": "7"}]})");

  EXPECT_FALSE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
      request, response, resource));
}

TEST_F(AttributionSrcLoaderTest, AttributionSrcRequestsInvalidEligibleHeaders) {
  KURL test_url = ToKURL("https://example1.com/foo.html");
  ResourceRequest request(test_url);
  request.SetRequestContext(mojom::blink::RequestContextType::ATTRIBUTION_SRC);

  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);

  const char* header_values[] = {"navigation-source, event-source, trigger",
                                 "!!!", ""};

  for (const char* header : header_values) {
    response.SetHttpHeaderField(
        http_names::kAttributionReportingRegisterTrigger, header);

    EXPECT_FALSE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
        request, response, resource))
        << header;
  }
}

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
    EXPECT_TRUE(attribution_src_loader_->RegisterNavigation(
        url, mojom::blink::AttributionNavigationType::kAnchor));
  }

  EXPECT_FALSE(attribution_src_loader_->RegisterNavigation(
      url, mojom::blink::AttributionNavigationType::kAnchor));

  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_TRUE(attribution_src_loader_->RegisterNavigation(
      url, mojom::blink::AttributionNavigationType::kAnchor));
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

  EXPECT_TRUE(client_->request_head()
                  .HttpHeaderField(http_names::kAttributionReportingSupport)
                  .IsNull());
}

TEST_F(AttributionSrcLoaderTest, EligibleHeader_RegisterNavigation) {
  KURL url = ToKURL("https://example1.com/foo.html");
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->RegisterNavigation(
      url, mojom::blink::AttributionNavigationType::kAnchor,
      /*element=*/nullptr);

  url_test_helpers::ServeAsynchronousRequests();

  const AtomicString& eligible = client_->request_head().HttpHeaderField(
      http_names::kAttributionReportingEligible);
  EXPECT_EQ(eligible, "navigation-source");

  absl::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(eligible.Utf8());
  ASSERT_TRUE(dict);
  ASSERT_EQ(dict->size(), 1u);
  EXPECT_TRUE(dict->contains("navigation-source"));

  EXPECT_TRUE(client_->request_head()
                  .HttpHeaderField(http_names::kAttributionReportingSupport)
                  .IsNull());
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

class AttributionSrcLoaderCrossAppWebEnabledTest
    : public AttributionSrcLoaderTest {
 public:
  AttributionSrcLoaderCrossAppWebEnabledTest() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kAttributionReportingCrossAppWeb};
};

TEST_F(AttributionSrcLoaderCrossAppWebEnabledTest, SupportHeader_Register) {
  KURL url = ToKURL("https://example1.com/foo.html");
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->Register(url, /*element=*/nullptr);

  url_test_helpers::ServeAsynchronousRequests();

  const AtomicString& support = client_->request_head().HttpHeaderField(
      http_names::kAttributionReportingSupport);
  EXPECT_EQ(support, "web");

  absl::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(support.Utf8());
  ASSERT_TRUE(dict);
  ASSERT_EQ(dict->size(), 1u);
  EXPECT_TRUE(dict->contains("web"));
}

TEST_F(AttributionSrcLoaderCrossAppWebEnabledTest,
       SupportHeader_RegisterNavigation) {
  KURL url = ToKURL("https://example1.com/foo.html");
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->RegisterNavigation(
      url, mojom::blink::AttributionNavigationType::kAnchor,
      /*element=*/nullptr);

  url_test_helpers::ServeAsynchronousRequests();

  const AtomicString& support = client_->request_head().HttpHeaderField(
      http_names::kAttributionReportingSupport);
  EXPECT_EQ(support, "web");

  absl::optional<net::structured_headers::Dictionary> dict =
      net::structured_headers::ParseDictionary(support.Utf8());
  ASSERT_TRUE(dict);
  ASSERT_EQ(dict->size(), 1u);
  EXPECT_TRUE(dict->contains("web"));
}

}  // namespace
}  // namespace blink
