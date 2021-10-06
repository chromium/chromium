// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_host.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "content/browser/attribution_reporting/conversion_manager.h"
#include "content/browser/attribution_reporting/conversion_test_utils.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace content {

class AttributionHostTestPeer {
 public:
  static std::unique_ptr<AttributionHost> CreateAttributionHost(
      WebContents* web_contents,
      std::unique_ptr<ConversionManager::Provider>
          conversion_manager_provider) {
    return base::WrapUnique(new AttributionHost(
        web_contents, std::move(conversion_manager_provider)));
  }

  static void SetCurrentTargetFrameForTesting(
      AttributionHost* conversion_host,
      RenderFrameHost* render_frame_host) {
    conversion_host->receivers_.SetCurrentTargetFrameForTesting(
        render_frame_host);
  }
};

namespace {

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
        web_contents(), std::make_unique<TestManagerProvider>(&test_manager_));
    AttributionHost::SetReceiverImplForTesting(conversion_host_.get());

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
  TestConversionManager test_manager_;
  std::unique_ptr<AttributionHost> conversion_host_;
};

TEST_F(AttributionHostTest, ValidConversionInSubframe_NoBadMessage) {
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
  EXPECT_EQ(1u, test_manager_.num_conversions());

  EXPECT_EQ(net::SchemefulSite(GURL("https://www.example.com")),
            test_manager_.last_conversion_destination());
}

TEST_F(AttributionHostTest,
       ConversionInSubframe_ConversionDestinationMatchesMainFrame) {
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
  EXPECT_EQ(1u, test_manager_.num_conversions());

  EXPECT_EQ(net::SchemefulSite(GURL("https://www.example.com")),
            test_manager_.last_conversion_destination());
}

TEST_F(AttributionHostTest, ConversionInSubframeOnInsecurePage_BadMessage) {
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
      "blink.mojom.ConversionHost can only be used in secure contexts with a "
      "secure conversion registration origin.",
      bad_message_observer.WaitForBadMessage());
  EXPECT_EQ(0u, test_manager_.num_conversions());
}

TEST_F(AttributionHostTest,
       ConversionInSubframe_EmbeddedDisabledContextOnMainFrame) {
  // Verifies that conversions from subframes use the correct origins when
  // checking if the operation is allowed by the embedded.

  ConfigurableConversionTestBrowserClient browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&browser_client);

  browser_client.BlockConversionMeasurementInContext(
      /*impression_origin=*/absl::nullopt,
      absl::make_optional(
          url::Origin::Create(GURL("https://blocked-top.example"))),
      absl::make_optional(
          url::Origin::Create(GURL("https://blocked-reporting.example"))));

  struct {
    GURL top_frame_url;
    GURL reporting_origin;
    bool conversion_allowed;
  } kTestCases[] = {{GURL("https://blocked-top.example"),
                     GURL("https://blocked-reporting.example"), false},
                    {GURL("https://blocked-reporting.example"),
                     GURL("https://blocked-top.example"), true},
                    {GURL("https://other.example"),
                     GURL("https://blocked-reporting.example"), true}};

  for (const auto& test_case : kTestCases) {
    contents()->NavigateAndCommit(test_case.top_frame_url);

    // Create a subframe and use it as a target for the conversion registration
    // mojo.
    content::RenderFrameHostTester* rfh_tester =
        content::RenderFrameHostTester::For(main_rfh());
    content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
    subframe = NavigationSimulatorImpl::NavigateAndCommitFromDocument(
        GURL("https://www.another.com"), subframe);
    SetCurrentTargetFrameForTesting(subframe);

    blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
    conversion->reporting_origin =
        url::Origin::Create(test_case.reporting_origin);
    conversion_host_mojom()->RegisterConversion(std::move(conversion));

    EXPECT_EQ(static_cast<size_t>(test_case.conversion_allowed),
              test_manager_.num_conversions())
        << "Top frame url: " << test_case.top_frame_url
        << ", reporting origin: " << test_case.reporting_origin;

    test_manager_.Reset();
  }

  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(AttributionHostTest, ConversionOnInsecurePage_BadMessage) {
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
      "blink.mojom.ConversionHost can only be used in secure contexts with a "
      "secure conversion registration origin.",
      bad_message_observer.WaitForBadMessage());
  EXPECT_EQ(0u, test_manager_.num_conversions());
}

