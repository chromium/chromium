// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_host.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/attribution_reporting/registration_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
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
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class AttributionHostTestPeer {
 public:
  static void SetCurrentTargetFrameForTesting(
      AttributionHost* conversion_host,
      RenderFrameHost* render_frame_host) {
    conversion_host->receivers_.SetCurrentTargetFrameForTesting(
        render_frame_host);
  }
};

namespace {

using ::attribution_reporting::SuitableOrigin;

using testing::_;
using testing::Return;

using ::attribution_reporting::mojom::RegistrationType;
using ::blink::mojom::AttributionNavigationType;

const char kConversionUrl[] = "https://b.com";

class MockDataHostManager : public AttributionDataHostManager {
 public:
  MockDataHostManager() = default;
  ~MockDataHostManager() override = default;

  MOCK_METHOD(
      void,
      RegisterDataHost,
      (mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
       SuitableOrigin context_origin,
       bool is_within_fenced_frame,
       RegistrationType),
      (override));

  MOCK_METHOD(
      bool,
      RegisterNavigationDataHost,
      (mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
       const blink::AttributionSrcToken& attribution_src_token,
       AttributionInputEvent input_event),
      (override));

  MOCK_METHOD(void,
              NotifyNavigationRedirectRegistration,
              (const blink::AttributionSrcToken& attribution_src_token,
               std::string header_value,
               SuitableOrigin reporting_origin,
               const SuitableOrigin& source_origin,
               AttributionInputEvent input_event,
               AttributionNavigationType,
               bool is_within_fenced_frame),
              (override));

  MOCK_METHOD(void,
              NotifyNavigationForDataHost,
              (const blink::AttributionSrcToken& attribution_src_token,
               const SuitableOrigin& source_origin,
               AttributionNavigationType,
               bool is_within_fenced_frame),
              (override));

  MOCK_METHOD(void,
              NotifyNavigationFailure,
              (const blink::AttributionSrcToken& attribution_src_token),
              (override));
};

class AttributionHostTest : public RenderViewHostTestHarness {
 public:
  AttributionHostTest() = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});

    RenderViewHostTestHarness::SetUp();

    auto data_host_manager = std::make_unique<MockDataHostManager>();
    mock_data_host_manager_ = data_host_manager.get();

    auto mock_manager = std::make_unique<MockAttributionManager>();
    mock_manager->SetDataHostManager(std::move(data_host_manager));
    OverrideAttributionManager(std::move(mock_manager));

    contents()->GetPrimaryMainFrame()->InitializeRenderFrameIfNeeded();
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

  void ClearAttributionManager() {
    mock_data_host_manager_ = nullptr;
    OverrideAttributionManager(nullptr);
  }

  MockDataHostManager* mock_data_host_manager() {
    return mock_data_host_manager_;
  }

 private:
  void OverrideAttributionManager(std::unique_ptr<AttributionManager> manager) {
    static_cast<StoragePartitionImpl*>(
        browser_context()->GetDefaultStoragePartition())
        ->OverrideAttributionManagerForTesting(std::move(manager));
  }

  raw_ptr<MockDataHostManager> mock_data_host_manager_;

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AttributionHostTest, NavigationWithNoImpression_Ignored) {
  EXPECT_CALL(*mock_data_host_manager(), NotifyNavigationForDataHost).Times(0);

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));
  NavigationSimulatorImpl::NavigateAndCommitFromDocument(GURL(kConversionUrl),
                                                         main_rfh());
}

TEST_F(AttributionHostTest, ValidAttributionSrc_ForwardedToManager) {
  blink::Impression impression;
  impression.nav_type = AttributionNavigationType::kWindowOpen;

  EXPECT_CALL(*mock_data_host_manager(),
              NotifyNavigationForDataHost(
                  impression.attribution_src_token,
                  *SuitableOrigin::Deserialize("https://secure_impression.com"),
                  impression.nav_type,
                  /*is_within_fenced_frame=*/false));

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));
  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(std::move(impression));
  navigation->Commit();
}

TEST_F(AttributionHostTest, ImpressionWithNoManagerAvailable_NoCrash) {
  ClearAttributionManager();

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(blink::Impression());
  navigation->Commit();
}

