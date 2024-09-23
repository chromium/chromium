// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/attribution_reporting/data_host.mojom-blink.h"
#include "components/attribution_reporting/os_registration.h"
#include "components/attribution_reporting/os_registration_error.mojom-shared.h"
#include "components/attribution_reporting/registration_eligibility.mojom-shared.h"
#include "components/attribution_reporting/registration_header_error.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_registration_error.mojom-shared.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "components/attribution_reporting/trigger_registration_error.mojom-shared.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/attribution.mojom-blink.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/fake_local_frame_host.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_factory.h"
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
using ::network::mojom::AttributionSupport;

using blink::url_test_helpers::RegisterMockedErrorURLLoad;
using blink::url_test_helpers::RegisterMockedURLLoad;
using blink::url_test_helpers::ToKURL;

const char kAttributionReportingSupport[] = "Attribution-Reporting-Support";

const char kAttributionSrcRequestStatusMetric[] =
    "Conversions.AttributionSrcRequestStatus";

const char kUrl[] = "https://example1.com/foo.html";

ResourceRequest GetAttributionRequest(
    const KURL& url,
    AttributionSupport support = AttributionSupport::kWeb) {
  ResourceRequest request(url);
  request.SetAttributionReportingSupport(support);
  return request;
}

class AttributionSrcLocalFrameClient : public EmptyLocalFrameClient {
 public:
  AttributionSrcLocalFrameClient() = default;

  std::unique_ptr<URLLoader> CreateURLLoaderForTesting() override {
    return URLLoaderMockFactory::GetSingletonInstance()->CreateURLLoader();
  }

  void DispatchFinalizeRequest(ResourceRequest& request) override {
    if (request.GetRequestContext() ==
        mojom::blink::RequestContextType::ATTRIBUTION_SRC) {
      request_head_ = request;
    }
  }

  const ResourceRequestHead& request_head() const { return request_head_; }

 private:
  ResourceRequestHead request_head_;
};

class MockDataHost : public attribution_reporting::mojom::blink::DataHost {
 public:
  explicit MockDataHost(
      mojo::PendingReceiver<attribution_reporting::mojom::blink::DataHost>
          data_host) {
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

  const std::vector<std::vector<attribution_reporting::OsRegistrationItem>>&
  os_sources() const {
    return os_sources_;
  }
  const std::vector<std::vector<attribution_reporting::OsRegistrationItem>>&
  os_triggers() const {
    return os_triggers_;
  }

  const Vector<attribution_reporting::RegistrationHeaderError>& header_errors()
      const {
    return header_errors_;
  }

  size_t disconnects() const { return disconnects_; }

  void Flush() { receiver_.FlushForTesting(); }

 private:
  void OnDisconnect() { disconnects_++; }

  // attribution_reporting::mojom::blink::DataHost:
  void SourceDataAvailable(
      attribution_reporting::SuitableOrigin reporting_origin,
      attribution_reporting::SourceRegistration data,
      bool was_fetched_via_serivce_worker) override {
    source_data_.push_back(std::move(data));
  }

  void TriggerDataAvailable(
      attribution_reporting::SuitableOrigin reporting_origin,
      attribution_reporting::TriggerRegistration data,
      bool was_fetched_via_serivce_worker) override {
    trigger_data_.push_back(std::move(data));
  }

  void OsSourceDataAvailable(
      attribution_reporting::SuitableOrigin reporting_origin,
      std::vector<attribution_reporting::OsRegistrationItem> registration_items,
      bool was_fetched_via_serivce_worker) override {
    os_sources_.emplace_back(std::move(registration_items));
  }

  void OsTriggerDataAvailable(
      attribution_reporting::SuitableOrigin reporting_origin,
      std::vector<attribution_reporting::OsRegistrationItem> registration_items,
      bool was_fetched_via_serivce_worker) override {
    os_triggers_.emplace_back(std::move(registration_items));
  }

  void ReportRegistrationHeaderError(
      attribution_reporting::SuitableOrigin reporting_origin,
      const attribution_reporting::RegistrationHeaderError& error) override {
    header_errors_.push_back(error);
  }

  Vector<attribution_reporting::SourceRegistration> source_data_;

  Vector<attribution_reporting::TriggerRegistration> trigger_data_;

  std::vector<std::vector<attribution_reporting::OsRegistrationItem>>
      os_sources_;
  std::vector<std::vector<attribution_reporting::OsRegistrationItem>>
      os_triggers_;

  Vector<attribution_reporting::RegistrationHeaderError> header_errors_;

  size_t disconnects_ = 0;
  mojo::Receiver<attribution_reporting::mojom::blink::DataHost> receiver_{this};
};

class MockAttributionHost : public mojom::blink::AttributionHost {
 public:
  explicit MockAttributionHost(blink::AssociatedInterfaceProvider* provider)
      : provider_(provider) {
    provider_->OverrideBinderForTesting(
        mojom::blink::AttributionHost::Name_,
        WTF::BindRepeating(&MockAttributionHost::BindReceiver,
                           WTF::Unretained(this)));
  }

