// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_animated_sources.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace {

class StyleAnimatedSourcesTest : public PageTestBase {
 private:
  ScopedTrackAnimatedSourcesForTest enable_feature_{true};
};

TEST_F(StyleAnimatedSourcesTest, AvoidsCopyOnWriteWhenSettingIdenticalSources) {
  SetBodyInnerHTML("<div id=a></div>");
  Element* a = GetElementById("a");

  ComputedStyleBuilder builder(ComputedStyle::GetInitialStyleSingleton());
  builder.SetAnimatedSource(CSSPropertyID::kOpacity, *a);
  const ComputedStyle* style1 = builder.TakeStyle();

  // Setting the same source compares equal, so the group is not copied.
  ComputedStyleBuilder matching_builder(*style1);
  matching_builder.SetAnimatedSource(CSSPropertyID::kOpacity, *a);
  const ComputedStyle* style2 = matching_builder.TakeStyle();

  EXPECT_EQ(&style1->NonInheritedAnimatedSources(),
            &style2->NonInheritedAnimatedSources());
}

TEST_F(StyleAnimatedSourcesTest, Iteration) {
  SetBodyInnerHTML("<div id=a></div>");
  Element* a = GetElementById("a");

  StyleAnimatedSources sources;
  EXPECT_TRUE(sources.begin() == sources.end());

  sources.Set(
      AnimatedSourceProperty::kTransform,
      AnimatedSource::ForElement(a, /*has_untracked_dependencies=*/true));
  sources.Set(AnimatedSourceProperty::kOpacity, AnimatedSource::ForElement(a));

  // Entries come out in property order, with their untracked dependencies.
  Vector<AnimatedSourceProperty> properties;
  Vector<bool> untracked;
  for (auto [property, source] : sources) {
    EXPECT_TRUE(source.IsOwnedBy(*a));
    properties.push_back(property);
    untracked.push_back(source.has_untracked_dependencies);
  }
  EXPECT_EQ(properties, (Vector<AnimatedSourceProperty>{
                            AnimatedSourceProperty::kOpacity,
                            AnimatedSourceProperty::kTransform}));
  EXPECT_EQ(untracked, (Vector<bool>{false, true}));
}

TEST_F(StyleAnimatedSourcesTest, GetHashEmptyAndCleared) {
  SetBodyInnerHTML("<div id=a></div>");
  Element* a = GetElementById("a");

  StyleAnimatedSources sources;
  EXPECT_EQ(sources.GetHash(), 0u);

  sources.Set(AnimatedSourceProperty::kColor, AnimatedSource::ForElement(a));
  EXPECT_NE(sources.GetHash(), 0u);

  sources.Clear(AnimatedSourceProperty::kColor);
  EXPECT_EQ(sources.GetHash(), 0u);
}

TEST_F(StyleAnimatedSourcesTest, GetHashEqualRegardlessOfInsertionOrder) {
  SetBodyInnerHTML("<div id=a></div><div id=b></div>");
  Element* a = GetElementById("a");
  Element* b = GetElementById("b");

  StyleAnimatedSources first;
  first.Set(AnimatedSourceProperty::kColor, AnimatedSource::ForElement(a));
  first.Set(AnimatedSourceProperty::kOpacity, AnimatedSource::ForElement(b));

  StyleAnimatedSources second;
  second.Set(AnimatedSourceProperty::kOpacity, AnimatedSource::ForElement(b));
  second.Set(AnimatedSourceProperty::kColor, AnimatedSource::ForElement(a));

  EXPECT_EQ(first, second);
  EXPECT_EQ(first.GetHash(), second.GetHash());
}

TEST_F(StyleAnimatedSourcesTest, GetHashDiffersByPropertyElementAndUntracked) {
  SetBodyInnerHTML("<div id=a></div><div id=b></div>");
  Element* a = GetElementById("a");
  Element* b = GetElementById("b");

  StyleAnimatedSources base;
  base.Set(AnimatedSourceProperty::kColor, AnimatedSource::ForElement(a));

  StyleAnimatedSources untracked;
  untracked.Set(
      AnimatedSourceProperty::kColor,
      AnimatedSource::ForElement(a, /*has_untracked_dependencies=*/true));
  EXPECT_NE(base.GetHash(), untracked.GetHash());

  StyleAnimatedSources different_element;
  different_element.Set(AnimatedSourceProperty::kColor,
                        AnimatedSource::ForElement(b));
  EXPECT_NE(base.GetHash(), different_element.GetHash());

  StyleAnimatedSources different_property;
  different_property.Set(AnimatedSourceProperty::kOpacity,
                         AnimatedSource::ForElement(a));
  EXPECT_NE(base.GetHash(), different_property.GetHash());
}

}  // namespace
}  // namespace blink
