// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {
namespace {

bool UsesNGLayout(const Element& element) {
  return !element.ShouldForceLegacyLayout() &&
         element.GetLayoutObject()->IsLayoutNGObject();
}

}  // anonymous namespace

class ForceLegacyLayoutTest : public RenderingTest {
 protected:
  ForceLegacyLayoutTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}
};

TEST_F(ForceLegacyLayoutTest, ForceLegacyMulticolSlot) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;
  if (RuntimeEnabledFeatures::LayoutNGBlockFragmentationEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <div id="host">
      <p id="slotted"></p>
    </div>
  )HTML");

  Element* host = GetDocument().getElementById("host");
  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML(R"HTML(
    <style>
      slot { columns: 2; display: block }
    </style>
    <slot></slot>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(UsesNGLayout(*GetDocument().getElementById("slotted")));
}

}  // namespace blink
