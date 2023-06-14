// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_host.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/attribution_reporting/registration_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_beacon_id.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/test/mock_attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/test/mock_attribution_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/common/content_features.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_declaration.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"
#include "third_party/blink/public/mojom/conversions/conversions.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// Friended helper class to access private `receivers_` for test.
class AttributionHostTestPeer {
 public:
  static void SetCurrentTargetFrameForTesting(
      AttributionHost* attribution_host,
      RenderFrameHost* render_frame_host) {
    attribution_host->receivers_.SetCurrentTargetFrameForTesting(
        render_frame_host);
  }
};

namespace {

using ::attribution_reporting::SuitableOrigin;

using ::testing::_;
using ::testing::Optional;
using ::testing::Return;

using ::attribution_reporting::mojom::RegistrationType;
using ::blink::mojom::AttributionNavigationType;

const char kConversionUrl[] = "https://b.com";

constexpr BeaconId kBeaconId(123);
constexpr int64_t kNavigationId(456);

class AttributionHostTest : public RenderViewHostTestHarness {
 public:
  AttributionHostTest() = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});

    RenderViewHostTestHarness::SetUp();

    auto data_host_manager = std::make_unique<MockAttributionDataHostManager>();
    mock_data_host_manager_ = data_host_manager.get();

    auto mock_manager = std::make_unique<MockAttributionManager>();
    mock_manager->SetDataHostManager(std::move(data_host_manager));
    OverrideAttributionManager(std::move(mock_manager));

    contents()->GetPrimaryMainFrame()->InitializeRenderFrameIfNeeded();
  }

  void TearDown() override {
    // Avoids dangling ref to `mock_data_host_manager_`.
    ClearAttributionManager();
    RenderViewHostTestHarness::TearDown();
  }

  TestWebContents* contents() {
    return static_cast<TestWebContents*>(web_contents());
  }

  blink::mojom::AttributionHost* attribution_host_mojom() {
    return attribution_host();
  }

  AttributionHost* attribution_host() {
    return AttributionHost::FromWebContents(web_contents());
  }

  void SetFencedFrameConfigPermissions(RenderFrameHost* fenced_frame) {
    // Permissions in a fenced frame are tied to its urn:uuid-bound properties
    // object. Because no navigation actually takes place in these tests, the
    // code path that sets the permissions in the properties is never actually
    // exercised. Instead, we need to manually inject the permissions into the
    // properties object to ensure that the fenced frame loads with the needed
    // permissions policy set.
    FrameTreeNode* fenced_frame_node =
        static_cast<RenderFrameHostImpl*>(fenced_frame)->frame_tree_node();
    absl::optional<FencedFrameProperties> new_props =
        fenced_frame_node->GetFencedFrameProperties();
    new_props->required_permissions_to_load.push_back(
        blink::mojom::PermissionsPolicyFeature::kAttributionReporting);
    fenced_frame_node->set_fenced_frame_properties(new_props);
  }

  void ClearAttributionManager() {
    mock_data_host_manager_ = nullptr;
    OverrideAttributionManager(nullptr);
  }

  MockAttributionDataHostManager* mock_data_host_manager() {
    return mock_data_host_manager_;
  }

 private:
  void OverrideAttributionManager(std::unique_ptr<AttributionManager> manager) {
    static_cast<StoragePartitionImpl*>(
        browser_context()->GetDefaultStoragePartition())
        ->OverrideAttributionManagerForTesting(std::move(manager));
  }

  raw_ptr<MockAttributionDataHostManager> mock_data_host_manager_;

  base::test::ScopedFeatureList feature_list_;
};

class ScopedAttributionHostTargetFrame {
 public:
  ScopedAttributionHostTargetFrame(AttributionHost* attribution_host,
                                   RenderFrameHost* render_frame_host)
      : attribution_host_(attribution_host) {
    AttributionHostTestPeer::SetCurrentTargetFrameForTesting(attribution_host_,
                                                             render_frame_host);
  }

  ~ScopedAttributionHostTargetFrame() {
    AttributionHostTestPeer::SetCurrentTargetFrameForTesting(attribution_host_,
                                                             nullptr);
  }

 private:
  const raw_ptr<AttributionHost> attribution_host_;
};

TEST_F(AttributionHostTest, NavigationWithNoImpression_Ignored) {
  EXPECT_CALL(*mock_data_host_manager(), NotifyNavigationRegistrationStarted)
      .Times(0);

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));
  NavigationSimulatorImpl::NavigateAndCommitFromDocument(GURL(kConversionUrl),
                                                         main_rfh());
}

