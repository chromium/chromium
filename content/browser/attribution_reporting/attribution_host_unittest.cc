// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_host.h"

#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/attribution_reporting/data_host.mojom.h"
#include "components/attribution_reporting/registration_eligibility.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_input_event.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_suitable_context.h"
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
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_declaration.h"
#include "third_party/blink/public/common/tokens/tokens.h"
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
using ::testing::AllOf;
using ::testing::Optional;
using ::testing::Property;
using ::testing::Return;

using ::attribution_reporting::mojom::RegistrationEligibility;

const char kConversionUrl[] = "https://b.com";
constexpr bool kIsForBackgroundRequests = true;

const char kRedirectHeaderData[] =
    "HTTP/1.1 301 Moved\0"
    "Location: http://foopy/\0"
    "\0";

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
    FencedFrameConfig new_config = FencedFrameConfig(GURL("about:blank"));
    new_config.AddEffectiveEnabledPermissionForTesting(
        blink::mojom::PermissionsPolicyFeature::kAttributionReporting);
    FencedFrameProperties new_props = FencedFrameProperties(new_config);
    fenced_frame_node->set_fenced_frame_properties(new_props);
  }

  blink::ParsedPermissionsPolicy RestrictivePermissionsPolicy(
      const url::Origin& allowed_origin) {
    return {blink::ParsedPermissionsPolicyDeclaration(
        blink::mojom::PermissionsPolicyFeature::kAttributionReporting,
        /*allowed_origins=*/
        {*blink::OriginWithPossibleWildcards::FromOrigin(allowed_origin)},
        /*self_if_matches=*/std::nullopt,
        /*matches_all_origins=*/false, /*matches_opaque_src=*/false)};
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

  EXPECT_CALL(
      *mock_data_host_manager(),
      NotifyNavigationRegistrationStarted(
          /*suitable_context=*/AllOf(
              Property(&AttributionSuitableContext::context_origin,
                       *SuitableOrigin::Deserialize(
                           "https://secure_impression.com")),
              Property(&AttributionSuitableContext::root_render_frame_id,
                       main_rfh()->GetGlobalId())),
          impression.attribution_src_token,
          /*navigation_id=*/_, /*devtools_request_id*/ _));

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));
  auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
      GURL(kConversionUrl), main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(std::move(impression));
  navigation->Commit();
}

TEST_F(AttributionHostTest, ValidSourceRegistrations_ForwardedToManager) {
  blink::Impression impression;

  auto redirect_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      std::string(kRedirectHeaderData, std::size(kRedirectHeaderData)));
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
          /*suitable_context=*/AllOf(
              Property(&AttributionSuitableContext::context_origin,
                       source_origin),
              Property(&AttributionSuitableContext::root_render_frame_id,
                       frame_id)),
          impression.attribution_src_token,
          /*navigation_id=*/_, /*devtools_request_id*/ _));
  EXPECT_CALL(*mock_data_host_manager(),
              NotifyNavigationRegistrationData(impression.attribution_src_token,
                                               redirect_headers.get(),
                                               /*reporting_url=*/b_url));
  EXPECT_CALL(*mock_data_host_manager(),
              NotifyNavigationRegistrationData(impression.attribution_src_token,
                                               redirect_headers.get(),
                                               /*reporting_url=*/c_url));
  EXPECT_CALL(*mock_data_host_manager(),
              NotifyNavigationRegistrationData(impression.attribution_src_token,
                                               headers.get(),
                                               /*reporting_url=*/d_url));
  EXPECT_CALL(*mock_data_host_manager(), NotifyNavigationRegistrationCompleted(
                                             impression.attribution_src_token));

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

