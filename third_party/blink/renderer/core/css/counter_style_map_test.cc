// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/counter_style_map.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class CounterStyleMapTest : public PageTestBase {
 public:
  ShadowRoot& AttachShadowTo(const char* host_id) {
    Element* host = GetElementById(host_id);
    return host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  }

  const CounterStyle& GetCounterStyle(const TreeScope& scope,
                                      const char* name) {
    return *CounterStyleMap::GetAuthorCounterStyleMap(scope)
                ->counter_styles_.at(AtomicString(name));
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

  const CounterStyle& foo = GetCounterStyle(GetDocument(), "foo");
  const CounterStyle& bar = GetCounterStyle(shadow, "bar");
  EXPECT_EQ(&foo, &bar.GetExtendedStyle());

  GetDocument().QuerySelector(AtomicString("style"))->remove();
  UpdateAllLifecyclePhasesForTest();

  // After counter style rule changes in the parent scope, the original
  // CounterStyle for 'bar' in child scopes will be dirtied, and will be
  // replaced by a new CounterStyle object.
  EXPECT_TRUE(foo.IsDirty());
  EXPECT_TRUE(bar.IsDirty());

  const CounterStyle& new_bar = GetCounterStyle(shadow, "bar");
  EXPECT_NE(&bar, &new_bar);
  EXPECT_EQ("decimal", new_bar.GetExtendedStyle().GetName());
}

TEST_F(CounterStyleMapTest, SpeakAsKeywords) {
  ScopedCSSAtRuleCounterStyleSpeakAsDescriptorForTest enabled(true);

  SetHtmlInnerHTML(R"HTML(
    <style>
      @counter-style implicit-auto { symbols: 'X'; }
      @counter-style explicit-auto { speak-as: auto; symbols: 'X'; }
      @counter-style bullets { speak-as: bullets; symbols: 'X'; }
      @counter-style numbers { speak-as: numbers; symbols: 'X'; }
      @counter-style words { speak-as: words; symbols: 'X'; }
    </style>
  )HTML");

  const CounterStyle& implicit_auto =
      GetCounterStyle(GetDocument(), "implicit-auto");
  EXPECT_EQ(CounterStyleSpeakAs::kAuto, implicit_auto.GetSpeakAs());

  const CounterStyle& explicit_auto =
      GetCounterStyle(GetDocument(), "explicit-auto");
  EXPECT_EQ(CounterStyleSpeakAs::kAuto, explicit_auto.GetSpeakAs());

  const CounterStyle& bullets = GetCounterStyle(GetDocument(), "bullets");
  EXPECT_EQ(CounterStyleSpeakAs::kBullets, bullets.GetSpeakAs());

  const CounterStyle& numbers = GetCounterStyle(GetDocument(), "numbers");
  EXPECT_EQ(CounterStyleSpeakAs::kNumbers, numbers.GetSpeakAs());

  const CounterStyle& words = GetCounterStyle(GetDocument(), "words");
  EXPECT_EQ(CounterStyleSpeakAs::kWords, words.GetSpeakAs());
}

TEST_F(CounterStyleMapTest, SpeakAsReference) {
  ScopedCSSAtRuleCounterStyleSpeakAsDescriptorForTest enabled(true);

  SetHtmlInnerHTML(R"HTML(
    <style>
      @counter-style base { symbols: 'X'; }
      @counter-style valid-author-ref { speak-as: base; symbols: 'X'; }
      @counter-style valid-ua-ref { speak-as: disc; symbols: 'X'; }
      @counter-style invalid { speak-as: unknown; symbols: 'X'; }
    </style>
  )HTML");

  const CounterStyle& valid_author_ref =
      GetCounterStyle(GetDocument(), "valid-author-ref");
  EXPECT_EQ(CounterStyleSpeakAs::kReference, valid_author_ref.GetSpeakAs());
  EXPECT_EQ("base", valid_author_ref.GetSpeakAsStyle().GetName());

  const CounterStyle& valid_ua_ref =
      GetCounterStyle(GetDocument(), "valid-ua-ref");
  EXPECT_EQ(CounterStyleSpeakAs::kReference, valid_ua_ref.GetSpeakAs());
  EXPECT_EQ("disc", valid_ua_ref.GetSpeakAsStyle().GetName());

  // Invalid 'speak-as' reference will be treated as 'speak-as: auto'.
  const CounterStyle& invalid = GetCounterStyle(GetDocument(), "invalid");
  EXPECT_EQ(CounterStyleSpeakAs::kAuto, invalid.GetSpeakAs());
}

TEST_F(CounterStyleMapTest, SpeakAsReferenceLoop) {
  ScopedCSSAtRuleCounterStyleSpeakAsDescriptorForTest enabled(true);

  SetHtmlInnerHTML(R"HTML(
    <style>
      @counter-style a { speak-as: b; symbols: 'X'; }
      @counter-style b { speak-as: a; symbols: 'X'; }
      @counter-style c { speak-as: b; symbols: 'X'; }
    </style>
  )HTML");

  const CounterStyle& a = GetCounterStyle(GetDocument(), "a");
  const CounterStyle& b = GetCounterStyle(GetDocument(), "b");
  const CounterStyle& c = GetCounterStyle(GetDocument(), "c");

  // Counter styles on a 'speak-as' loop will be treated as 'speak-as: auto'.
  EXPECT_EQ(CounterStyleSpeakAs::kAuto, a.GetSpeakAs());
  EXPECT_EQ(CounterStyleSpeakAs::kAuto, b.GetSpeakAs());

  // c is not on the loop, so its reference remains valid.
  EXPECT_EQ(CounterStyleSpeakAs::kReference, c.GetSpeakAs());
  EXPECT_EQ(&b, &c.GetSpeakAsStyle());
}

}  // namespace blink
