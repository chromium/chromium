// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutMultiColumnSetTest : public RenderingTest {};

// crbug.com/1420201
TEST_F(LayoutMultiColumnSetTest, ScrollAnchroingCrash) {
  SetBodyInnerHTML(R"HTML(
<style>
.c3 {
  padding-top: 100%;
}
.c4 {
  appearance: button;
  column-span: all;
}
.c7 {
  position: absolute;
  padding-left: 65536px;
  zoom: 5;
  column-width: 10px;
}
.c13 {
  zoom: 5;
  column-span: all;
  height: 10px;
}
</style>
<div class=c7><div class=c13></div><map class=c4></map></div>
<h1 class=c3><button></button></h1>)HTML");
  // Triggers scroll anchoring.
  GetDocument().QuerySelector(AtomicString("button"))->Focus();
  UpdateAllLifecyclePhasesForTest();

  // Reattach c13.
  Element* target = GetDocument().QuerySelector(AtomicString(".c13"));
  auto* parent = target->parentNode();
  parent->removeChild(target);
  parent->insertBefore(target, parent->firstChild());
  // Make sure LayoutMultiColumnSet::UpdateGeometry() is called.
  parent->GetLayoutBox()->InvalidateCachedGeometry();
  UpdateAllLifecyclePhasesForTest();
  // Pass if no crash in UpdateGeometry() called through ScrollAnchor.
}

}  // namespace blink