TEST_F(AttributionHostTest, ConversionWithInsecureReportingOrigin_BadMessage) {
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
      "blink.mojom.ConversionHost can only be used in secure contexts with a "
      "secure conversion registration origin.",
      bad_message_observer.WaitForBadMessage());
  EXPECT_EQ(0u, test_manager_.num_conversions());
}

TEST_F(AttributionHostTest, ValidConversion_NoBadMessage) {
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
  EXPECT_EQ(1u, test_manager_.num_conversions());
}

TEST_F(AttributionHostTest, ValidConversionWithEmbedderDisable_NoConversion) {
  ConversionDisallowingContentBrowserClient disallowed_browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&disallowed_browser_client);

  // Create a page with a secure origin.
  contents()->NavigateAndCommit(GURL("https://www.example.com"));
  SetCurrentTargetFrameForTesting(main_rfh());

  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin =
      url::Origin::Create(GURL("https://secure.com"));
  conversion_host_mojom()->RegisterConversion(std::move(conversion));

  EXPECT_EQ(0u, test_manager_.num_conversions());
  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(AttributionHostTest, EmbedderDisabledContext_ConversionDisallowed) {
  ConfigurableConversionTestBrowserClient browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&browser_client);

  browser_client.BlockConversionMeasurementInContext(
      /*impression_origin=*/absl::nullopt,
      absl::make_optional(url::Origin::Create(GURL("https://top.example"))),
      absl::make_optional(
          url::Origin::Create(GURL("https://embedded.example"))));

  struct {
    GURL top_frame_url;
    GURL reporting_origin;
    bool conversion_allowed;
  } kTestCases[] = {
      {GURL("https://top.example"), GURL("https://embedded.example"), false},
      {GURL("https://embedded.example"), GURL("https://top.example"), true},
      {GURL("https://other.example"), GURL("https://embedded.example"), true}};

  for (const auto& test_case : kTestCases) {
    contents()->NavigateAndCommit(test_case.top_frame_url);
    SetCurrentTargetFrameForTesting(main_rfh());

    blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
    conversion->reporting_origin =
        url::Origin::Create(test_case.reporting_origin);
    conversion_host_mojom()->RegisterConversion(std::move(conversion));

    EXPECT_EQ(static_cast<size_t>(test_case.conversion_allowed),
              test_manager_.num_conversions())
        << "Top frame url: " << test_case.top_frame_url
        << ", reporting origin: " << test_case.reporting_origin;

    test_manager_.Reset();
  }

  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(AttributionHostTest, EmbedderDisabledContext_ImpressionDisallowed) {
  ConfigurableConversionTestBrowserClient browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&browser_client);

  browser_client.BlockConversionMeasurementInContext(
      absl::make_optional(url::Origin::Create(GURL("https://top.example"))),
      /*conversion_origin=*/absl::nullopt,
      absl::make_optional(
          url::Origin::Create(GURL("https://embedded.example"))));

  struct {
    GURL top_frame_url;
    GURL reporting_origin;
    bool impression_allowed;
  } kTestCases[] = {
      {GURL("https://top.example"), GURL("https://embedded.example"), false},
      {GURL("https://embedded.example"), GURL("https://top.example"), true},
      {GURL("https://other.example"), GURL("https://embedded.example"), true}};

  for (const auto& test_case : kTestCases) {
    contents()->NavigateAndCommit(test_case.top_frame_url);
    auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
        GURL(kConversionUrl), main_rfh());
    navigation->SetInitiatorFrame(main_rfh());

    blink::Impression impression;
    impression.reporting_origin =
        url::Origin::Create(GURL(test_case.reporting_origin));
    impression.conversion_destination =
        url::Origin::Create(GURL(kConversionUrl));
    navigation->set_impression(std::move(impression));
    navigation->Commit();

    EXPECT_EQ(static_cast<size_t>(test_case.impression_allowed),
              test_manager_.num_impressions())
        << "Top frame url: " << test_case.top_frame_url
        << ", reporting origin: " << test_case.reporting_origin;

    test_manager_.Reset();
  }

  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(AttributionHostTest, ValidImpressionWithEmbedderDisable_NoImpression) {
  ConversionDisallowingContentBrowserClient disallowed_browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&disallowed_browser_client);

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));
  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();

  EXPECT_EQ(0u, test_manager_.num_impressions());
  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(AttributionHostTest, Conversion_AssociatedWithConversionSite) {
  // Create a page with a secure origin.
  contents()->NavigateAndCommit(GURL("https://sub.conversion.com"));
  SetCurrentTargetFrameForTesting(main_rfh());

  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin =
      url::Origin::Create(GURL("https://secure.com"));
  conversion_host_mojom()->RegisterConversion(std::move(conversion));
  EXPECT_EQ(1u, test_manager_.num_conversions());

  // Verify that we use the domain of the page where the conversion occurred
  // instead of the origin.
  EXPECT_EQ(net::SchemefulSite(GURL("https://conversion.com")),
            test_manager_.last_conversion_destination());
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
    conversion_host_mojom()->RegisterConversion(conversion->Clone());
    EXPECT_EQ(1u, test_manager_.num_conversions());
    test_manager_.Reset();
  }

  conversion->reporting_origin =
      url::Origin::Create(GURL("https://anothersecure.com"));
  conversion_host_mojom()->RegisterConversion(conversion->Clone());
  EXPECT_EQ(1u, test_manager_.num_conversions());
  test_manager_.Reset();

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
  // null ConversionManager.
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

