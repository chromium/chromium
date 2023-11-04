// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"

#include <stddef.h>

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/registration_eligibility.mojom-shared.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/trigger_verification.h"
#include "services/network/public/mojom/attribution.mojom-blink.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-blink.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/fake_local_frame_host.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_factory.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_resource.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

using ::network::mojom::AttributionReportingEligibility;

using blink::url_test_helpers::RegisterMockedErrorURLLoad;
using blink::url_test_helpers::RegisterMockedURLLoad;
using blink::url_test_helpers::ToKURL;

const char kAttributionReportingSupport[] = "Attribution-Reporting-Support";

const char kUrl[] = "https://example1.com/foo.html";

class AttributionSrcLocalFrameClient : public EmptyLocalFrameClient {
 public:
  AttributionSrcLocalFrameClient() = default;

  std::unique_ptr<URLLoader> CreateURLLoaderForTesting() override {
    return URLLoaderMockFactory::GetSingletonInstance()->CreateURLLoader();
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
        WTF::BindOnce(&MockDataHost::OnDisconnect, WTF::Unretained(this)));
  }

  ~MockDataHost() override = default;

  const Vector<attribution_reporting::SourceRegistration>& source_data() const {
    return source_data_;
  }

  const Vector<attribution_reporting::TriggerRegistration>& trigger_data()
      const {
    return trigger_data_;
  }

  const Vector<Vector<network::TriggerVerification>>& trigger_verifications()
      const {
    return trigger_verifications_;
  }

  const std::vector<std::vector<attribution_reporting::OsRegistrationItem>>&
  os_sources() const {
    return os_sources_;
  }
  const std::vector<std::vector<attribution_reporting::OsRegistrationItem>>&
  os_triggers() const {
    return os_triggers_;
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
      attribution_reporting::TriggerRegistration data,
      Vector<network::TriggerVerification> verifications) override {
    trigger_data_.push_back(std::move(data));
    trigger_verifications_.push_back(std::move(verifications));
  }

  void OsSourceDataAvailable(
      std::vector<attribution_reporting::OsRegistrationItem> registration_items)
      override {
    os_sources_.emplace_back(std::move(registration_items));
  }

  void OsTriggerDataAvailable(
      std::vector<attribution_reporting::OsRegistrationItem> registration_items)
      override {
    os_triggers_.emplace_back(std::move(registration_items));
  }

  Vector<attribution_reporting::SourceRegistration> source_data_;

  Vector<attribution_reporting::TriggerRegistration> trigger_data_;

  Vector<Vector<network::TriggerVerification>> trigger_verifications_;

  std::vector<std::vector<attribution_reporting::OsRegistrationItem>>
      os_sources_;
  std::vector<std::vector<attribution_reporting::OsRegistrationItem>>
      os_triggers_;

  size_t disconnects_ = 0;
  mojo::Receiver<mojom::blink::AttributionDataHost> receiver_{this};
};

class MockAttributionHost : public mojom::blink::AttributionHost {
 public:
  explicit MockAttributionHost(blink::AssociatedInterfaceProvider* provider) {
    provider->OverrideBinderForTesting(
        mojom::blink::AttributionHost::Name_,
        WTF::BindRepeating(&MockAttributionHost::BindReceiver,
                           WTF::Unretained(this)));
  }

  ~MockAttributionHost() override = default;

  void WaitUntilBoundAndFlush() {
    if (receiver_.is_bound()) {
      return;
    }
    base::RunLoop wait_loop;
    quit_ = wait_loop.QuitClosure();
    wait_loop.Run();
    receiver_.FlushForTesting();
  }

  MockDataHost* mock_data_host() { return mock_data_host_.get(); }