  ~MockAttributionHost() override {
    CHECK(provider_);
    provider_->OverrideBinderForTesting(mojom::blink::AttributionHost::Name_,
                                        base::NullCallback());
  }

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
      mojo::PendingReceiver<attribution_reporting::mojom::blink::DataHost>
          data_host,
      attribution_reporting::mojom::RegistrationEligibility eligibility,
      bool is_for_background_requests) override {
    mock_data_host_ = std::make_unique<MockDataHost>(std::move(data_host));
  }

  void RegisterNavigationDataHost(
      mojo::PendingReceiver<attribution_reporting::mojom::blink::DataHost>
          data_host,
      const blink::AttributionSrcToken& attribution_src_token) override {}

  void NotifyNavigationWithBackgroundRegistrationsWillStart(
      const blink::AttributionSrcToken& attribution_src_token,
      uint32_t expected_registrations) override {}

  mojo::AssociatedReceiver<mojom::blink::AttributionHost> receiver_{this};
  base::OnceClosure quit_;
  blink::AssociatedInterfaceProvider* provider_;

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
        GetFrame().DomWindow()->GetSecurityContext();
    security_context.SetSecurityOriginForTesting(nullptr);
    security_context.SetSecurityOrigin(
        SecurityOrigin::CreateFromString("https://example.com"));

    attribution_src_loader_ =
        MakeGarbageCollected<AttributionSrcLoader>(&GetFrame());

    GetPage().SetAttributionSupport(AttributionSupport::kWeb);
  }

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
    PageTestBase::TearDown();
  }

 protected:
  Persistent<AttributionSrcLocalFrameClient> client_;
  Persistent<AttributionSrcLoader> attribution_src_loader_;
};

TEST_F(AttributionSrcLoaderTest, RegisterTrigger) {
  const struct {
    const std::optional<AttributionReportingEligibility> eligibility;
    const std::string name;
  } kTestCases[] = {
      {std::nullopt, "unset"},
      {AttributionReportingEligibility::kTrigger, "kTrigger"},
      {AttributionReportingEligibility::kEventSourceOrTrigger,
       "kEventSourceOrTrigger"},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE("Eligibility: " + test_case.name);
    KURL test_url = ToKURL("https://example1.com/foo.html");

    ResourceRequest request = GetAttributionRequest(test_url);
    if (test_case.eligibility) {
      request.SetAttributionReportingEligibility(test_case.eligibility.value());
    }

    ResourceResponse response(test_url);
    response.SetHttpStatusCode(200);
    response.SetHttpHeaderField(
        http_names::kAttributionReportingRegisterTrigger,
        AtomicString(R"({"event_trigger_data":[{"trigger_data": "7"}]})"));

    MockAttributionHost host(
        GetFrame().GetRemoteNavigationAssociatedInterfaces());
    attribution_src_loader_->MaybeRegisterAttributionHeaders(request, response);
    host.WaitUntilBoundAndFlush();

    auto* mock_data_host = host.mock_data_host();
    ASSERT_TRUE(mock_data_host);

    mock_data_host->Flush();
    EXPECT_EQ(mock_data_host->trigger_data().size(), 1u);
  }
}