TEST_F(AttributionHostTest, PerPageImpressionMetrics) {
  base::HistogramTester histograms;

  contents()->NavigateAndCommit(GURL("https://www.example.com"));

  // Initial document should not log metrics.
  histograms.ExpectTotalCount(
      "Conversions.UniqueReportingOriginsPerPage.Impressions", 0);

  SetCurrentTargetFrameForTesting(main_rfh());
  blink::Impression impression = CreateValidImpression();

  for (size_t i = 0u; i < 8u; i++) {
    conversion_host_mojom()->RegisterImpression(impression);

    // Run loop to allow the bad message code to run if a bad message was
    // triggered.
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(1u, test_manager_.num_impressions());
    test_manager_.Reset();
  }

  impression.reporting_origin =
      url::Origin::Create(GURL("https://anothersecure.com"));
  conversion_host_mojom()->RegisterImpression(impression);
  // Run loop to allow the bad message code to run if a bad message was
  // triggered.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_manager_.num_impressions());
  test_manager_.Reset();

  // Same document navs should not reset the counter.
  contents()->NavigateAndCommit(GURL("https://www.example.com#hash"));
  histograms.ExpectTotalCount(
      "Conversions.UniqueReportingOriginsPerPage.Impressions", 0);

  // Re-navigating should reset the counter.
  contents()->NavigateAndCommit(GURL("https://www.example-next.com"));

  histograms.ExpectBucketCount(
      "Conversions.UniqueReportingOriginsPerPage.Impressions", 2, 1);
}