TEST_F(AttributionHostTest,
       ValidAndInvalidSourceRegistrations_ForwardedToManager) {
  blink::Impression impression;

  auto redirect_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      std::string(kRedirectHeaderData, std::size(kRedirectHeaderData)));
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

  const SuitableOrigin source_origin =
      *SuitableOrigin::Deserialize("https://secure_impression.com");

  const GURL b_url(kConversionUrl);
  const SuitableOrigin b_origin = *SuitableOrigin::Create(b_url);

  const GURL c_url("https://c.com");
  const SuitableOrigin c_origin = *SuitableOrigin::Create(c_url);

  const GURL d_url("http://d.com");

  GlobalRenderFrameHostId frame_id = main_rfh()->GetGlobalId();
  EXPECT_CALL(
      *mock_data_host_manager(),
      NotifyNavigationRegistrationStarted(
          /*suitable_context=*/AllOf(
              Property(&AttributionSuitableContext::context_origin,
                       source_origin),
              Property(&AttributionSuitableContext::root_render_frame_id,
                       frame_id)),
          impression.attribution_src_token,
          /*navigation_id=*/_, /*devtools_request_id*/ _));
  EXPECT_CALL(*mock_data_host_manager(),
              NotifyNavigationRegistrationData(impression.attribution_src_token,
                                               redirect_headers.get(),
                                               /*reporting_url=*/b_url));
  EXPECT_CALL(*mock_data_host_manager(),
              NotifyNavigationRegistrationData(impression.attribution_src_token,
                                               redirect_headers.get(),
                                               /*reporting_url=*/c_url));
  // Expect no call for origin d as the reporting origin is not suitable.

  // Expect that `NotifyNavigationRegistrationCompleted` gets called even if the
  // last reporting_origin was not suitable.
  EXPECT_CALL(*mock_data_host_manager(), NotifyNavigationRegistrationCompleted(
                                             impression.attribution_src_token));

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
// ignore the associated impression but still notify when the navigation
// completes.
TEST_F(AttributionHostTest, ImpressionNavigationWithDeadInitiator_Ignored) {
  EXPECT_CALL(*mock_data_host_manager(), NotifyNavigationRegistrationStarted)
      .Times(0);
  EXPECT_CALL(*mock_data_host_manager(), NotifyNavigationRegistrationData)
      .Times(0);
  EXPECT_CALL(*mock_data_host_manager(), NotifyNavigationRegistrationCompleted)
      .Times(1);

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

  EXPECT_CALL(
      *mock_data_host_manager(),
      NotifyNavigationRegistrationData(impression.attribution_src_token, _, _));

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

  EXPECT_CALL(
      *mock_data_host_manager(),
      NotifyNavigationRegistrationData(impression.attribution_src_token, _, _));

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
    ,
    AttributionHostOriginTrustworthyChecksTest,
    ::testing::ValuesIn(kOriginTrustworthyChecksTestCases));