TEST_F(AttributionHostTest, ImpressionInSubframe_Ignored) {
  EXPECT_CALL(*mock_data_host_manager(), NotifyNavigationForDataHost).Times(0);

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
  EXPECT_CALL(*mock_data_host_manager(), NotifyNavigationForDataHost).Times(0);

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
  EXPECT_CALL(*mock_data_host_manager(), NotifyNavigationForDataHost).Times(0);

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

  EXPECT_CALL(*mock_data_host_manager(),
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
  EXPECT_CALL(*mock_data_host_manager(), NotifyNavigationForDataHost).Times(0);

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(blink::Impression());
  navigation->AbortCommit();
}

TEST_F(AttributionHostTest, AttributionSrcNavigationAborts_Ignored) {
  blink::Impression impression;

  EXPECT_CALL(*mock_data_host_manager(),
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
  EXPECT_CALL(*mock_data_host_manager(), NotifyNavigationForDataHost);

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
  const char* source_origin;
  const char* destination_origin;
  bool expected_valid;
};

const OriginTrustworthyChecksTestCase kOriginTrustworthyChecksTestCases[] = {
    {.source_origin = kLocalHost,
     .destination_origin = kLocalHost,
     .expected_valid = true},
    {.source_origin = "http://127.0.0.1",
     .destination_origin = "http://127.0.0.1",
     .expected_valid = true},
    {.source_origin = kLocalHost,
     .destination_origin = "http://insecure.com",
     .expected_valid = true},
    {.source_origin = "http://insecure.com",
     .destination_origin = kLocalHost,
     .expected_valid = false},
    {.source_origin = "https://secure.com",
     .destination_origin = "https://secure.com",
     .expected_valid = true},
};

class AttributionHostOriginTrustworthyChecksTest
    : public AttributionHostTest,
      public ::testing::WithParamInterface<OriginTrustworthyChecksTestCase> {};

}  // namespace

TEST_P(AttributionHostOriginTrustworthyChecksTest,
       ImpressionNavigation_OriginTrustworthyChecksPerformed) {
  const OriginTrustworthyChecksTestCase& test_case = GetParam();

  if (test_case.expected_valid) {
    EXPECT_CALL(*mock_data_host_manager(), NotifyNavigationForDataHost);
  } else {
    EXPECT_CALL(*mock_data_host_manager(), NotifyNavigationFailure);
  }

  contents()->NavigateAndCommit(GURL(test_case.source_origin));
  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(test_case.destination_origin), main_rfh());

  navigation->set_impression(blink::Impression());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->Commit();
}

INSTANTIATE_TEST_SUITE_P(
    AttributionHostOriginTrustworthyChecks,
    AttributionHostOriginTrustworthyChecksTest,
    ::testing::ValuesIn(kOriginTrustworthyChecksTestCases));

TEST_F(AttributionHostTest, DataHost_RegisteredWithContext) {
  EXPECT_CALL(*mock_data_host_manager(),
              RegisterDataHost(
                  _, *SuitableOrigin::Deserialize("https://top.example"),
                  /*is_within_fenced_frame=*/false, RegistrationType::kSource));

  contents()->NavigateAndCommit(GURL("https://top.example"));
  SetCurrentTargetFrameForTesting(main_rfh());

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  conversion_host_mojom()->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), RegistrationType::kSource);

  // Run loop to allow the bad message code to run if a bad message was
  // triggered.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

// crbug.com/1378749.
TEST_F(AttributionHostTest, DISABLED_DataHostOnInsecurePage_BadMessage) {
  contents()->NavigateAndCommit(GURL("http://top.example"));
  SetCurrentTargetFrameForTesting(main_rfh());

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  conversion_host_mojom()->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), RegistrationType::kSource);

  EXPECT_EQ(
      "blink.mojom.ConversionHost can only be used with a secure top-level "
      "frame.",
      bad_message_observer.WaitForBadMessage());
}

// crbug.com/1378749.
TEST_F(AttributionHostTest,
       DISABLED_NavigationDataHostOnInsecurePage_BadMessage) {
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
  ON_CALL(*mock_data_host_manager(), RegisterNavigationDataHost)
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
  EXPECT_CALL(*mock_data_host_manager(),
              RegisterDataHost(
                  _, *SuitableOrigin::Deserialize("https://top.example"),
                  /*is_within_fenced_frame=*/false, RegistrationType::kSource));

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
      data_host_remote.BindNewPipeAndPassReceiver(), RegistrationType::kSource);

  // Run loop to allow the bad message code to run if a bad message was
  // triggered.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

// crbug.com/1378749.
TEST_F(AttributionHostTest,
       DISABLED_DataHostInSubframeOnInsecurePage_BadMessage) {
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
      data_host_remote.BindNewPipeAndPassReceiver(), RegistrationType::kSource);

  EXPECT_EQ(
      "blink.mojom.ConversionHost can only be used with a secure top-level "
      "frame.",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(AttributionHostTest, DataHost_RegisteredWithFencedFrame) {
  EXPECT_CALL(*mock_data_host_manager(),
              RegisterDataHost(
                  _, *SuitableOrigin::Deserialize("https://top.example"),
                  /*is_within_fenced_frame=*/true, RegistrationType::kSource));

  contents()->NavigateAndCommit(GURL("https://top.example"));
  RenderFrameHost* fenced_frame =
      RenderFrameHostTester::For(main_rfh())
          ->AppendFencedFrame(blink::mojom::FencedFrameMode::kOpaqueAds);
  fenced_frame = NavigationSimulatorImpl::NavigateAndCommitFromDocument(
      GURL("https://fencedframe.example"), fenced_frame);
  SetCurrentTargetFrameForTesting(fenced_frame);

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  conversion_host_mojom()->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), RegistrationType::kSource);

  // Run loop to allow the bad message code to run if a bad message was
  // triggered.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

}  // namespace
}  // namespace content