TEST_F(AttributionSrcLoaderTest, AttributionSrcRequestsIgnored) {
  KURL test_url = ToKURL("https://example1.com/foo.html");
  ResourceRequest request(test_url);
  request.SetRequestContext(mojom::blink::RequestContextType::ATTRIBUTION_SRC);

  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterTrigger,
      AtomicString(R"({"event_trigger_data":[{"trigger_data": "7"}]})"));

  EXPECT_FALSE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
      request, response));
}

TEST_F(AttributionSrcLoaderTest, AttributionSrcRequestsInvalidEligibleHeaders) {
  KURL test_url = ToKURL("https://example1.com/foo.html");
  ResourceRequest request(test_url);
  request.SetRequestContext(mojom::blink::RequestContextType::ATTRIBUTION_SRC);

  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);

  const char* header_values[] = {"navigation-source, event-source, trigger",
                                 "!!!", ""};

  for (const char* header : header_values) {
    response.SetHttpHeaderField(
        http_names::kAttributionReportingRegisterTrigger, AtomicString(header));

    EXPECT_FALSE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
        request, response))
        << header;
  }
}

TEST_F(AttributionSrcLoaderTest, AttributionSrcRequest_HistogramsRecorded) {
  base::HistogramTester histograms;

  KURL url1 = ToKURL(kUrl);
  RegisterMockedURLLoad(url1, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->Register(AtomicString(kUrl), /*element=*/nullptr,
                                    network::mojom::ReferrerPolicy::kDefault);

  static constexpr char kUrl2[] = "https://example2.com/foo.html";
  KURL url2 = ToKURL(kUrl2);
  RegisterMockedErrorURLLoad(url2);

  attribution_src_loader_->Register(AtomicString(kUrl2), /*element=*/nullptr,
                                    network::mojom::ReferrerPolicy::kDefault);

  // True = 1.
  histograms.ExpectBucketCount("Conversions.AllowedByPermissionPolicy", 1, 2);

  // kRequested = 0.
  histograms.ExpectUniqueSample(kAttributionSrcRequestStatusMetric, 0, 2);

  url_test_helpers::ServeAsynchronousRequests();

  // kReceived = 1.
  histograms.ExpectBucketCount(kAttributionSrcRequestStatusMetric, 1, 1);

  // kFailed = 2.
  histograms.ExpectBucketCount(kAttributionSrcRequestStatusMetric, 2, 1);
}

TEST_F(AttributionSrcLoaderTest, Referrer) {
  KURL url = ToKURL("https://example1.com/foo.html");
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->Register(AtomicString(kUrl), /*element=*/nullptr,
                                    network::mojom::ReferrerPolicy::kDefault);

  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_EQ(client_->request_head().GetReferrerPolicy(),
            network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin);
}

TEST_F(AttributionSrcLoaderTest, NoReferrer) {
  KURL url = ToKURL("https://example1.com/foo.html");
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->Register(AtomicString(kUrl), /*element=*/nullptr,
                                    network::mojom::ReferrerPolicy::kNever);

  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_EQ(client_->request_head().GetReferrerPolicy(),
            network::mojom::ReferrerPolicy::kNever);
}

TEST_F(AttributionSrcLoaderTest, EligibleHeader_Register) {
  KURL url = ToKURL(kUrl);
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->Register(AtomicString(kUrl), /*element=*/nullptr,
                                    network::mojom::ReferrerPolicy::kDefault);

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
      /*has_transient_user_activation=*/true,
      network::mojom::ReferrerPolicy::kDefault);

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
  attribution_src_loader_->Register(AtomicString(kUrl), /*element=*/nullptr,
                                    network::mojom::ReferrerPolicy::kDefault);
  host.WaitUntilBoundAndFlush();
  url_test_helpers::ServeAsynchronousRequests();

  auto* mock_data_host = host.mock_data_host();
  ASSERT_TRUE(mock_data_host);
  EXPECT_EQ(mock_data_host->disconnects(), 1u);
}

