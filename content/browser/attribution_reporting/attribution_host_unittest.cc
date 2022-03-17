// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_host.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_manager_provider.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace content {

class AttributionHostTestPeer {
 public:
  static std::unique_ptr<AttributionHost> CreateAttributionHost(
      WebContents* web_contents,
      std::unique_ptr<AttributionManagerProvider>
          attribution_manager_provider) {
    return base::WrapUnique(new AttributionHost(
        web_contents, std::move(attribution_manager_provider)));
  }

  static void SetCurrentTargetFrameForTesting(
      AttributionHost* conversion_host,
      RenderFrameHost* render_frame_host) {
    conversion_host->receivers_.SetCurrentTargetFrameForTesting(
        render_frame_host);
  }
};

namespace {

using ConversionMeasurementOperation =
    ::content::ContentBrowserClient::ConversionMeasurementOperation;

using testing::_;
using testing::InSequence;
using testing::Mock;

using Checkpoint = ::testing::MockFunction<void(int step)>;

const char kConversionUrl[] = "https://b.com";
const char kConversionUrlWithFragment[] = "https://b.com/#fragment";
const char kConversionUrlWithSubDomain[] = "https://sub.b.com";

blink::Impression CreateValidImpression() {
  blink::Impression result;
  result.conversion_destination = url::Origin::Create(GURL(kConversionUrl));
  result.reporting_origin = url::Origin::Create(GURL("https://c.com"));
  result.impression_data = 1UL;
  result.priority = 10;
  return result;
}

class AttributionHostTest : public RenderViewHostTestHarness {
 public:
  AttributionHostTest() = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    conversion_host_ = AttributionHostTestPeer::CreateAttributionHost(
        web_contents(), std::make_unique<TestManagerProvider>(&mock_manager_));
    AttributionHost::SetReceiverImplForTesting(conversion_host_.get());

    auto data_host_manager = std::make_unique<MockDataHostManager>();
    mock_data_host_manager_ = data_host_manager.get();
    mock_manager_.SetDataHostManager(std::move(data_host_manager));

    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
  }

  void TearDown() override {
    AttributionHost::SetReceiverImplForTesting(nullptr);
    RenderViewHostTestHarness::TearDown();
  }

  TestWebContents* contents() {
    return static_cast<TestWebContents*>(web_contents());
  }

  blink::mojom::ConversionHost* conversion_host_mojom() {
    return conversion_host_.get();
  }

  AttributionHost* conversion_host() { return conversion_host_.get(); }

  void SetCurrentTargetFrameForTesting(RenderFrameHost* render_frame_host) {
    AttributionHostTestPeer::SetCurrentTargetFrameForTesting(
        conversion_host_.get(), render_frame_host);
  }

 protected:
  MockAttributionManager mock_manager_;
  MockDataHostManager* mock_data_host_manager_;
  std::unique_ptr<AttributionHost> conversion_host_;
};

