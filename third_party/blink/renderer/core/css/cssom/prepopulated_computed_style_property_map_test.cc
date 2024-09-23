// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/prepopulated_computed_style_property_map.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class PrepopulatedComputedStylePropertyMapTest : public PageTestBase {
 public:
  PrepopulatedComputedStylePropertyMapTest() = default;

  void SetElementWithStyle(const String& value) {
    GetDocument().body()->setInnerHTML("<div id='target' style='" + value +
                                       "'></div>");
    UpdateAllLifecyclePhasesForTest();
  }

  const CSSValue* GetNativeValue(const CSSPropertyID& property_id) {
    Element* element = GetDocument().getElementById(AtomicString("target"));
    return CSSProperty::Get(property_id)
        .CSSValueFromComputedStyle(
            element->ComputedStyleRef(), nullptr /* layout_object */,
            false /* allow_visited_style */, CSSValuePhase::kComputedValue);
  }

  CSSComputedStyleDeclaration* Declaration() const {
    return declaration_.Get();
  }

  void SetUp() override {
    PageTestBase::SetUp(gfx::Size());
    declaration_ = MakeGarbageCollected<CSSComputedStyleDeclaration>(
        GetDocument().documentElement());
  }

  Element* RootElement() { return GetDocument().documentElement(); }

 private:
  Persistent<CSSComputedStyleDeclaration> declaration_;
};

TEST_F(PrepopulatedComputedStylePropertyMapTest, NativePropertyAccessors) {
  Vector<CSSPropertyID> native_properties(
      {CSSPropertyID::kColor, CSSPropertyID::kAlignItems});
  Vector<AtomicString> empty_custom_properties;

  UpdateAllLifecyclePhasesForTest();
  Element* element = RootElement();

  PrepopulatedComputedStylePropertyMap* map =
      MakeGarbageCollected<PrepopulatedComputedStylePropertyMap>(
          GetDocument(), element->ComputedStyleRef(), native_properties,
          empty_custom_properties);

  {
    DummyExceptionStateForTesting exception_state;

    map->get(GetDocument().GetExecutionContext(), "color", exception_state);
    EXPECT_FALSE(exception_state.HadException());

    map->has(GetDocument().GetExecutionContext(), "color", exception_state);
    EXPECT_FALSE(exception_state.HadException());

    map->getAll(GetDocument().GetExecutionContext(), "color", exception_state);
    EXPECT_FALSE(exception_state.HadException());
  }

  {
    DummyExceptionStateForTesting exception_state;
    map->get(GetDocument().GetExecutionContext(), "align-contents",
             exception_state);
    EXPECT_TRUE(exception_state.HadException());
  }

  {
    DummyExceptionStateForTesting exception_state;
    map->has(GetDocument().GetExecutionContext(), "align-contents",
             exception_state);
    EXPECT_TRUE(exception_state.HadException());
  }

  {
    DummyExceptionStateForTesting exception_state;
    map->getAll(GetDocument().GetExecutionContext(), "align-contents",
                exception_state);
    EXPECT_TRUE(exception_state.HadException());
  }
}

TEST_F(PrepopulatedComputedStylePropertyMapTest, CustomPropertyAccessors) {
  Vector<CSSPropertyID> empty_native_properties;
  Vector<AtomicString> custom_properties(
      {AtomicString("--foo"), AtomicString("--bar")});

  UpdateAllLifecyclePhasesForTest();
  Element* element = RootElement();

  PrepopulatedComputedStylePropertyMap* map =
      MakeGarbageCollected<PrepopulatedComputedStylePropertyMap>(
          GetDocument(), element->ComputedStyleRef(), empty_native_properties,
          custom_properties);

  DummyExceptionStateForTesting exception_state;

  const CSSStyleValue* foo =
      map->get(GetDocument().GetExecutionContext(), "--foo", exception_state);
  ASSERT_NE(nullptr, foo);
  ASSERT_EQ(CSSStyleValue::kUnparsedType, foo->GetType());
  EXPECT_FALSE(exception_state.HadException());

  EXPECT_EQ(true, map->has(GetDocument().GetExecutionContext(), "--foo",
                           exception_state));
  EXPECT_FALSE(exception_state.HadException());

  CSSStyleValueVector fooAll = map->getAll(GetDocument().GetExecutionContext(),
                                           "--foo", exception_state);
  EXPECT_EQ(1U, fooAll.size());
  ASSERT_NE(nullptr, fooAll[0]);
  ASSERT_EQ(CSSStyleValue::kUnparsedType, fooAll[0]->GetType());
  EXPECT_FALSE(exception_state.HadException());

  EXPECT_EQ(nullptr, map->get(GetDocument().GetExecutionContext(), "--quix",
                              exception_state));
  EXPECT_FALSE(exception_state.HadException());

  EXPECT_EQ(false, map->has(GetDocument().GetExecutionContext(), "--quix",
                            exception_state));
  EXPECT_FALSE(exception_state.HadException());

  EXPECT_EQ(CSSStyleValueVector(),
            map->getAll(GetDocument().GetExecutionContext(), "--quix",
                        exception_state));
  EXPECT_FALSE(exception_state.HadException());
}

TEST_F(PrepopulatedComputedStylePropertyMapTest, WidthBeingAuto) {
  SetElementWithStyle("width:auto");
  const CSSValue* value = GetNativeValue(CSSPropertyID::kWidth);
  EXPECT_EQ("auto", value->CssText());
}

}  // namespace blink
