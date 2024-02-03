// Copyright 2021 The Chromium Authors
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
using PermissionsPolicyDevtoolsSupportSimTest = SimTest;

// Note: fullscreen has default allowlist 'EnableForSelf'.

TEST_F(PermissionsPolicyDevtoolsSupportSimTest, DetectIframeAttributeBlockage) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest iframe_resource("https://example.com/foo.html", "text/html");

  LoadURL("https://example.com");
  main_resource.Complete(R"(
      <iframe src="https://example.com/foo.html" allow="fullscreen 'none'"></iframe>
    )");
  iframe_resource.Finish();

  std::optional<PermissionsPolicyBlockLocator> locator =
      TracePermissionsPolicyBlockSource(
          MainFrame().GetFrame()->FirstChild(),
          mojom::blink::PermissionsPolicyFeature::kFullscreen);

  ASSERT_NE(locator, std::nullopt);
  EXPECT_EQ(locator->frame_id,
            IdentifiersFactory::FrameId(MainFrame().GetFrame()->FirstChild()));
  EXPECT_EQ(locator->reason, PermissionsPolicyBlockReason::kIframeAttribute);
}

TEST_F(PermissionsPolicyDevtoolsSupportSimTest,
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

  std::optional<PermissionsPolicyBlockLocator> locator =
      TracePermissionsPolicyBlockSource(
          MainFrame().GetFrame()->FirstChild()->FirstChild(),
          mojom::blink::PermissionsPolicyFeature::kFullscreen);

  ASSERT_NE(locator, std::nullopt);
  EXPECT_EQ(locator->frame_id,
            IdentifiersFactory::FrameId(MainFrame().GetFrame()->FirstChild()));
  EXPECT_EQ(locator->reason, PermissionsPolicyBlockReason::kIframeAttribute);
}

TEST_F(PermissionsPolicyDevtoolsSupportSimTest, DetectHeaderBlockage) {
  SimRequest::Params main_params;
  main_params.response_http_headers = {
      {"Permissions-Policy", "fullscreen=()"},
  };
  SimRequest main_resource("https://example.com", "text/html", main_params);

  LoadURL("https://example.com");
  main_resource.Finish();

  std::optional<PermissionsPolicyBlockLocator> locator =
      TracePermissionsPolicyBlockSource(
          MainFrame().GetFrame(),
          mojom::blink::PermissionsPolicyFeature::kFullscreen);

  ASSERT_NE(locator, std::nullopt);
  EXPECT_EQ(locator->frame_id,
            IdentifiersFactory::FrameId(MainFrame().GetFrame()));
  EXPECT_EQ(locator->reason, PermissionsPolicyBlockReason::kHeader);
}

TEST_F(PermissionsPolicyDevtoolsSupportSimTest, DetectNestedHeaderBlockage) {
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

  std::optional<PermissionsPolicyBlockLocator> locator =
      TracePermissionsPolicyBlockSource(
          MainFrame().GetFrame()->FirstChild(),
          mojom::blink::PermissionsPolicyFeature::kFullscreen);

  ASSERT_NE(locator, std::nullopt);
  EXPECT_EQ(locator->frame_id,
            IdentifiersFactory::FrameId(MainFrame().GetFrame()));
  EXPECT_EQ(locator->reason, PermissionsPolicyBlockReason::kHeader);
}

// When feature is disabled at multiple level of frames, report blockage
// closest to the root of frame tree.
TEST_F(PermissionsPolicyDevtoolsSupportSimTest, DetectRootHeaderBlockage) {
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

  std::optional<PermissionsPolicyBlockLocator> locator =
      TracePermissionsPolicyBlockSource(
          MainFrame().GetFrame()->FirstChild(),
          mojom::blink::PermissionsPolicyFeature::kFullscreen);

  ASSERT_NE(locator, std::nullopt);
  EXPECT_EQ(locator->frame_id,
            IdentifiersFactory::FrameId(MainFrame().GetFrame()));
  EXPECT_EQ(locator->reason, PermissionsPolicyBlockReason::kHeader);
}