TEST_F(AttributionSrcLoaderTest, NoneSupport_NoAttributionSrcRequest) {
  GetPage().SetAttributionSupport(AttributionSupport::kNone);

  base::HistogramTester histograms;

  KURL url = ToKURL(kUrl);
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->Register(AtomicString(kUrl), /*element=*/nullptr,
                                    network::mojom::ReferrerPolicy::kDefault);

  histograms.ExpectTotalCount(kAttributionSrcRequestStatusMetric, 0);
}

TEST_F(AttributionSrcLoaderTest, WebDisabled_TriggerNotRegistered) {
  KURL test_url = ToKURL("https://example1.com/foo.html");

  for (auto attribution_support :
       {AttributionSupport::kNone, AttributionSupport::kOs}) {
    ResourceRequest request =
        GetAttributionRequest(test_url, attribution_support);
    ResourceResponse response(test_url);
    response.SetHttpStatusCode(200);
    response.SetHttpHeaderField(
        http_names::kAttributionReportingRegisterTrigger,
        AtomicString(R"({"event_trigger_data":[{"trigger_data": "7"}]})"));

    MockAttributionHost host(
        GetFrame().GetRemoteNavigationAssociatedInterfaces());
    EXPECT_TRUE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
        request, response));
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

  ResourceRequest request = GetAttributionRequest(test_url);
  request.SetAttributionReportingEligibility(
      AttributionReportingEligibility::kTrigger);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(http_names::kAttributionReportingRegisterTrigger,
                              register_trigger_json);

  attribution_src_loader_->MaybeRegisterAttributionHeaders(request, response);
  histograms.ExpectUniqueSample("Conversions.HeadersSize.RegisterTrigger",
                                register_trigger_json.length(), 1);

  request.SetAttributionReportingEligibility(
      AttributionReportingEligibility::kEventSource);
  response.SetHttpHeaderField(http_names::kAttributionReportingRegisterSource,
                              register_source_json);

  attribution_src_loader_->MaybeRegisterAttributionHeaders(request, response);
  histograms.ExpectUniqueSample("Conversions.HeadersSize.RegisterSource",
                                register_source_json.length(), 1);
}

class AttributionSrcLoaderCrossAppWebRuntimeDisabledTest
    : public AttributionSrcLoaderTest {
 public:
  AttributionSrcLoaderCrossAppWebRuntimeDisabledTest() {
    WebRuntimeFeatures::EnableFeatureFromString(
        /*name=*/"AttributionReportingCrossAppWeb", /*enable=*/false);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      network::features::kAttributionReportingCrossAppWeb};
};

TEST_F(AttributionSrcLoaderCrossAppWebRuntimeDisabledTest,
       OsTriggerNotRegistered) {
  GetPage().SetAttributionSupport(AttributionSupport::kWebAndOs);

  KURL test_url = ToKURL("https://example1.com/foo.html");

  ResourceRequest request(test_url);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterOSTrigger,
      AtomicString(R"("https://r.test/x")"));

  EXPECT_FALSE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
      request, response));
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
  auto attribution_support = AttributionSupport::kWebAndOs;

  GetPage().SetAttributionSupport(attribution_support);

  KURL url = ToKURL(kUrl);
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->Register(AtomicString(kUrl), /*element=*/nullptr,
                                    network::mojom::ReferrerPolicy::kDefault);

  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_EQ(client_->request_head().GetAttributionReportingSupport(),
            attribution_support);
}

TEST_F(AttributionSrcLoaderCrossAppWebEnabledTest,
       SupportHeader_RegisterNavigation) {
  auto attribution_support = AttributionSupport::kWebAndOs;

  GetPage().SetAttributionSupport(attribution_support);

  KURL url = ToKURL(kUrl);
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  std::ignore = attribution_src_loader_->RegisterNavigation(
      /*navigation_url=*/KURL(), /*attribution_src=*/AtomicString(kUrl),
      /*element=*/MakeGarbageCollected<HTMLAnchorElement>(GetDocument()),
      /*has_transient_user_activation=*/true,
      network::mojom::ReferrerPolicy::kDefault);

  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_EQ(client_->request_head().GetAttributionReportingSupport(),
            attribution_support);
}

