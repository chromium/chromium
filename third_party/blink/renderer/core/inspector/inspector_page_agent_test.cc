// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_page_agent.h"

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_resource_content_loader.h"
#include "third_party/blink/renderer/core/inspector/protocol/page.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class InspectorPageAgentFrameOriginTest : public SimTest {
 protected:
  InspectorPageAgent* CreatePageAgent() {
    LocalFrame* frame = MainFrame().GetFrame();
    auto* inspected_frames = MakeGarbageCollected<InspectedFrames>(frame);
    auto* resource_content_loader =
        MakeGarbageCollected<InspectorResourceContentLoader>(frame);
    // `getFrameTree()` does not use `client` or `injected_script_manager`, so
    // passing `nullptr` for both is sufficient for these frame-tree tests.
    return MakeGarbageCollected<InspectorPageAgent>(
        inspected_frames, /*client=*/nullptr, resource_content_loader,
        /*script_to_evaluate_on_load=*/String(),
        /*injected_script_manager=*/nullptr);
  }
};

TEST_F(InspectorPageAgentFrameOriginTest,
       ReportsTupleSecurityOriginForNormalIframe) {
  SimRequest main_resource("http://localhost/index.html", "text/html");
  SimRequest normal_resource("http://localhost/normal.html", "text/html");

  LoadURL("http://localhost/index.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <iframe src="http://localhost/normal.html"></iframe>
  )HTML");
  normal_resource.Complete("<!DOCTYPE html><p>Normal content</p>");

  InspectorPageAgent* page_agent = CreatePageAgent();
  std::unique_ptr<protocol::Page::FrameTree> frame_tree;
  ASSERT_TRUE(page_agent->getFrameTree(&frame_tree).IsSuccess());
  ASSERT_TRUE(frame_tree);

  protocol::Page::Frame* main_frame = frame_tree->getFrame();
  ASSERT_TRUE(main_frame);
  EXPECT_EQ(main_frame->getSecurityOrigin(), "http://localhost");
  ASSERT_TRUE(main_frame->getSecurityOriginDetails());
  EXPECT_TRUE(main_frame->getSecurityOriginDetails()->getIsLocalhost());

  protocol::Array<protocol::Page::FrameTree>* children =
      frame_tree->getChildFrames(nullptr);
  ASSERT_TRUE(children);
  ASSERT_EQ(children->size(), 1u);

  protocol::Page::Frame* normal_frame = children->at(0)->getFrame();
  ASSERT_TRUE(normal_frame);
  EXPECT_EQ(normal_frame->getUrl(), "http://localhost/normal.html");
  EXPECT_EQ(normal_frame->getSecurityOrigin(), "http://localhost");
  ASSERT_TRUE(normal_frame->getSecurityOriginDetails());
  EXPECT_TRUE(normal_frame->getSecurityOriginDetails()->getIsLocalhost());
}

TEST_F(InspectorPageAgentFrameOriginTest,
       ReportsOpaqueSecurityOriginForSandboxedIframe) {
  SimRequest main_resource("http://localhost/index.html", "text/html");
  SimRequest sandboxed_resource("http://localhost/sandboxed.html", "text/html");

  LoadURL("http://localhost/index.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <iframe sandbox="allow-scripts" src="http://localhost/sandboxed.html"></iframe>
  )HTML");
  sandboxed_resource.Complete("<!DOCTYPE html><p>Sandboxed content</p>");

  InspectorPageAgent* page_agent = CreatePageAgent();
  std::unique_ptr<protocol::Page::FrameTree> frame_tree;
  ASSERT_TRUE(page_agent->getFrameTree(&frame_tree).IsSuccess());
  ASSERT_TRUE(frame_tree);

  protocol::Array<protocol::Page::FrameTree>* children =
      frame_tree->getChildFrames(nullptr);
  ASSERT_TRUE(children);
  ASSERT_EQ(children->size(), 1u);

  protocol::Page::Frame* sandboxed_frame = children->at(0)->getFrame();
  ASSERT_TRUE(sandboxed_frame);
  EXPECT_EQ(sandboxed_frame->getUrl(), "http://localhost/sandboxed.html");
  EXPECT_EQ(sandboxed_frame->getSecurityOrigin(), "://");
  ASSERT_TRUE(sandboxed_frame->getSecurityOriginDetails());
  EXPECT_FALSE(sandboxed_frame->getSecurityOriginDetails()->getIsLocalhost());
}

