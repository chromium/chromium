// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/platform/web_prescient_networking.h"
#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

const char kMainFrame[] = "https://example.com/main.html";

class DnsPrefetchRecorder : public WebPrescientNetworking {
 public:
  void PrefetchDNS(const WebURL& url) override {
    prefetched_urls.push_back(url);
  }

  Vector<KURL> prefetched_urls;
};

class ConnectionAllowlistSimTest : public SimTest {
 public:
  ConnectionAllowlistSimTest() = default;

  void LoadWindowWithConnectionAllowlistHeader(const String& header_name) {
    SimRequest::Params params;
    params.response_http_headers.insert(header_name, "(response_origin)");

    SimRequest main(kMainFrame, "text/html", params);
    const String& document = String("<html><body>Hello</body></html>");

    LoadURL(kMainFrame);
    main.Complete(document);
    test::RunPendingTasks();
  }
};

TEST_F(ConnectionAllowlistSimTest, UseCounterForEnforcedHeader) {
  LoadWindowWithConnectionAllowlistHeader("Connection-Allowlist");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kConnectionAllowlist));
}

TEST_F(ConnectionAllowlistSimTest, UseCounterForReportOnlyHeader) {
  LoadWindowWithConnectionAllowlistHeader("Connection-Allowlist-Report-Only");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kConnectionAllowlist));
}

TEST_F(ConnectionAllowlistSimTest, AnchorHoverUsesSubframeNetworkHints) {
  ResizeView(gfx::Size(800, 600));
  SimRequest::Params main_params;
  main_params.response_http_headers.insert("X-DNS-Prefetch-Control", "on");
  SimRequest main(kMainFrame, "text/html", main_params);
  SimRequest::Params child_params;
  child_params.response_http_headers.insert("Connection-Allowlist", "()");
  child_params.response_http_headers.insert("X-DNS-Prefetch-Control", "on");
  SimRequest child("https://example.com/child.html", "text/html", child_params);

  LoadURL(kMainFrame);
  main.Complete(R"HTML(
    <style>
      body { margin: 0; }
      a { display: block; height: 100px; }
      iframe { display: block; border: 0; width: 300px; height: 200px; }
    </style>
    <a href="https://parent-link.test/">Parent link</a>
    <iframe id="child" src="child.html"></iframe>
  )HTML");
  child.Complete(R"HTML(
    <style>
      body { margin: 0; }
      a { display: block; height: 100px; }
    </style>
    <a href="https://child-link.test/">Child link</a>
  )HTML");
  Compositor().BeginFrame();

  auto* iframe = To<HTMLIFrameElement>(GetElementById("child"));
  Document* child_document = iframe->contentDocument();
  ASSERT_TRUE(child_document);
  ASSERT_TRUE(GetDocument().IsDNSPrefetchEnabled());
  ASSERT_TRUE(child_document->IsDNSPrefetchEnabled());

  auto main_networking = std::make_unique<DnsPrefetchRecorder>();
  auto* main_recorder = main_networking.get();
  GetDocument().GetFrame()->SetPrescientNetworkingForTesting(
      std::move(main_networking));
  auto child_networking = std::make_unique<DnsPrefetchRecorder>();
  auto* child_recorder = child_networking.get();
  child_document->GetFrame()->SetPrescientNetworkingForTesting(
      std::move(child_networking));

  for (const auto& position : {gfx::PointF(20, 20), gfx::PointF(20, 120)}) {
    WebMouseEvent event(WebInputEvent::Type::kMouseMove, position, position,
                        WebPointerProperties::Button::kNoButton, 0,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests());
    GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(event, {},
                                                                     {});
  }

  EXPECT_THAT(main_recorder->prefetched_urls,
              testing::ElementsAre(KURL("https://parent-link.test/")));
  EXPECT_THAT(child_recorder->prefetched_urls,
              testing::ElementsAre(KURL("https://child-link.test/")));
}

}  // namespace
}  // namespace blink
