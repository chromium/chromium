// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_host.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_manager_provider.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_client.h"
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

namespace content {

class AttributionHostTestPeer {
 public:
  static void SetAttributionManagerProvider(
      AttributionHost* host,
      std::unique_ptr<AttributionManagerProvider> provider) {
    host->attribution_manager_provider_ = std::move(provider);
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
using testing::Mock;
using testing::Return;

const char kConversionUrl[] = "https://b.com";

class AttributionHostTest : public RenderViewHostTestHarness {
 public:
  AttributionHostTest() = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    AttributionHostTestPeer::SetAttributionManagerProvider(
        conversion_host(),
        std::make_unique<TestManagerProvider>(&mock_manager_));

    auto data_host_manager = std::make_unique<MockDataHostManager>();
    mock_data_host_manager_ = data_host_manager.get();
    mock_manager_.SetDataHostManager(std::move(data_host_manager));

    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
  }

  TestWebContents* contents() {
    return static_cast<TestWebContents*>(web_contents());
  }

  blink::mojom::ConversionHost* conversion_host_mojom() {
    return conversion_host();
  }

  AttributionHost* conversion_host() {
    return AttributionHost::FromWebContents(web_contents());
  }

  void SetCurrentTargetFrameForTesting(RenderFrameHost* render_frame_host) {
    AttributionHostTestPeer::SetCurrentTargetFrameForTesting(conversion_host(),
                                                             render_frame_host);
  }

 private:
  MockAttributionManager mock_manager_;

 protected:
  MockDataHostManager* mock_data_host_manager_;
};

TEST_F(AttributionHostTest, NavigationWithNoImpression_Ignored) {
  EXPECT_CALL(*mock_data_host_manager_, NotifyNavigationForDataHost).Times(0);

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));
  NavigationSimulatorImpl::NavigateAndCommitFromDocument(GURL(kConversionUrl),
                                                         main_rfh());
}

TEST_F(AttributionHostTest, ValidAttributionSrc_ForwardedToManager) {
  blink::Impression impression;

  EXPECT_CALL(*mock_data_host_manager_,
              NotifyNavigationForDataHost(
                  impression.attribution_src_token,
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
  AttributionHostTestPeer::SetAttributionManagerProvider(
      conversion_host(), std::make_unique<TestManagerProvider>(nullptr));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(blink::Impression());
  navigation->Commit();
}

TEST_F(AttributionHostTest, ImpressionInSubframe_Ignored) {
  EXPECT_CALL(*mock_data_host_manager_, NotifyNavigationForDataHost).Times(0);

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  // Create a subframe and use it as a target for the conversion registration
  // mojo.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), subframe);
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(blink::Impression());
  navigation->Commit();
}

// Test that if we cannot access the initiator frame of the navigation, we
// ignore the associated impression.
TEST_F(AttributionHostTest, ImpressionNavigationWithDeadInitiator_Ignored) {
  EXPECT_CALL(*mock_data_host_manager_, NotifyNavigationForDataHost).Times(0);

  base::HistogramTester histograms;

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  // This test explicitly requires no initiator frame being set.
  navigation->SetInitiatorFrame(nullptr);
  navigation->set_impression(blink::Impression());
  navigation->Commit();

  histograms.ExpectUniqueSample(
      "Conversions.ImpressionNavigationHasDeadInitiator", true, 1);
}

TEST_F(AttributionHostTest, ImpressionNavigationCommitsToErrorPage_Ignored) {
  EXPECT_CALL(*mock_data_host_manager_, NotifyNavigationForDataHost).Times(0);

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(blink::Impression());
  navigation->Fail(net::ERR_FAILED);
  navigation->CommitErrorPage();
}

TEST_F(AttributionHostTest,
       AttributionSrcNavigationCommitsToErrorPage_Ignored) {
  blink::Impression impression;

  EXPECT_CALL(*mock_data_host_manager_,
              NotifyNavigationFailure(impression.attribution_src_token));

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(std::move(impression));
  navigation->Fail(net::ERR_FAILED);
  navigation->CommitErrorPage();
}

TEST_F(AttributionHostTest, ImpressionNavigationAborts_Ignored) {
  EXPECT_CALL(*mock_data_host_manager_, NotifyNavigationForDataHost).Times(0);

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(blink::Impression());
  navigation->AbortCommit();
}

TEST_F(AttributionHostTest, AttributionSrcNavigationAborts_Ignored) {
  blink::Impression impression;

  EXPECT_CALL(*mock_data_host_manager_,
              NotifyNavigationFailure(impression.attribution_src_token));

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(std::move(impression));
  navigation->AbortCommit();
}

TEST_F(AttributionHostTest,
       CommittedOriginDiffersFromConversionDesintation_Propagated) {
  EXPECT_CALL(*mock_data_host_manager_, NotifyNavigationForDataHost);

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL("https://different.com"), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(blink::Impression());
  navigation->Commit();
}

namespace {
const char kLocalHost[] = "http://localhost";

struct OriginTrustworthyChecksTestCase {
  const char* impression_origin;
  const char* conversion_origin;
  bool impression_expected;
};

const OriginTrustworthyChecksTestCase kOriginTrustworthyChecksTestCases[] = {
    {.impression_origin = kLocalHost,
     .conversion_origin = kLocalHost,
     .impression_expected = true},
    {.impression_origin = "http://127.0.0.1",
     .conversion_origin = "http://127.0.0.1",
     .impression_expected = true},
    {.impression_origin = kLocalHost,
     .conversion_origin = "http://insecure.com",
     .impression_expected = true},
    {.impression_origin = "http://insecure.com",
     .conversion_origin = kLocalHost,
     .impression_expected = true},
    {.impression_origin = "https://secure.com",
     .conversion_origin = "https://secure.com",
     .impression_expected = true},
};

class AttributionHostOriginTrustworthyChecksTest
    : public AttributionHostTest,
      public ::testing::WithParamInterface<OriginTrustworthyChecksTestCase> {};

}  // namespace

TEST_P(AttributionHostOriginTrustworthyChecksTest,
       ImpressionNavigation_OriginTrustworthyChecksPerformed) {
  const OriginTrustworthyChecksTestCase& test_case = GetParam();

  EXPECT_CALL(*mock_data_host_manager_, NotifyNavigationForDataHost)
      .Times(test_case.impression_expected);

  contents()->NavigateAndCommit(GURL(test_case.impression_origin));
  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(test_case.conversion_origin), main_rfh());

  navigation->set_impression(blink::Impression());
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

TEST_F(AttributionHostTest, DuplicateAttributionSrcToken_BadMessage) {
  ON_CALL(*mock_data_host_manager_, RegisterNavigationDataHost)
      .WillByDefault(Return(false));

  contents()->NavigateAndCommit(GURL("https://top.example"));
  SetCurrentTargetFrameForTesting(main_rfh());

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  conversion_host_mojom()->RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      blink::AttributionSrcToken());

  EXPECT_EQ(
      "Renderer attempted to register a data host with a duplicate "
      "AttribtionSrcToken.",
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

}  // namespace
}  // namespace content