TEST_F(AttributionHostTest, NoManager_NoPerPageImpressionMetrics) {
  // Replace the AttributionHost on the WebContents with one that is backed by a
  // null ConversionManager.
  conversion_host_ = AttributionHostTestPeer::CreateAttributionHost(
      web_contents(), std::make_unique<TestManagerProvider>(nullptr));
  AttributionHost::SetReceiverImplForTesting(conversion_host_.get());
  contents()->NavigateAndCommit(GURL("https://www.example.com"));

  base::HistogramTester histograms;
  SetCurrentTargetFrameForTesting(main_rfh());
  blink::Impression impression = CreateValidImpression();
  conversion_host_mojom()->RegisterImpression(std::move(impression));

  // Navigate again to trigger histogram code.
  contents()->NavigateAndCommit(GURL("https://www.example-next.com"));
  histograms.ExpectTotalCount(
      "Conversions.UniqueReportingOriginsPerPage.Impressions", 0);
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
  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));
  NavigationSimulatorImpl::NavigateAndCommitFromDocument(GURL(kConversionUrl),
                                                         main_rfh());

  EXPECT_EQ(0u, test_manager_.num_impressions());
}

TEST_F(AttributionHostTest, ValidImpression_ForwardedToManager) {
  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));
  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();

  EXPECT_EQ(1u, test_manager_.num_impressions());
}

TEST_F(AttributionHostTest, ImpressionWithNoManagerAvilable_NoCrash) {
  // Replace the AttributionHost on the WebContents with one that is backed by a
  // null ConversionManager.
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

  EXPECT_EQ(0u, test_manager_.num_impressions());
}

// Test that if we cannot access the initiator frame of the navigation, we
// ignore the associated impression.
TEST_F(AttributionHostTest, ImpressionNavigationWithDeadInitiator_Ignored) {
  base::HistogramTester histograms;

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  // This test explicitly requires no initiator frame being set.
  navigation->SetInitiatorFrame(nullptr);
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();

  EXPECT_EQ(0u, test_manager_.num_impressions());

  histograms.ExpectUniqueSample(
      "Conversions.ImpressionNavigationHasDeadInitiator", true, 2);
}

TEST_F(AttributionHostTest, ImpressionNavigationCommitsToErrorPage_Ignored) {
  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->Fail(net::ERR_FAILED);
  navigation->CommitErrorPage();

  EXPECT_EQ(0u, test_manager_.num_impressions());
}

TEST_F(AttributionHostTest, ImpressionNavigationAborts_Ignored) {
  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->AbortCommit();

  EXPECT_EQ(0u, test_manager_.num_impressions());
}

TEST_F(AttributionHostTest,
       CommittedOriginDiffersFromConversionDesintation_Ignored) {
  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL("https://different.com"), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();

  EXPECT_EQ(0u, test_manager_.num_impressions());
}

TEST_F(AttributionHostTest,
       ImpressionNavigation_OriginTrustworthyChecksPerformed) {
  const char kLocalHost[] = "http://localhost";

  struct {
    std::string impression_origin;
    std::string conversion_origin;
    std::string reporting_origin;
    bool impression_expected;
  } kTestCases[] = {
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

  for (const auto& test_case : kTestCases) {
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

    EXPECT_EQ(test_case.impression_expected, test_manager_.num_impressions())
        << "For test case: " << test_case.impression_origin << " | "
        << test_case.conversion_origin << " | " << test_case.reporting_origin;
    test_manager_.Reset();
  }
}

TEST_F(AttributionHostTest,
       ImpressionInSubframe_ImpressionOriginMatchesTopPageOrigin) {
  contents()->NavigateAndCommit(GURL("https://www.example.com"));

  // Create a subframe and use it as a target for the impression registration
  // mojo.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  subframe = NavigationSimulatorImpl::NavigateAndCommitFromDocument(
      GURL("https://www.impression.com"), subframe);
  SetCurrentTargetFrameForTesting(subframe);

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  const blink::Impression impression = CreateValidImpression();
  conversion_host_mojom()->RegisterImpression(impression);

  // Run loop to allow the bad message code to run if a bad message was
  // triggered.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bad_message_observer.got_bad_message());
  EXPECT_EQ(1u, test_manager_.num_impressions());

  EXPECT_EQ(url::Origin::Create(GURL("https://www.example.com")),
            test_manager_.last_impression_origin());
}