TEST_F(PermissionsPolicyDevtoolsSupportSimTest,
       DetectCrossOriginHeaderBlockage) {
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

  std::optional<PermissionsPolicyBlockLocator> locator =
      TracePermissionsPolicyBlockSource(
          MainFrame().GetFrame()->FirstChild(),
          mojom::blink::PermissionsPolicyFeature::kFullscreen);

  ASSERT_NE(locator, std::nullopt);
  EXPECT_EQ(locator->frame_id,
            IdentifiersFactory::FrameId(MainFrame().GetFrame()));
  EXPECT_EQ(locator->reason, PermissionsPolicyBlockReason::kHeader);
}

TEST_F(PermissionsPolicyDevtoolsSupportSimTest,
       DetectCrossOriginDefaultAllowlistBlockage) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest iframe_resource("https://foo.com", "text/html");

  LoadURL("https://example.com");
  main_resource.Complete(R"(
      <iframe src="https://foo.com"></iframe>
    )");
  iframe_resource.Finish();

  std::optional<PermissionsPolicyBlockLocator> locator =
      TracePermissionsPolicyBlockSource(
          MainFrame().GetFrame()->FirstChild(),
          mojom::blink::PermissionsPolicyFeature::kFullscreen);

  ASSERT_NE(locator, std::nullopt);
  EXPECT_EQ(locator->frame_id,
            IdentifiersFactory::FrameId(MainFrame().GetFrame()->FirstChild()));
  EXPECT_EQ(locator->reason, PermissionsPolicyBlockReason::kIframeAttribute);
}

TEST_F(PermissionsPolicyDevtoolsSupportSimTest,
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

  std::optional<PermissionsPolicyBlockLocator> locator =
      TracePermissionsPolicyBlockSource(
          MainFrame().GetFrame()->FirstChild(),
          mojom::blink::PermissionsPolicyFeature::kFullscreen);

  ASSERT_NE(locator, std::nullopt);
  EXPECT_EQ(locator->frame_id,
            IdentifiersFactory::FrameId(MainFrame().GetFrame()->FirstChild()));
  EXPECT_EQ(locator->reason, PermissionsPolicyBlockReason::kIframeAttribute);
}

TEST_F(PermissionsPolicyDevtoolsSupportSimTest,
       DetectNestedCrossOriginNoBlockage) {
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

  std::optional<PermissionsPolicyBlockLocator> locator =
      TracePermissionsPolicyBlockSource(
          MainFrame().GetFrame()->FirstChild()->FirstChild(),
          mojom::blink::PermissionsPolicyFeature::kFullscreen);

  SecurityContext::FeatureStatus status =
      MainFrame().GetFrame()->GetSecurityContext()->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kFullscreen);
  EXPECT_TRUE(status.enabled);
  EXPECT_FALSE(status.should_report);
  EXPECT_EQ(status.reporting_endpoint, std::nullopt);

  status = MainFrame()
               .GetFrame()
               ->FirstChild()
               ->GetSecurityContext()
               ->IsFeatureEnabled(
                   mojom::blink::PermissionsPolicyFeature::kFullscreen);
  EXPECT_TRUE(status.enabled);
  EXPECT_FALSE(status.should_report);
  EXPECT_EQ(status.reporting_endpoint, std::nullopt);

  status = MainFrame()
               .GetFrame()
               ->FirstChild()
               ->FirstChild()
               ->GetSecurityContext()
               ->IsFeatureEnabled(
                   mojom::blink::PermissionsPolicyFeature::kFullscreen);
  EXPECT_TRUE(status.enabled);
  EXPECT_FALSE(status.should_report);
  EXPECT_EQ(status.reporting_endpoint, std::nullopt);

  EXPECT_EQ(locator, std::nullopt);
}

TEST_F(PermissionsPolicyDevtoolsSupportSimTest, DetectNoBlockage) {
  SimRequest::Params main_params;
  main_params.response_http_headers = {
      {"Permissions-Policy", "fullscreen=*"},
  };
  SimRequest main_resource("https://example.com", "text/html", main_params);

  LoadURL("https://example.com");
  main_resource.Finish();

  std::optional<PermissionsPolicyBlockLocator> locator =
      TracePermissionsPolicyBlockSource(
          MainFrame().GetFrame(),
          mojom::blink::PermissionsPolicyFeature::kFullscreen);

  EXPECT_EQ(locator, std::nullopt);
}
}  // namespace blink
