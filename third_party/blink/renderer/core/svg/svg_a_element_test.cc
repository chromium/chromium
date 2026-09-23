// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_a_element.h"

#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/url/dom_origin.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

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

TEST_F(SVGAElementTest, GetDOMOrigin) {
  SetBodyInnerHTML(R"HTML(
    <svg xmlns:xlink="http://www.w3.org/1999/xlink">
      <a id="no-href"></a>
      <a id="svg-href" href="https://example.com/test"></a>
      <a id="xlink-href" xlink:href="https://example.com/test"></a>
      <a id="opaque-href" href="about:blank"></a>
    </svg>
  )HTML");

  auto* no_href = To<SVGAElement>(GetElementById("no-href"));
  EXPECT_EQ(nullptr, no_href->GetDOMOrigin(GetDocument().domWindow()));

  auto* svg_href = To<SVGAElement>(GetElementById("svg-href"));
  DOMOrigin* svg_origin = svg_href->GetDOMOrigin(GetDocument().domWindow());
  ASSERT_NE(nullptr, svg_origin);
  EXPECT_FALSE(svg_origin->opaque());
  EXPECT_TRUE(SecurityOrigin::CreateFromString("https://example.com")
                  ->IsSameOriginWith(svg_origin->GetOriginForTesting()));

  auto* xlink_href = To<SVGAElement>(GetElementById("xlink-href"));
  DOMOrigin* xlink_origin = xlink_href->GetDOMOrigin(GetDocument().domWindow());
  ASSERT_NE(nullptr, xlink_origin);
  EXPECT_FALSE(xlink_origin->opaque());
  EXPECT_TRUE(SecurityOrigin::CreateFromString("https://example.com")
                  ->IsSameOriginWith(xlink_origin->GetOriginForTesting()));

  auto* opaque_href = To<SVGAElement>(GetElementById("opaque-href"));
  DOMOrigin* opaque_origin =
      opaque_href->GetDOMOrigin(GetDocument().domWindow());
  ASSERT_NE(nullptr, opaque_origin);
  EXPECT_TRUE(opaque_origin->opaque());
}

}  // namespace blink
