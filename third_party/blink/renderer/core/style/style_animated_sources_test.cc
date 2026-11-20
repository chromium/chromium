// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_animated_sources.h"

#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/style/animated_color.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
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

TEST_F(StyleAnimatedSourcesTest, VisitedDependentAnimatedColor) {
  ScopedCompositeColorAnimationForTest enable_color(true);
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes to_blue { to { background-color: blue; } }
      #target { background-color: lime; animation: to_blue 10s steps(1, end); }
    </style>
    <div id="target"></div>
  )HTML");

  Element* target = GetElementById("target");
  const ComputedStyle& style = target->ComputedStyleRef();
  const cc::DeferredPropertyKey expected_key{
      CompositorElementIdFromDOMNodeId(target->GetDomNodeId()),
      cc::DeferredPropertyType::kBackgroundColor};
  EXPECT_EQ(style.VisitedDependentAnimatedColor(GetCSSPropertyBackgroundColor())
                .ToPaintColor(),
            cc::PaintColor(SkColors::kGreen, expected_key));

  // Disabling the feature drops the animated source.
  ScopedCompositeColorAnimationForTest disable_color(false);
  EXPECT_EQ(style.VisitedDependentAnimatedColor(GetCSSPropertyBackgroundColor())
                .ToPaintColor(),
            cc::PaintColor(SkColors::kGreen));
}

TEST_F(StyleAnimatedSourcesTest, AnimatedColorIsFullyTransparent) {
  SetBodyInnerHTML("<div id=a></div>");
  const AnimatedSource source = AnimatedSource::ForElement(GetElementById("a"));

  EXPECT_TRUE(AnimatedColor(Color::kTransparent).IsFullyTransparent());
  // Non-color properties do not map to a deferred key, so stay transparent.
  EXPECT_TRUE(AnimatedColor(Color::kTransparent, source,
                            AnimatedSourceProperty::kOpacity)
                  .IsFullyTransparent());
  // A keyed color stays non-transparent so paint records its draw op.
  EXPECT_FALSE(AnimatedColor(Color::kTransparent, source,
                             AnimatedSourceProperty::kBackgroundColor)
                   .IsFullyTransparent());
}

TEST_F(StyleAnimatedSourcesTest,
       AnimatedColorUntrackedDependencyNeedsMainThreadRepaint) {
  ScopedCompositeColorAnimationForTest enable_color(true);
  SetBodyInnerHTML("<div id=a></div>");
  Element* a = GetElementById("a");

  ComputedStyleBuilder source_builder(
      ComputedStyle::GetInitialStyleSingleton());
  source_builder.SetAnimatedSource(CSSPropertyID::kBackgroundColor, *a);
  const ComputedStyle* source_style = source_builder.TakeStyle();

  ComputedStyleBuilder builder(*source_style);
  builder.CopyAnimatedSourceFrom(CSSPropertyID::kBackgroundColor, source_style,
                                 /*has_untracked_dependencies=*/true);
  const ComputedStyle* untracked = builder.TakeStyle();

  EXPECT_TRUE(
      untracked->VisitedDependentAnimatedColor(GetCSSPropertyBackgroundColor())
          .ToPaintColor()
          .needs_main_thread_repaint);
}

TEST_F(StyleAnimatedSourcesTest, HasBackgroundWithTransparentAnimatedSource) {
  ScopedCompositeColorAnimationForTest enable_color(true);
  SetBodyInnerHTML("<div id=a></div>");
  Element* a = GetElementById("a");

  // A transparent background with an animated source still reports
  // HasBackground() so paint emits a keyed draw op without a local animation.
  ComputedStyleBuilder builder(ComputedStyle::GetInitialStyleSingleton());
  builder.SetAnimatedSource(CSSPropertyID::kBackgroundColor, *a);
  const ComputedStyle* style = builder.TakeStyle();

  EXPECT_FALSE(style->HasCurrentBackgroundColorAnimation());
  EXPECT_TRUE(style->HasBackground());

  ScopedCompositeColorAnimationForTest disable_color(false);
  EXPECT_FALSE(style->HasBackground());
}

}  // namespace
}  // namespace blink
