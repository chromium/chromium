// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/permissions_policy/permissions_policy_devtools_support.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {
using FeaturePolicyDevtoolsSupportSimTest = SimTest;

// Note: fullscreen has default allowlist 'EnableForSelf'.

TEST_F(FeaturePolicyDevtoolsSupportSimTest, DetectIframeAttributeBlockage) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest iframe_resource("https://example.com/foo.html", "text/html");

  LoadURL("https://example.com");
  main_resource.Complete(R"(
      <iframe src="https://example.com/foo.html" allow="fullscreen 'none'"></iframe>
    )");
  iframe_resource.Finish();

  base::Optional<FeaturePolicyBlockLocator> locator =
      TraceFeaturePolicyBlockSource(
          MainFrame().GetFrame()->FirstChild(),
          mojom::blink::FeaturePolicyFeature::kFullscreen);

  ASSERT_NE(locator, base::nullopt);
  EXPECT_EQ(locator->frame_id,
            IdentifiersFactory::FrameId(MainFrame().GetFrame()->FirstChild()));
  EXPECT_EQ(locator->reason, FeaturePolicyBlockReason::kIframeAttribute);
}

TEST_F(FeaturePolicyDevtoolsSupportSimTest,
       DetectNestedIframeAttributeBlockage) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest iframe_resource1("https://example.com/foo.html", "text/html");
  SimRequest iframe_resource2("https://example.com/bar.html", "text/html");

  LoadURL("https://example.com");
  main_resource.Complete(R"(
      <iframe src="https://example.com/foo.html" allow="fullscreen 'none'"></iframe>
    )");
  iframe_resource1.Complete(R"(
      <iframe src="https://example.com/bar.html"></iframe>
    )");
  iframe_resource2.Finish();

  base::Optional<FeaturePolicyBlockLocator> locator =
      TraceFeaturePolicyBlockSource(
          MainFrame().GetFrame()->FirstChild()->FirstChild(),
          mojom::blink::FeaturePolicyFeature::kFullscreen);

  ASSERT_NE(locator, base::nullopt);
  EXPECT_EQ(locator->frame_id,
            IdentifiersFactory::FrameId(MainFrame().GetFrame()->FirstChild()));
  EXPECT_EQ(locator->reason, FeaturePolicyBlockReason::kIframeAttribute);
}

TEST_F(FeaturePolicyDevtoolsSupportSimTest, DetectHeaderBlockage) {
  SimRequest::Params main_params;
  main_params.response_http_headers = {
      {"Permissions-Policy", "fullscreen=()"},
  };
  SimRequest main_resource("https://example.com", "text/html", main_params);

  LoadURL("https://example.com");
  main_resource.Finish();

  base::Optional<FeaturePolicyBlockLocator> locator =
      TraceFeaturePolicyBlockSource(
          MainFrame().GetFrame(),
          mojom::blink::FeaturePolicyFeature::kFullscreen);

  ASSERT_NE(locator, base::nullopt);
  EXPECT_EQ(locator->frame_id,
            IdentifiersFactory::FrameId(MainFrame().GetFrame()));
  EXPECT_EQ(locator->reason, FeaturePolicyBlockReason::kHeader);
}

TEST_F(FeaturePolicyDevtoolsSupportSimTest, DetectNestedHeaderBlockage) {
  SimRequest::Params main_params;
  main_params.response_http_headers = {
      {"Permissions-Policy", "fullscreen=()"},
  };
  SimRequest main_resource("https://example.com", "text/html", main_params);

  SimRequest iframe_resource("https://example.com/foo.html", "text/html");

  LoadURL("https://example.com");
  main_resource.Complete(R"(
      <iframe src="https://example.com/foo.html"></iframe>
    )");
  iframe_resource.Finish();

  base::Optional<FeaturePolicyBlockLocator> locator =
      TraceFeaturePolicyBlockSource(
          MainFrame().GetFrame()->FirstChild(),
          mojom::blink::FeaturePolicyFeature::kFullscreen);

  ASSERT_NE(locator, base::nullopt);
  EXPECT_EQ(locator->frame_id,
            IdentifiersFactory::FrameId(MainFrame().GetFrame()));
  EXPECT_EQ(locator->reason, FeaturePolicyBlockReason::kHeader);
}

