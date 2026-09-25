// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/inherited_animations/inherited_animation_tracker.h"

#include <algorithm>

#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/animated_source_handle.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_animated_sources.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

// Returns true if any of `style`'s animated sources is another element's.
bool HasSourceFromAnotherElement(const ComputedStyle& style,
                                 const Element& element) {
  auto is_other = [&](auto entry) { return !entry.value.IsOwnedBy(element); };
  return std::ranges::any_of(style.InheritedAnimatedSources(), is_other) ||
         std::ranges::any_of(style.NonInheritedAnimatedSources(), is_other);
}

}  // namespace

void InheritedAnimationTracker::StyleDidChange(LayoutObject& object,
                                               const ComputedStyle* old_style) {
  if (!RuntimeEnabledFeatures::TrackAnimatedSourcesEnabled()) {
    return;
  }
  // Initial style attachment already marks the object for a paint property
  // update.
  if (!old_style) {
    return;
  }
  const auto* element = DynamicTo<Element>(object.GetNode());
  if (!element) {
    return;
  }
  const ComputedStyle& new_style = object.StyleRef();
  if (old_style->InheritedAnimatedSources() ==
          new_style.InheritedAnimatedSources() &&
      old_style->NonInheritedAnimatedSources() ==
          new_style.NonInheritedAnimatedSources()) {
    return;
  }
  // The element's own animations are not inherited, so only another element's
  // source matters.
  if (HasSourceFromAnotherElement(*old_style, *element) ||
      HasSourceFromAnotherElement(new_style, *element)) {
    object.SetNeedsPaintPropertyUpdate();
  }
}

void InheritedAnimationTracker::DowngradeForUnsupportedInheritance(
    const LayoutObject& object) {
  if (!RuntimeEnabledFeatures::TrackAnimatedSourcesEnabled()) {
    return;
  }
  CHECK_EQ(object.GetDocument().Lifecycle().GetState(),
           DocumentLifecycle::kInPrePaint);
  const auto* element = DynamicTo<Element>(object.GetNode());
  if (!element) {
    return;
  }
  // Every tracked property is recorded here for now.
  // TODO(crbug.com/545152722): Properties used in paint rather than by paint
  // property nodes (e.g. color) should be recorded by their painters instead.
  auto record = [element](const StyleAnimatedSources& sources) {
    for (auto [property, source] : sources) {
      // The element's own animations target its own paint property nodes,
      // which the compositor animates directly.
      if (source.IsOwnedBy(*element)) {
        continue;
      }
      Element* source_element = source.animated_source.GetElement();
      if (!source_element) {
        continue;
      }
      ElementAnimations* element_animations =
          source_element->GetElementAnimations();
      DCHECK(element_animations);
      AnimatedSourceBitset& unsupported_inherited_properties =
          element_animations->UnsupportedInheritedProperties();
      if (!unsupported_inherited_properties.Has(property)) {
        unsupported_inherited_properties.Put(property);
        element_animations->DowngradeCompositedAnimationsAffectingProperties(
            AnimatedSourceBitset({property}));
      }
    }
  };
  const ComputedStyle& style = object.StyleRef();
  record(style.InheritedAnimatedSources());
  record(style.NonInheritedAnimatedSources());
}

}  // namespace blink