TEST_F(AttributionHostTest, ValidImpression_NoBadMessage) {
  // Create a page with a secure origin.
  contents()->NavigateAndCommit(GURL("https://www.example.com"));
  SetCurrentTargetFrameForTesting(main_rfh());

  // Create a fake dispatch context to listen for bad messages.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  const blink::Impression impression = CreateValidImpression();
  conversion_host_mojom()->RegisterImpression(impression);

  // Run loop to allow the bad message code to run if a bad message was
  // triggered.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bad_message_observer.got_bad_message());
  EXPECT_EQ(1u, test_manager_.num_impressions());
  EXPECT_EQ(StorableSource::SourceType::kEvent,
            test_manager_.last_impression_source_type());
  EXPECT_EQ(10, test_manager_.last_attribution_source_priority());
}

TEST_F(AttributionHostTest, RegisterImpression_RecordsAllowedMetric) {
  // Create a page with a secure origin.
  contents()->NavigateAndCommit(GURL("https://www.example.com"));
  SetCurrentTargetFrameForTesting(main_rfh());

  ConversionDisallowingContentBrowserClient disallowed_browser_client;
  ConfigurableConversionTestBrowserClient allowed_browser_client;

  const struct {
    TestContentBrowserClient* browser_client;
    bool want_allowed;
  } kTestCases[] = {
      {&allowed_browser_client, true},
      {&disallowed_browser_client, false},
  };

  for (const auto& test_case : kTestCases) {
    ContentBrowserClient* old_browser_client =
        SetBrowserClientForTesting(test_case.browser_client);

    base::HistogramTester histograms;
    conversion_host_mojom()->RegisterImpression(CreateValidImpression());
    histograms.ExpectUniqueSample("Conversions.RegisterImpressionAllowed",
                                  test_case.want_allowed, 1);

    SetBrowserClientForTesting(old_browser_client);
  }
}

TEST_F(AttributionHostTest, RegisterConversion_RecordsAllowedMetric) {
  // Create a page with a secure origin.
  contents()->NavigateAndCommit(GURL("https://www.example.com"));
  SetCurrentTargetFrameForTesting(main_rfh());

  ConversionDisallowingContentBrowserClient disallowed_browser_client;
  ConfigurableConversionTestBrowserClient allowed_browser_client;

  const struct {
    TestContentBrowserClient* browser_client;
    bool want_allowed;
  } kTestCases[] = {
      {&allowed_browser_client, true},
      {&disallowed_browser_client, false},
  };

  for (const auto& test_case : kTestCases) {
    ContentBrowserClient* old_browser_client =
        SetBrowserClientForTesting(test_case.browser_client);

    base::HistogramTester histograms;
    blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
    conversion->reporting_origin =
        url::Origin::Create(GURL("https://secure.com"));
    conversion_host_mojom()->RegisterConversion(std::move(conversion));
    histograms.ExpectUniqueSample("Conversions.RegisterConversionAllowed",
                                  test_case.want_allowed, 1);

    SetBrowserClientForTesting(old_browser_client);
  }
}

// In pre-loaded CCT navigations, the attribution can arrive after the
// navigation begins but before it's committed. Currently only used on Android
// but should work cross-platform.
TEST_F(AttributionHostTest, AndroidConversion_DuringNavigation) {
  std::string origin(
#if defined(OS_ANDROID)
      "android-app:com.any.app");
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme(kAndroidAppScheme, url::SCHEME_WITH_HOST);
#else
      "https://secure.com");
#endif

  auto navigation = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL(kConversionUrl), contents());
  navigation->Start();

  EXPECT_EQ(0u, test_manager_.num_impressions());

  conversion_host()->ReportAttributionForCurrentNavigation(
      url::Origin::Create(GURL(origin)), CreateValidImpression());

  EXPECT_EQ(0u, test_manager_.num_impressions());

  navigation->Commit();

  EXPECT_EQ(1u, test_manager_.num_impressions());
}

// In pre-loaded CCT navigations, the attribution can arrive after the
// navigation completes. Currently only used on Android but should work
// cross-platform.
TEST_F(AttributionHostTest, AndroidConversion_AfterNavigation) {
  std::string origin(
#if defined(OS_ANDROID)
      "android-app:com.any.app");
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme(kAndroidAppScheme, url::SCHEME_WITH_HOST);
#else
      "https://secure.com");