TEST_F(AttributionHostTest, ValidConversionInSubframe_NoBadMessage) {
  EXPECT_CALL(mock_manager_,
              HandleTrigger(TriggerDestinationOriginIs(
                  url::Origin::Create(GURL("https://www.example.com")))));

  contents()->NavigateAndCommit(GURL("https://www.example.com"));

  // Create a subframe and use it as a target for the conversion registration
  // mojo.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  SetCurrentTargetFrameForTesting(subframe);

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin =
      url::Origin::Create(GURL("https://secure.com"));
  conversion_host_mojom()->RegisterConversion(std::move(conversion));

  // Run loop to allow the bad message code to run if a bad message was
  // triggered.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

TEST_F(AttributionHostTest,
       ConversionInSubframe_ConversionDestinationMatchesMainFrame) {
  EXPECT_CALL(mock_manager_,
              HandleTrigger(TriggerDestinationOriginIs(
                  url::Origin::Create(GURL("https://www.example.com")))));

  contents()->NavigateAndCommit(GURL("https://www.example.com"));

  // Create a subframe and use it as a target for the conversion registration
  // mojo.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  subframe = NavigationSimulatorImpl::NavigateAndCommitFromDocument(
      GURL("https://www.conversion.com"), subframe);
  SetCurrentTargetFrameForTesting(subframe);

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin =
      url::Origin::Create(GURL("https://secure.com"));
  conversion_host_mojom()->RegisterConversion(std::move(conversion));

  // Run loop to allow the bad message code to run if a bad message was
  // triggered.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

TEST_F(AttributionHostTest, ConversionInSubframeOnInsecurePage_BadMessage) {
  EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);

  contents()->NavigateAndCommit(GURL("http://www.example.com"));

  // Create a subframe and use it as a target for the conversion registration
  // mojo.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  subframe = NavigationSimulatorImpl::NavigateAndCommitFromDocument(
      GURL("https://www.example.com"), subframe);
  SetCurrentTargetFrameForTesting(subframe);

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin =
      url::Origin::Create(GURL("https://secure.com"));
  conversion_host_mojom()->RegisterConversion(std::move(conversion));
  EXPECT_EQ(
      "blink.mojom.ConversionHost can only be used with a secure top-level "
      "frame.",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(AttributionHostTest, ConversionOnInsecurePage_BadMessage) {
  EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);

  // Create a page with an insecure origin.
  contents()->NavigateAndCommit(GURL("http://www.example.com"));
  SetCurrentTargetFrameForTesting(main_rfh());

  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin =
      url::Origin::Create(GURL("https://secure.com"));

  // Message should be ignored because it was registered from an insecure page.
  conversion_host_mojom()->RegisterConversion(std::move(conversion));
  EXPECT_EQ(
      "blink.mojom.ConversionHost can only be used in secure contexts.",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(AttributionHostTest, ConversionWithInsecureReportingOrigin_BadMessage) {
  EXPECT_CALL(mock_manager_, HandleTrigger).Times(0);

  contents()->NavigateAndCommit(GURL("https://www.example.com"));
  SetCurrentTargetFrameForTesting(main_rfh());

  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin = url::Origin::Create(GURL("http://secure.com"));

  // Message should be ignored because it was registered with an insecure
  // redirect.
  conversion_host_mojom()->RegisterConversion(std::move(conversion));
  EXPECT_EQ(
      "blink.mojom.ConversionHost can only be used with a secure conversion "
      "registration origin.",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(AttributionHostTest, ValidConversion_NoBadMessage) {
  EXPECT_CALL(mock_manager_, HandleTrigger);

  // Create a page with a secure origin.
  contents()->NavigateAndCommit(GURL("https://www.example.com"));
  SetCurrentTargetFrameForTesting(main_rfh());

  // Create a fake dispatch context to listen for bad messages.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin =
      url::Origin::Create(GURL("https://secure.com"));
  conversion_host_mojom()->RegisterConversion(std::move(conversion));

  // Run loop to allow the bad message code to run if a bad message was
  // triggered.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

TEST_F(AttributionHostTest, Conversion_AssociatedWithConversionSite) {
  // Verify that we use the domain of the page where the conversion occurred
  // instead of the origin.
  EXPECT_CALL(mock_manager_,
              HandleTrigger(TriggerDestinationOriginIs(
                  url::Origin::Create(GURL("https://sub.conversion.com")))));

  // Create a page with a secure origin.
  contents()->NavigateAndCommit(GURL("https://sub.conversion.com"));
  SetCurrentTargetFrameForTesting(main_rfh());

  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin =
      url::Origin::Create(GURL("https://secure.com"));
  conversion_host_mojom()->RegisterConversion(std::move(conversion));
}

TEST_F(AttributionHostTest, PerPageConversionMetrics) {
  base::HistogramTester histograms;

  contents()->NavigateAndCommit(GURL("https://www.example.com"));

  // Initial document should not log metrics.
  histograms.ExpectTotalCount("Conversions.RegisteredConversionsPerPage", 0);
  histograms.ExpectTotalCount(
      "Conversions.UniqueReportingOriginsPerPage.Conversions", 0);

  SetCurrentTargetFrameForTesting(main_rfh());
  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin =
      url::Origin::Create(GURL("https://secure.com"));

  for (size_t i = 0u; i < 8u; i++) {
    EXPECT_CALL(mock_manager_, HandleTrigger);
    conversion_host_mojom()->RegisterConversion(conversion->Clone());
    Mock::VerifyAndClear(&mock_manager_);
  }

  EXPECT_CALL(mock_manager_, HandleTrigger);
  conversion->reporting_origin =
      url::Origin::Create(GURL("https://anothersecure.com"));
  conversion_host_mojom()->RegisterConversion(conversion->Clone());
  Mock::VerifyAndClear(&mock_manager_);

  // Same document navs should not reset the counter.
  contents()->NavigateAndCommit(GURL("https://www.example.com#hash"));
  histograms.ExpectTotalCount("Conversions.RegisteredConversionsPerPage", 0);
  histograms.ExpectTotalCount(
      "Conversions.UniqueReportingOriginsPerPage.Conversions", 0);

  // Re-navigating should reset the counter.
  contents()->NavigateAndCommit(GURL("https://www.example-next.com"));

  // TODO(johnidel): This test creates a second conversion host which gets
  // injected with a TestManager. However, the AttributionHost owned by the
  // WebContents is still active for this test, and will record a zero sample in
  // this histogram. Consider modifying this test suite so that we do not have
  // metrics being recorded in multiple places.
  histograms.ExpectBucketCount("Conversions.RegisteredConversionsPerPage", 9,
                               1);
  histograms.ExpectBucketCount("Conversions.RegisteredConversionsPerPage", 1,
                               0);
  histograms.ExpectBucketCount(
      "Conversions.UniqueReportingOriginsPerPage.Conversions", 2, 1);
}

TEST_F(AttributionHostTest, NoManager_NoPerPageConversionMetrics) {
  // Replace the AttributionHost on the WebContents with one that is backed by a
  // null AttributionManager.
  conversion_host_ = AttributionHostTestPeer::CreateAttributionHost(
      web_contents(), std::make_unique<TestManagerProvider>(nullptr));
  AttributionHost::SetReceiverImplForTesting(conversion_host_.get());
  contents()->NavigateAndCommit(GURL("https://www.example.com"));

  base::HistogramTester histograms;
  SetCurrentTargetFrameForTesting(main_rfh());
  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin =
      url::Origin::Create(GURL("https://secure.com"));
  conversion_host_mojom()->RegisterConversion(std::move(conversion));

  // Navigate again to trigger histogram code.
  contents()->NavigateAndCommit(GURL("https://www.example-next.com"));
  histograms.ExpectBucketCount("Conversions.RegisteredConversionsPerPage", 1,
                               0);
  histograms.ExpectTotalCount(
      "Conversions.UniqueReportingOriginsPerPage.Conversions", 0);
}

TEST_F(AttributionHostTest, NavigationWithImpression_PerPageImpressionMetrics) {
  base::HistogramTester histograms;

  contents()->NavigateAndCommit(GURL("https://www.example.com"));

  // Initial document should not log metrics.
  histograms.ExpectTotalCount(
      "Conversions.UniqueReportingOriginsPerPage.Impressions", 0);

  blink::Impression impression = CreateValidImpression();

  for (size_t i = 0u; i < 2u; i++) {
    auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
        GURL(kConversionUrl), main_rfh());
    navigation->SetInitiatorFrame(main_rfh());
    navigation->set_impression(impression);
    navigation->Commit();
  }

  // Navigate again to trigger histogram code.
  contents()->NavigateAndCommit(GURL("https://www.example-next.com"));

  histograms.ExpectBucketCount(
      "Conversions.UniqueReportingOriginsPerPage.Impressions", 1, 2);
}

TEST_F(AttributionHostTest, NavigationWithNoImpression_Ignored) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));
  NavigationSimulatorImpl::NavigateAndCommitFromDocument(GURL(kConversionUrl),
                                                         main_rfh());
}

TEST_F(AttributionHostTest, ValidImpression_ForwardedToManager) {
  EXPECT_CALL(mock_manager_, HandleSource);

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));
  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();
}

