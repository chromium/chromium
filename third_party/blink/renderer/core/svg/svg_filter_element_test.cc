// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_filter_element.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class SVGFilterElementSimTest : public SimTest {
 protected:
  void LoadPage(const String& source) {
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");
    main_resource.Complete(source);
    Compositor().BeginFrame();
    test::RunPendingTasks();
  }
};

TEST_F(SVGFilterElementSimTest,
       FilterInvalidatedIfPrimitivesChangeDuringParsing) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  String document_text(R"HTML(
    <!doctype html>
    <div id="target" style="width: 100px; height: 100px; filter: url(#green)">
    </div>
    <svg><filter id="green"><feFlood flood-color="green"/></filter></svg>
  )HTML");
  const wtf_size_t cut_offset = document_text.Find("<feFlood");
  ASSERT_NE(cut_offset, kNotFound);

  main_resource.Write(document_text.Left(cut_offset));
  Compositor().BeginFrame();
  test::RunPendingTasks();

  const Element* target_element =
      GetDocument().getElementById(AtomicString("target"));
  const LayoutObject* target = target_element->GetLayoutObject();

  EXPECT_TRUE(target->StyleRef().HasFilter());
  ASSERT_FALSE(target->NeedsPaintPropertyUpdate());
  EXPECT_NE(nullptr, target->FirstFragment().PaintProperties()->Filter());

  main_resource.Complete(document_text.Right(cut_offset));

  ASSERT_TRUE(target->NeedsPaintPropertyUpdate());
}

TEST_F(SVGFilterElementSimTest, SVGFEBlendElementUseCounter) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kSVGFEBlendElement));
  LoadPage(R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <defs>
        <filter id="blendEffect">
          <feBlend in="SourceGraphic" in2="SourceGraphic" mode="difference" />
        </filter>
      </defs>
      <rect width="100" height="100" fill="green" filter="url(#blendEffect)" />
    </svg>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kSVGFEBlendElement));
}

TEST_F(SVGFilterElementSimTest, SVGFEColorMatrixElementUseCounter) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kSVGFEColorMatrixElement));
  LoadPage(R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <defs>
        <filter id="effect">
          <feColorMatrix in="SourceGraphic" type="matrix" values="1 0 0 0 0  0 1 0 0 0  0 0 1 0 0  0 0 0 1 0" />
        </filter>
      </defs>
      <rect width="100" height="100" fill="green" filter="url(#effect)" />
    </svg>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kSVGFEColorMatrixElement));
}

TEST_F(SVGFilterElementSimTest, SVGFEComponentTransferElementUseCounter) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kSVGFEComponentTransferElement));
  LoadPage(R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <defs>
        <filter id="effect">
          <feComponentTransfer>
            <feFuncR type="identity"/>
            <feFuncG type="identity"/>
            <feFuncB type="identity"/>
            <feFuncA type="identity"/>
          </feComponentTransfer>
        </filter>
      </defs>
      <rect width="100" height="100" fill="green" filter="url(#effect)" />
    </svg>
  )HTML");
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kSVGFEComponentTransferElement));
}

TEST_F(SVGFilterElementSimTest, SVGFECompositeElementUseCounter) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kSVGFECompositeElement));
  LoadPage(R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <defs>
        <filter id="effect">
          <feComposite in="SourceGraphic" in2="SourceGraphic" operator="over" k1="0" k2="0" k3="0" k4="0" />
        </filter>
      </defs>
      <rect width="100" height="100" fill="green" filter="url(#effect)" />
    </svg>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kSVGFECompositeElement));
}

TEST_F(SVGFilterElementSimTest, SVGFEConvolveMatrixElementUseCounter) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kSVGFEConvolveMatrixElement));
  LoadPage(R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <defs>
        <filter id="effect">
          <feConvolveMatrix order="3" kernelMatrix="0 0 0 0 1 0 0 0 0" />
        </filter>
      </defs>
      <rect width="100" height="100" fill="green" filter="url(#effect)" />
    </svg>
  )HTML");
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kSVGFEConvolveMatrixElement));
}

TEST_F(SVGFilterElementSimTest, SVGFEDiffuseLightingElementUseCounter) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kSVGFEDiffuseLightingElement));
  LoadPage(R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <defs>
        <filter id="effect">
          <feDiffuseLighting in="SourceGraphic">
            <fePointLight x="0" y="0" z="10"/>
          </feDiffuseLighting>
        </filter>
      </defs>
      <rect width="100" height="100" fill="green" filter="url(#effect)" />
    </svg>
  )HTML");
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kSVGFEDiffuseLightingElement));
}