 private:
  void BindReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<mojom::blink::AttributionHost>(
            std::move(handle)));
    if (quit_) {
      std::move(quit_).Run();
    }
  }

  void RegisterDataHost(
      mojo::PendingReceiver<mojom::blink::AttributionDataHost> data_host,
      attribution_reporting::mojom::RegistrationEligibility) override {
    mock_data_host_ = std::make_unique<MockDataHost>(std::move(data_host));
  }

  void RegisterNavigationDataHost(
      mojo::PendingReceiver<mojom::blink::AttributionDataHost> data_host,
      const blink::AttributionSrcToken& attribution_src_token) override {}

  mojo::AssociatedReceiver<mojom::blink::AttributionHost> receiver_{this};
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
            mojom::blink::AttributionHost::Name_,
            WTF::BindRepeating([](mojo::ScopedInterfaceEndpointHandle) {}));
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
      AtomicString(R"({"event_trigger_data":[{"trigger_data": "7"}]})"));

  MockAttributionHost host(
      GetFrame().GetRemoteNavigationAssociatedInterfaces());
  EXPECT_TRUE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
      request, response, resource));
  host.WaitUntilBoundAndFlush();

  auto* mock_data_host = host.mock_data_host();
  ASSERT_TRUE(mock_data_host);

  mock_data_host->Flush();
  EXPECT_EQ(mock_data_host->trigger_data().size(), 1u);
  ASSERT_EQ(mock_data_host->trigger_verifications().size(), 1u);
  ASSERT_THAT(mock_data_host->trigger_verifications().at(0),
              testing::IsEmpty());
}

// TODO(https://crbug.com/1412566): Improve tests to properly cover the
// different `kAttributionReportingEligible` header values.
TEST_F(AttributionSrcLoaderTest, RegisterTriggerWithTriggerHeader) {
  KURL test_url = ToKURL("https://example1.com/foo.html");

  ResourceRequest request(test_url);
  request.SetAttributionReportingEligibility(
      AttributionReportingEligibility::kTrigger);
  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterTrigger,
      AtomicString(R"({"event_trigger_data":[{"trigger_data": "7"}]})"));

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
  request.SetAttributionReportingEligibility(
      AttributionReportingEligibility::kEventSourceOrTrigger);
  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterTrigger,
      AtomicString(R"({"event_trigger_data":[{"trigger_data": "7"}]})"));

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

TEST_F(AttributionSrcLoaderTest, RegisterTriggerOsHeadersIgnored) {
  KURL test_url = ToKURL("https://example1.com/foo.html");

  ResourceRequest request(test_url);
  request.SetAttributionReportingEligibility(
      AttributionReportingEligibility::kEventSourceOrTrigger);
  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterTrigger,
      AtomicString(R"({"event_trigger_data":[{"trigger_data": "7"}]})"));

  // These should be ignored because the relevant feature is disabled by
  // default.
  response.SetHttpHeaderField(http_names::kAttributionReportingRegisterOSSource,
                              AtomicString(R"("https://r.test/x")"));
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterOSTrigger,
      AtomicString(R"("https://r.test/y")"));

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

TEST_F(AttributionSrcLoaderTest, RegisterTriggerWithVerifications) {
  KURL test_url = ToKURL("https://example1.com/foo.html");

  ResourceRequest request(test_url);
  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterTrigger,
      AtomicString(R"({"event_trigger_data":[{"trigger_data": "7"}]})"));

  response.SetTriggerVerifications(
      {*network::TriggerVerification::Create(
           "token-1",
           base::Uuid::ParseLowercase("11fa6760-8e5c-4ccb-821d-b5d82bef2b37")),
       *network::TriggerVerification::Create(
           "token-2", base::Uuid::ParseLowercase(
                          "22fa6760-8e5c-4ccb-821d-b5d82bef2b37"))});

  MockAttributionHost host(
      GetFrame().GetRemoteNavigationAssociatedInterfaces());
  EXPECT_TRUE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
      request, response, resource));

  host.WaitUntilBoundAndFlush();

  auto* mock_data_host = host.mock_data_host();
  ASSERT_TRUE(mock_data_host);
  mock_data_host->Flush();

  ASSERT_EQ(mock_data_host->trigger_verifications().size(), 1u);
  const Vector<network::TriggerVerification>& verifications =
      mock_data_host->trigger_verifications().at(0);
  ASSERT_EQ(verifications.size(), 2u);
  EXPECT_EQ(verifications.at(0).token(), "token-1");
  EXPECT_EQ(verifications.at(0).aggregatable_report_id().AsLowercaseString(),
            "11fa6760-8e5c-4ccb-821d-b5d82bef2b37");
  EXPECT_EQ(verifications.at(1).token(), "token-2");
  EXPECT_EQ(verifications.at(1).aggregatable_report_id().AsLowercaseString(),
            "22fa6760-8e5c-4ccb-821d-b5d82bef2b37");
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
      AtomicString(R"({"event_trigger_data":[{"trigger_data": "7"}]})"));

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
        http_names::kAttributionReportingRegisterTrigger, AtomicString(header));

    EXPECT_FALSE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
        request, response, resource))
        << header;
  }
}

