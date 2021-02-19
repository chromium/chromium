// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_foreign_object_element.h"

#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class SVGForeignObjectElementTest : public PageTestBase {};

TEST_F(SVGForeignObjectElementTest, NoLayoutObjectInNonRendered) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <svg>
      <pattern>
        <foreignObject id="fo"></foreignObject>
      </pattern>
    </svg>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* foreign_object = GetDocument().getElementById("fo");
  EXPECT_FALSE(foreign_object->GetLayoutObject());

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  LayoutObject* layout_object =
      foreign_object->CreateLayoutObject(*style, LegacyLayout::kAuto);
  EXPECT_FALSE(layout_object);
}

TEST_F(SVGForeignObjectElementTest, ReferenceForeignObjectInNonRenderedCrash) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      div { writing-mode: vertical-rl; }
      div > svg { float: right; }
    </style>
    <svg>
      <radialGradient id="gradient">
        <pattern>
          <foreignObject>
            <div id="foRoot">
              <svg><rect fill="url(#gradient)" /></svg>
            </div>
          </foreignObject>
        </pattern>
      </radialGradient>
    </svg>
  )HTML");

  // This should not trigger any DCHECK failures or crashes.
  UpdateAllLifecyclePhasesForTest();
}

}  // namespace blink
