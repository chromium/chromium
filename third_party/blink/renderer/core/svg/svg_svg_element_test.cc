// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_svg_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/geometry/physical_size.h"

namespace blink {

class SVGSVGElementTest : public SimTest {};

TEST_F(SVGSVGElementTest, ViewSpecSetAfterFirstEmbeddeeLayout) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  main_resource.Complete(R"HTML(<!doctype html>
      <object style="height: 100px" data="test.svg#aspect"></object>)HTML");

  SimRequest object_subresource("https://example.com/test.svg#aspect",
                                "image/svg+xml");
  object_subresource.Write(R"SVG(
      <svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 200 100'>
        <rect width='100' height='100' fill='green'/>
        <view id='aspect' viewBox='0 0 100 100'/>
      </svg>)SVG");

  ASSERT_TRUE(Compositor().NeedsBeginFrame());
  Compositor().BeginFrame();

  Document& document = GetDocument();
  Element* object_element = document.body()->firstElementChild();
  const LayoutBox* layout_box = object_element->GetLayoutBox();
  EXPECT_EQ(layout_box->StitchedSize(), PhysicalSize(200, 100));

  object_subresource.Finish();

  ASSERT_TRUE(Compositor().NeedsBeginFrame());
  Compositor().BeginFrame();

  EXPECT_EQ(layout_box->StitchedSize(), PhysicalSize(100, 100));
}

}  // namespace blink