TEST_F(AttributionHostTest, DataHost_RegisteredWithContext) {
  EXPECT_CALL(
      *mock_data_host_manager(),
      RegisterDataHost(
          _,
          AllOf(Property(&AttributionSuitableContext::context_origin,
                         *SuitableOrigin::Deserialize("https://top.example")),
                Property(&AttributionSuitableContext::root_render_frame_id,
                         main_rfh()->GetGlobalId())),
          RegistrationEligibility::kSource, kIsForBackgroundRequests));

  contents()->NavigateAndCommit(GURL("https://top.example"));
  ScopedAttributionHostTargetFrame frame_scope(attribution_host(), main_rfh());

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  attribution_host_mojom()->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      RegistrationEligibility::kSource, kIsForBackgroundRequests);

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

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  attribution_host_mojom()->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      RegistrationEligibility::kSource, kIsForBackgroundRequests);

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

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
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

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  attribution_host_mojom()->RegisterNavigationDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      blink::AttributionSrcToken());

  EXPECT_EQ(
      "Renderer attempted to register a data host with a duplicate "
      "AttributionSrcToken.",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(
    AttributionHostTest,
    NotifyNavigationWithBackgroundRegistrationsWillStart_DuplicateAttributionSrcToken_BadMessage) {
  ON_CALL(*mock_data_host_manager(),
          NotifyNavigationWithBackgroundRegistrationsWillStart)
      .WillByDefault(Return(false));

  contents()->NavigateAndCommit(GURL("https://top.example"));
  ScopedAttributionHostTargetFrame frame_scope(attribution_host(), main_rfh());

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  attribution_host_mojom()
      ->NotifyNavigationWithBackgroundRegistrationsWillStart(
          blink::AttributionSrcToken(), /*expected_registrations=*/1);

  EXPECT_EQ(
      "Renderer attempted to notify of expected registrations with a duplicate "
      "AttributionSrcToken or an invalid number of expected registrations.",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(
    AttributionHostTest,
    NotifyNavigationWithBackgroundRegistrationsWillStart_InsecureContext_Ignored) {
  contents()->NavigateAndCommit(GURL("http://top.example"));
  ScopedAttributionHostTargetFrame frame_scope(attribution_host(), main_rfh());

  EXPECT_CALL(*mock_data_host_manager(),
              NotifyNavigationWithBackgroundRegistrationsWillStart)
      .Times(0);

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  attribution_host_mojom()
      ->NotifyNavigationWithBackgroundRegistrationsWillStart(
          blink::AttributionSrcToken(), /*expected_registrations=*/1);
}

TEST_F(AttributionHostTest, DataHostInSubframe_ContextIsOutermostFrame) {
  EXPECT_CALL(
      *mock_data_host_manager(),
      RegisterDataHost(
          _,
          AllOf(Property(&AttributionSuitableContext::context_origin,
                         *SuitableOrigin::Deserialize("https://top.example")),
                Property(&AttributionSuitableContext::root_render_frame_id,
                         main_rfh()->GetGlobalId())),
          RegistrationEligibility::kSource, kIsForBackgroundRequests));

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

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  attribution_host_mojom()->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      RegistrationEligibility::kSource, kIsForBackgroundRequests);

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

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  attribution_host_mojom()->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      RegistrationEligibility::kSource, kIsForBackgroundRequests);

  EXPECT_EQ(
      "blink.mojom.AttributionHost can only be used with a secure top-level "
      "frame.",
      bad_message_observer.WaitForBadMessage());
}

TEST_F(AttributionHostTest, DataHost_RegisteredWithFencedFrame) {
  EXPECT_CALL(
      *mock_data_host_manager(),
      RegisterDataHost(
          _,
          AllOf(Property(&AttributionSuitableContext::context_origin,
                         *SuitableOrigin::Deserialize("https://top.example")),
                Property(&AttributionSuitableContext::root_render_frame_id,
                         main_rfh()->GetGlobalId())),
          RegistrationEligibility::kSource, kIsForBackgroundRequests));

  contents()->NavigateAndCommit(GURL("https://top.example"));
  RenderFrameHost* fenced_frame =
      RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();
  static_cast<RenderFrameHostImpl*>(fenced_frame)
      ->frame_tree_node()
      ->SetFencedFramePropertiesOpaqueAdsModeForTesting();
  SetFencedFrameConfigPermissions(fenced_frame);
  fenced_frame = NavigationSimulatorImpl::NavigateAndCommitFromDocument(
      GURL("https://fencedframe.example"), fenced_frame);
  ScopedAttributionHostTargetFrame frame_scope(attribution_host(),
                                               fenced_frame);

  // Create a fake dispatch context to trigger a bad message in.
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
  attribution_host_mojom()->RegisterDataHost(
      data_host_remote.BindNewPipeAndPassReceiver(),
      RegistrationEligibility::kSource, kIsForBackgroundRequests);

  // Run loop to allow the bad message code to run if a bad message was
  // triggered.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

TEST_F(AttributionHostTest, ImpressionNavigation_FeaturePolicyChecked) {
  blink::Impression impression;

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
    simulator1->SetPermissionsPolicyHeader(RestrictivePermissionsPolicy(
        url::Origin::Create(GURL(kAllowedOriginUrl))));
    simulator1->Commit();

    auto simulator2 = NavigationSimulatorImpl::CreateRendererInitiated(
        GURL(kConversionUrl), main_rfh());
    simulator2->SetInitiatorFrame(main_rfh());
    simulator2->set_impression(impression);
    simulator2->Commit();
  }
}

TEST_F(AttributionHostTest, RegisterDataHost_FeaturePolicyChecked) {
  contents()->NavigateAndCommit(GURL("https://top.example"));

  static constexpr char kAllowedOriginUrl[] = "https://a.test";

  const struct {
    const char* subframe_url;
    bool expected;
  } kTestCases[] = {
      {kAllowedOriginUrl, true},
      {"https://b.test", false},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_CALL(*mock_data_host_manager(), RegisterDataHost)
        .Times(test_case.expected);

    content::RenderFrameHostTester* rfh_tester =
        content::RenderFrameHostTester::For(main_rfh());
    content::RenderFrameHost* subframe = rfh_tester->AppendChildWithPolicy(
        "subframe", RestrictivePermissionsPolicy(
                        url::Origin::Create(GURL(kAllowedOriginUrl))));
    subframe = NavigationSimulatorImpl::NavigateAndCommitFromDocument(
        GURL(test_case.subframe_url), subframe);
    ScopedAttributionHostTargetFrame frame_scope(attribution_host(), subframe);

    mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
    attribution_host_mojom()->RegisterDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        RegistrationEligibility::kSource, kIsForBackgroundRequests);

    base::RunLoop().RunUntilIdle();
  }
}
TEST_F(AttributionHostTest, RegisterNavigationDataHost_FeaturePolicyChecked) {
  contents()->NavigateAndCommit(GURL("https://top.example"));

  static constexpr char kAllowedOriginUrl[] = "https://a.test";

  const struct {
    const char* subframe_url;
    bool expected;
  } kTestCases[] = {
      {kAllowedOriginUrl, true},
      {"https://b.test", false},
  };

  for (const auto& test_case : kTestCases) {
    if (test_case.expected) {
      EXPECT_CALL(*mock_data_host_manager(), RegisterNavigationDataHost)
          .WillOnce(Return(true));
    } else {
      EXPECT_CALL(*mock_data_host_manager(), RegisterNavigationDataHost)
          .Times(0);
    }

    content::RenderFrameHostTester* rfh_tester =
        content::RenderFrameHostTester::For(main_rfh());
    content::RenderFrameHost* subframe = rfh_tester->AppendChildWithPolicy(
        "subframe", RestrictivePermissionsPolicy(
                        url::Origin::Create(GURL(kAllowedOriginUrl))));
    subframe = NavigationSimulatorImpl::NavigateAndCommitFromDocument(
        GURL(test_case.subframe_url), subframe);
    ScopedAttributionHostTargetFrame frame_scope(attribution_host(), subframe);

    mojo::Remote<attribution_reporting::mojom::DataHost> data_host_remote;
    attribution_host_mojom()->RegisterNavigationDataHost(
        data_host_remote.BindNewPipeAndPassReceiver(),
        blink::AttributionSrcToken());

    base::RunLoop().RunUntilIdle();
  }
}
TEST_F(
    AttributionHostTest,
    NotifyNavigationWithBackgroundRegistrationsWillStart_FeaturePolicyChecked) {
  contents()->NavigateAndCommit(GURL("https://top.example"));

  static constexpr char kAllowedOriginUrl[] = "https://a.test";

  const struct {
    const char* subframe_url;
    bool expected;
  } kTestCases[] = {
      {kAllowedOriginUrl, true},
      {"https://b.test", false},
  };

  for (const auto& test_case : kTestCases) {
    if (test_case.expected) {
      EXPECT_CALL(*mock_data_host_manager(),
                  NotifyNavigationWithBackgroundRegistrationsWillStart)
          .WillOnce(Return(true));
    } else {
      EXPECT_CALL(*mock_data_host_manager(),
                  NotifyNavigationWithBackgroundRegistrationsWillStart)
          .Times(0);
    }

    content::RenderFrameHostTester* rfh_tester =
        content::RenderFrameHostTester::For(main_rfh());
    content::RenderFrameHost* subframe = rfh_tester->AppendChildWithPolicy(
        "subframe", RestrictivePermissionsPolicy(
                        url::Origin::Create(GURL(kAllowedOriginUrl))));
    subframe = NavigationSimulatorImpl::NavigateAndCommitFromDocument(
        GURL(test_case.subframe_url), subframe);
    ScopedAttributionHostTargetFrame frame_scope(attribution_host(), subframe);

    attribution_host_mojom()
        ->NotifyNavigationWithBackgroundRegistrationsWillStart(
            blink::AttributionSrcToken(), /*expected_registrations=*/1);

    base::RunLoop().RunUntilIdle();
  }
}

