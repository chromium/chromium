// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_host.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "content/browser/conversions/conversion_manager.h"
#include "content/browser/conversions/conversion_test_utils.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/fake_mojo_message_dispatch_context.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

const char kConversionUrl[] = "https://b.com";

blink::Impression CreateValidImpression() {
  blink::Impression result;
  result.conversion_destination = url::Origin::Create(GURL(kConversionUrl));
  result.reporting_origin = url::Origin::Create(GURL("https://c.com"));
  result.impression_data = 1UL;
  return result;
}

}  // namespace

class ConversionHostTest : public RenderViewHostTestHarness {
 public:
  ConversionHostTest() = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    static_cast<WebContentsImpl*>(web_contents())
        ->RemoveReceiverSetForTesting(blink::mojom::ConversionHost::Name_);

    conversion_host_ = ConversionHost::CreateForTesting(
        web_contents(), std::make_unique<TestManagerProvider>(&test_manager_));
    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
  }

  TestWebContents* contents() {
    return static_cast<TestWebContents*>(web_contents());
  }

  ConversionHost* conversion_host() { return conversion_host_.get(); }

 protected:
  TestConversionManager test_manager_;

 private:
  std::unique_ptr<ConversionHost> conversion_host_;
};

TEST_F(ConversionHostTest, ConversionInSubframe_BadMessage) {
  contents()->NavigateAndCommit(GURL("http://www.example.com"));

  // Create a subframe and use it as a target for the conversion registration
  // mojo.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  conversion_host()->SetCurrentTargetFrameForTesting(subframe);

  // Create a fake dispatch context to trigger a bad message in.
  FakeMojoMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();

  // Message should be ignored because it was registered from a subframe.
  conversion_host()->RegisterConversion(std::move(conversion));
  EXPECT_EQ("blink.mojom.ConversionHost can only be used by the main frame.",
            bad_message_observer.WaitForBadMessage());
  EXPECT_EQ(0u, test_manager_.num_conversions());
}

TEST_F(ConversionHostTest, ConversionOnInsecurePage_BadMessage) {
  // Create a page with an insecure origin.
  contents()->NavigateAndCommit(GURL("http://www.example.com"));
  conversion_host()->SetCurrentTargetFrameForTesting(main_rfh());

  FakeMojoMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin =
      url::Origin::Create(GURL("https://secure.com"));

  // Message should be ignored because it was registered from an insecure page.
  conversion_host()->RegisterConversion(std::move(conversion));
  EXPECT_EQ(
      "blink.mojom.ConversionHost can only be used in secure contexts with a "
      "secure conversion registration origin.",
      bad_message_observer.WaitForBadMessage());
  EXPECT_EQ(0u, test_manager_.num_conversions());
}

TEST_F(ConversionHostTest, ConversionWithInsecureReportingOrigin_BadMessage) {
  contents()->NavigateAndCommit(GURL("https://www.example.com"));
  conversion_host()->SetCurrentTargetFrameForTesting(main_rfh());

  FakeMojoMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin = url::Origin::Create(GURL("http://secure.com"));

  // Message should be ignored because it was registered with an insecure
  // redirect.
  conversion_host()->RegisterConversion(std::move(conversion));
  EXPECT_EQ(
      "blink.mojom.ConversionHost can only be used in secure contexts with a "
      "secure conversion registration origin.",
      bad_message_observer.WaitForBadMessage());
  EXPECT_EQ(0u, test_manager_.num_conversions());
}

TEST_F(ConversionHostTest, ValidConversion_NoBadMessage) {
  // Create a page with a secure origin.
  contents()->NavigateAndCommit(GURL("https://www.example.com"));
  conversion_host()->SetCurrentTargetFrameForTesting(main_rfh());

  // Create a fake dispatch context to listen for bad messages.
  FakeMojoMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin =
      url::Origin::Create(GURL("https://secure.com"));
  conversion_host()->RegisterConversion(std::move(conversion));

  // Run loop to allow the bad message code to run if a bad message was
  // triggered.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bad_message_observer.got_bad_message());
  EXPECT_EQ(1u, test_manager_.num_conversions());
}