TEST_F(InspectorPageAgentFrameOriginTest,
       ReportsTupleSecurityOriginForSandboxedAllowSameOriginIframe) {
  SimRequest main_resource("http://localhost/index.html", "text/html");
  SimRequest same_origin_resource("http://localhost/same-origin.html",
                                  "text/html");

  LoadURL("http://localhost/index.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <iframe sandbox="allow-scripts allow-same-origin" src="http://localhost/same-origin.html"></iframe>
  )HTML");
  same_origin_resource.Complete("<!DOCTYPE html><p>Same-origin content</p>");

  InspectorPageAgent* page_agent = CreatePageAgent();
  std::unique_ptr<protocol::Page::FrameTree> frame_tree;
  ASSERT_TRUE(page_agent->getFrameTree(&frame_tree).IsSuccess());
  ASSERT_TRUE(frame_tree);

  protocol::Array<protocol::Page::FrameTree>* children =
      frame_tree->getChildFrames(nullptr);
  ASSERT_TRUE(children);
  ASSERT_EQ(children->size(), 1u);

  protocol::Page::Frame* allow_same_origin_frame = children->at(0)->getFrame();
  ASSERT_TRUE(allow_same_origin_frame);
  EXPECT_EQ(allow_same_origin_frame->getUrl(),
            "http://localhost/same-origin.html");
  EXPECT_EQ(allow_same_origin_frame->getSecurityOrigin(), "http://localhost");
  ASSERT_TRUE(allow_same_origin_frame->getSecurityOriginDetails());
  EXPECT_TRUE(
      allow_same_origin_frame->getSecurityOriginDetails()->getIsLocalhost());
}

TEST_F(InspectorPageAgentFrameOriginTest,
       ReportsOpaqueSecurityOriginForCspSandboxedIframe) {
  SimRequest main_resource("http://localhost/index.html", "text/html");
  SimRequest::Params csp_params;
  csp_params.response_http_headers.insert("Content-Security-Policy",
                                          "sandbox allow-scripts");
  SimRequest csp_sandboxed_resource("http://localhost/csp-sandboxed.html",
                                    "text/html", csp_params);

  LoadURL("http://localhost/index.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <iframe src="http://localhost/csp-sandboxed.html"></iframe>
  )HTML");
  csp_sandboxed_resource.Complete(
      "<!DOCTYPE html><p>CSP sandboxed content</p>");

  InspectorPageAgent* page_agent = CreatePageAgent();
  std::unique_ptr<protocol::Page::FrameTree> frame_tree;
  ASSERT_TRUE(page_agent->getFrameTree(&frame_tree).IsSuccess());
  ASSERT_TRUE(frame_tree);

  protocol::Array<protocol::Page::FrameTree>* children =
      frame_tree->getChildFrames(nullptr);
  ASSERT_TRUE(children);
  ASSERT_EQ(children->size(), 1u);

  protocol::Page::Frame* csp_sandboxed_frame = children->at(0)->getFrame();
  ASSERT_TRUE(csp_sandboxed_frame);
  EXPECT_EQ(csp_sandboxed_frame->getUrl(),
            "http://localhost/csp-sandboxed.html");
  EXPECT_EQ(csp_sandboxed_frame->getSecurityOrigin(), "://");
  ASSERT_TRUE(csp_sandboxed_frame->getSecurityOriginDetails());
  EXPECT_FALSE(
      csp_sandboxed_frame->getSecurityOriginDetails()->getIsLocalhost());
}

TEST_F(InspectorPageAgentFrameOriginTest,
       ReportsInheritedSecurityOriginForAboutBlankIframe) {
  SimRequest main_resource("http://localhost/index.html", "text/html");

  LoadURL("http://localhost/index.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <iframe src="about:blank"></iframe>
  )HTML");

  InspectorPageAgent* page_agent = CreatePageAgent();
  std::unique_ptr<protocol::Page::FrameTree> frame_tree;
  ASSERT_TRUE(page_agent->getFrameTree(&frame_tree).IsSuccess());
  ASSERT_TRUE(frame_tree);

  protocol::Array<protocol::Page::FrameTree>* children =
      frame_tree->getChildFrames(nullptr);
  ASSERT_TRUE(children);
  ASSERT_EQ(children->size(), 1u);

  protocol::Page::Frame* about_blank_frame = children->at(0)->getFrame();
  ASSERT_TRUE(about_blank_frame);
  EXPECT_EQ(about_blank_frame->getUrl(), "about:blank");
  EXPECT_EQ(about_blank_frame->getSecurityOrigin(), "http://localhost");
  ASSERT_TRUE(about_blank_frame->getSecurityOriginDetails());
  EXPECT_TRUE(about_blank_frame->getSecurityOriginDetails()->getIsLocalhost());
}

TEST_F(InspectorPageAgentFrameOriginTest,
       ReportsOpaqueSecurityOriginForMainFrameWithOpaqueOrigin) {
  KURL url("http://localhost/csp-sandbox.html");
  auto params = WebNavigationParams::CreateWithEmptyHTMLForTesting(url);
  params->origin_to_commit =
      SecurityOrigin::Create(url)->DeriveNewOpaqueOrigin();
  MainFrame().CommitNavigation(std::move(params), /*extra_data=*/nullptr);

  InspectorPageAgent* page_agent = CreatePageAgent();
  std::unique_ptr<protocol::Page::FrameTree> frame_tree;
  ASSERT_TRUE(page_agent->getFrameTree(&frame_tree).IsSuccess());
  ASSERT_TRUE(frame_tree);

  protocol::Page::Frame* main_frame = frame_tree->getFrame();
  ASSERT_TRUE(main_frame);
  EXPECT_EQ(main_frame->getUrl(), "http://localhost/csp-sandbox.html");
  EXPECT_EQ(main_frame->getSecurityOrigin(), "://");
  ASSERT_TRUE(main_frame->getSecurityOriginDetails());
  EXPECT_FALSE(main_frame->getSecurityOriginDetails()->getIsLocalhost());
}

}  // namespace blink