TEST_F(AttributionHostTest, InsecureTaintTracking) {
  blink::Impression impression;

  auto redirect_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      std::string(kRedirectHeaderData, std::size(kRedirectHeaderData)));
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

  const SuitableOrigin source_origin =
      *SuitableOrigin::Deserialize("https://secure_impression.com");

  const GURL b_url(kConversionUrl);
  const SuitableOrigin b_origin = *SuitableOrigin::Create(b_url);

  const GURL insecure_url("http://insecure.com");

  const GURL d_url("https://d.com");
  const SuitableOrigin d_origin = *SuitableOrigin::Create(d_url);

  EXPECT_CALL(
      *mock_data_host_manager(),
      NotifyNavigationRegistrationStarted(
          AllOf(Property(&AttributionSuitableContext::context_origin,
                         source_origin),
                Property(&AttributionSuitableContext::root_render_frame_id,
                         main_rfh()->GetGlobalId())),
          impression.attribution_src_token,
          /*navigation_id=*/_, /*devtools_request_id=*/_));
  EXPECT_CALL(*mock_data_host_manager(),
              NotifyNavigationRegistrationData(impression.attribution_src_token,
                                               redirect_headers.get(),
                                               /*reporting_url=*/b_url))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_data_host_manager(),
              NotifyNavigationRegistrationData(impression.attribution_src_token,
                                               headers.get(),
                                               /*reporting_url=*/d_url))
      .WillOnce(Return(true));

  contents()->NavigateAndCommit(GURL("https://secure_impression.com"));

  base::HistogramTester histograms;

  auto navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(b_url, main_rfh());
  navigation->SetInitiatorFrame(main_rfh());
  navigation->set_impression(std::move(impression));
  navigation->SetRedirectHeaders(redirect_headers);
  navigation->Redirect(insecure_url);
  navigation->SetRedirectHeaders(redirect_headers);
  navigation->Redirect(d_url);
  navigation->SetResponseHeaders(headers);
  navigation->Commit();

  histograms.ExpectUniqueSample("Conversions.IncrementalTaintingFailures", 1,
                                1);
}

}  // namespace
}  // namespace content