TEST_F(AttributionSrcLoaderTest, AttributionSrcRequestStatusHistogram) {
  base::HistogramTester histograms;

  KURL url1 = ToKURL(kUrl);
  RegisterMockedURLLoad(url1, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->Register(AtomicString(kUrl), /*element=*/nullptr);

  static constexpr char kUrl2[] = "https://example2.com/foo.html";
  KURL url2 = ToKURL(kUrl2);
  RegisterMockedErrorURLLoad(url2);

  attribution_src_loader_->Register(AtomicString(kUrl2), /*element=*/nullptr);

  // kRequested = 0.
  histograms.ExpectUniqueSample("Conversions.AttributionSrcRequestStatus", 0,
                                2);

  url_test_helpers::ServeAsynchronousRequests();

  // kReceived = 1.
  histograms.ExpectBucketCount("Conversions.AttributionSrcRequestStatus", 1, 1);

  // kFailed = 2.
  histograms.ExpectBucketCount("Conversions.AttributionSrcRequestStatus", 2, 1);
}

TEST_F(AttributionSrcLoaderTest, Referrer) {
  KURL url = ToKURL("https://example1.com/foo.html");
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->Register(AtomicString(kUrl), /*element=*/nullptr);

  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_EQ(client_->request_head().GetReferrerPolicy(),
            network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin);
  EXPECT_EQ(client_->request_head().ReferrerString(), String());
}

TEST_F(AttributionSrcLoaderTest, EligibleHeader_Register) {
  KURL url = ToKURL(kUrl);
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->Register(AtomicString(kUrl), /*element=*/nullptr);

  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_EQ(client_->request_head().GetAttributionReportingEligibility(),
            AttributionReportingEligibility::kEventSourceOrTrigger);

  EXPECT_FALSE(client_->request_head().GetAttributionSrcToken());

  EXPECT_TRUE(client_->request_head()
                  .HttpHeaderField(AtomicString(kAttributionReportingSupport))
                  .IsNull());
}

TEST_F(AttributionSrcLoaderTest, EligibleHeader_RegisterNavigation) {
  KURL url = ToKURL(kUrl);
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  std::ignore = attribution_src_loader_->RegisterNavigation(
      /*navigation_url=*/KURL(), /*attribution_src=*/AtomicString(kUrl),
      /*element=*/MakeGarbageCollected<HTMLAnchorElement>(GetDocument()),
      /*has_transient_user_activation=*/true);

  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_EQ(client_->request_head().GetAttributionReportingEligibility(),
            AttributionReportingEligibility::kNavigationSource);

  EXPECT_TRUE(client_->request_head().GetAttributionSrcToken());

  EXPECT_TRUE(client_->request_head()
                  .HttpHeaderField(AtomicString(kAttributionReportingSupport))
                  .IsNull());
}

// Regression test for crbug.com/1336797, where we didn't eagerly disconnect a
// source-eligible data host even if we knew there is no more data to be
// received on that channel. This test confirms the channel properly
// disconnects in this case.
TEST_F(AttributionSrcLoaderTest, EagerlyClosesRemote) {
  KURL url = ToKURL(kUrl);
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  MockAttributionHost host(
      GetFrame().GetRemoteNavigationAssociatedInterfaces());
  attribution_src_loader_->Register(AtomicString(kUrl), /*element=*/nullptr);
  host.WaitUntilBoundAndFlush();
  url_test_helpers::ServeAsynchronousRequests();

  auto* mock_data_host = host.mock_data_host();
  ASSERT_TRUE(mock_data_host);
  EXPECT_EQ(mock_data_host->disconnects(), 1u);
}

TEST_F(AttributionSrcLoaderTest, NoneSupported_CannotRegister) {
  GetPage().SetAttributionSupport(network::mojom::AttributionSupport::kNone);

  KURL test_url = ToKURL("https://example1.com/foo.html");

  EXPECT_FALSE(
      attribution_src_loader_->CanRegister(test_url, /*element=*/nullptr,
                                           /*request_id=*/absl::nullopt));
}

TEST_F(AttributionSrcLoaderTest, WebDisabled_TriggerNotRegistered) {
  KURL test_url = ToKURL("https://example1.com/foo.html");

  for (auto attribution_support : {network::mojom::AttributionSupport::kNone,
                                   network::mojom::AttributionSupport::kOs}) {
    ResourceRequest request(test_url);
    request.SetAttributionReportingSupport(attribution_support);
    auto* resource = MakeGarbageCollected<MockResource>(test_url);
    ResourceResponse response(test_url);
    response.SetHttpStatusCode(200);
    response.SetHttpHeaderField(
        http_names::kAttributionReportingRegisterTrigger,
        AtomicString(R"({"event_trigger_data":[{"trigger_data": "7"}]})"));

    MockAttributionHost host(
        GetFrame().GetRemoteNavigationAssociatedInterfaces());
    EXPECT_TRUE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
        request, response, resource));
    host.WaitUntilBoundAndFlush();

    auto* mock_data_host = host.mock_data_host();
    ASSERT_TRUE(mock_data_host);

    mock_data_host->Flush();
    EXPECT_THAT(mock_data_host->trigger_data(), testing::IsEmpty());
  }
}

