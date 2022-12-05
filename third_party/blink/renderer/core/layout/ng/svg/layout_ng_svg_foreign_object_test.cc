// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class LayoutNGSVGForeignObjectTest : public NGLayoutTest {
 public:
  LayoutNGSVGForeignObjectTest() = default;
};

// crbug.com/1372886
TEST_F(LayoutNGSVGForeignObjectTest, SubtreeLayoutCrash) {
  SetBodyInnerHTML(R"HTML(
<svg style="position:absolute;">
  <svg></svg>
  <foreignObject>
    <div id="in-foreign"></div>
  </foreignObject>
</svg>
<div></div>
<span></span>
<div id="sibling-div"></div>
<svg><pattern id="pat"></pattern>
</svg>)HTML");
  UpdateAllLifecyclePhasesForTest();
  GetElementById("in-foreign")->setAttribute("style", "display: inline-block");
  UpdateAllLifecyclePhasesForTest();
  GetElementById("pat")->setAttribute("viewBox", "972 815 1088 675");
  UpdateAllLifecyclePhasesForTest();
  GetElementById("sibling-div")->setAttribute("style", "display: none");
  UpdateAllLifecyclePhasesForTest();
  // Pass if no crashes.
}

}  // namespace blink