TEST_F(AttributionHostTest, ValidAttributionSrc_ForwardedToManager) {
  auto impression = CreateValidImpression();
  impression.attribution_src_token = blink::AttributionSrcToken();

  EXPECT_CALL(*mock_data_host_manager_,
              NotifyNavigationForDataHost(
                  *impression.attribution_src_token,
                  url::Origin::Create(GURL("https://secure_impression.com")),
                  url::Origin::Create(GURL(kConversionUrl))));

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));
  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(std::move(impression));
  navigation->Commit();
}

TEST_F(AttributionHostTest, ImpressionWithNoManagerAvilable_NoCrash) {
  // Replace the AttributionHost on the WebContents with one that is backed by a
  // null AttributionManager.
  conversion_host_ = AttributionHostTestPeer::CreateAttributionHost(
      web_contents(), std::make_unique<TestManagerProvider>(nullptr));
  AttributionHost::SetReceiverImplForTesting(conversion_host_.get());

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();
}

TEST_F(AttributionHostTest, ImpressionInSubframe_Ignored) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  // Create a subframe and use it as a target for the conversion registration
  // mojo.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), subframe);
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();
}

// Test that if we cannot access the initiator frame of the navigation, we
// ignore the associated impression.
TEST_F(AttributionHostTest, ImpressionNavigationWithDeadInitiator_Ignored) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  base::HistogramTester histograms;

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  // This test explicitly requires no initiator frame being set.
  navigation->SetInitiatorFrame(nullptr);
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();

  histograms.ExpectUniqueSample(
      "Conversions.ImpressionNavigationHasDeadInitiator", true, 2);
}