// When feature is disabled at multiple level of frames, report blockage
// closest to the root of frame tree.
TEST_F(FeaturePolicyDevtoolsSupportSimTest, DetectRootHeaderBlockage) {
  SimRequest::Params main_params;
  main_params.response_http_headers = {
      {"Permissions-Policy", "fullscreen=()"},
  };
  SimRequest main_resource("https://example.com", "text/html", main_params);

  SimRequest::Params iframe_params;
  iframe_params.response_http_headers = {
      {"Permissions-Policy", "fullscreen=()"},
  };
  SimRequest iframe_resource("https://example.com/foo.html", "text/html",
                             iframe_params);

  LoadURL("https://example.com");
  main_resource.Complete(R"(
      <iframe src="https://example.com/foo.html"></iframe>
    )");
  iframe_resource.Finish();

  base::Optional<FeaturePolicyBlockLocator> locator =
      TraceFeaturePolicyBlockSource(
          MainFrame().GetFrame()->FirstChild(),
          mojom::blink::FeaturePolicyFeature::kFullscreen);

  ASSERT_NE(locator, base::nullopt);
  EXPECT_EQ(locator->frame_id,
            IdentifiersFactory::FrameId(MainFrame().GetFrame()));
  EXPECT_EQ(locator->reason, FeaturePolicyBlockReason::kHeader);
}

TEST_F(FeaturePolicyDevtoolsSupportSimTest, DetectCrossOriginHeaderBlockage) {
  SimRequest::Params main_params;
  main_params.response_http_headers = {
      {"Permissions-Policy", "fullscreen=self"},
  };
  SimRequest main_resource("https://example.com", "text/html", main_params);

  SimRequest::Params iframe_params;
  iframe_params.response_http_headers = {
      {"Permissions-Policy", "fullscreen=*"},
  };
  SimRequest iframe_resource("https://foo.com", "text/html", iframe_params);

  LoadURL("https://example.com");
  main_resource.Complete(R"(
      <iframe src="https://foo.com" allow="fullscreen *"></iframe>
    )");
  iframe_resource.Finish();

  base::Optional<FeaturePolicyBlockLocator> locator =
      TraceFeaturePolicyBlockSource(
          MainFrame().GetFrame()->FirstChild(),
          mojom::blink::FeaturePolicyFeature::kFullscreen);

  ASSERT_NE(locator, base::nullopt);
  EXPECT_EQ(locator->frame_id,
            IdentifiersFactory::FrameId(MainFrame().GetFrame()));
  EXPECT_EQ(locator->reason, FeaturePolicyBlockReason::kHeader);
}

TEST_F(FeaturePolicyDevtoolsSupportSimTest,
       DetectCrossOriginDefaultAllowlistBlockage) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest iframe_resource("https://foo.com", "text/html");

  LoadURL("https://example.com");
  main_resource.Complete(R"(
      <iframe src="https://foo.com"></iframe>
    )");
  iframe_resource.Finish();

  base::Optional<FeaturePolicyBlockLocator> locator =
      TraceFeaturePolicyBlockSource(
          MainFrame().GetFrame()->FirstChild(),
          mojom::blink::FeaturePolicyFeature::kFullscreen);

  ASSERT_NE(locator, base::nullopt);
  EXPECT_EQ(locator->frame_id,
            IdentifiersFactory::FrameId(MainFrame().GetFrame()->FirstChild()));
  EXPECT_EQ(locator->reason, FeaturePolicyBlockReason::kIframeAttribute);
}