TEST_F(AttributionSrcLoaderCrossAppWebEnabledTest, RegisterOsTrigger) {
  KURL test_url = ToKURL("https://example1.com/foo.html");

  ResourceRequest request =
      GetAttributionRequest(test_url, AttributionSupport::kWebAndOs);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterOSTrigger,
      AtomicString(R"("https://r.test/x")"));

  MockAttributionHost host(
      GetFrame().GetRemoteNavigationAssociatedInterfaces());
  EXPECT_TRUE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
      request, response));
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

  KURL test_url = ToKURL("https://example1.com/foo.html");
  AtomicString os_registration(R"("https://r.test/x")");

  ResourceRequest request =
      GetAttributionRequest(test_url, AttributionSupport::kWebAndOs);
  request.SetAttributionReportingEligibility(
      AttributionReportingEligibility::kTrigger);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterOSTrigger, os_registration);

  EXPECT_TRUE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
      request, response));
  histograms.ExpectUniqueSample("Conversions.HeadersSize.RegisterOsTrigger",
                                os_registration.length(), 1);

  request.SetAttributionReportingEligibility(
      AttributionReportingEligibility::kEventSource);
  response.SetHttpHeaderField(http_names::kAttributionReportingRegisterOSSource,
                              os_registration);

  EXPECT_TRUE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
      request, response));
  histograms.ExpectUniqueSample("Conversions.HeadersSize.RegisterOsSource",
                                os_registration.length(), 1);
}

class AttributionSrcLoaderInBrowserMigrationEnabledTest
    : public AttributionSrcLoaderTest {
 public:
  AttributionSrcLoaderInBrowserMigrationEnabledTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kKeepAliveInBrowserMigration,
         blink::features::kAttributionReportingInBrowserMigration},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AttributionSrcLoaderInBrowserMigrationEnabledTest,
       MaybeRegisterAttributionHeaders_KeepAliveRequestsResponseIgnored) {
  KURL test_url = ToKURL("https://example1.com/foo.html");

  for (bool is_keep_alive : {true, false}) {
    ResourceRequest request = GetAttributionRequest(test_url);
    request.SetKeepalive(is_keep_alive);
    request.SetAttributionReportingEligibility(
        AttributionReportingEligibility::kTrigger);
    ResourceResponse response(test_url);
    response.SetHttpStatusCode(200);
    response.SetHttpHeaderField(
        http_names::kAttributionReportingRegisterTrigger,
        AtomicString(R"({"event_trigger_data":[{"trigger_data": "7"}]})"));

    EXPECT_EQ(attribution_src_loader_->MaybeRegisterAttributionHeaders(
                  request, response),
              is_keep_alive ? false : true);
  }
}

TEST_F(
    AttributionSrcLoaderInBrowserMigrationEnabledTest,
    MaybeRegisterAttributionHeadersNonKeepAlive_ResponseViaServiceWorkerProcessed) {
  KURL test_url = ToKURL("https://example1.com/foo.html");

  ResourceRequest request = GetAttributionRequest(test_url);
  request.SetKeepalive(true);
  request.SetAttributionReportingEligibility(
      AttributionReportingEligibility::kTrigger);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(
      http_names::kAttributionReportingRegisterTrigger,
      AtomicString(R"({"event_trigger_data":[{"trigger_data": "7"}]})"));
  response.SetWasFetchedViaServiceWorker(true);

  EXPECT_TRUE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
      request, response));
}

struct PreferredPlatformTestCase {
  bool feature_enabled = true;
  const char* info_header;
  bool has_web_header;
  bool has_os_header;
  AttributionSupport support;
  bool expected_web;
  bool expected_os;
};