TEST_F(AttributionHostTest, ValidAttributionSrc_ForwardedToManager) {
  blink::Impression impression;
  impression.nav_type = AttributionNavigationType::kWindowOpen;

  EXPECT_CALL(*mock_data_host_manager(),
              NotifyNavigationRegistrationStarted(
                  impression.attribution_src_token,
                  *SuitableOrigin::Deserialize("https://secure_impression.com"),
                  impression.nav_type,
                  /*is_within_fenced_frame=*/false, main_rfh()->GetGlobalId(),
                  /*navigation_id=*/_));

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));
  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(std::move(impression));
  navigation->Commit();
}

TEST_F(AttributionHostTest, ValidSourceRegistrations_ForwardedToManager) {
  blink::Impression impression;
  impression.nav_type = AttributionNavigationType::kWindowOpen;

  auto redirect_headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

  const SuitableOrigin source_origin =
      *SuitableOrigin::Deserialize("https://secure_impression.com");

  const GURL b_url(kConversionUrl);
  const SuitableOrigin b_origin = *SuitableOrigin::Create(b_url);

  const GURL c_url("https://c.com");
  const SuitableOrigin c_origin = *SuitableOrigin::Create(c_url);

  const GURL d_url("https://d.com");
  const SuitableOrigin d_origin = *SuitableOrigin::Create(d_url);

  GlobalRenderFrameHostId frame_id = main_rfh()->GetGlobalId();
  EXPECT_CALL(
      *mock_data_host_manager(),
      NotifyNavigationRegistrationStarted(
          impression.attribution_src_token, source_origin, impression.nav_type,
          /*is_within_fenced_frame=*/false, frame_id,
          /*navigation_id=*/_));
  EXPECT_CALL(
      *mock_data_host_manager(),
      NotifyNavigationRegistrationData(
          impression.attribution_src_token, redirect_headers.get(),
          /*reporting_origin=*/b_origin, source_origin, _, impression.nav_type,
          /*is_within_fenced_frame=*/false, frame_id, _, _,
          /*is_final_response=*/false));
  EXPECT_CALL(
      *mock_data_host_manager(),
      NotifyNavigationRegistrationData(
          impression.attribution_src_token, redirect_headers.get(),
          /*reporting_origin=*/c_origin, source_origin, _, impression.nav_type,
          /*is_within_fenced_frame=*/false, frame_id, _, _,
          /*is_final_response=*/false));
  EXPECT_CALL(
      *mock_data_host_manager(),
      NotifyNavigationRegistrationData(
          impression.attribution_src_token, headers.get(),
          /*reporting_origin=*/d_origin, source_origin, _, impression.nav_type,
          /*is_within_fenced_frame=*/false, frame_id, _, _,
          /*is_final_response=*/true));

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(b_url, main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(std::move(impression));
  navigation->SetRedirectHeaders(redirect_headers);
  navigation->Redirect(c_url);
  navigation->SetRedirectHeaders(redirect_headers);
  navigation->Redirect(d_url);
  navigation->SetResponseHeaders(headers);
  navigation->Commit();
}

TEST_F(AttributionHostTest, ImpressionInSubframe_Ignored) {
  EXPECT_CALL(*mock_data_host_manager(), NotifyNavigationRegistrationStarted)
      .Times(0);

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
  EXPECT_CALL(*mock_data_host_manager(), NotifyNavigationRegistrationStarted)
      .Times(0);

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

TEST_F(AttributionHostTest,
       AttributionSrcNavigationCommitsToErrorPage_Notified) {
  blink::Impression impression;

  EXPECT_CALL(*mock_data_host_manager(),
              NotifyNavigationRegistrationData(impression.attribution_src_token,
                                               _, _, _, _, _, _, _, _, _,
                                               /*is_final_response=*/true));

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(std::move(impression));
  navigation->Fail(net::ERR_FAILED);
  navigation->CommitErrorPage();
}

TEST_F(AttributionHostTest, AttributionSrcNavigationAborts_Notified) {
  blink::Impression impression;

  EXPECT_CALL(*mock_data_host_manager(),
              NotifyNavigationRegistrationData(impression.attribution_src_token,
                                               _, _, _, _, _, _, _, _, _,
                                               /*is_final_response=*/true));

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(std::move(impression));
  navigation->AbortCommit();
}

TEST_F(AttributionHostTest,
       CommittedOriginDiffersFromConversionDesintation_Notified) {
  EXPECT_CALL(*mock_data_host_manager(), NotifyNavigationRegistrationData);

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

  EXPECT_CALL(*mock_data_host_manager(), NotifyNavigationRegistrationStarted)
      .Times(test_case.expected_valid);

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
                  /*is_within_fenced_frame=*/false, RegistrationType::kSource,
                  main_rfh()->GetGlobalId(), /*last_navigation_id=*/_));

  contents()->NavigateAndCommit(GURL("https://top.example"));
  ScopedAttributionHostTargetFrame frame_scope(attribution_host(), main_rfh());

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  attribution_host_mojom()->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), RegistrationType::kSource);

  // Run loop to allow the bad message code to run if a bad message was
  // triggered.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

