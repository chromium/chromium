// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class PseudoElementTest : public RenderingTest {};

TEST_F(PseudoElementTest, AttachLayoutTree) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
    #marker1 { display: list-item; }
    #marker2 { display: flow-root list-item; }
    #marker3 { display: inline flow list-item; }
    #marker4 { display: inline flow-root list-item; }
    </style>
    <div id="marker1"></div>
    <div id="marker2"></div>
    <div id="marker3"></div>
    <div id="marker4"></div>
    )HTML");
  GetDocument().UpdateStyleAndLayoutTree();

  EXPECT_TRUE(GetLayoutObjectByElementId("marker1")
                  ->SlowFirstChild()
                  ->IsLayoutOutsideListMarker());
  EXPECT_TRUE(GetLayoutObjectByElementId("marker2")
                  ->SlowFirstChild()
                  ->IsLayoutOutsideListMarker());
  EXPECT_TRUE(GetLayoutObjectByElementId("marker3")
                  ->SlowFirstChild()
                  ->IsLayoutInsideListMarker());
  EXPECT_TRUE(GetLayoutObjectByElementId("marker4")
                  ->SlowFirstChild()
                  ->IsLayoutOutsideListMarker());
}

}  // namespace blink