const PreferredPlatformTestCase kPreferredPlatformTestCases[] = {
    {
        .info_header = nullptr,
        .has_web_header = true,
        .has_os_header = true,
        .support = AttributionSupport::kWebAndOs,
        .expected_web = false,
        .expected_os = false,
    },
    {
        .info_header = "preferred-platform=os",
        .has_web_header = true,
        .has_os_header = true,
        .support = AttributionSupport::kWebAndOs,
        .expected_web = false,
        .expected_os = true,
    },
    {
        .info_header = "preferred-platform=os",
        .has_web_header = true,
        .has_os_header = true,
        .support = AttributionSupport::kOs,
        .expected_web = false,
        .expected_os = true,
    },
    {
        .info_header = "preferred-platform=os",
        .has_web_header = true,
        .has_os_header = true,
        .support = AttributionSupport::kWeb,
        .expected_web = true,
        .expected_os = false,
    },
    {
        .info_header = "preferred-platform=os",
        .has_web_header = true,
        .has_os_header = true,
        .support = AttributionSupport::kNone,
        .expected_web = false,
        .expected_os = false,
    },
    {
        .info_header = "preferred-platform=os",
        .has_web_header = false,
        .has_os_header = true,
        .support = AttributionSupport::kWeb,
        .expected_web = false,
        .expected_os = false,
    },
    {
        .info_header = "preferred-platform=os",
        .has_web_header = true,
        .has_os_header = false,
        .support = AttributionSupport::kWeb,
        .expected_web = false,
        .expected_os = false,
    },
    {
        .info_header = "preferred-platform=web",
        .has_web_header = true,
        .has_os_header = true,
        .support = AttributionSupport::kWebAndOs,
        .expected_web = true,
        .expected_os = false,
    },
    {
        .info_header = "preferred-platform=web",
        .has_web_header = true,
        .has_os_header = true,
        .support = AttributionSupport::kWeb,
        .expected_web = true,
        .expected_os = false,
    },
    {
        .info_header = "preferred-platform=web",
        .has_web_header = true,
        .has_os_header = true,
        .support = AttributionSupport::kOs,
        .expected_web = false,
        .expected_os = true,
    },
    {
        .info_header = "preferred-platform=web",
        .has_web_header = true,
        .has_os_header = true,
        .support = AttributionSupport::kNone,
        .expected_web = false,
        .expected_os = false,
    },
    {
        .info_header = "preferred-platform=web",
        .has_web_header = true,
        .has_os_header = false,
        .support = AttributionSupport::kOs,
        .expected_web = false,
        .expected_os = false,
    },
    {
        .info_header = "preferred-platform=web",
        .has_web_header = false,
        .has_os_header = true,
        .support = AttributionSupport::kOs,
        .expected_web = false,
        .expected_os = false,
    },
};

class AttributionSrcLoaderPreferredPlatformEnabledTest
    : public AttributionSrcLoaderCrossAppWebEnabledTest,
      public ::testing::WithParamInterface<PreferredPlatformTestCase> {};

class AttributionSrcLoaderPreferredPlatformSourceTest
    : public AttributionSrcLoaderPreferredPlatformEnabledTest {};

INSTANTIATE_TEST_SUITE_P(All,
                         AttributionSrcLoaderPreferredPlatformSourceTest,
                         ::testing::ValuesIn(kPreferredPlatformTestCases));