TEST_F(AttributionHostTest, ImpressionNavigationCommitsToErrorPage_Ignored) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->Fail(net::ERR_FAILED);
  navigation->CommitErrorPage();
}

TEST_F(AttributionHostTest,
       AttributionSrcNavigationCommitsToErrorPage_Ignored) {
  auto impression = CreateValidImpression();
  impression.attribution_src_token = blink::AttributionSrcToken();

  EXPECT_CALL(
      *mock_data_host_manager_,
      NotifyNavigationFailure(impression.attribution_src_token.value()));

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(std::move(impression));
  navigation->Fail(net::ERR_FAILED);
  navigation->CommitErrorPage();
}

TEST_F(AttributionHostTest, ImpressionNavigationAborts_Ignored) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->AbortCommit();
}

TEST_F(AttributionHostTest, AttributionSrcNavigationAborts_Ignored) {
  auto impression = CreateValidImpression();
  impression.attribution_src_token = blink::AttributionSrcToken();

  EXPECT_CALL(
      *mock_data_host_manager_,
      NotifyNavigationFailure(impression.attribution_src_token.value()));

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(std::move(impression));
  navigation->AbortCommit();
}

TEST_F(AttributionHostTest,
       CommittedOriginDiffersFromConversionDesintation_Ignored) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL("https://different.com"), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();
}