TEST_F(SVGFilterElementSimTest, SVGFEDisplacementMapElementUseCounter) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kSVGFEDisplacementMapElement));
  LoadPage(R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <defs>
        <filter id="effect">
          <feDisplacementMap in="SourceGraphic" in2="SourceGraphic" scale="10" />
        </filter>
      </defs>
      <rect width="100" height="100" fill="green" filter="url(#effect)" />
    </svg>
  )HTML");
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kSVGFEDisplacementMapElement));
}

TEST_F(SVGFilterElementSimTest, SVGFEDropShadowElementUseCounter) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kSVGFEDropShadowElement));
  LoadPage(R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <defs>
        <filter id="effect">
          <feDropShadow dx="2" dy="2" stdDeviation="2" />
        </filter>
      </defs>
      <rect width="100" height="100" fill="green" filter="url(#effect)" />
    </svg>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kSVGFEDropShadowElement));
}

TEST_F(SVGFilterElementSimTest, SVGFEFloodElementUseCounter) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kSVGFEFloodElement));
  LoadPage(R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <defs>
        <filter id="effect">
          <feFlood flood-color="green" flood-opacity="0.5" />
        </filter>
      </defs>
      <rect width="100" height="100" fill="green" filter="url(#effect)" />
    </svg>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kSVGFEFloodElement));
}

TEST_F(SVGFilterElementSimTest, SVGFEGaussianBlurElementUseCounter) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kSVGFEGaussianBlurElement));
  LoadPage(R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <defs>
        <filter id="effect">
          <feGaussianBlur stdDeviation="5" />
        </filter>
      </defs>
      <rect width="100" height="100" fill="green" filter="url(#effect)" />
    </svg>
  )HTML");
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kSVGFEGaussianBlurElement));
}

TEST_F(SVGFilterElementSimTest, SVGFEImageElementUseCounter) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kSVGFEImageElement));
  LoadPage(R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <defs>
        <filter id="effect">
          <feImage />
        </filter>
      </defs>
      <rect width="100" height="100" fill="green" filter="url(#effect)" />
    </svg>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kSVGFEImageElement));
}

TEST_F(SVGFilterElementSimTest, SVGFEMergeElementUseCounter) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kSVGFEMergeElement));
  LoadPage(R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <defs>
        <filter id="effect">
          <feMerge>
            <feMergeNode in="SourceGraphic"/>
          </feMerge>
        </filter>
      </defs>
      <rect width="100" height="100" fill="green" filter="url(#effect)" />
    </svg>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kSVGFEMergeElement));
}

TEST_F(SVGFilterElementSimTest, SVGFEMorphologyElementUseCounter) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kSVGFEMorphologyElement));
  LoadPage(R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <defs>
        <filter id="effect">
          <feMorphology operator="erode" radius="1" />
        </filter>
      </defs>
      <rect width="100" height="100" fill="green" filter="url(#effect)" />
    </svg>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kSVGFEMorphologyElement));
}

TEST_F(SVGFilterElementSimTest, SVGFEOffsetElementUseCounter) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kSVGFEOffsetElement));
  LoadPage(R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <defs>
        <filter id="effect">
          <feOffset dx="10" dy="10" />
        </filter>
      </defs>
      <rect width="100" height="100" fill="green" filter="url(#effect)" />
    </svg>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kSVGFEOffsetElement));
}

TEST_F(SVGFilterElementSimTest, SVGFESpecularLightingElementUseCounter) {
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kSVGFESpecularLightingElement));
  LoadPage(R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <defs>
        <filter id="effect">
          <feSpecularLighting in="SourceGraphic" specularConstant="1" specularExponent="1" surfaceScale="1" lighting-color="white">
            <fePointLight x="0" y="0" z="10"/>
          </feSpecularLighting>
        </filter>
      </defs>
      <rect width="100" height="100" fill="green" filter="url(#effect)" />
    </svg>
  )HTML");
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kSVGFESpecularLightingElement));
}

TEST_F(SVGFilterElementSimTest, SVGFETileElementUseCounter) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kSVGFETileElement));
  LoadPage(R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <defs>
        <filter id="effect">
          <feTile />
        </filter>
      </defs>
      <rect width="100" height="100" fill="green" filter="url(#effect)" />
    </svg>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kSVGFETileElement));
}

TEST_F(SVGFilterElementSimTest, SVGFETurbulenceElementUseCounter) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kSVGFETurbulenceElement));
  LoadPage(R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <defs>
        <filter id="effect">
          <feTurbulence baseFrequency="0.05" numOctaves="1" type="turbulence" />
        </filter>
      </defs>
      <rect width="100" height="100" fill="green" filter="url(#effect)" />
    </svg>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kSVGFETurbulenceElement));
}

}  // namespace blink
