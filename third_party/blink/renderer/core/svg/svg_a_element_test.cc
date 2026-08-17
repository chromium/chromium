// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_a_element.h"

#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/xlink_names.h"

namespace blink {

class SVGAElementTest : public RenderingTest {};

// crbug.com/481373475
TEST_F(SVGAElementTest, DefaultEventHandlerCrash) {
  SetBodyInnerHTML(R"HTML(
<svg width=100 height=100>
<text x=10 y=30><a id="a" href="">link-1</a></text>
</svg>
)HTML");
  auto* target = GetElementById("a");
  target->DispatchSimulatedClick(nullptr,
                                 SimulatedClickCreationScope::kFromScript);
  // Pass if no crashes.
}

TEST_F(SVGAElementTest, HrefChangePseudoStateInvalidation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      :any-link text { fill: green; }
    </style>
    <svg>
      <a id="link" href="https://www.chromium.org/">
        <text>link text</text>
      </a>
    </svg>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  auto* link = GetElementById("link");

  // Changing href to another valid URL shouldn't invalidate :any-link,
  // since the element remains a link.
  link->setAttribute(svg_names::kHrefAttr,
                     AtomicString("https://www.example.com/"));
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  // Removing href changes the element from being a link to not being one,
  // so :any-link is invalidated.
  link->removeAttribute(svg_names::kHrefAttr);
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  // Setting href changes the element from not being a link to being one,
  // so :any-link is invalidated.
  link->setAttribute(svg_names::kHrefAttr,
                     AtomicString("https://www.chromium.org/"));
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());

  // Test xlink:href as well.
  SetBodyInnerHTML(R"HTML(
    <style>
      :any-link text { fill: green; }
    </style>
    <svg xmlns:xlink="http://www.w3.org/1999/xlink">
      <a id="link" xlink:href="https://www.chromium.org/">
        <text>link text</text>
      </a>
    </svg>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  link = GetElementById("link");

  link->setAttribute(xlink_names::kHrefAttr,
                     AtomicString("https://www.example.com/"));
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  link->removeAttribute(xlink_names::kHrefAttr);
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  link->setAttribute(xlink_names::kHrefAttr,
                     AtomicString("https://www.chromium.org/"));
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
}

}  // namespace blink