namespace {
const char kLocalHost[] = "http://localhost";

struct OriginTrustworthyChecksTestCase {
  const char* impression_origin;
  const char* conversion_origin;
  const char* reporting_origin;
  bool impression_expected;
};

const OriginTrustworthyChecksTestCase kOriginTrustworthyChecksTestCases[] = {
    {.impression_origin = kLocalHost,
     .conversion_origin = kLocalHost,
     .reporting_origin = kLocalHost,
     .impression_expected = true},
    {.impression_origin = "http://127.0.0.1",
     .conversion_origin = "http://127.0.0.1",
     .reporting_origin = "http://127.0.0.1",
     .impression_expected = true},
    {.impression_origin = kLocalHost,
     .conversion_origin = kLocalHost,
     .reporting_origin = "http://insecure.com",
     .impression_expected = false},
    {.impression_origin = kLocalHost,
     .conversion_origin = "http://insecure.com",
     .reporting_origin = kLocalHost,
     .impression_expected = false},
    {.impression_origin = "http://insecure.com",
     .conversion_origin = kLocalHost,
     .reporting_origin = kLocalHost,
     .impression_expected = false},
    {.impression_origin = "https://secure.com",
     .conversion_origin = "https://secure.com",
     .reporting_origin = "https://secure.com",
     .impression_expected = true},
};

class AttributionHostOriginTrustworthyChecksTest
    : public AttributionHostTest,
      public ::testing::WithParamInterface<OriginTrustworthyChecksTestCase> {};

}  // namespace

TEST_P(AttributionHostOriginTrustworthyChecksTest,
       ImpressionNavigation_OriginTrustworthyChecksPerformed) {
  const OriginTrustworthyChecksTestCase& test_case = GetParam();

  EXPECT_CALL(mock_manager_, HandleSource).Times(test_case.impression_expected);

  contents()->NavigateAndCommit(GURL(test_case.impression_origin));
  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(test_case.conversion_origin), main_rfh());

  blink::Impression impression;
  impression.conversion_destination =
      url::Origin::Create(GURL(test_case.conversion_origin));
  impression.reporting_origin =
      url::Origin::Create(GURL(test_case.reporting_origin));
  navigation->set_impression(impression);
  navigation->SetInitiatorFrame(main_rfh());
  navigation->Commit();
}

INSTANTIATE_TEST_SUITE_P(
    AttributionHostOriginTrustworthyChecks,
    AttributionHostOriginTrustworthyChecksTest,
    ::testing::ValuesIn(kOriginTrustworthyChecksTestCases));

TEST_F(AttributionHostTest, DataHost_RegisteredWithContext) {
  EXPECT_CALL(
      *mock_data_host_manager_,
      RegisterDataHost(_, url::Origin::Create(GURL("https://top.example"))));

  contents()->NavigateAndCommit(GURL("https://top.example"));
  SetCurrentTargetFrameForTesting(main_rfh());

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  conversion_host_mojom()->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver());

  // Run loop to allow the bad message code to run if a bad message was
  // triggered.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