TEST_F(FeaturePolicyDevtoolsSupportSimTest,
       DetectCrossOriginIframeAttributeBlockage) {
  SimRequest::Params main_params;
  main_params.response_http_headers = {
      {"Permissions-Policy", "fullscreen=*"},
  };
  SimRequest main_resource("https://example.com", "text/html", main_params);

  SimRequest::Params iframe_params;
  iframe_params.response_http_headers = {
      {"Permissions-Policy", "fullscreen=*"},
  };
  SimRequest iframe_resource("https://foo.com", "text/html", iframe_params);

  LoadURL("https://example.com");
  main_resource.Complete(R"(
      <iframe src="https://foo.com" allow="fullscreen 'self'"></iframe>
    )");
  iframe_resource.Finish();

  base::Optional<FeaturePolicyBlockLocator> locator =
      TraceFeaturePolicyBlockSource(
          MainFrame().GetFrame()->FirstChild(),
          mojom::blink::FeaturePolicyFeature::kFullscreen);

  ASSERT_NE(locator, base::nullopt);
  EXPECT_EQ(locator->frame_id,
            IdentifiersFactory::FrameId(MainFrame().GetFrame()->FirstChild()));
  EXPECT_EQ(locator->reason, FeaturePolicyBlockReason::kIframeAttribute);
}

TEST_F(FeaturePolicyDevtoolsSupportSimTest, DetectNestedCrossOriginNoBlockage) {
  SimRequest::Params main_params;
  main_params.response_http_headers = {
      {"Permissions-Policy", "fullscreen=(self \"https://foo.com)\""},
  };
  SimRequest main_resource("https://example.com", "text/html", main_params);

  SimRequest::Params foo_params;
  foo_params.response_http_headers = {
      {"Permissions-Policy", "fullscreen=*"},
  };
  SimRequest foo_resource("https://foo.com", "text/html", foo_params);

  SimRequest::Params bar_params;
  bar_params.response_http_headers = {
      {"Permissions-Policy", "fullscreen=*"},
  };
  SimRequest bar_resource("https://bar.com", "text/html", bar_params);

  LoadURL("https://example.com");
  main_resource.Complete(R"(
      <iframe src="https://foo.com" allow="fullscreen *"></iframe>
    )");
  foo_resource.Complete(R"(
      <iframe src="https://bar.com" allow="fullscreen *"></iframe>
    )");
  bar_resource.Finish();

  base::Optional<FeaturePolicyBlockLocator> locator =
      TraceFeaturePolicyBlockSource(
          MainFrame().GetFrame()->FirstChild()->FirstChild(),
          mojom::blink::FeaturePolicyFeature::kFullscreen);

  EXPECT_TRUE(MainFrame().GetFrame()->GetSecurityContext()->IsFeatureEnabled(
      mojom::blink::FeaturePolicyFeature::kFullscreen));
  EXPECT_TRUE(
      MainFrame()
          .GetFrame()
          ->FirstChild()
          ->GetSecurityContext()
          ->IsFeatureEnabled(mojom::blink::FeaturePolicyFeature::kFullscreen));
  EXPECT_TRUE(
      MainFrame()
          .GetFrame()
          ->FirstChild()
          ->FirstChild()
          ->GetSecurityContext()
          ->IsFeatureEnabled(mojom::blink::FeaturePolicyFeature::kFullscreen));
  EXPECT_EQ(locator, base::nullopt);
}

TEST_F(FeaturePolicyDevtoolsSupportSimTest, DetectNoBlockage) {
  SimRequest::Params main_params;
  main_params.response_http_headers = {
      {"Permissions-Policy", "fullscreen=*"},
  };
  SimRequest main_resource("https://example.com", "text/html", main_params);

  LoadURL("https://example.com");
  main_resource.Finish();

  base::Optional<FeaturePolicyBlockLocator> locator =
      TraceFeaturePolicyBlockSource(
          MainFrame().GetFrame(),
          mojom::blink::FeaturePolicyFeature::kFullscreen);

  EXPECT_EQ(locator, base::nullopt);
}
}  // namespace blink