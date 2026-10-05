// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/pseudo_element.h"

#include <tuple>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class PseudoElementTest : public RenderingTest {};

TEST_F(PseudoElementTest, DisallowV8WrapperCreation) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
    #target::before { content: "hello"; }
    </style>
    <div id="target"></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetElementById("target");
  ASSERT_TRUE(target);
  PseudoElement* pseudo = target->GetPseudoElement(kPseudoIdBefore);
  ASSERT_TRUE(pseudo);

  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  v8::Isolate* isolate = script_state->GetIsolate();
  ScriptState::Scope scope(script_state);
  EXPECT_DEATH_IF_SUPPORTED(
      std::ignore = ToV8Traits<Node>::ToV8(script_state, pseudo), "");
  EXPECT_DEATH_IF_SUPPORTED(
      std::ignore = pseudo->AssociateWithWrapper(
          isolate, pseudo->GetWrapperTypeInfo(), v8::Object::New(isolate)),
      "");
}

TEST_F(PseudoElementTest, AttachLayoutTree) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
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
