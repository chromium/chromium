// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_element.h"

#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

using LifecycleUpdateReason = DocumentUpdateReason;

class SVGUseElementTest : public PageTestBase {};

TEST_F(SVGUseElementTest, InstanceInvalidatedWhenNonAttachedTargetRemoved) {
  GetDocument().body()->setInnerHTML(R"HTML(
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
  UpdateAllLifecyclePhasesForTest();

  // Remove #target.
  ASSERT_TRUE(GetDocument().getElementById(AtomicString("target")));
  GetDocument().getElementById(AtomicString("target"))->remove();

  // This should cause a rebuild of the <use> shadow tree.
  UpdateAllLifecyclePhasesForTest();

  // There should be no instance for #target anymore, since that element was
  // removed.
  auto* use =
      To<SVGUseElement>(GetDocument().getElementById(AtomicString("use")));
  ASSERT_TRUE(use);
  ASSERT_TRUE(use->GetShadowRoot());
  ASSERT_FALSE(use->GetShadowRoot()->getElementById(AtomicString("target")));
}

TEST_F(SVGUseElementTest,
       InstanceInvalidatedWhenNonAttachedTargetMovedInDocument) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <svg>
      <use id="use" href="#path"/>
      <textPath id="path">
        <textPath>
          <a id="target" systemLanguage="th"></a>
        </textPath>
      </textPath>
    </svg>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  // Move #target in the document (leaving it still "connected").
  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  GetDocument().body()->appendChild(target);

  // This should cause a rebuild of the <use> shadow tree.
  UpdateAllLifecyclePhasesForTest();

  // There should be no instance for #target anymore, since that element was
  // removed.
  auto* use =
      To<SVGUseElement>(GetDocument().getElementById(AtomicString("use")));
  ASSERT_TRUE(use);
  ASSERT_TRUE(use->GetShadowRoot());
  ASSERT_FALSE(use->GetShadowRoot()->getElementById(AtomicString("target")));
}

TEST_F(SVGUseElementTest, NullInstanceRootWhenNotConnectedToDocument) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <svg>
      <defs>
        <rect id="r" width="100" height="100" fill="blue"/>
      </defs>
      <use id="target" href="#r"/>
    </svg>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* target =
      To<SVGUseElement>(GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(target);
  ASSERT_TRUE(target->InstanceRoot());

  target->remove();

  ASSERT_FALSE(target->InstanceRoot());
}

TEST_F(SVGUseElementTest, NullInstanceRootWhenConnectedToInactiveDocument) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <svg>
      <defs>
        <rect id="r" width="100" height="100" fill="blue"/>
      </defs>
      <use id="target" href="#r"/>
    </svg>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* target =
      To<SVGUseElement>(GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(target);
  ASSERT_TRUE(target->InstanceRoot());

  Document* other_document =
      GetDocument().implementation().createHTMLDocument();
  other_document->body()->appendChild(target);

  ASSERT_FALSE(target->InstanceRoot());
}

TEST_F(SVGUseElementTest, NullInstanceRootWhenShadowTreePendingRebuild) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <svg>
      <defs>
        <rect id="r" width="100" height="100" fill="blue"/>
      </defs>
      <use id="target" href="#r"/>
    </svg>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* target =
      To<SVGUseElement>(GetDocument().getElementById(AtomicString("target")));
  ASSERT_TRUE(target);
  ASSERT_TRUE(target->InstanceRoot());

  GetDocument()
      .getElementById(AtomicString("r"))
      ->setAttribute(html_names::kWidthAttr, AtomicString("50"));

  ASSERT_FALSE(target->InstanceRoot());
}

}  // namespace blink
