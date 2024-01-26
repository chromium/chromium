// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_suitable_context.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_web_contents.h"
#include "services/network/public/mojom/attribution.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_declaration.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::SuitableOrigin;

class AttributionSuitableContextTest : public RenderViewHostTestHarness {
 public:
  AttributionSuitableContextTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    test_web_contents()->GetPrimaryMainFrame()->InitializeRenderFrameIfNeeded();
  }

 protected:
  TestWebContents* test_web_contents() {
    return static_cast<TestWebContents*>(web_contents());
  }

  base::test::ScopedFeatureList& scoped_feature_list() {
    return scoped_feature_list_;
  }

  blink::ParsedPermissionsPolicy RestrictivePermissionsPolicy(
      const GURL& allowed_url) {
    const auto origin = blink::OriginWithPossibleWildcards::FromOrigin(
        url::Origin::Create(allowed_url));
    CHECK(origin.has_value());
    return {blink::ParsedPermissionsPolicyDeclaration(
        blink::mojom::PermissionsPolicyFeature::kAttributionReporting,
        /*allowed_origins=*/{origin.value()},
        /*self_if_matches=*/std::nullopt,
        /*matches_all_origins=*/false, /*matches_opaque_src=*/false)};
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AttributionSuitableContextTest,
       SecureContextCreatedFromMainframe_Suitable) {
  const GURL context_url("https://top.example");

  test_web_contents()->NavigateAndCommit(context_url);

  GlobalRenderFrameHostId id =
      test_web_contents()->GetPrimaryMainFrame()->GetGlobalId();
  auto context = AttributionSuitableContext::Create(id);
  ASSERT_TRUE(context.has_value());

  EXPECT_FALSE(context->is_nested_within_fenced_frame());
  EXPECT_EQ(context->root_render_frame_id(), id);
  EXPECT_EQ(context->context_origin(), *SuitableOrigin::Create(context_url));
  EXPECT_EQ(context->last_navigation_id(),
            test_web_contents()->GetPrimaryMainFrame()->navigation_id());
}

TEST_F(AttributionSuitableContextTest,
       SecureContextCreatedFromSubframe_Suitable) {
  const GURL context_url("https://top.example");
  const GURL subframe_url("https://subframe.example");

  test_web_contents()->NavigateAndCommit(context_url);

  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  subframe = NavigationSimulatorImpl::NavigateAndCommitFromDocument(
      subframe_url, subframe);
  CHECK(subframe);

  auto context = AttributionSuitableContext::Create(subframe->GetGlobalId());
  ASSERT_TRUE(context.has_value());
  // Can also be created by passing the pointer directly instead of the id.
  ASSERT_TRUE(AttributionSuitableContext::Create(
                  static_cast<RenderFrameHostImpl*>(subframe))
                  .has_value());

  EXPECT_FALSE(context->is_nested_within_fenced_frame());
  EXPECT_EQ(context->root_render_frame_id(),
            subframe->GetOutermostMainFrame()->GetGlobalId());
  EXPECT_EQ(context->context_origin(), *SuitableOrigin::Create(context_url));
  EXPECT_EQ(context->last_navigation_id(),
            static_cast<RenderFrameHostImpl*>(subframe)->navigation_id());
}

TEST_F(AttributionSuitableContextTest,
       ConversionMeasurementDisabled_NonSuitable) {
  scoped_feature_list().InitAndDisableFeature(
      attribution_reporting::features::kConversionMeasurement);

  test_web_contents()->NavigateAndCommit(GURL("https://top.example"));

  GlobalRenderFrameHostId id =
      test_web_contents()->GetPrimaryMainFrame()->GetGlobalId();
  auto context = AttributionSuitableContext::Create(id);
  ASSERT_FALSE(context.has_value());

  // Validates that it would create with the feature enabled.
  scoped_feature_list().Reset();
  scoped_feature_list().InitWithFeatures(
      {attribution_reporting::features::kConversionMeasurement}, {});
  auto context_ok = AttributionSuitableContext::Create(id);
  ASSERT_TRUE(context_ok.has_value());
}

TEST_F(AttributionSuitableContextTest,
       UnableToFindRenderFrameHost_NonSuitable) {
  auto context =
      AttributionSuitableContext::Create(GlobalRenderFrameHostId(10, 15));
  ASSERT_FALSE(context.has_value());
}

TEST_F(AttributionSuitableContextTest, MissingPolicy_NonSuitable) {
  const GURL context_url("https://top.example");
  const GURL subframe_url("https://subframe.example");

  test_web_contents()->NavigateAndCommit(context_url);

  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())
          ->AppendChildWithPolicy("subframe",
                                  RestrictivePermissionsPolicy(context_url));
  subframe = NavigationSimulatorImpl::NavigateAndCommitFromDocument(
      subframe_url, subframe);
  CHECK(subframe);

  auto context = AttributionSuitableContext::Create(subframe->GetGlobalId());
  ASSERT_FALSE(context.has_value());

  // Validates that it would create with a matching policy
  subframe = NavigationSimulatorImpl::NavigateAndCommitFromDocument(context_url,
                                                                    subframe);
  auto context_ok = AttributionSuitableContext::Create(subframe->GetGlobalId());
  ASSERT_TRUE(context_ok.has_value());
}

TEST_F(AttributionSuitableContextTest, InsecureContextOrigin_NonSuitable) {
  test_web_contents()->NavigateAndCommit(GURL("http://top.example"));
  auto context = AttributionSuitableContext::Create(
      test_web_contents()->GetPrimaryMainFrame()->GetGlobalId());
  ASSERT_FALSE(context.has_value());

  // Validates that it would create with a suitable context origin.
  test_web_contents()->NavigateAndCommit(GURL("https://top.example"));
  auto context_ok = AttributionSuitableContext::Create(
      test_web_contents()->GetPrimaryMainFrame()->GetGlobalId());
  ASSERT_TRUE(context_ok.has_value());
}

TEST_F(AttributionSuitableContextTest, InsecureSubframeOrigin_NonSuitable) {
  const GURL context_url("https://top.example");

  test_web_contents()->NavigateAndCommit(context_url);

  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  subframe = NavigationSimulatorImpl::NavigateAndCommitFromDocument(
      GURL("http://subframe"), subframe);

  // We cannot have a subframe within an insecure url within a secure context.
  ASSERT_EQ(subframe->GetLastCommittedOrigin(),
            url::Origin::Create(context_url));
}

}  // namespace
}  // namespace content