// crbug.com/1378749.
TEST_F(AttributionHostTest, DISABLED_DataHostOnInsecurePage_BadMessage) {
  contents()->NavigateAndCommit(GURL("http://top.example"));
  ScopedAttributionHostTargetFrame frame_scope(attribution_host(), main_rfh());

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  attribution_host_mojom()->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), RegistrationType::kSource);

  EXPECT_EQ(
      "blink.mojom.AttributionHost can only be used with a secure top-level "
      "frame.",
      bad_message_observer.WaitForBadMessage());
}

// crbug.com/1378749.
TEST_F(AttributionHostTest,
       DISABLED_NavigationDataHostOnInsecurePage_BadMessage) {
  contents()->NavigateAndCommit(GURL("http://top.example"));
  ScopedAttributionHostTargetFrame frame_scope(attribution_host(), main_rfh());

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  attribution_host_mojom()->RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      blink::AttributionSrcToken());

  EXPECT_EQ(
      "blink.mojom.AttributionHost can only be used with a secure top-level "
      "frame.",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(AttributionHostTest, DuplicateAttributionSrcToken_BadMessage) {
  ON_CALL(*mock_data_host_manager(), RegisterNavigationDataHost)
      .WillByDefault(Return(false));

  contents()->NavigateAndCommit(GURL("https://top.example"));
  ScopedAttributionHostTargetFrame frame_scope(attribution_host(), main_rfh());

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  attribution_host_mojom()->RegisterNavigationDataHost(
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
                  /*is_within_fenced_frame=*/false, RegistrationType::kSource,
                  main_rfh()->GetGlobalId(), /*last_navigation_id=*/_));

  contents()->NavigateAndCommit(GURL("https://top.example"));

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  subframe = NavigationSimulatorImpl::NavigateAndCommitFromDocument(
      GURL("https://subframe.example"), subframe);
  ScopedAttributionHostTargetFrame frame_scope(attribution_host(), subframe);

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  attribution_host_mojom()->RegisterDataHost(
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
  ScopedAttributionHostTargetFrame frame_scope(attribution_host(), subframe);

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  attribution_host_mojom()->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), RegistrationType::kSource);

  EXPECT_EQ(
      "blink.mojom.AttributionHost can only be used with a secure top-level "
      "frame.",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(AttributionHostTest, DataHost_RegisteredWithFencedFrame) {
  EXPECT_CALL(*mock_data_host_manager(),
              RegisterDataHost(
                  _, *SuitableOrigin::Deserialize("https://top.example"),
                  /*is_within_fenced_frame=*/true, RegistrationType::kSource,
                  main_rfh()->GetGlobalId(), /*last_navigation_id=*/_));

  contents()->NavigateAndCommit(GURL("https://top.example"));
  RenderFrameHost* fenced_frame =
      RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();
  static_cast<RenderFrameHostImpl*>(fenced_frame)
      ->frame_tree_node()
      ->SetFencedFramePropertiesOpaqueAdsModeForTesting();
  fenced_frame = NavigationSimulatorImpl::NavigateAndCommitFromDocument(
      GURL("https://fencedframe.example"), fenced_frame);
  ScopedAttributionHostTargetFrame frame_scope(attribution_host(),
                                               fenced_frame);

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<blink::mojom::AttributionDataHost> data_host_remote;
  attribution_host_mojom()->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(), RegistrationType::kSource);

  // Run loop to allow the bad message code to run if a bad message was
  // triggered.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

TEST_F(AttributionHostTest, FeatureDisabled_FencedFrameReportingBeaconDropped) {
  contents()->NavigateAndCommit(GURL("https:/secure.com"));

  EXPECT_CALL(*mock_data_host_manager(),
              NotifyFencedFrameReportingBeaconStarted)
      .Times(0);

  RenderFrameHost* fenced_frame =
      RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();
  static_cast<RenderFrameHostImpl*>(fenced_frame)
      ->frame_tree_node()
      ->SetFencedFramePropertiesOpaqueAdsModeForTesting();
  fenced_frame = NavigationSimulatorImpl::NavigateAndCommitFromDocument(
      GURL("https://fencedframe.example"), fenced_frame);

  EXPECT_FALSE(attribution_host()->NotifyFencedFrameReportingBeaconStarted(
      kBeaconId, kNavigationId,
      static_cast<RenderFrameHostImpl*>(fenced_frame)));
}

TEST_F(AttributionHostTest, NotifyFencedFrameReportingBeaconStarted) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAttributionFencedFrameReportingBeacon);

  const struct {
    const char* source_origin;
    bool expected_valid;
  } kTestCases[] = {
      {kLocalHost, true},
      {"http://127.0.0.1", true},
      {"http://insecure.com", false},
      {"https:/secure.com", true},
  };

  for (const auto& test_case : kTestCases) {
    contents()->NavigateAndCommit(GURL(test_case.source_origin));
    if (test_case.expected_valid) {
      EXPECT_CALL(
          *mock_data_host_manager(),
          NotifyFencedFrameReportingBeaconStarted(
              kBeaconId, Optional(kNavigationId),
              *SuitableOrigin::Deserialize(test_case.source_origin),
              /*is_within_fenced_frame=*/true, _, main_rfh()->GetGlobalId()));
    } else {
      EXPECT_CALL(*mock_data_host_manager(),
                  NotifyFencedFrameReportingBeaconStarted)
          .Times(0);
    }

    RenderFrameHost* fenced_frame =
        RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();
    static_cast<RenderFrameHostImpl*>(fenced_frame)
        ->frame_tree_node()
        ->SetFencedFramePropertiesOpaqueAdsModeForTesting();
    SetFencedFrameConfigPermissions(fenced_frame);
    fenced_frame = NavigationSimulatorImpl::NavigateAndCommitFromDocument(
        GURL("https://fencedframe.example"), fenced_frame);

    EXPECT_EQ(attribution_host()->NotifyFencedFrameReportingBeaconStarted(
                  kBeaconId, kNavigationId,
                  static_cast<RenderFrameHostImpl*>(fenced_frame)),
              test_case.expected_valid);
  }
}

