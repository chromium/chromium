// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class HTMLFrameOwnerTest : public SimTest,
                           public testing::WithParamInterface<const char*> {};

INSTANTIATE_TEST_SUITE_P(All,
                         HTMLFrameOwnerTest,
                         testing::Values("iframe", "object"));

// Verify that re-inserting a frame-owner element that still has a live
// ContentFrame() (e.g. from an aborted state-preserving atomic move)
// triggers a SECURITY_CHECK. This prevents "zombie" frames that are
// disconnected from their owner element but still present in the frame tree.
TEST_P(HTMLFrameOwnerTest, InsertWithContentFrameCrashes) {
  SimRequest main("https://example.com/", "text/html");
  SimRequest child("https://example.com/child.html", "text/html");
  LoadURL("https://example.com/");

  if (std::string_view(GetParam()) == "iframe") {
    main.Complete(R"HTML(
      <!doctype html>
      <div id="container">
        <iframe id="el" src="child.html"></iframe>
      </div>
    )HTML");
  } else {
    main.Complete(R"HTML(
      <!doctype html>
      <div id="container">
        <object id="el" type="text/html" data="child.html"></object>
      </div>
    )HTML");
  }
  child.Complete("<!doctype html>child");
  test::RunPendingTasks();

  auto* container = GetDocument().getElementById(AtomicString("container"));
  auto* el = To<HTMLFrameOwnerElement>(
      GetDocument().getElementById(AtomicString("el")));

  ASSERT_TRUE(el && el->ContentFrame());

  // Set up the invalid state:
  GetDocument().SetStatePreservingAtomicMoveInProgress(true);
  container->removeChild(el);
  GetDocument().SetStatePreservingAtomicMoveInProgress(false);

  // Verify explosions:
  ASSERT_FALSE(el->isConnected());
  ASSERT_TRUE(el->ContentFrame());
  EXPECT_DEATH_IF_SUPPORTED(container->appendChild(el), "");

  // Cleanup el to avoid shutdown checks.
  if (el->ContentFrame()) {
    if (el->parentNode()) {
      el->remove();
    }
    el->IncrementConnectedSubframeCount();
    el->ContentFrame()->Detach(FrameDetachType::kRemove);
  }
}

}  // namespace blink
