// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_field_set_element.h"

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class HTMLFieldSetElementTest : public RenderingTest {};

// crbug.com/1240414
TEST_F(HTMLFieldSetElementTest, DidRecalcStyleWithDescendantReattach) {
  SetBodyInnerHTML(R"HTML(
<fieldset id="fieldset">
 <legend>legend</legend>
 <div><span id="span" style="display:none">span</span></div>
</fieldset>)HTML");
  UpdateAllLifecyclePhasesForTest();

  // Reattach of a fieldset descendant should not reattach the fieldset.
  auto* previous_layout_box = GetLayoutBoxByElementId("fieldset");
  auto* descendant = GetElementById("span");
  descendant->removeAttribute(html_names::kStyleAttr);
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_EQ(previous_layout_box, GetLayoutBoxByElementId("fieldset"));
}

}  // namespace blink