TEST_P(AttributionSrcLoaderPreferredPlatformSourceTest, PreferredPlatform) {
  KURL test_url = ToKURL("https://example1.com/foo.html");

  const auto& test_case = GetParam();

  ResourceRequest request = GetAttributionRequest(test_url, test_case.support);
  request.SetAttributionReportingEligibility(
      AttributionReportingEligibility::kEventSourceOrTrigger);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  if (test_case.has_web_header) {
    response.SetHttpHeaderField(
        http_names::kAttributionReportingRegisterSource,
        AtomicString(R"({"destination":"https://destination.example"})"));
  }
  if (test_case.has_os_header) {
    response.SetHttpHeaderField(
        http_names::kAttributionReportingRegisterOSSource,
        AtomicString(R"("https://r.test/x")"));
  }
  if (test_case.info_header) {
    response.SetHttpHeaderField(http_names::kAttributionReportingInfo,
                                AtomicString(test_case.info_header));
  }

  MockAttributionHost host(
      GetFrame().GetRemoteNavigationAssociatedInterfaces());
  EXPECT_TRUE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
      request, response));
  host.WaitUntilBoundAndFlush();

  auto* mock_data_host = host.mock_data_host();
  ASSERT_TRUE(mock_data_host);

  mock_data_host->Flush();

  EXPECT_THAT(mock_data_host->source_data(),
              ::testing::SizeIs(test_case.expected_web));
  EXPECT_THAT(mock_data_host->os_sources(),
              ::testing::SizeIs(test_case.expected_os));
}

class AttributionSrcLoaderPreferredPlatformTriggerTest
    : public AttributionSrcLoaderPreferredPlatformEnabledTest {};

INSTANTIATE_TEST_SUITE_P(All,
                         AttributionSrcLoaderPreferredPlatformTriggerTest,
                         ::testing::ValuesIn(kPreferredPlatformTestCases));

TEST_P(AttributionSrcLoaderPreferredPlatformTriggerTest, PreferredPlatform) {
  KURL test_url = ToKURL("https://example1.com/foo.html");

  const auto& test_case = GetParam();

  ResourceRequest request = GetAttributionRequest(test_url, test_case.support);
  request.SetAttributionReportingEligibility(
      AttributionReportingEligibility::kEventSourceOrTrigger);
  ResourceResponse response(test_url);
  response.SetHttpStatusCode(200);
  if (test_case.has_web_header) {
    response.SetHttpHeaderField(
        http_names::kAttributionReportingRegisterTrigger,
        AtomicString(R"({})"));
  }
  if (test_case.has_os_header) {
    response.SetHttpHeaderField(
        http_names::kAttributionReportingRegisterOSTrigger,
        AtomicString(R"("https://r.test/x")"));
  }
  if (test_case.info_header) {
    response.SetHttpHeaderField(http_names::kAttributionReportingInfo,
                                AtomicString(test_case.info_header));
  }

  MockAttributionHost host(
      GetFrame().GetRemoteNavigationAssociatedInterfaces());
  EXPECT_TRUE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
      request, response));
  host.WaitUntilBoundAndFlush();

  auto* mock_data_host = host.mock_data_host();
  ASSERT_TRUE(mock_data_host);

  mock_data_host->Flush();

  EXPECT_THAT(mock_data_host->trigger_data(),
              ::testing::SizeIs(test_case.expected_web));
  EXPECT_THAT(mock_data_host->os_triggers(),
              ::testing::SizeIs(test_case.expected_os));
}

TEST_F(AttributionSrcLoaderTest, InvalidWebHeader_ErrorReported) {
  const struct {
    AtomicString header_name;
    attribution_reporting::RegistrationHeaderErrorDetails error_details;
  } kTestCases[] = {
      {
          http_names::kAttributionReportingRegisterSource,
          attribution_reporting::mojom::SourceRegistrationError::kInvalidJson,
      },
      {
          http_names::kAttributionReportingRegisterTrigger,
          attribution_reporting::mojom::TriggerRegistrationError::kInvalidJson,
      },
  };

  KURL test_url = ToKURL("https://example.com/foo.html");

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.header_name);
    for (const bool report_header_errors : {false, true}) {
      SCOPED_TRACE(report_header_errors);

      ResourceRequest request = GetAttributionRequest(test_url);
      request.SetAttributionReportingEligibility(
          AttributionReportingEligibility::kEventSourceOrTrigger);
      ResourceResponse response(test_url);
      response.SetHttpStatusCode(200);
      response.SetHttpHeaderField(test_case.header_name,
                                  AtomicString(R"(!!!)"));
      if (report_header_errors) {
        response.SetHttpHeaderField(http_names::kAttributionReportingInfo,
                                    AtomicString(R"(report-header-errors)"));
      }

      MockAttributionHost host(
          GetFrame().GetRemoteNavigationAssociatedInterfaces());
      EXPECT_TRUE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
          request, response));
      host.WaitUntilBoundAndFlush();

      auto* mock_data_host = host.mock_data_host();
      ASSERT_TRUE(mock_data_host);

      mock_data_host->Flush();
      if (report_header_errors) {
        EXPECT_THAT(mock_data_host->header_errors(),
                    ::testing::ElementsAre(
                        attribution_reporting::RegistrationHeaderError(
                            /*header_value=*/"!!!", test_case.error_details)));
      } else {
        EXPECT_THAT(mock_data_host->header_errors(), ::testing::IsEmpty());
      }
    }
  }
}

