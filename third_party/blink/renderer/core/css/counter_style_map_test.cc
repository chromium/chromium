// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/counter_style_map.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class CounterStyleMapTest : public PageTestBase,
                            private ScopedCSSAtRuleCounterStyleForTest {
 public:
  CounterStyleMapTest() : ScopedCSSAtRuleCounterStyleForTest(true) {}

  ShadowRoot& AttachShadowTo(const char* host_id) {
    Element* host = GetElementById(host_id);
    return host->AttachShadowRootInternal(ShadowRootType::kOpen);
  }

  const CounterStyle& GetCounterStyle(const TreeScope& scope,
                                      const AtomicString& name) {
    return *CounterStyleMap::GetAuthorCounterStyleMap(scope)
                ->counter_styles_.at(name);
  }
};

TEST_F(CounterStyleMapTest, ExtendsUAStyle) {
  SetHtmlInnerHTML(R"HTML(
    <style> @counter-style foo { system: extends disc; } </style>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  const CounterStyle& foo = GetCounterStyle(GetDocument(), "foo");
  EXPECT_EQ("disc", foo.GetExtendedStyle().GetName());
}

TEST_F(CounterStyleMapTest, ExtendsAuthorStyle) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      @counter-style foo { symbols: 'X'; }
      @counter-style bar { system: extends foo; }
    </style>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  const CounterStyle& bar = GetCounterStyle(GetDocument(), "bar");
  EXPECT_EQ("foo", bar.GetExtendedStyle().GetName());
}

TEST_F(CounterStyleMapTest, ExtendsParentScopeStyle) {
  SetHtmlInnerHTML(R"HTML(
    <style> @counter-style foo { symbols: 'X'; } </style>
    <div id=host></div>
  )HTML");
  ShadowRoot& shadow = AttachShadowTo("host");
  shadow.setInnerHTML(
      "<style>@counter-style bar { system: extends foo; }</style>");
  UpdateAllLifecyclePhasesForTest();

  const CounterStyle& bar = GetCounterStyle(shadow, "bar");
  EXPECT_EQ("foo", bar.GetExtendedStyle().GetName());
}

TEST_F(CounterStyleMapTest, ExtendsCyclic) {
  // Cyclic extends resolve to 'decimal'.
  SetHtmlInnerHTML(R"HTML(
    <style>
      @counter-style foo { system: extends bar; }
      @counter-style bar { system: extends baz; }
      @counter-style baz { system: extends bar; }
    </style>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  const CounterStyle& foo = GetCounterStyle(GetDocument(), "foo");
  EXPECT_EQ("bar", foo.GetExtendedStyle().GetName());

  const CounterStyle& bar = GetCounterStyle(GetDocument(), "bar");
  EXPECT_EQ("decimal", bar.GetExtendedStyle().GetName());

  const CounterStyle& baz = GetCounterStyle(GetDocument(), "baz");
  EXPECT_EQ("decimal", baz.GetExtendedStyle().GetName());
}

TEST_F(CounterStyleMapTest, ExtendsNonexistentStyle) {
  // Extending non-existent style resolves to 'decimal'.
  SetHtmlInnerHTML(R"HTML(
    <style>
      @counter-style foo { system: extends bar; }
      @counter-style bar { system: extends baz; }
    </style>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  const CounterStyle& foo = GetCounterStyle(GetDocument(), "foo");
  EXPECT_EQ("bar", foo.GetExtendedStyle().GetName());

  const CounterStyle& bar = GetCounterStyle(GetDocument(), "bar");
  EXPECT_EQ("decimal", bar.GetExtendedStyle().GetName());
}

TEST_F(CounterStyleMapTest, FallbackToUAStyle) {
  SetHtmlInnerHTML(R"HTML(
    <style> @counter-style foo { symbols: 'X'; fallback: disc; } </style>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  const CounterStyle& foo = GetCounterStyle(GetDocument(), "foo");
  EXPECT_EQ("disc", foo.GetFallbackStyle().GetName());
}

TEST_F(CounterStyleMapTest, FallbackToAuthorStyle) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      @counter-style foo { symbols: 'X'; }
      @counter-style bar { symbols: 'Y'; fallback: foo; }
    </style>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  const CounterStyle& bar = GetCounterStyle(GetDocument(), "bar");
  EXPECT_EQ("foo", bar.GetFallbackStyle().GetName());
}

TEST_F(CounterStyleMapTest, FallbackOnExtends) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      @counter-style foo { symbols: 'X'; fallback: disc; }
      @counter-style bar { system: extends foo; }
    </style>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  const CounterStyle& bar = GetCounterStyle(GetDocument(), "bar");
  EXPECT_EQ("disc", bar.GetFallbackStyle().GetName());
}

TEST_F(CounterStyleMapTest, FallbackCyclic) {
  // Cyclic fallbacks are allowed. We break cycles when generating counter text.
  SetHtmlInnerHTML(R"HTML(
    <style>
      @counter-style foo { symbols: 'X'; fallback: bar; }
      @counter-style bar { symbols: 'X'; fallback: foo; }
    </style>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  const CounterStyle& foo = GetCounterStyle(GetDocument(), "foo");
  EXPECT_EQ("bar", foo.GetFallbackStyle().GetName());

  const CounterStyle& bar = GetCounterStyle(GetDocument(), "bar");
  EXPECT_EQ("foo", bar.GetFallbackStyle().GetName());
}

TEST_F(CounterStyleMapTest, FallbackToNonexistentStyle) {
  // Fallback to non-existent style resolves to 'decimal'.
  SetHtmlInnerHTML(R"HTML(
    <style>
      @counter-style foo { symbols: 'X'; fallback: bar; }
      @counter-style bar { symbols: 'X'; fallback: baz; }
    </style>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  const CounterStyle& foo = GetCounterStyle(GetDocument(), "foo");
  EXPECT_EQ("bar", foo.GetFallbackStyle().GetName());

  const CounterStyle& bar = GetCounterStyle(GetDocument(), "bar");
  EXPECT_EQ("decimal", bar.GetFallbackStyle().GetName());
}

TEST_F(CounterStyleMapTest, UpdateReferencesInChildScope) {
  SetHtmlInnerHTML(R"HTML(
    <style> @counter-style foo { symbols: 'X'; } </style>
    <div id=host></div>
  )HTML");
  ShadowRoot& shadow = AttachShadowTo("host");
  shadow.setInnerHTML(
      "<style>@counter-style bar { system: extends foo; }</style>");
  UpdateAllLifecyclePhasesForTest();

  const CounterStyle& bar = GetCounterStyle(shadow, "bar");
  EXPECT_EQ("foo", bar.GetExtendedStyle().GetName());

  GetDocument().QuerySelector("style")->remove();
  UpdateAllLifecyclePhasesForTest();

  // Counter styles in child scopes should be updated after chaning the counter
  // styles in the parent scope.
  EXPECT_EQ("decimal", bar.GetExtendedStyle().GetName());
}

}  // namespace blink
