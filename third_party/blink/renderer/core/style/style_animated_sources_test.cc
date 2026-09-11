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
  const ComputedStyle* initial = ComputedStyle::GetInitialStyleSingleton();

  ComputedStyleBuilder builder(*initial);
  builder.SetAnimatedSource(CSSPropertyID::kOpacity, *a);
  const ComputedStyle* style1 = builder.TakeStyle();

  // Setting the same source compares equal, so the group is not copied.
  ComputedStyleBuilder matching_builder(*style1);
  matching_builder.SetAnimatedSource(CSSPropertyID::kOpacity, *a);
  const ComputedStyle* style2 = matching_builder.TakeStyle();

  EXPECT_EQ(&style1->NonInheritedAnimatedSources(),
            &style2->NonInheritedAnimatedSources());
}

}  // namespace
}  // namespace blink