TEST_F(AttributionHostTest, DataHostOnInsecurePage_BadMessage) {
  contents()->NavigateAndCommit(GURL("http://top.example"));
  SetCurrentTargetFrameForTesting(main_rfh());

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  conversion_host_mojom()->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver());

  EXPECT_EQ(
      "blink.mojom.ConversionHost can only be used with a secure top-level "
      "frame.",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(AttributionHostTest, NavigationDataHostOnInsecurePage_BadMessage) {
  contents()->NavigateAndCommit(GURL("http://top.example"));
  SetCurrentTargetFrameForTesting(main_rfh());

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  conversion_host_mojom()->RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      blink::AttributionSrcToken());

  EXPECT_EQ(
      "blink.mojom.ConversionHost can only be used with a secure top-level "
      "frame.",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(AttributionHostTest, DataHostInSubframe_ContextIsOutermostFrame) {
  EXPECT_CALL(
      *mock_data_host_manager_,
      RegisterDataHost(_, url::Origin::Create(GURL("https://top.example"))));

  contents()->NavigateAndCommit(GURL("https://top.example"));

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  subframe = NavigationSimulatorImpl::NavigateAndCommitFromDocument(
      GURL("https://subframe.example"), subframe);
  SetCurrentTargetFrameForTesting(subframe);

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  conversion_host_mojom()->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver());

  // Run loop to allow the bad message code to run if a bad message was
  // triggered.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

TEST_F(AttributionHostTest, DataHostInSubframeOnInsecurePage_BadMessage) {
  contents()->NavigateAndCommit(GURL("http://top.example"));

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  subframe = NavigationSimulatorImpl::NavigateAndCommitFromDocument(
      GURL("https://subframe.example"), subframe);
  SetCurrentTargetFrameForTesting(subframe);

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  conversion_host_mojom()->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver());

  EXPECT_EQ(
      "blink.mojom.ConversionHost can only be used with a secure top-level "
      "frame.",
      bad_message_observer.WaitForBadMessage());
}

// In pre-loaded CCT navigations, the attribution can arrive after the
// navigation begins but before it's committed. Currently only used on Android
// but should work cross-platform.
TEST_F(AttributionHostTest, AndroidConversion_DuringNavigation) {
  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleSource).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleSource).Times(0);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_manager_, HandleSource);
  }

  std::string origin(
#if BUILDFLAG(IS_ANDROID)
      "android-app:com.any.app");
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme(kAndroidAppScheme, url::SCHEME_WITH_HOST);
#else
      "https://secure.com");
#endif

  auto navigation = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL(kConversionUrl), contents());
  navigation->Start();

  checkpoint.Call(1);

  conversion_host()->ReportAttributionForCurrentNavigation(
      url::Origin::Create(GURL(origin)), CreateValidImpression());

  checkpoint.Call(2);

  navigation->Commit();
}

// In pre-loaded CCT navigations, the attribution can arrive after the
// navigation completes. Currently only used on Android but should work
// cross-platform.
TEST_F(AttributionHostTest, AndroidConversion_AfterNavigation) {
  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(mock_manager_, HandleSource).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_manager_, HandleSource);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_manager_, HandleSource).Times(0);
  }

  std::string origin(
#if BUILDFLAG(IS_ANDROID)
      "android-app:com.any.app");
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme(kAndroidAppScheme, url::SCHEME_WITH_HOST);
#else
      "https://secure.com");
#endif

  auto navigation = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL(kConversionUrl), contents());
  navigation->Commit();

  checkpoint.Call(1);

  conversion_host()->ReportAttributionForCurrentNavigation(
      url::Origin::Create(GURL(origin)), CreateValidImpression());

  checkpoint.Call(2);

  // Make sure we don't allow repeated attributions for the same navigation.
  conversion_host()->ReportAttributionForCurrentNavigation(
      url::Origin::Create(GURL(origin)), CreateValidImpression());
}

TEST_F(AttributionHostTest, AndroidConversion_AfterNavigation_SubDomain) {
  EXPECT_CALL(mock_manager_, HandleSource);

  std::string origin(
#if BUILDFLAG(IS_ANDROID)
      "android-app:com.any.app");
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme(kAndroidAppScheme, url::SCHEME_WITH_HOST);
#else
      "https://secure.com");
#endif

  auto navigation = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL(kConversionUrlWithSubDomain), contents());
  navigation->Commit();

  conversion_host()->ReportAttributionForCurrentNavigation(
      url::Origin::Create(GURL(origin)), CreateValidImpression());
}

// In pre-loaded CCT navigations, the attribution can arrive after the
// navigation completes, but the destination must match the attribution.
TEST_F(AttributionHostTest,
       AndroidConversion_AfterNavigation_WrongDestination) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  std::string origin(
#if BUILDFLAG(IS_ANDROID)
      "android-app:com.any.app");
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme(kAndroidAppScheme, url::SCHEME_WITH_HOST);
#else
      "https://secure.com");