TEST_F(AttributionSrcLoaderTest,
       HasAttributionHeaderInAttributionSrcResponseMetric) {
  KURL url = ToKURL(kUrl);

  for (const bool has_header : {false, true}) {
    SCOPED_TRACE(has_header);

    base::HistogramTester histograms;

    ResourceResponse response(url);
    response.SetHttpStatusCode(200);
    if (has_header) {
      response.SetHttpHeaderField(
          http_names::kAttributionReportingRegisterSource, AtomicString("!"));
    }

    url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
        url, test::CoreTestDataPath("foo.html"),
        WrappedResourceResponse(std::move(response)));

    attribution_src_loader_->Register(AtomicString(kUrl), /*element=*/nullptr,
                                      network::mojom::ReferrerPolicy::kDefault);

    url_test_helpers::ServeAsynchronousRequests();

    histograms.ExpectBucketCount(
        "Conversions.HasAttributionHeaderInAttributionSrcResponse", has_header,
        1);

    url_test_helpers::RegisterMockedURLUnregister(url);
  }
}

TEST_F(AttributionSrcLoaderCrossAppWebEnabledTest,
       InvalidOsHeader_ErrorReported) {
  const struct {
    AtomicString header_name;
    attribution_reporting::RegistrationHeaderErrorDetails error_details;
  } kTestCases[] = {
      {
          http_names::kAttributionReportingRegisterOSSource,
          attribution_reporting::OsSourceRegistrationError(
              attribution_reporting::mojom::OsRegistrationError::kInvalidList),
      },
      {
          http_names::kAttributionReportingRegisterOSTrigger,
          attribution_reporting::OsTriggerRegistrationError(
              attribution_reporting::mojom::OsRegistrationError::kInvalidList),
      },
  };

  KURL test_url = ToKURL("https://example.com/foo.html");

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.header_name);
    for (const bool report_header_errors : {false, true}) {
      SCOPED_TRACE(report_header_errors);

      ResourceRequest request =
          GetAttributionRequest(test_url, AttributionSupport::kOs);
      request.SetAttributionReportingEligibility(
          AttributionReportingEligibility::kEventSourceOrTrigger);
      ResourceResponse response(test_url);
      response.SetHttpStatusCode(200);
      response.SetHttpHeaderField(test_case.header_name,
                                  AtomicString(R"(!!!)"));
      if (report_header_errors) {
        response.SetHttpHeaderField(http_names::kAttributionReportingInfo,
                                    AtomicString(R"(report-header-errors)"));
      }

      MockAttributionHost host(
          GetFrame().GetRemoteNavigationAssociatedInterfaces());
      EXPECT_TRUE(attribution_src_loader_->MaybeRegisterAttributionHeaders(
          request, response));
      host.WaitUntilBoundAndFlush();

      auto* mock_data_host = host.mock_data_host();
      ASSERT_TRUE(mock_data_host);

      mock_data_host->Flush();
      if (report_header_errors) {
        EXPECT_THAT(mock_data_host->header_errors(),
                    ::testing::ElementsAre(
                        attribution_reporting::RegistrationHeaderError(
                            /*header_value=*/"!!!", test_case.error_details)));
      } else {
        EXPECT_THAT(mock_data_host->header_errors(), ::testing::IsEmpty());
      }
    }
  }
}

// Regression test for https://crbug.com/363947060.
TEST_F(AttributionSrcLoaderTest,
       UnsetAttributionSupportForNonAttributionSrcRequest_NoCrash) {
  KURL url = ToKURL(kUrl);
  ResourceRequest request(url);

  ResourceResponse response(url);
  response.SetHttpStatusCode(200);
  response.SetHttpHeaderField(http_names::kAttributionReportingRegisterTrigger,
                              AtomicString(R"({})"));

  MockAttributionHost host(
      GetFrame().GetRemoteNavigationAssociatedInterfaces());
  attribution_src_loader_->MaybeRegisterAttributionHeaders(request, response);
  host.WaitUntilBoundAndFlush();

  auto* mock_data_host = host.mock_data_host();
  ASSERT_TRUE(mock_data_host);

  mock_data_host->Flush();
  EXPECT_EQ(mock_data_host->trigger_data().size(), 1u);
}

}  // namespace
}  // namespace blink