TEST_F(AttributionHostTest, FencedFrameReportingBeacon_FeaturePolicyChecked) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kAttributionFencedFrameReportingBeacon);

  contents()->NavigateAndCommit(GURL("https://secure.com"));

  RenderFrameHost* fenced_frame =
      RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();
  static_cast<RenderFrameHostImpl*>(fenced_frame)
      ->frame_tree_node()
      ->SetFencedFramePropertiesOpaqueAdsModeForTesting();
  SetFencedFrameConfigPermissions(fenced_frame);

  static constexpr char kAllowedOriginUrl[] = "https://a.test";

  const struct {
    const char* fenced_frame_url;
    bool expected;
  } kTestCases[] = {
      {kAllowedOriginUrl, true},
      {"https://b.test", false},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_CALL(*mock_data_host_manager(),
                NotifyFencedFrameReportingBeaconStarted)
        .Times(test_case.expected);

    auto simulator = NavigationSimulatorImpl::CreateRendererInitiated(
        GURL(test_case.fenced_frame_url), fenced_frame);
    simulator->SetPermissionsPolicyHeader(
        {blink::ParsedPermissionsPolicyDeclaration(
            blink::mojom::PermissionsPolicyFeature::kAttributionReporting,
            /*allowed_origins=*/
            {blink::OriginWithPossibleWildcards(
                url::Origin::Create(GURL(kAllowedOriginUrl)),
                /*has_subdomain_wildcard=*/false)},
            /*self_if_matches=*/absl::nullopt,
            /*matches_all_origins=*/false, /*matches_opaque_src=*/false)});
    simulator->Commit();
    fenced_frame = simulator->GetFinalRenderFrameHost();

    EXPECT_EQ(attribution_host()->NotifyFencedFrameReportingBeaconStarted(
                  kBeaconId, /*navigation_id=*/absl::nullopt,
                  static_cast<RenderFrameHostImpl*>(fenced_frame)),
              test_case.expected);
  }
}

TEST_F(AttributionHostTest, ImpressionNavigation_FeaturePolicyChecked) {
  blink::Impression impression;
  impression.nav_type = AttributionNavigationType::kWindowOpen;

  static constexpr char kAllowedOriginUrl[] = "https://a.test";

  const struct {
    const char* url;
    bool expected;
  } kTestCases[] = {
      {kAllowedOriginUrl, true},
      {"https://b.test", false},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_CALL(*mock_data_host_manager(), NotifyNavigationRegistrationStarted)
        .Times(test_case.expected);

    auto simulator1 = NavigationSimulatorImpl::CreateRendererInitiated(
        GURL(test_case.url), main_rfh());
    simulator1->SetPermissionsPolicyHeader(
        {blink::ParsedPermissionsPolicyDeclaration(
            blink::mojom::PermissionsPolicyFeature::kAttributionReporting,
            /*allowed_origins=*/{},
            /*self_if_matches*/ url::Origin::Create(GURL(kAllowedOriginUrl)),
            /*matches_all_origins=*/false, /*matches_opaque_src=*/false)});
    simulator1->Commit();

    auto simulator2 = NavigationSimulatorImpl::CreateRendererInitiated(
        GURL(kConversionUrl), main_rfh());
    simulator2->SetInitiatorFrame(main_rfh());
    simulator2->set_impression(impression);
    simulator2->Commit();
  }
}

}  // namespace
}  // namespace content