TEST_F(AttributionSrcLoaderTest, HeadersSize_RecordsMetrics) {
  base::HistogramTester histograms;
  KURL test_url = ToKURL("https://example1.com/foo.html");
  AtomicString register_trigger_json(
      R"({"event_trigger_data":[{"trigger_data": "7"}]})");
  AtomicString register_source_json(
      R"({"source_event_id":"5","destination":"https://destination.example"})");

  ResourceRequest request(test_url);
  request.SetAttributionReportingEligibility(
      AttributionReportingEligibility::kTrigger);
  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(http_names::kAttributionReportingRegisterTrigger,
                              register_trigger_json);

  attribution_src_loader_->MaybeRegisterAttributionHeaders(request, response,
                                                           resource);
  histograms.ExpectUniqueSample("Conversions.HeadersSize.RegisterTrigger",
                                register_trigger_json.length(), 1);

  request.SetAttributionReportingEligibility(
      AttributionReportingEligibility::kEventSource);
  response.SetHttpHeaderField(http_names::kAttributionReportingRegisterSource,
                              register_source_json);

  attribution_src_loader_->MaybeRegisterAttributionHeaders(request, response,
                                                           resource);
  histograms.ExpectUniqueSample("Conversions.HeadersSize.RegisterSource",
                                register_source_json.length(), 1);
}

class AttributionSrcLoaderCrossAppWebRuntimeDisabledTest
    : public AttributionSrcLoaderTest {
 public:
  AttributionSrcLoaderCrossAppWebRuntimeDisabledTest() = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      network::features::kAttributionReportingCrossAppWeb};
};

TEST_F(AttributionSrcLoaderCrossAppWebRuntimeDisabledTest,
       OsTriggerNotRegistered) {
  GetPage().SetAttributionSupport(
      network::mojom::AttributionSupport::kWebAndOs);

  KURL test_url = ToKURL("https://example1.com/foo.html");

  ResourceRequest request(test_url);
  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterOSTrigger,
      AtomicString(R"("https://r.test/x")"));

  EXPECT_FALSE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
      request, response, resource));
}