TEST_F(ConversionHostTest, ValidConversionWithEmbedderDisable_NoConversion) {
  ConversionDisallowingContentBrowserClient disallowed_browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&disallowed_browser_client);

  // Create a page with a secure origin.
  contents()->NavigateAndCommit(GURL("https://www.example.com"));
  conversion_host()->SetCurrentTargetFrameForTesting(main_rfh());

  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin =
      url::Origin::Create(GURL("https://secure.com"));
  conversion_host()->RegisterConversion(std::move(conversion));

  EXPECT_EQ(0u, test_manager_.num_conversions());
  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(ConversionHostTest, EmbedderDisabledContext_ConversionDisallowed) {
  ConfigurableConversionTestBrowserClient browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&browser_client);

  browser_client.BlockConversionMeasurementInContext(
      base::nullopt /* impression_origin */,
      base::make_optional(url::Origin::Create(GURL("https://top.example"))),
      base::make_optional(
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
    conversion_host()->SetCurrentTargetFrameForTesting(main_rfh());

    blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
    conversion->reporting_origin =
        url::Origin::Create(test_case.reporting_origin);
    conversion_host()->RegisterConversion(std::move(conversion));

    EXPECT_EQ(static_cast<size_t>(test_case.conversion_allowed),
              test_manager_.num_conversions())
        << "Top frame url: " << test_case.top_frame_url
        << ", reporting origin: " << test_case.reporting_origin;

    test_manager_.Reset();
  }

  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(ConversionHostTest, EmbedderDisabledContext_ImpressionDisallowed) {
  ConfigurableConversionTestBrowserClient browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&browser_client);

  browser_client.BlockConversionMeasurementInContext(
      base::make_optional(url::Origin::Create(GURL("https://top.example"))),
      base::nullopt /* conversion_origin */,
      base::make_optional(
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

TEST_F(ConversionHostTest, ValidImpressionWithEmbedderDisable_NoImpression) {
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

TEST_F(ConversionHostTest, Conversion_AssociatedWithConversionSite) {
  // Create a page with a secure origin.
  contents()->NavigateAndCommit(GURL("https://sub.conversion.com"));
  conversion_host()->SetCurrentTargetFrameForTesting(main_rfh());

  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin =
      url::Origin::Create(GURL("https://secure.com"));
  conversion_host()->RegisterConversion(std::move(conversion));
  EXPECT_EQ(1u, test_manager_.num_conversions());

  // Verify that we use the domain of the page where the conversion occurred
  // instead of the origin.
  EXPECT_EQ(net::SchemefulSite(GURL("https://conversion.com")),
            test_manager_.last_conversion_destination());
}

TEST_F(ConversionHostTest, PerPageConversionMetrics) {
  base::HistogramTester histograms;

  contents()->NavigateAndCommit(GURL("https://www.example.com"));

  // Initial document should not log metrics.
  histograms.ExpectTotalCount("Conversions.RegisteredConversionsPerPage", 0);

  conversion_host()->SetCurrentTargetFrameForTesting(main_rfh());
  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin =
      url::Origin::Create(GURL("https://secure.com"));

  for (size_t i = 0u; i < 8u; i++) {
    conversion_host()->RegisterConversion(conversion->Clone());
    EXPECT_EQ(1u, test_manager_.num_conversions());
    test_manager_.Reset();
  }

  // Same document navs should not reset the counter.
  contents()->NavigateAndCommit(GURL("https://www.example.com#hash"));
  histograms.ExpectTotalCount("Conversions.RegisteredConversionsPerPage", 0);

  // Re-navigating should reset the counter.
  contents()->NavigateAndCommit(GURL("https://www.example-next.com"));
  histograms.ExpectUniqueSample("Conversions.RegisteredConversionsPerPage", 8,
                                1);
}

TEST_F(ConversionHostTest, NoManager_NoPerPageConversionMetrics) {
  // Replace the ConversionHost on the WebContents with one that is backed by a
  // null ConversionManager.
  static_cast<WebContentsImpl*>(web_contents())
      ->RemoveReceiverSetForTesting(blink::mojom::ConversionHost::Name_);
  auto conversion_host = ConversionHost::CreateForTesting(
      web_contents(), std::make_unique<TestManagerProvider>(nullptr));
  contents()->NavigateAndCommit(GURL("https://www.example.com"));

  base::HistogramTester histograms;
  conversion_host->SetCurrentTargetFrameForTesting(main_rfh());
  blink::mojom::ConversionPtr conversion = blink::mojom::Conversion::New();
  conversion->reporting_origin =
      url::Origin::Create(GURL("https://secure.com"));
  conversion_host->RegisterConversion(std::move(conversion));

  // Navigate again to trigger histogram code.
  contents()->NavigateAndCommit(GURL("https://www.example-next.com"));
  histograms.ExpectBucketCount("Conversions.RegisteredConversionsPerPage", 1,
                               0);
}

TEST_F(ConversionHostTest, NavigationWithNoImpression_Ignored) {
  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));
  NavigationSimulatorImpl::NavigateAndCommitFromDocument(GURL(kConversionUrl),
                                                         main_rfh());

  EXPECT_EQ(0u, test_manager_.num_impressions());
}

TEST_F(ConversionHostTest, ValidImpression_ForwardedToManager) {
  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));
  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();

  EXPECT_EQ(1u, test_manager_.num_impressions());
}

