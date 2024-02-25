// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/forms/layout_fieldset.h"

#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutFieldsetTest : public RenderingTest {};

TEST_F(LayoutFieldsetTest, AddChildWhitespaceCrash) {
  SetBodyInnerHTML(R"HTML(
<fieldset>
<small>s</small>
<!-- -->
<legend></legend>
</fieldset>)HTML");
  UpdateAllLifecyclePhasesForTest();

  Node* text =
      GetDocument().QuerySelector(AtomicString("small"))->nextSibling();
  ASSERT_TRUE(IsA<Text>(text));
  text->remove();
  UpdateAllLifecyclePhasesForTest();

  // Passes if no crash in LayoutFieldset::AddChild().
}

TEST_F(LayoutFieldsetTest, AddChildAnonymousInlineCrash) {
  SetBodyInnerHTML(R"HTML(
<fieldset>
<span id="a">A</span> <span style="display:contents; hyphens:auto">&#x20;
<legend>B</legend></span></fieldset>)HTML");
  UpdateAllLifecyclePhasesForTest();

  GetElementById("a")->nextSibling()->remove();
  UpdateAllLifecyclePhasesForTest();

  // Passes if no crash in LayoutFieldset::AddChild().
}

}  // namespace blink