#endif

  auto navigation = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL(kConversionUrl), contents());
  navigation->Commit();

  EXPECT_EQ(0u, test_manager_.num_impressions());

  conversion_host()->ReportAttributionForCurrentNavigation(
      url::Origin::Create(GURL(origin)), CreateValidImpression());

  EXPECT_EQ(1u, test_manager_.num_impressions());

  // Make sure we don't allow repeated attributions for the same navigation.
  conversion_host()->ReportAttributionForCurrentNavigation(
      url::Origin::Create(GURL(origin)), CreateValidImpression());

  EXPECT_EQ(1u, test_manager_.num_impressions());
}

TEST_F(AttributionHostTest, AndroidConversion_AfterNavigation_SubDomain) {
  std::string origin(
#if defined(OS_ANDROID)
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

  EXPECT_EQ(1u, test_manager_.num_impressions());
}

// In pre-loaded CCT navigations, the attribution can arrive after the
// navigation completes, but the destination must match the attribution.
TEST_F(AttributionHostTest,
       AndroidConversion_AfterNavigation_WrongDestination) {
  std::string origin(
#if defined(OS_ANDROID)
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

  EXPECT_EQ(0u, test_manager_.num_impressions());

  // Navigating to the correct URL after navigation to the wrong one still
  // shouldn't allow the attribution.
  auto good_navigation = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL(kConversionUrl), contents());
  good_navigation->Commit();

  EXPECT_EQ(0u, test_manager_.num_impressions());
}

// Ensure we don't re-use pending Impressions after an aborted commit. Currently
// only used on Android but should work cross-platform.
TEST_F(AttributionHostTest, AndroidConversion_NavigationAborted) {
  std::string origin(
#if defined(OS_ANDROID)
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

  EXPECT_EQ(0u, test_manager_.num_impressions());

  auto navigation_commit = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL(kConversionUrl), contents());

  navigation_commit->Commit();

  EXPECT_EQ(0u, test_manager_.num_impressions());
}

// Ensure we don't re-use pending Impressions after an Error page commit.
// Currently only used on Android but should work cross-platform.
TEST_F(AttributionHostTest, AndroidConversion_NavigationError) {
  std::string origin(
#if defined(OS_ANDROID)
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

  EXPECT_EQ(0u, test_manager_.num_impressions());

  auto navigation_commit = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL(kConversionUrl), contents());

  navigation_commit->Commit();

  EXPECT_EQ(0u, test_manager_.num_impressions());
}

// We don't allow attributions before a navigation begins. Currently only used
// on Android but should work cross-platform.
TEST_F(AttributionHostTest, AndroidConversion_BeforeNavigation) {
  std::string origin(
#if defined(OS_ANDROID)
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

  EXPECT_EQ(0u, test_manager_.num_impressions());
}

// We ignore same-document navigations.
TEST_F(AttributionHostTest, AndroidConversion_SameDocument) {
  std::string origin(
#if defined(OS_ANDROID)
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

  EXPECT_EQ(1u, test_manager_.num_impressions());
}

#if defined(OS_ANDROID)
TEST_F(AttributionHostTest, AndroidConversion) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme(kAndroidAppScheme, url::SCHEME_WITH_HOST);
  auto navigation = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL(kConversionUrl), contents());
  navigation->set_initiator_origin(
      url::Origin::Create(GURL("android-app:com.any.app")));
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();

  EXPECT_EQ(1u, test_manager_.num_impressions());
}

TEST_F(AttributionHostTest, AndroidConversion_BadScheme) {
  auto navigation = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL(kConversionUrl), contents());
  navigation->set_initiator_origin(
      url::Origin::Create(GURL("https://com.any.app")));
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();

  EXPECT_EQ(0u, test_manager_.num_impressions());
}
#endif

}  // namespace
}  // namespace content