TEST_F(ConversionHostTest, ImpressionWithNoManagerAvilable_NoCrash) {
  // Replace the ConversionHost on the WebContents with one that is backed by a
  // null ConversionManager.
  static_cast<WebContentsImpl*>(web_contents())
      ->RemoveReceiverSetForTesting(blink::mojom::ConversionHost::Name_);
  auto conversion_host = ConversionHost::CreateForTesting(
      web_contents(), std::make_unique<TestManagerProvider>(nullptr));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();
}

TEST_F(ConversionHostTest, ImpressionInSubframe_Ignored) {
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
TEST_F(ConversionHostTest, ImpressionNavigationWithDeadInitiator_Ignored) {
  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();

  EXPECT_EQ(0u, test_manager_.num_impressions());
}

TEST_F(ConversionHostTest, ImpressionNavigationCommitsToErrorPage_Ignored) {
  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->Fail(net::ERR_FAILED);
  navigation->CommitErrorPage();

  EXPECT_EQ(0u, test_manager_.num_impressions());
}

TEST_F(ConversionHostTest, ImpressionNavigationAborts_Ignored) {
  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->AbortCommit();

  EXPECT_EQ(0u, test_manager_.num_impressions());
}

TEST_F(ConversionHostTest,
       CommittedOriginDiffersFromConversionDesintation_Ignored) {
  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL("https://different.com"), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(CreateValidImpression());
  navigation->Commit();

  EXPECT_EQ(0u, test_manager_.num_impressions());
}

TEST_F(ConversionHostTest,
       ImpressionNavigation_OriginTrustworthyChecksPerformed) {
  const char kLocalHost[] = "http://localhost";

  struct {
    std::string impression_origin;
    std::string conversion_origin;
    std::string reporting_origin;
    bool impression_expected;
  } kTestCases[] = {
      {kLocalHost /* impression_origin */, kLocalHost /* conversion_origin */,
       kLocalHost /* reporting_origin */, true /* impression_expected */},
      {"http://127.0.0.1" /* impression_origin */,
       "http://127.0.0.1" /* conversion_origin */,
       "http://127.0.0.1" /* reporting_origin */,
       true /* impression_expected */},
      {kLocalHost /* impression_origin */, kLocalHost /* conversion_origin */,
       "http://insecure.com" /* reporting_origin */,
       false /* impression_expected */},
      {kLocalHost /* impression_origin */,
       "http://insecure.com" /* conversion_origin */,
       kLocalHost /* reporting_origin */, false /* impression_expected */},
      {"http://insecure.com" /* impression_origin */,
       kLocalHost /* conversion_origin */, kLocalHost /* reporting_origin */,
       false /* impression_expected */},
      {"https://secure.com" /* impression_origin */,
       "https://secure.com" /* conversion_origin */,
       "https://secure.com" /* reporting_origin */,
       true /* impression_expected */},
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

}  // namespace content
