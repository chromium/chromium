// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_element.h"

#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

using LifecycleUpdateReason = DocumentLifecycle::LifecycleUpdateReason;

class SVGUseElementTest : public PageTestBase {};

TEST_F(SVGUseElementTest, InstanceInvalidatedWhenNonAttachedTargetRemoved) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <style></style>
    <svg>
        <unknown>
          <g id="parent">
            <a id="target">
          </g>
          <use id="use" href="#parent">
        </unknown>
    </svg>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases(LifecycleUpdateReason::kTest);

  // Remove #target.
  ASSERT_TRUE(GetDocument().getElementById("target"));
  GetDocument().getElementById("target")->remove();

  // This should cause a rebuild of the <use> shadow tree.
  GetDocument().View()->UpdateAllLifecyclePhases(LifecycleUpdateReason::kTest);

  // There should be no instance for #target anymore, since that element was
  // removed.
  auto* use = ToSVGUseElement(GetDocument().getElementById("use"));
  ASSERT_TRUE(use);
  ASSERT_TRUE(use->GetShadowRoot());
  ASSERT_FALSE(use->GetShadowRoot()->getElementById("target"));
}

TEST_F(SVGUseElementTest,
       InstanceInvalidatedWhenNonAttachedTargetMovedInDocument) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <svg>
      <use id="use" href="#path"/>
      <textPath id="path">
        <textPath>
          <a id="target" systemLanguage="th"></a>
        </textPath>
      </textPath>
    </svg>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases(LifecycleUpdateReason::kTest);

  // Move #target in the document (leaving it still "connected").
  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  GetDocument().body()->appendChild(target);

  // This should cause a rebuild of the <use> shadow tree.
  GetDocument().View()->UpdateAllLifecyclePhases(LifecycleUpdateReason::kTest);

  // There should be no instance for #target anymore, since that element was
  // removed.
  auto* use = ToSVGUseElement(GetDocument().getElementById("use"));
  ASSERT_TRUE(use);
  ASSERT_TRUE(use->GetShadowRoot());
  ASSERT_FALSE(use->GetShadowRoot()->getElementById("target"));
}

TEST_F(SVGUseElementTest, NullInstanceRootWhenNotConnectedToDocument) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <svg>
      <defs>
        <rect id="r" width="100" height="100" fill="blue"/>
      </defs>
      <use id="target" href="#r"/>
    </svg>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases(LifecycleUpdateReason::kTest);

  auto* target = To<SVGUseElement>(GetDocument().getElementById("target"));
  ASSERT_TRUE(target);
  ASSERT_TRUE(target->InstanceRoot());

  target->remove();

  ASSERT_FALSE(target->InstanceRoot());
}

TEST_F(SVGUseElementTest, NullInstanceRootWhenConnectedToInactiveDocument) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <svg>
      <defs>
        <rect id="r" width="100" height="100" fill="blue"/>
      </defs>
      <use id="target" href="#r"/>
    </svg>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases(LifecycleUpdateReason::kTest);

  auto* target = To<SVGUseElement>(GetDocument().getElementById("target"));
  ASSERT_TRUE(target);
  ASSERT_TRUE(target->InstanceRoot());

  Document* other_document =
      GetDocument().implementation().createHTMLDocument();
  other_document->body()->appendChild(target);

  ASSERT_FALSE(target->InstanceRoot());
}

TEST_F(SVGUseElementTest, NullInstanceRootWhenShadowTreePendingRebuild) {
  GetDocument().body()->SetInnerHTMLFromString(R"HTML(
    <svg>
      <defs>
        <rect id="r" width="100" height="100" fill="blue"/>
      </defs>
      <use id="target" href="#r"/>
    </svg>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases(LifecycleUpdateReason::kTest);

  auto* target = To<SVGUseElement>(GetDocument().getElementById("target"));
  ASSERT_TRUE(target);
  ASSERT_TRUE(target->InstanceRoot());

  GetDocument().getElementById("r")->setAttribute(html_names::kWidthAttr, "50");

  ASSERT_FALSE(target->InstanceRoot());
}

}  // namespace blink