class AttributionSrcLoaderCrossAppWebEnabledTest
    : public AttributionSrcLoaderTest {
 public:
  AttributionSrcLoaderCrossAppWebEnabledTest() {
    WebRuntimeFeatures::EnableFeatureFromString(
        /*name=*/"AttributionReportingCrossAppWeb", /*enable=*/true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      network::features::kAttributionReportingCrossAppWeb};
};

TEST_F(AttributionSrcLoaderCrossAppWebEnabledTest, SupportHeader_Register) {
  auto attribution_support = network::mojom::AttributionSupport::kWebAndOs;

  GetPage().SetAttributionSupport(attribution_support);

  KURL url = ToKURL(kUrl);
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->Register(AtomicString(kUrl), /*element=*/nullptr);

  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_EQ(client_->request_head().GetAttributionReportingSupport(),
            attribution_support);
}

TEST_F(AttributionSrcLoaderCrossAppWebEnabledTest,
       SupportHeader_RegisterNavigation) {
  auto attribution_support = network::mojom::AttributionSupport::kWebAndOs;

  GetPage().SetAttributionSupport(attribution_support);

  KURL url = ToKURL(kUrl);
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  std::ignore = attribution_src_loader_->RegisterNavigation(
      /*navigation_url=*/KURL(), /*attribution_src=*/AtomicString(kUrl),
      /*element=*/MakeGarbageCollected<HTMLAnchorElement>(GetDocument()),
      /*has_transient_user_activation=*/true);

  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_EQ(client_->request_head().GetAttributionReportingSupport(),
            attribution_support);
}

TEST_F(AttributionSrcLoaderCrossAppWebEnabledTest, RegisterOsTrigger) {
  KURL test_url = ToKURL("https://example1.com/foo.html");

  ResourceRequest request(test_url);
  request.SetAttributionReportingSupport(
      network::mojom::AttributionSupport::kWebAndOs);
  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterOSTrigger,
      AtomicString(R"("https://r.test/x")"));

  MockAttributionHost host(
      GetFrame().GetRemoteNavigationAssociatedInterfaces());
  EXPECT_TRUE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
      request, response, resource));
  host.WaitUntilBoundAndFlush();

  auto* mock_data_host = host.mock_data_host();
  ASSERT_TRUE(mock_data_host);

  mock_data_host->Flush();
  EXPECT_THAT(mock_data_host->os_triggers(),
              ::testing::ElementsAre(::testing::ElementsAre(
                  attribution_reporting::OsRegistrationItem{
                      .url = GURL("https://r.test/x")})));
}

TEST_F(AttributionSrcLoaderCrossAppWebEnabledTest,
       HeadersSize_OsMetricsRecorded) {
  base::HistogramTester histograms;
  GetPage().SetAttributionSupport(
      network::mojom::AttributionSupport::kWebAndOs);

  KURL test_url = ToKURL("https://example1.com/foo.html");
  AtomicString os_registration(R"("https://r.test/x")");

  ResourceRequest request(test_url);
  request.SetAttributionReportingEligibility(
      AttributionReportingEligibility::kTrigger);
  auto* resource = MakeGarbageCollected<MockResource>(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterOSTrigger, os_registration);

  EXPECT_TRUE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
      request, response, resource));
  histograms.ExpectUniqueSample("Conversions.HeadersSize.RegisterOsTrigger",
                                os_registration.length(), 1);

  request.SetAttributionReportingEligibility(
      AttributionReportingEligibility::kEventSource);
  response.SetHttpHeaderField(http_names::kAttributionReportingRegisterOSSource,
                              os_registration);

  EXPECT_TRUE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
      request, response, resource));
  histograms.ExpectUniqueSample("Conversions.HeadersSize.RegisterOsSource",
                                os_registration.length(), 1);
}

}  // namespace
}  // namespace blink