#endif

  auto bad_navigation = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL("https://other.com"), contents());
  bad_navigation->Commit();

  conversion_host()->ReportAttributionForCurrentNavigation(
      url::Origin::Create(GURL(origin)), CreateValidImpression());

  // Navigating to the correct URL after navigation to the wrong one still
  // shouldn't allow the attribution.
  auto good_navigation = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL(kConversionUrl), contents());
  good_navigation->Commit();
}

// Ensure we don't re-use pending Impressions after an aborted commit. Currently
// only used on Android but should work cross-platform.
TEST_F(AttributionHostTest, AndroidConversion_NavigationAborted) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  std::string origin(
#if BUILDFLAG(IS_ANDROID)
      "android-app:com.any.app");
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme(kAndroidAppScheme, url::SCHEME_WITH_HOST);
#else
      "https://secure.com");
#endif

  auto navigation_abort = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL(kConversionUrl), contents());
  navigation_abort->Start();

  conversion_host()->ReportAttributionForCurrentNavigation(
      url::Origin::Create(GURL(origin)), CreateValidImpression());

  navigation_abort->AbortCommit();

  auto navigation_commit = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL(kConversionUrl), contents());

  navigation_commit->Commit();
}

// Ensure we don't re-use pending Impressions after an Error page commit.
// Currently only used on Android but should work cross-platform.
TEST_F(AttributionHostTest, AndroidConversion_NavigationError) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  std::string origin(
#if BUILDFLAG(IS_ANDROID)
      "android-app:com.any.app");
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme(kAndroidAppScheme, url::SCHEME_WITH_HOST);
#else
      "https://secure.com");
#endif

  auto navigation_error = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL(kConversionUrl), contents());
  navigation_error->Start();

  conversion_host()->ReportAttributionForCurrentNavigation(
      url::Origin::Create(GURL(origin)), CreateValidImpression());

  navigation_error->Fail(net::ERR_UNEXPECTED);
  navigation_error->CommitErrorPage();

  auto navigation_commit = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL(kConversionUrl), contents());

  navigation_commit->Commit();
}

// We don't allow attributions before a navigation begins. Currently only used
// on Android but should work cross-platform.
TEST_F(AttributionHostTest, AndroidConversion_BeforeNavigation) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  std::string origin(
#if BUILDFLAG(IS_ANDROID)
      "android-app:com.any.app");
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme(kAndroidAppScheme, url::SCHEME_WITH_HOST);
#else
      "https://secure.com");
#endif

  auto navigation = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL(kConversionUrl), contents());

  conversion_host()->ReportAttributionForCurrentNavigation(
      url::Origin::Create(GURL(origin)), CreateValidImpression());

  navigation->Commit();
}

// We ignore same-document navigations.
TEST_F(AttributionHostTest, AndroidConversion_SameDocument) {
  EXPECT_CALL(mock_manager_, HandleSource);

  std::string origin(
#if BUILDFLAG(IS_ANDROID)
      "android-app:com.any.app");
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme(kAndroidAppScheme, url::SCHEME_WITH_HOST);
#else
      "https://secure.com");
#endif
  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->Commit();
  auto navigation2 = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrlWithFragment), main_rfh());
  navigation2->CommitSameDocument();

  conversion_host()->ReportAttributionForCurrentNavigation(
      url::Origin::Create(GURL(origin)), CreateValidImpression());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(AttributionHostTest, AndroidConversion) {
  EXPECT_CALL(mock_manager_, HandleSource);

  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme(kAndroidAppScheme, url::SCHEME_WITH_HOST);
  auto navigation = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL(kConversionUrl), contents());
  navigation->set_initiator_origin(
      url::Origin::Create(GURL("android-app:com.any.app")));
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();
}

TEST_F(AttributionHostTest, AndroidConversion_BadScheme) {
  EXPECT_CALL(mock_manager_, HandleSource).Times(0);

  auto navigation = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL(kConversionUrl), contents());
  navigation->set_initiator_origin(
      url::Origin::Create(GURL("https://com.any.app")));
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();
}
#endif

}  // namespace
}  // namespace content
