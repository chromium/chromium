/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/style/computed_style.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/memory/values_equivalent.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/clamped_math.h"
#include "build/build_config.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/paint/paint_flags.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom-blink.h"
#include "third_party/blink/renderer/core/animation/css/css_animation_data.h"
#include "third_party/blink/renderer/core/animation/css/css_transition_data.h"
#include "third_party/blink/renderer/core/css/css_paint_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_equality.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/css_unresolved_property.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_legend_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_li_element.h"
#include "third_party/blink/renderer/core/html/html_progress_element.h"
#include "third_party/blink/renderer/core/layout/custom/layout_worklet.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/map_coordinates_flags.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/core/style/basic_shapes.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/computed_style_initial_values.h"
#include "third_party/blink/renderer/core/style/content_data.h"
#include "third_party/blink/renderer/core/style/coord_box_offset_path_operation.h"
#include "third_party/blink/renderer/core/style/cursor_data.h"
#include "third_party/blink/renderer/core/style/gap_data.h"
#include "third_party/blink/renderer/core/style/reference_offset_path_operation.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/core/style/shape_offset_path_operation.h"
#include "third_party/blink/renderer/core/style/style_difference.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/core/style/style_generated_image.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/core/style/style_inherited_variables.h"
#include "third_party/blink/renderer/core/style/style_non_inherited_variables.h"
#include "third_party/blink/renderer/core/style/style_ray.h"
#include "third_party/blink/renderer/core/style/style_shape.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_geometry_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_functions.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/geometry/path.h"
#include "third_party/blink/renderer/platform/geometry/path_builder.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/capitalize.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/text/quotes_data.h"
#include "third_party/blink/renderer/platform/transforms/rotate_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/scale_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/translate_transform_operation.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/case_map.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/code_point_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/math_transform.h"
#include "third_party/blink/renderer/platform/wtf/text/text_offset_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

// Since different compilers/architectures pack ComputedStyle differently,
// re-create the same structure for an accurate size comparison.
//
// Keep a separate struct for ComputedStyleBase so that we can recreate the
// inheritance structure. Make sure the fields have the same access specifiers
// as in the "real" class since it can affect the layout. Reference the fields
// so that they are not seen as unused (-Wunused-private-field).
struct SameSizeAsComputedStyleBase
    : public GarbageCollected<SameSizeAsComputedStyleBase> {
  SameSizeAsComputedStyleBase() {
    base::debug::Alias(&pointers);
    base::debug::Alias(&bitfields);
  }

 private:
  Member<void*> pointers[10];
  unsigned bitfields[5];
};

struct SameSizeAsComputedStyle : public SameSizeAsComputedStyleBase {
  SameSizeAsComputedStyle() { base::debug::Alias(&own_ptrs); }

 private:
  Member<void*> own_ptrs[1];
};

// If this assert fails, it means that size of ComputedStyle has changed. Please
// check that you really *do* want to increase the size of ComputedStyle, then
// update the SameSizeAsComputedStyle struct to match the updated storage of
// ComputedStyle.
ASSERT_SIZE(ComputedStyle, SameSizeAsComputedStyle);

StyleCachedData& ComputedStyle::EnsureCachedData() const {
  if (!cached_data_) {
    cached_data_ = MakeGarbageCollected<StyleCachedData>();
  }
  return *cached_data_;
}

bool ComputedStyle::HasCachedPseudoElementStyles() const {
  return cached_data_ && cached_data_->pseudo_element_styles_ &&
         cached_data_->pseudo_element_styles_->size();
}

PseudoElementStyleCache* ComputedStyle::GetPseudoElementStyleCache() const {
  if (cached_data_) {
    return cached_data_->pseudo_element_styles_.Get();
  }
  return nullptr;
}

PseudoElementStyleCache& ComputedStyle::EnsurePseudoElementStyleCache() const {
  if (!cached_data_ || !cached_data_->pseudo_element_styles_) {
    EnsureCachedData().pseudo_element_styles_ =
        MakeGarbageCollected<PseudoElementStyleCache>();
  }
  return *cached_data_->pseudo_element_styles_;
}

const ComputedStyle* ComputedStyle::GetInitialStyleSingleton() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      ThreadSpecific<Persistent<const ComputedStyle>>,
      thread_specific_initial_style, ());
  Persistent<const ComputedStyle>& persistent = *thread_specific_initial_style;
  if (!persistent) [[unlikely]] {
    persistent = MakeGarbageCollected<ComputedStyle>(PassKey());
    LEAK_SANITIZER_IGNORE_OBJECT(&persistent);
  }
  return persistent.Get();
}

Vector<AtomicString>* ComputedStyle::GetVariableNamesCache() const {
  if (cached_data_) {
    return cached_data_->variable_names_.get();
  }
  return nullptr;
}

Vector<AtomicString>& ComputedStyle::EnsureVariableNamesCache() const {
  if (!cached_data_ || !cached_data_->variable_names_) {
    EnsureCachedData().variable_names_ =
        std::make_unique<Vector<AtomicString>>();
  }
  return *cached_data_->variable_names_;
}

ALWAYS_INLINE ComputedStyle::ComputedStyle() = default;

ALWAYS_INLINE ComputedStyle::ComputedStyle(const ComputedStyle& initial_style)
    : ComputedStyleBase(initial_style) {}

ALWAYS_INLINE ComputedStyle::ComputedStyle(const ComputedStyleBuilder& builder)
    : ComputedStyleBase(builder) {}

ALWAYS_INLINE ComputedStyle::ComputedStyle(PassKey key) : ComputedStyle() {}

ALWAYS_INLINE ComputedStyle::ComputedStyle(BuilderPassKey key,
                                           const ComputedStyle& initial_style)
    : ComputedStyle(initial_style) {}

ALWAYS_INLINE ComputedStyle::ComputedStyle(BuilderPassKey key,
                                           const ComputedStyleBuilder& builder)
    : ComputedStyle(builder) {}

static bool PseudoElementStylesEqual(const ComputedStyle& old_style,
                                     const ComputedStyle& new_style) {
  if (!old_style.HasAnyPseudoElementStyles() &&
      !new_style.HasAnyPseudoElementStyles()) {
    return true;
  }
  for (PseudoId pseudo_id = kFirstPublicPseudoId;
       pseudo_id <= kLastTrackedPublicPseudoId;
       pseudo_id = static_cast<PseudoId>(pseudo_id + 1)) {
    if (!old_style.HasPseudoElementStyle(pseudo_id) &&
        !new_style.HasPseudoElementStyle(pseudo_id)) {
      continue;
    }
    // Highlight pseudo styles are stored in StyleHighlightData, and compared
    // like any other inherited field, yielding Difference::kInherited.
    if (IsHighlightPseudoElement(pseudo_id)) {
      continue;
    }
    const ComputedStyle* new_pseudo_style =
        new_style.GetCachedPseudoElementStyle(pseudo_id);
    if (!new_pseudo_style) {
      return false;
    }
    const ComputedStyle* old_pseudo_style =
        old_style.GetCachedPseudoElementStyle(pseudo_id);
    if (old_pseudo_style && *old_pseudo_style != *new_pseudo_style) {
      return false;
    }
  }
  return true;
}

static bool DiffAffectsContainerQueries(const ComputedStyle& old_style,
                                        const ComputedStyle& new_style) {
  if (!old_style.IsContainerForSizeContainerQueries() &&
      !new_style.IsContainerForSizeContainerQueries() &&
      !old_style.IsContainerForScrollStateContainerQueries() &&
      !new_style.IsContainerForScrollStateContainerQueries()) {
    return false;
  }
  if (!base::ValuesEquivalent(old_style.ContainerName(),
                              new_style.ContainerName()) ||
      (old_style.ContainerType() != new_style.ContainerType())) {
    return true;
  }
  if (new_style.Display() != old_style.Display()) {
    if (new_style.Display() == EDisplay::kNone ||
        new_style.Display() == EDisplay::kContents) {
      return true;
    }
  }
  return false;
}

static bool DiffAffectsScrollAnimations(const ComputedStyle& old_style,
                                        const ComputedStyle& new_style) {
  if (!base::ValuesEquivalent(old_style.ScrollTimelineName(),
                              new_style.ScrollTimelineName()) ||
      (old_style.ScrollTimelineAxis() != new_style.ScrollTimelineAxis())) {
    return true;
  }
  if (!base::ValuesEquivalent(old_style.ViewTimelineName(),
                              new_style.ViewTimelineName()) ||
      (old_style.ViewTimelineAxis() != new_style.ViewTimelineAxis()) ||
      (old_style.ViewTimelineInset() != new_style.ViewTimelineInset())) {
    return true;
  }
  if (!base::ValuesEquivalent(old_style.TimelineScope(),
                              new_style.TimelineScope())) {
    return true;
  }
  return false;
}

static bool DiffNeedsFullLayoutForAnimationTriggers(
    const ComputedStyle& old_style,
    const ComputedStyle& new_style) {
  const CSSAnimationData* old_animations = old_style.Animations();
  const CSSAnimationData* new_animations = new_style.Animations();
  if (old_animations && new_animations) {
    return (old_animations != new_animations) &&
           !old_animations->AnimationsMatchForStyleRecalc(*new_animations);
  } else if (old_animations || new_animations) {
    // If one of the ComputedStyles didn't have CSSAnimationData and the other
    // did, the other is only meaningfully different if it declared a named
    // trigger.
    const CSSAnimationData* animations =
        new_animations ? new_animations : old_animations;
    return std::any_of(animations->TimelineTriggerNameList().begin(),
                       animations->TimelineTriggerNameList().end(),
                       [](Member<const ScopedCSSName> trigger_name) {
                         return trigger_name.Get();
                       });
  }
  return false;
}

bool ComputedStyle::NeedsReattachLayoutTree(const Element& element,
                                            const ComputedStyle* old_style,
                                            const ComputedStyle* new_style) {
  if (old_style == new_style) {
    return false;
  }
  if (!old_style || !new_style) {
    return true;
  }
  if (old_style->Display() != new_style->Display()) {
    return true;
  }
  if (old_style->HasPseudoElementStyle(kPseudoIdFirstLetter) !=
      new_style->HasPseudoElementStyle(kPseudoIdFirstLetter)) {
    return true;
  }
  if (!old_style->ContentDataEquivalent(*new_style)) {
    return true;
  }
  if (old_style->HasTextCombine() != new_style->HasTextCombine()) {
    return true;
  }
  if (!old_style->ScrollMarkerGroupEqual(*new_style)) {
    return true;
  }
  if (old_style->OverscrollArea() != new_style->OverscrollArea()) {
    return true;
  }
  // We need to perform a reattach if a "display: layout(foo)" has changed to a
  // "display: layout(bar)". This is because one custom layout could be
  // registered and the other may not, affecting the box-tree construction.
  if (old_style->DisplayLayoutCustomName() !=
      new_style->DisplayLayoutCustomName()) {
    return true;
  }
  if (old_style->HasEffectiveAppearance() !=
          new_style->HasEffectiveAppearance() &&
      IsA<HTMLProgressElement>(element)) {
    // HTMLProgressElement::CreateLayoutObject creates different LayoutObjects
    // based on appearance.
    return true;
  }

  // LayoutObject tree structure for <legend> depends on whether it's a
  // rendered legend or not.
  if (IsA<HTMLLegendElement>(element) &&
      (old_style->IsFloating() != new_style->IsFloating() ||
       old_style->HasOutOfFlowPosition() != new_style->HasOutOfFlowPosition()))
      [[unlikely]] {
    return true;
  }

  // We use LayoutTextCombine only for vertical typographic mode.
  if (new_style->HasTextCombine() &&
      LayoutTextCombine::IsSupportedMode(old_style->GetWritingMode()) !=
          LayoutTextCombine::IsSupportedMode(new_style->GetWritingMode())) {
    DCHECK_EQ(old_style->HasTextCombine(), new_style->HasTextCombine());
    return true;
  }

  // LayoutNG needs an anonymous inline wrapper if ::first-line is applied.
  // Also see |LayoutBlockFlow::NeedsAnonymousInlineWrapper()|.
  if (new_style->HasPseudoElementStyle(kPseudoIdFirstLine) &&
      !old_style->HasPseudoElementStyle(kPseudoIdFirstLine)) {
    return true;
  }

  if (old_style->Overlay() != new_style->Overlay()) {
    return true;
  }
  if (old_style->ListStylePosition() != new_style->ListStylePosition()) {
    return true;
  }
  return false;
}

ComputedStyle::Difference ComputedStyle::ComputeDifference(
    const ComputedStyle* old_style,
    const ComputedStyle* new_style) {
  if (old_style == new_style) {
    return Difference::kEqual;
  }
  if (!old_style || !new_style) {
    return Difference::kInherited;
  }

  // For inline elements, the new computed first line style will be |new_style|
  // inheriting from the parent's first line style. If |new_style| is different
  // from |old_style|'s cached inherited first line style, the new computed
  // first line style may be different from the old even if |new_style| and
  // |old_style| equal. Especially if the difference is on inherited properties,
  // we need to propagate the difference to descendants.
  // See external/wpt/css/css-pseudo/first-line-change-inline-color*.html.
  auto inherited_first_line_style_diff = Difference::kEqual;
  if (const ComputedStyle* cached_inherited_first_line_style =
          old_style->GetCachedPseudoElementStyle(kPseudoIdFirstLineInherited)) {
    DCHECK(
        !new_style->GetCachedPseudoElementStyle(kPseudoIdFirstLineInherited));
    inherited_first_line_style_diff =
        ComputeDifferenceIgnoringInheritedFirstLineStyle(
            *cached_inherited_first_line_style, *new_style);
  }
  return std::max(
      inherited_first_line_style_diff,
      ComputeDifferenceIgnoringInheritedFirstLineStyle(*old_style, *new_style));
}

ComputedStyle::Difference
ComputedStyle::ComputeDifferenceIgnoringInheritedFirstLineStyle(
    const ComputedStyle& old_style,
    const ComputedStyle& new_style) {
  DCHECK_NE(&old_style, &new_style);
  if (DiffAffectsScrollAnimations(old_style, new_style)) {
    return Difference::kDescendantAffecting;
  }
  if (old_style.Display() != new_style.Display() &&
      (old_style.BlockifiesChildren() != new_style.BlockifiesChildren() ||
       old_style.InlinifiesChildren() != new_style.InlinifiesChildren())) {
    return Difference::kDescendantAffecting;
  }
  if (old_style.ScrollMarkerGroupNone() != new_style.ScrollMarkerGroupNone()) {
    return Difference::kDescendantAffecting;
  }
  // TODO(crbug.com/1213888): Only recalc affected descendants.
  if (DiffAffectsContainerQueries(old_style, new_style)) {
    return Difference::kDescendantAffecting;
  }
  if (!old_style.NonIndependentInheritedEqual(new_style)) {
    return Difference::kInherited;
  }
  if (old_style.JustifyItems() != new_style.JustifyItems()) {
    return Difference::kInherited;
  }
  if (old_style.AppliedTextDecorations() !=
      new_style.AppliedTextDecorations()) {
    return Difference::kInherited;
  }
  bool non_inherited_equal = old_style.NonInheritedEqual(new_style);
  if (!non_inherited_equal && old_style.ChildHasExplicitInheritance()) {
    return Difference::kInherited;
  }
  bool variables_independent =
      !old_style.HasVariableReference() && !old_style.HasVariableDeclaration();
  bool inherited_variables_equal = old_style.InheritedVariablesEqual(new_style);
  if (!inherited_variables_equal && !variables_independent) {
    return Difference::kInherited;
  }
  if (!old_style.IndependentInheritedEqual(new_style) ||
      !inherited_variables_equal) {
    return Difference::kIndependentInherited;
  }
  if (non_inherited_equal) {
    DCHECK(old_style == new_style);
    if (PseudoElementStylesEqual(old_style, new_style)) {
      return Difference::kEqual;
    }
    return Difference::kPseudoElementStyle;
  }
  if (old_style.OverscrollArea() != new_style.OverscrollArea()) {
    // TODO(crbug.com/447642032): Should we return kDescendantAffecting since
    // descendants may move into or out of a newly declared or no longer
    // declared overscroll area?
    return Difference::kPseudoElementStyle;
  }

  if (new_style.HasAnyPseudoElementStyles() ||
      old_style.HasAnyPseudoElementStyles()) {
    return Difference::kPseudoElementStyle;
  }
  if (old_style.Display() != new_style.Display() &&
      (new_style.IsDisplayListItem() || old_style.IsDisplayListItem())) {
    return Difference::kPseudoElementStyle;
  }
  return Difference::kNonInherited;
}

StyleSelfAlignmentData ResolvedSelfAlignment(
    const StyleSelfAlignmentData& value,
    const StyleSelfAlignmentData& normal_value_behavior,
    bool has_anchor_center_offset) {
  if (value.GetPosition() == ItemPosition::kLegacy ||
      value.GetPosition() == ItemPosition::kNormal ||
      value.GetPosition() == ItemPosition::kAuto) {
    return normal_value_behavior;
  }
  if (!has_anchor_center_offset &&
      value.GetPosition() == ItemPosition::kAnchorCenter) {
    return {ItemPosition::kCenter, value.Overflow(), value.PositionType()};
  }
  return value;
}

StyleSelfAlignmentData ComputedStyle::ResolvedAlignSelf(
    const StyleSelfAlignmentData& normal_value_behavior,
    const ComputedStyle* parent_style) const {
  // We will return the behaviour of 'normal' value if needed, which is specific
  // of each layout model.
  if (!parent_style || AlignSelf().GetPosition() != ItemPosition::kAuto) {
    return ResolvedSelfAlignment(AlignSelf(), normal_value_behavior,
                                 AnchorCenterOffset().has_value());
  }

  // The 'auto' keyword computes to the parent's align-items computed value.
  return ResolvedSelfAlignment(parent_style->AlignItems(),
                               normal_value_behavior,
                               AnchorCenterOffset().has_value());
}

StyleSelfAlignmentData ComputedStyle::ResolvedJustifySelf(
    const StyleSelfAlignmentData& normal_value_behavior,
    const ComputedStyle* parent_style) const {
  // We will return the behaviour of 'normal' value if needed, which is specific
  // of each layout model.
  if (!parent_style || JustifySelf().GetPosition() != ItemPosition::kAuto) {
    return ResolvedSelfAlignment(JustifySelf(), normal_value_behavior,
                                 AnchorCenterOffset().has_value());
  }

  // The auto keyword computes to the parent's justify-items computed value.
  return ResolvedSelfAlignment(parent_style->JustifyItems(),
                               normal_value_behavior,
                               AnchorCenterOffset().has_value());
}

bool ComputedStyle::operator==(const ComputedStyle& o) const {
  return InheritedEqual(o) && NonInheritedEqual(o) &&
         InheritedVariablesEqual(o);
}

bool ComputedStyle::HighlightPseudoElementStylesDependOnRelativeUnits() const {
  const StyleHighlightData& highlight_data = HighlightData();
  if (highlight_data.Selection() &&
      highlight_data.Selection()->HasAnyRelativeUnits()) {
    return true;
  }
  if (highlight_data.TargetText() &&
      highlight_data.TargetText()->HasAnyRelativeUnits()) {
    return true;
  }
  if (highlight_data.SpellingError() &&
      highlight_data.SpellingError()->HasAnyRelativeUnits()) {
    return true;
  }
  if (highlight_data.GrammarError() &&
      highlight_data.GrammarError()->HasAnyRelativeUnits()) {
    return true;
  }
  const CustomHighlightsStyleMap& custom_highlights =
      highlight_data.CustomHighlights();
  for (const auto& custom_highlight : custom_highlights) {
    if (custom_highlight.value->HasAnyRelativeUnits()) {
      return true;
    }
  }

  return false;
}

bool ComputedStyle::HighlightPseudoElementStylesDependOnContainerUnits() const {
  const StyleHighlightData& highlight_data = HighlightData();
  if (highlight_data.Selection() &&
      highlight_data.Selection()->HasContainerRelativeValue()) {
    return true;
  }
  if (highlight_data.TargetText() &&
      highlight_data.TargetText()->HasContainerRelativeValue()) {
    return true;
  }
  if (highlight_data.SpellingError() &&
      highlight_data.SpellingError()->HasContainerRelativeValue()) {
    return true;
  }
  if (highlight_data.GrammarError() &&
      highlight_data.GrammarError()->HasContainerRelativeValue()) {
    return true;
  }
  const CustomHighlightsStyleMap& custom_highlights =
      highlight_data.CustomHighlights();
  for (const auto& custom_highlight : custom_highlights) {
    if (custom_highlight.value->HasContainerRelativeValue()) {
      return true;
    }
  }

  return false;
}

bool ComputedStyle::HighlightPseudoElementStylesDependOnViewportUnits() const {
  const StyleHighlightData& highlight_data = HighlightData();
  if (highlight_data.Selection() &&
      highlight_data.Selection()->HasViewportUnits()) {
    return true;
  }
  if (highlight_data.TargetText() &&
      highlight_data.TargetText()->HasViewportUnits()) {
    return true;
  }
  if (highlight_data.SpellingError() &&
      highlight_data.SpellingError()->HasViewportUnits()) {
    return true;
  }
  if (highlight_data.GrammarError() &&
      highlight_data.GrammarError()->HasViewportUnits()) {
    return true;
  }
  const CustomHighlightsStyleMap& custom_highlights =
      highlight_data.CustomHighlights();
  for (const auto& custom_highlight : custom_highlights) {
    if (custom_highlight.value->HasViewportUnits()) {
      return true;
    }
  }

  return false;
}

bool ComputedStyle::HighlightPseudoElementStylesHaveVariableReferences() const {
  const StyleHighlightData& highlight_data = HighlightData();
  if (highlight_data.Selection() &&
      highlight_data.Selection()->HasVariableReference()) {
    return true;
  }
  if (highlight_data.TargetText() &&
      highlight_data.TargetText()->HasVariableReference()) {
    return true;
  }
  if (highlight_data.SpellingError() &&
      highlight_data.SpellingError()->HasVariableReference()) {
    return true;
  }
  if (highlight_data.GrammarError() &&
      highlight_data.GrammarError()->HasVariableReference()) {
    return true;
  }
  const CustomHighlightsStyleMap& custom_highlights =
      highlight_data.CustomHighlights();
  for (const auto& custom_highlight : custom_highlights) {
    if (custom_highlight.value->HasVariableReference()) {
      return true;
    }
  }

  return false;
}

const ComputedStyle* ComputedStyle::GetCachedPseudoElementStyle(
    PseudoId pseudo_id,
    const AtomicString& pseudo_argument) const {
  if (!HasCachedPseudoElementStyles()) {
    return nullptr;
  }

  auto result = GetPseudoElementStyleCache()->find(
      PseudoElementStyleCacheKey{pseudo_id, pseudo_argument});
  if (result == GetPseudoElementStyleCache()->end()) {
    return nullptr;
  } else {
    return result->value.Get();
  }
}

const ComputedStyle* ComputedStyle::AddCachedPseudoElementStyle(
    const ComputedStyle* pseudo,
    PseudoId pseudo_id,
    const AtomicString& pseudo_argument) const {
  DCHECK(pseudo);

  // Confirm that the styles being cached are for the PseudoId that
  // the caller intended (and presumably had checked was not present).
  DCHECK_EQ(static_cast<unsigned>(pseudo->StyleType()),
            static_cast<unsigned>(pseudo_id));

  const ComputedStyle* result = pseudo;

  auto add_result = EnsurePseudoElementStyleCache().insert(
      PseudoElementStyleCacheKey{pseudo_id, pseudo_argument},
      std::move(pseudo));

  // The pseudo style cache assumes that only one entry will be added for any
  // any given (PseudoId,argument). Adding more than one entry is a bug, even
  // if the styles being cached are equal.
  DCHECK(add_result.is_new_entry);

  return result;
}

const ComputedStyle* ComputedStyle::ReplaceCachedPseudoElementStyle(
    const ComputedStyle* pseudo_style,
    PseudoId pseudo_id,
    const AtomicString& pseudo_argument) const {
  DCHECK(pseudo_style->StyleType() != kPseudoIdNone &&
         pseudo_style->StyleType() != kPseudoIdFirstLineInherited);
  if (HasCachedPseudoElementStyles()) {
    auto slot = GetPseudoElementStyleCache()->find(
        PseudoElementStyleCacheKey{pseudo_id, pseudo_argument});
    if (slot != GetPseudoElementStyleCache()->end()) {
      Member<const ComputedStyle>& cached_style = slot->value;
      SECURITY_CHECK(cached_style->IsEnsuredInDisplayNone());
      cached_style = pseudo_style;
      return pseudo_style;
    }
  }
  return AddCachedPseudoElementStyle(pseudo_style, pseudo_id, pseudo_argument);
}

void ComputedStyle::ClearCachedPseudoElementStyles() const {
  if (cached_data_ && cached_data_->pseudo_element_styles_) {
    cached_data_->pseudo_element_styles_->clear();
  }
}

const ComputedStyle* ComputedStyle::GetBaseComputedStyle() const {
  if (StyleBaseData* base_data = BaseData()) {
    return base_data->GetBaseComputedStyle();
  }
  return nullptr;
}

const CSSBitset* ComputedStyle::GetBaseImportantSet() const {
  if (StyleBaseData* base_data = BaseData()) {
    return base_data->GetBaseImportantSet();
  }
  return nullptr;
}

bool ComputedStyle::InheritedEqual(const ComputedStyle& other) const {
  return IndependentInheritedEqual(other) &&
         NonIndependentInheritedEqual(other);
}

bool ComputedStyle::IndependentInheritedEqual(
    const ComputedStyle& other) const {
  return ComputedStyleBase::IndependentInheritedEqual(other);
}

bool ComputedStyle::NonIndependentInheritedEqual(
    const ComputedStyle& other) const {
  return ComputedStyleBase::NonIndependentInheritedEqual(other);
}

bool ComputedStyle::NonInheritedEqual(const ComputedStyle& other) const {
  // compare everything except the pseudoStyle pointer
  return ComputedStyleBase::NonInheritedEqual(other);
}

bool ComputedStyle::InheritedEqualIncludingInheritedVariables(
    const ComputedStyle& other) const {
  // We use a by-value check that is a bit more expensive than
  // pointer comparison, but yields many more MPC hits,
  // so it generally makes up for it.
  return ComputedStyleBase::InheritedEqualIncludingInheritedVariables(other);
}

StyleDifference ComputedStyle::VisualInvalidationDiff(
    const Document& document,
    const ComputedStyle& other) const {
  StyleDifference diff;
  uint64_t field_diff = FieldInvalidationDiff(*this, other);

  if (DiffNeedsReshape(other, field_diff)) {
    diff.SetNeedsReshape();
    diff.SetNeedsFullLayout();
    diff.SetNeedsNormalPaintInvalidation();
  }

  if (IsStackingContextWithoutContainment() !=
      other.IsStackingContextWithoutContainment()) {
    diff.SetNeedsFullLayout();
    diff.SetNeedsNormalPaintInvalidation();
    diff.SetZIndexChanged();
  }

  if ((!diff.NeedsFullLayout() || !diff.NeedsNormalPaintInvalidation()) &&
      DiffNeedsFullLayoutAndPaintInvalidation(other, field_diff)) {
    diff.SetNeedsFullLayout();
    diff.SetNeedsNormalPaintInvalidation();
  }

  if (!diff.NeedsFullLayout() &&
      DiffNeedsFullLayout(document, other, field_diff)) {
    diff.SetNeedsFullLayout();
  }

  if (!diff.NeedsLayout()) {
    if ((field_diff & kOutOfFlow) && HasOutOfFlowPosition()) {
      diff.SetNeedsPositionedMovementLayout();
    } else if ((field_diff & kInset) && HasInFlowPosition()) {
      diff.SetNeedsPositionedMovementLayout();
    }
  }

  if (!diff.NeedsNormalPaintInvalidation() &&
      DiffNeedsNormalPaintInvalidation(document, other, field_diff)) {
    diff.SetNeedsNormalPaintInvalidation();
  }

  if (DiffNeedsRecomputeVisualOverflow(other, field_diff)) {
    diff.SetNeedsRecomputeVisualOverflow();
  }

  if (DiffCompositingReasonsChanged(other, field_diff)) {
    diff.SetCompositingReasonsChanged();
  }

  if (field_diff & kBackgroundColor) {
    // If the background color change is not due to a composited animation,
    // then paint invalidation is required; but we can defer the decision until
    // we know whether the color change will be rendered by the compositor.
    diff.SetBackgroundColorChanged();
  }
  if (field_diff & kBlendMode) {
    diff.SetBlendModeChanged();
  }
  if (field_diff & kBorderRadius) {
    diff.SetBorderRadiusChanged();
  }
  if (field_diff & kBorderShape) {
    diff.SetBorderShapeChanged();
  }
  if (field_diff & kClip) {
    bool has_clip = HasOutOfFlowPosition() && !HasAutoClip();
    bool other_has_clip = other.HasOutOfFlowPosition() && !other.HasAutoClip();
    if (has_clip != other_has_clip || (has_clip && Clip() != other.Clip())) {
      diff.SetCSSClipChanged();
    }
  }
  if (field_diff & kClipPath) {
    diff.SetClipPathChanged();
  }
  if (field_diff & kColor) {
    diff.SetTextDecorationOrColorChanged();
  }
  if (field_diff & kFilterData) {
    diff.SetFilterChanged();
  }
  if (field_diff & kHasTransform) {
    if (HasTransform() != other.HasTransform()) {
      diff.SetOtherTransformPropertyChanged();
    }
  }
  if (field_diff & kMask) {
    diff.SetMaskChanged();
  }
  if (field_diff & kOpacity) {
    diff.SetOpacityChanged();
  }
  if (field_diff & kScrollbarColor) {
    if (UsedScrollbarColor() != other.UsedScrollbarColor()) {
      diff.SetNeedsNormalPaintInvalidation();
    }
  }
  if (field_diff & kScrollbarStyle) {
    if (HasPseudoElementStyle(kPseudoIdScrollbar) !=
            other.HasPseudoElementStyle(kPseudoIdScrollbar) ||
        UsesStandardScrollbarStyle() != other.UsesStandardScrollbarStyle()) {
      diff.SetNeedsFullLayout();
      diff.SetNeedsNormalPaintInvalidation();
    }
  }
  if (field_diff & kTextDecoration) {
    diff.SetTextDecorationOrColorChanged();
  }
  if (field_diff & kTransformData) {
    diff.SetTransformDataChanged();
  }
  if (field_diff & kTransformOther) {
    diff.SetOtherTransformPropertyChanged();
  }
  if (field_diff & kTransformProperty) {
    diff.SetTransformPropertyChanged();
  }
  if (field_diff & kVisibility) {
    if ((Visibility() == EVisibility::kCollapse) !=
        (other.Visibility() == EVisibility::kCollapse)) {
      diff.SetNeedsFullLayout();
    }
  }
  if (field_diff & kZIndex) {
    diff.SetZIndexChanged();
  }

  // If the (current)color changes and a filter or backdrop-filter uses it, the
  // filter or backdrop-filter needs to be updated. This reads
  // `diff.TextDecorationOrColorChanged()` and so needs to be after the setters,
  // above.
  if (diff.TextDecorationOrColorChanged()) {
    if (HasFilter() && Filter().UsesCurrentColor()) {
      diff.SetFilterChanged();
    }
    if (HasBackdropFilter() && BackdropFilter().UsesCurrentColor()) {
      // This could be optimized with a targeted backdrop-filter-changed
      // invalidation.
      diff.SetCompositingReasonsChanged();
    }
  }

  // The following condition needs to be at last, because it may depend on
  // conditions in diff computed above.
  if ((field_diff & kScrollAnchor) || diff.TransformChanged()) {
    diff.SetScrollAnchorDisablingPropertyChanged();
  }

  // Cursors are not checked, since they will be set appropriately in response
  // to mouse events, so they don't need to cause any paint invalidation or
  // layout.

  // Animations don't need to be checked either. We always set the new style on
  // the layoutObject, so we will get a chance to fire off the resulting
  // transition properly.

  return diff;
}

bool ComputedStyle::DiffNeedsReshape(const ComputedStyle& other,
                                     uint64_t field_diff) const {
  if (field_diff & kReshape) {
    return true;
  }

  if (field_diff & kBorderWidth) {
    if (Display() == EDisplay::kInline && HasBorder() != other.HasBorder()) {
      return true;
    }
  }

  return false;
}

bool ComputedStyle::DiffNeedsFullLayoutAndPaintInvalidation(
    const ComputedStyle& other,
    uint64_t field_diff) const {
  if (IsDisplayTableType(Display())) {
    // In the collapsing border model, 'hidden' suppresses other borders, while
    // 'none' does not, so these style differences can be width differences.
    if ((BorderCollapse() == EBorderCollapse::kCollapse) &&
        ((BorderTopStyle() == EBorderStyle::kHidden &&
          other.BorderTopStyle() == EBorderStyle::kNone) ||
         (BorderTopStyle() == EBorderStyle::kNone &&
          other.BorderTopStyle() == EBorderStyle::kHidden) ||
         (BorderBottomStyle() == EBorderStyle::kHidden &&
          other.BorderBottomStyle() == EBorderStyle::kNone) ||
         (BorderBottomStyle() == EBorderStyle::kNone &&
          other.BorderBottomStyle() == EBorderStyle::kHidden) ||
         (BorderLeftStyle() == EBorderStyle::kHidden &&
          other.BorderLeftStyle() == EBorderStyle::kNone) ||
         (BorderLeftStyle() == EBorderStyle::kNone &&
          other.BorderLeftStyle() == EBorderStyle::kHidden) ||
         (BorderRightStyle() == EBorderStyle::kHidden &&
          other.BorderRightStyle() == EBorderStyle::kNone) ||
         (BorderRightStyle() == EBorderStyle::kNone &&
          other.BorderRightStyle() == EBorderStyle::kHidden))) {
      return true;
    }
  }

  // Movement of non-static-positioned object is special cased in
  // ComputedStyle::VisualInvalidationDiff().

  return false;
}

bool ComputedStyle::DiffNeedsFullLayout(const Document& document,
                                        const ComputedStyle& other,
                                        uint64_t field_diff) const {
  if (field_diff & kLayout) {
    return true;
  }

  if (field_diff & kBorderWidth) {
    if (BorderTopWidth() != other.BorderTopWidth() ||
        BorderRightWidth() != other.BorderRightWidth() ||
        BorderBottomWidth() != other.BorderBottomWidth() ||
        BorderLeftWidth() != other.BorderLeftWidth()) {
      return true;
    }
  }

  if ((field_diff & kMargin) && !HasOutOfFlowPosition()) {
    return true;
  }

  if (field_diff & kStroke) {
    if (HasStroke() != other.HasStroke()) {
      return true;
    }
    if (static_cast<bool>(StrokeDashArray()) !=
        static_cast<bool>(other.StrokeDashArray())) {
      return true;
    }
  }

  if (IsDisplayLayoutCustomBox() &&
      DiffNeedsFullLayoutForLayoutCustom(document, other)) {
    return true;
  }

  if (DisplayLayoutCustomParentName() &&
      DiffNeedsFullLayoutForLayoutCustomChild(document, other)) {
    return true;
  }

  if (field_diff & kGapDecorations) {
    bool column_rule_style_changed_from_none =
        ColumnRuleStyle() ==
            ComputedStyleInitialValues::InitialColumnRuleStyle() &&
        other.ColumnRuleStyle() !=
            ComputedStyleInitialValues::InitialColumnRuleStyle();
    bool row_rule_style_changed_from_none =
        RowRuleStyle() == ComputedStyleInitialValues::InitialRowRuleStyle() &&
        other.RowRuleStyle() !=
            ComputedStyleInitialValues::InitialRowRuleStyle();
    if (column_rule_style_changed_from_none ||
        row_rule_style_changed_from_none) {
      return true;
    }
  }

  if (DiffNeedsFullLayoutForAnimationTriggers(*this, other)) {
    return true;
  }

  return false;
}

bool ComputedStyle::DiffNeedsFullLayoutForLayoutCustom(
    const Document& document,
    const ComputedStyle& other) const {
  DCHECK(IsDisplayLayoutCustomBox());

  LayoutWorklet* worklet = LayoutWorklet::From(*document.domWindow());
  const AtomicString& name = DisplayLayoutCustomName();

  if (!worklet->GetDocumentDefinitionMap()->Contains(name)) {
    return false;
  }

  const DocumentLayoutDefinition* definition =
      worklet->GetDocumentDefinitionMap()->at(name);
  if (definition == kInvalidDocumentLayoutDefinition) {
    return false;
  }

  if (!PropertiesEqual(definition->NativeInvalidationProperties(), other)) {
    return true;
  }

  if (!CustomPropertiesEqual(definition->CustomInvalidationProperties(),
                             other)) {
    return true;
  }

  return false;
}

bool ComputedStyle::DiffNeedsFullLayoutForLayoutCustomChild(
    const Document& document,
    const ComputedStyle& other) const {
  LayoutWorklet* worklet = LayoutWorklet::From(*document.domWindow());
  const AtomicString& name = DisplayLayoutCustomParentName();

  if (!worklet->GetDocumentDefinitionMap()->Contains(name)) {
    return false;
  }

  const DocumentLayoutDefinition* definition =
      worklet->GetDocumentDefinitionMap()->at(name);
  if (definition == kInvalidDocumentLayoutDefinition) {
    return false;
  }

  if (!PropertiesEqual(definition->ChildNativeInvalidationProperties(),
                       other)) {
    return true;
  }

  if (!CustomPropertiesEqual(definition->ChildCustomInvalidationProperties(),
                             other)) {
    return true;
  }

  return false;
}

bool ComputedStyle::DiffNeedsNormalPaintInvalidation(
    const Document& document,
    const ComputedStyle& other,
    uint64_t field_diff) const {
  if (field_diff & kPaint) {
    return true;
  }

  if ((field_diff & kAccentColor) &&
      AccentColorResolved() != other.AccentColorResolved()) {
    return true;
  }

  if ((field_diff & kOutline) && !OutlineVisuallyEqual(other)) {
    return true;
  }

  if ((field_diff & kBackground) &&
      !BackgroundInternal().VisuallyEqual(other.BackgroundInternal())) {
    return true;
  }

  if (field_diff & kCurrentcolor) {
    // If a property has a value that contains a <color> that depends on
    // 'currentcolor', for example:
    //
    //   background-image: linear-gradient(currentColor, #fff)
    //   background-color: color-mix(in srgb, currentcolor ...)
    //
    // If the (current)color has changed, we need to recompute it even though
    // the old and new property values are identical.
    //
    // NOTE: This is also handled to some degree by
    // LayoutObject::AdjustStyleDifference. We should probably re-distribute
    // the responsibilities between these two locations.
    if ((GetCurrentColor() != other.GetCurrentColor() ||
         GetInternalVisitedCurrentColor() !=
             other.GetInternalVisitedCurrentColor()) &&
        HasPropertyDependingOnCurrentColor()) {
      return true;
    }
  }

  if ((field_diff & kBorderVisual) && !BorderVisuallyEqual(other)) {
    return true;
  }

  if ((field_diff & kBorderOutlineVisitedColor) &&
      BorderOutlineVisitedColorChanged(other)) {
    return true;
  }

  if (PaintImagesInternal()) {
    for (const auto& image : PaintImagesInternal()->Images()) {
      DCHECK(image);
      if (DiffNeedsPaintInvalidationForPaintImage(*image, other, document)) {
        return true;
      }
    }
  }

  return false;
}

bool ComputedStyle::DiffNeedsPaintInvalidationForPaintImage(
    const StyleImage& image,
    const ComputedStyle& other,
    const Document& document) const {
  // https://crbug.com/835589: early exit when paint target is associated with
  // a link.
  if (InsideLink() != EInsideLink::kNotInsideLink) {
    return false;
  }

  CSSPaintValue* value = To<CSSPaintValue>(image.CssValue());

  // NOTE: If the invalidation properties vectors are null, we are invalid as
  // we haven't yet been painted (and can't provide the invalidation
  // properties yet).
  if (!value->NativeInvalidationProperties(document) ||
      !value->CustomInvalidationProperties(document)) {
    return true;
  }

  if (!PropertiesEqual(*value->NativeInvalidationProperties(document), other)) {
    return true;
  }

  if (!CustomPropertiesEqual(*value->CustomInvalidationProperties(document),
                             other)) {
    return true;
  }

  return false;
}

bool ComputedStyle::PropertiesEqual(const Vector<CSSPropertyID>& properties,
                                    const ComputedStyle& other) const {
  for (CSSPropertyID property_id : properties) {
    // TODO(ikilpatrick): remove IsInterpolableProperty check once
    // CSSPropertyEquality::PropertiesEqual correctly handles all properties.
    const CSSProperty& property = CSSProperty::Get(property_id);
    if (!property.IsInterpolable() ||
        !CSSPropertyEquality::PropertiesEqual(PropertyHandle(property), *this,
                                              other)) {
      return false;
    }
  }

  return true;
}

bool ComputedStyle::CustomPropertiesEqual(
    const Vector<AtomicString>& properties,
    const ComputedStyle& other) const {
  // Short-circuit if neither of the styles have custom properties.
  if (!HasVariables() && !other.HasVariables()) {
    return true;
  }

  for (const AtomicString& property_name : properties) {
    if (!base::ValuesEquivalent(GetVariableData(property_name),
                                other.GetVariableData(property_name))) {
      return false;
    }
    if (!base::ValuesEquivalent(GetVariableValue(property_name),
                                other.GetVariableValue(property_name))) {
      return false;
    }
  }

  return true;
}

bool ComputedStyle::PotentialCompositingReasonsFor3DTransformChanged(
    const ComputedStyle& other) const {
  // Compositing reasons for 3D transforms depend on the LayoutObject type (see:
  // |LayoutObject::HasTransformRelatedProperty|)) This will return true for
  // some LayoutObjects that end up not supporting transforms.
  return CompositingReasonFinder::PotentialCompositingReasonsFor3DTransform(
             *this) !=
         CompositingReasonFinder::PotentialCompositingReasonsFor3DTransform(
             other);
}

bool ComputedStyle::DiffNeedsRecomputeVisualOverflow(
    const ComputedStyle& other,
    uint64_t field_diff) const {
  if (field_diff & kVisualOverflow) {
    return true;
  }

  if ((field_diff & kBorderImage) && !BorderVisualOverflowEqual(other)) {
    return true;
  }

  if ((field_diff & kOutline) && !OutlineVisuallyEqual(other)) {
    return true;
  }

  if ((field_diff & kTextDecoration) &&
      TextDecorationVisualOverflowChanged(other)) {
    return true;
  }

  return false;
}

bool ComputedStyle::DiffCompositingReasonsChanged(const ComputedStyle& other,
                                                  uint64_t field_diff) const {
  if (field_diff & kCompositing) {
    return true;
  }

  if (UsedTransformStyle3D() != other.UsedTransformStyle3D()) {
    return true;
  }

  if (ContainsPaint() != other.ContainsPaint()) {
    return true;
  }

  if (IsOverflowVisibleAlongBothAxes() !=
      other.IsOverflowVisibleAlongBothAxes()) {
    return true;
  }

  if (PotentialCompositingReasonsFor3DTransformChanged(other)) {
    return true;
  }

  return false;
}

bool ComputedStyle::HasCSSPaintImagesUsingCustomProperty(
    const AtomicString& custom_property_name,
    const Document& document) const {
  if (PaintImagesInternal()) {
    for (const auto& image : PaintImagesInternal()->Images()) {
      DCHECK(image);
      // IsPaintImage is true for CSS Paint images only, please refer to the
      // constructor of StyleGeneratedImage.
      if (image->IsPaintImage()) {
        return To<StyleGeneratedImage>(image.Get())
            ->IsUsingCustomProperty(custom_property_name, document);
      }
    }
  }
  return false;
}

static bool HasPropertyThatCreatesStackingContext(
    const Vector<CSSPropertyID>& properties) {
  for (CSSPropertyID property : properties) {
    switch (ResolveCSSPropertyID(property)) {
      case CSSPropertyID::kOpacity:
      case CSSPropertyID::kTransform:
      case CSSPropertyID::kTransformStyle:
      case CSSPropertyID::kPerspective:
      case CSSPropertyID::kTranslate:
      case CSSPropertyID::kRotate:
      case CSSPropertyID::kScale:
      case CSSPropertyID::kOffsetPath:
      case CSSPropertyID::kOffsetPosition:
      case CSSPropertyID::kMask:  // Matches longhand.
      case CSSPropertyID::kMaskImage:
      case CSSPropertyID::kWebkitMaskBoxImage:  // Matches longhand
      case CSSPropertyID::kWebkitMaskBoxImageSource:
      case CSSPropertyID::kClipPath:
      case CSSPropertyID::kWebkitBoxReflect:
      case CSSPropertyID::kFilter:
      case CSSPropertyID::kBackdropFilter:
      case CSSPropertyID::kZIndex:
      case CSSPropertyID::kPosition:
      case CSSPropertyID::kMixBlendMode:
      case CSSPropertyID::kIsolation:
      case CSSPropertyID::kContain:
      case CSSPropertyID::kViewTransitionName:
        return true;
      default:
        break;
    }
  }
  return false;
}

static bool IsWillChangeTransformHintProperty(CSSPropertyID property) {
  switch (ResolveCSSPropertyID(property)) {
    case CSSPropertyID::kTransform:
    case CSSPropertyID::kPerspective:
    case CSSPropertyID::kTransformStyle:
      return true;
    default:
      break;
  }
  return false;
}

static bool IsWillChangeHintForAnyTransformProperty(CSSPropertyID property) {
  switch (ResolveCSSPropertyID(property)) {
    case CSSPropertyID::kTransform:
    case CSSPropertyID::kPerspective:
    case CSSPropertyID::kTranslate:
    case CSSPropertyID::kScale:
    case CSSPropertyID::kRotate:
    case CSSPropertyID::kOffsetPath:
    case CSSPropertyID::kOffsetPosition:
    case CSSPropertyID::kTransformStyle:
      return true;
    default:
      break;
  }
  return false;
}

static bool IsWillChangeCompositingHintProperty(CSSPropertyID property) {
  if (IsWillChangeHintForAnyTransformProperty(property)) {
    return true;
  }
  switch (ResolveCSSPropertyID(property)) {
    case CSSPropertyID::kOpacity:
    case CSSPropertyID::kFilter:
    case CSSPropertyID::kBackdropFilter:
    case CSSPropertyID::kTop:
    case CSSPropertyID::kLeft:
    case CSSPropertyID::kBottom:
    case CSSPropertyID::kRight:
      return true;
    default:
      break;
  }
  return false;
}

bool ComputedStyle::HasWillChangeCompositingHint() const {
  return std::ranges::any_of(WillChangeProperties(),
                             IsWillChangeCompositingHintProperty);
}

bool ComputedStyle::HasWillChangeTransformHint() const {
  return std::ranges::any_of(WillChangeProperties(),
                             IsWillChangeTransformHintProperty);
}

bool ComputedStyle::HasWillChangeHintForAnyTransformProperty() const {
  return std::ranges::any_of(WillChangeProperties(),
                             IsWillChangeHintForAnyTransformProperty);
}

bool ComputedStyle::RequireTransformOrigin(
    ApplyTransformOrigin apply_origin,
    ApplyMotionPath apply_motion_path) const {
  // transform-origin brackets the transform with translate operations.
  // Optimize for the case where the only transform is a translation, since the
  // transform-origin is irrelevant in that case.
  if (apply_origin != kIncludeTransformOrigin) {
    return false;
  }

  if (apply_motion_path == kIncludeMotionPath) {
    return true;
  }

  for (const auto& operation : Transform().Operations()) {
    TransformOperation::OperationType type = operation->GetType();
    if (type != TransformOperation::kTranslateX &&
        type != TransformOperation::kTranslateY &&
        type != TransformOperation::kTranslate &&
        type != TransformOperation::kTranslateZ &&
        type != TransformOperation::kTranslate3D) {
      return true;
    }
  }

  return Scale() || Rotate();
}

InterpolationQuality ComputedStyle::GetInterpolationQuality() const {
  if (ImageRendering() == EImageRendering::kPixelated) {
    return kInterpolationNone;
  }

  if (ImageRendering() == EImageRendering::kWebkitOptimizeContrast) {
    return kInterpolationLow;
  }

  return GetDefaultInterpolationQuality();
}

void ComputedStyle::LoadDeferredImages(Document& document) const {
  if (HasBackgroundImage()) {
    for (const FillLayer* background_layer = &BackgroundLayers();
         background_layer; background_layer = background_layer->Next()) {
      if (StyleImage* image = background_layer->GetImage()) {
        if (image->IsImageResource() && image->IsLazyloadPossiblyDeferred()) {
          To<StyleFetchedImage>(image)->LoadDeferredImage(document);
        }
      }
    }
  }
}

ETransformBox ComputedStyle::UsedTransformBox(
    TransformBoxContext box_context) const {
  ETransformBox transform_box = TransformBox();
  if (box_context == TransformBoxContext::kSvg) {
    // For SVG elements without associated CSS layout box, the used value for
    // content-box is fill-box and for border-box is stroke-box.
    switch (transform_box) {
      case ETransformBox::kContentBox:
        transform_box = ETransformBox::kFillBox;
        break;
      case ETransformBox::kBorderBox:
        transform_box = ETransformBox::kStrokeBox;
        break;
      case ETransformBox::kFillBox:
      case ETransformBox::kStrokeBox:
      case ETransformBox::kViewBox:
        break;
    }
    // If transform-box is stroke-box and the element has "vector-effect:
    // non-scaling-stroke", then the used transform-box is fill-box.
    if (transform_box == ETransformBox::kStrokeBox &&
        VectorEffect() == EVectorEffect::kNonScalingStroke) {
      transform_box = ETransformBox::kFillBox;
    }
  } else {
    // For elements with associated CSS layout box, the used value for fill-box
    // is content-box and for stroke-box and view-box is border-box.
    switch (transform_box) {
      case ETransformBox::kContentBox:
      case ETransformBox::kBorderBox:
        break;
      case ETransformBox::kFillBox:
        transform_box = ETransformBox::kContentBox;
        break;
      case ETransformBox::kStrokeBox:
      case ETransformBox::kViewBox:
        transform_box = ETransformBox::kBorderBox;
        break;
    }
  }
  return transform_box;
}

void ComputedStyle::ApplyTransform(
    gfx::Transform& result,
    const LayoutBox* box,
    const PhysicalRect& reference_box,
    ApplyTransformOperations apply_operations,
    ApplyTransformOrigin apply_origin,
    ApplyMotionPath apply_motion_path,
    ApplyIndependentTransformProperties apply_independent_transform_properties)
    const {
  ApplyTransform(result, box, gfx::RectF(reference_box), apply_operations,
                 apply_origin, apply_motion_path,
                 apply_independent_transform_properties);
}

void ComputedStyle::ApplyTransform(
    gfx::Transform& result,
    const LayoutBox* box,
    const gfx::RectF& bounding_box,
    ApplyTransformOperations apply_operations,
    ApplyTransformOrigin apply_origin,
    ApplyMotionPath apply_motion_path,
    ApplyIndependentTransformProperties apply_independent_transform_properties)
    const {
  if (!HasOffset()) {
    apply_motion_path = kExcludeMotionPath;
  }
  bool apply_transform_origin =
      RequireTransformOrigin(apply_origin, apply_motion_path);

  float origin_x = 0;
  float origin_y = 0;
  float origin_z = 0;

  const gfx::SizeF& box_size = bounding_box.size();
  if (apply_transform_origin ||
      // We need to calculate originX and originY for applying motion path.
      apply_motion_path == kIncludeMotionPath) {
    origin_x = FloatValueForLength(GetTransformOrigin().X(), box_size.width()) +
               bounding_box.x();
    origin_y =
        FloatValueForLength(GetTransformOrigin().Y(), box_size.height()) +
        bounding_box.y();
    if (apply_transform_origin) {
      origin_z = GetTransformOrigin().Z();
      result.Translate3d(origin_x, origin_y, origin_z);
    }
  }

  if (apply_independent_transform_properties ==
      kIncludeIndependentTransformProperties) {
    if (Translate()) {
      Translate()->Apply(result, box_size);
    }

    if (Rotate()) {
      Rotate()->Apply(result, box_size);
    }

    if (Scale()) {
      Scale()->Apply(result, box_size);
    }
  }

  if (apply_motion_path == kIncludeMotionPath) {
    ApplyMotionPathTransform(origin_x, origin_y, box, bounding_box, result);
  }

  if (apply_operations == kIncludeTransformOperations) {
    for (const auto& operation : Transform().Operations()) {
      operation->Apply(result, box_size);
    }
  }

  if (apply_transform_origin) {
    result.Translate3d(-origin_x, -origin_y, -origin_z);
  }
}

namespace {

gfx::RectF GetReferenceBox(const LayoutBox* box, CoordBox coord_box) {
  if (box) {
    if (const LayoutBlock* containing_block = box->ContainingBlock()) {
      // In SVG contexts, all values behave as view-box.
      if (box->IsSVG()) {
        return gfx::RectF(SVGViewportResolver(*box).ResolveViewport());
      }
      // https://drafts.csswg.org/css-box-4/#typedef-coord-box
      switch (coord_box) {
        case CoordBox::kFillBox:
        case CoordBox::kContentBox:
          return gfx::RectF(containing_block->PhysicalContentBoxRect());
        case CoordBox::kPaddingBox:
          return gfx::RectF(containing_block->PhysicalPaddingBoxRect());
        case CoordBox::kViewBox:
        case CoordBox::kStrokeBox:
        case CoordBox::kBorderBox:
          return gfx::RectF(containing_block->PhysicalBorderBoxRect());
      }
    }
  }
  // As the motion path calculations can be called before all the layout
  // has been correctly calculated, we can end up here.
  return gfx::RectF();
}

gfx::PointF GetOffsetFromContainingBlock(const LayoutBox* box) {
  if (box) {
    if (const LayoutBlock* containing_block = box->ContainingBlock()) {
      gfx::PointF offset = box->LocalToAncestorPoint(
          gfx::PointF(), containing_block, kIgnoreTransforms);
      return offset;
    }
  }
  return {0, 0};
}

// https://drafts.fxtf.org/motion/#offset-position-property
gfx::PointF GetStartingPointOfThePath(
    const gfx::PointF& offset_from_reference_box,
    const LengthPoint& offset_position,
    const gfx::SizeF& reference_box_size) {
  if (offset_position.X().IsAuto()) {
    return offset_from_reference_box;
  }
  if (offset_position.X().IsNone()) {
    // Currently all the use cases will behave as "at center".
    return PointForLengthPoint(
        LengthPoint(Length::Percent(50), Length::Percent(50)),
        reference_box_size);
  }
  return PointForLengthPoint(offset_position, reference_box_size);
}

}  // namespace

PointAndTangent ComputedStyle::CalculatePointAndTangentOnBasicShape(
    const BasicShape& shape,
    const gfx::PointF& starting_point,
    const gfx::SizeF& reference_box_size) const {
  Path path;
  if (const auto* circle_or_ellipse =
          DynamicTo<BasicShapeWithCenterAndRadii>(shape);
      circle_or_ellipse && !circle_or_ellipse->HasExplicitCenter()) {
    // For all <basic-shape>s, if they accept an at <position> argument
    // but that argument is omitted, and the element defines
    // an offset starting position via offset-position,
    // it uses the specified offset starting position for that argument.
    path = circle_or_ellipse->GetPathFromCenter(
        starting_point, gfx::RectF(reference_box_size), /*path_scale=*/1.f);
  } else {
    path = shape.GetPath(gfx::RectF(reference_box_size), EffectiveZoom(),
                         /*path_scale=*/1.f);
  }
  float shape_length = path.length();
  float path_length = FloatValueForLength(OffsetDistance(), shape_length);
  // All the shapes are closed at this point.
  if (shape_length > 0) {
    path_length = fmod(path_length, shape_length);
    if (path_length < 0) {
      path_length += shape_length;
    }
  }
  return path.PointAndNormalAtLength(path_length);
}

PointAndTangent ComputedStyle::CalculatePointAndTangentOnRay(
    const StyleRay& ray,
    const LayoutBox* box,
    const gfx::PointF& starting_point,
    const gfx::SizeF& reference_box_size) const {
  float ray_length =
      ray.CalculateRayPathLength(starting_point, reference_box_size);
  if (ray.Contain() && box) {
    // The length of the offset path is reduced so that the element stays
    // within the containing block even at offset-distance: 100%.
    // Specifically, the path’s length is reduced by half the width
    // or half the height of the element’s border box,
    // whichever is larger, and floored at zero.
    const PhysicalRect border_box_rect = box->PhysicalBorderBoxRect();
    const float largest_side = std::max(border_box_rect.Width().ToFloat(),
                                        border_box_rect.Height().ToFloat());
    ray_length -= largest_side / 2;
    ray_length = std::max(ray_length, 0.f);
  }
  const float path_length = FloatValueForLength(OffsetDistance(), ray_length);
  return ray.PointAndNormalAtLength(starting_point, path_length);
}

PointAndTangent ComputedStyle::CalculatePointAndTangentOnPath(
    const Path& path) const {
  float zoom = EffectiveZoom();
  float path_length = path.length();
  float float_distance =
      FloatValueForLength(OffsetDistance(), path_length * zoom) / zoom;
  float computed_distance;
  if (path.IsClosed() && path_length > 0) {
    computed_distance = fmod(float_distance, path_length);
    if (computed_distance < 0) {
      computed_distance += path_length;
    }
  } else {
    computed_distance = ClampTo<float>(float_distance, 0, path_length);
  }
  PointAndTangent path_position =
      path.PointAndNormalAtLength(computed_distance);
  path_position.point.Scale(zoom, zoom);
  return path_position;
}

void ComputedStyle::ApplyMotionPathTransform(float origin_x,
                                             float origin_y,
                                             const LayoutBox* box,
                                             const gfx::RectF& bounding_box,
                                             gfx::Transform& transform) const {
  const OffsetPathOperation* offset_path = OffsetPath();
  if (!offset_path) {
    return;
  }

  const LengthPoint& position = OffsetPosition();
  const StyleOffsetRotation& rotate = OffsetRotate();
  CoordBox coord_box = offset_path->GetCoordBox();

  PointAndTangent path_position;
  if (const auto* shape_operation =
          DynamicTo<ShapeOffsetPathOperation>(offset_path)) {
    const BasicShape& basic_shape = shape_operation->GetBasicShape();
    switch (basic_shape.GetType()) {
      case BasicShape::kStylePathType: {
        const StylePath& path = To<StylePath>(basic_shape);
        path_position = CalculatePointAndTangentOnPath(path.GetPath());
        break;
      }
      case BasicShape::kStyleRayType: {
        const gfx::RectF reference_box = GetReferenceBox(box, coord_box);
        const gfx::PointF offset_from_reference_box =
            GetOffsetFromContainingBlock(box) -
            reference_box.OffsetFromOrigin();
        const gfx::SizeF& reference_box_size = reference_box.size();
        const StyleRay& ray = To<StyleRay>(basic_shape);
        // Specifies the origin of the ray, where the ray’s line begins (the 0%
        // position). It’s resolved by using the <position> to position a 0x0
        // object area within the box’s containing block. If omitted, it uses
        // the offset starting position of the element, given by
        // offset-position. If the element doesn’t have an offset starting
        // position either, it behaves as at center.
        // NOTE: In current parsing implementation:
        // if `at position` is omitted, it will be computed as 50% 50%.
        gfx::PointF starting_point;
        if (ray.HasExplicitCenter() || position.X().IsNone()) {
          starting_point = PointForCenterCoordinate(
              ray.CenterX(), ray.CenterY(), reference_box_size);
        } else {
          starting_point = GetStartingPointOfThePath(
              offset_from_reference_box, position, reference_box_size);
        }
        path_position = CalculatePointAndTangentOnRay(ray, box, starting_point,
                                                      reference_box_size);
        // `path_position.point` is now relative to the containing block.
        // Make it relative to the box.
        path_position.point -= offset_from_reference_box.OffsetFromOrigin();
        break;
      }
      case BasicShape::kBasicShapeCircleType:
      case BasicShape::kBasicShapeEllipseType:
      case BasicShape::kBasicShapeInsetType:
      case BasicShape::kBasicShapePolygonType:
      case BasicShape::kStyleShapeType: {
        const gfx::RectF reference_box = GetReferenceBox(box, coord_box);
        const gfx::PointF offset_from_reference_box =
            GetOffsetFromContainingBlock(box) -
            reference_box.OffsetFromOrigin();
        const gfx::SizeF& reference_box_size = reference_box.size();
        const gfx::PointF starting_point = GetStartingPointOfThePath(
            offset_from_reference_box, position, reference_box_size);
        path_position = CalculatePointAndTangentOnBasicShape(
            basic_shape, starting_point, reference_box_size);
        // `path_position.point` is now relative to the containing block.
        // Make it relative to the box.
        path_position.point -= offset_from_reference_box.OffsetFromOrigin();
        break;
      }
    }
  } else if (IsA<CoordBoxOffsetPathOperation>(offset_path)) {
    if (box && box->ContainingBlock()) {
      BasicShapeInset* inset = MakeGarbageCollected<BasicShapeInset>();
      inset->SetTop(Length::Fixed(0));
      inset->SetBottom(Length::Fixed(0));
      inset->SetLeft(Length::Fixed(0));
      inset->SetRight(Length::Fixed(0));
      const ComputedStyle& style = box->ContainingBlock()->StyleRef();
      inset->SetTopLeftRadius(style.BorderTopLeftRadius());
      inset->SetTopRightRadius(style.BorderTopRightRadius());
      inset->SetBottomRightRadius(style.BorderBottomRightRadius());
      inset->SetBottomLeftRadius(style.BorderBottomLeftRadius());
      const gfx::RectF reference_box = GetReferenceBox(box, coord_box);
      const gfx::PointF offset_from_reference_box =
          GetOffsetFromContainingBlock(box) - reference_box.OffsetFromOrigin();
      const gfx::SizeF& reference_box_size = reference_box.size();
      const gfx::PointF starting_point = GetStartingPointOfThePath(
          offset_from_reference_box, position, reference_box_size);
      path_position = CalculatePointAndTangentOnBasicShape(
          *inset, starting_point, reference_box_size);
      // `path_position.point` is now relative to the containing block.
      // Make it relative to the box.
      path_position.point -= offset_from_reference_box.OffsetFromOrigin();
    }
  } else {
    const auto* url_operation =
        DynamicTo<ReferenceOffsetPathOperation>(offset_path);
    if (!url_operation->Resource()) {
      return;
    }
    const auto* target =
        DynamicTo<SVGGeometryElement>(url_operation->Resource()->Target());
    Path path;
    if (!target || !target->GetComputedStyle()) {
      // Failure to find a shape should be equivalent to a "m0,0" path.
      path = PathBuilder().MoveTo({0, 0}).Finalize();
    } else {
      path = target->AsPath();
    }
    path_position = CalculatePointAndTangentOnPath(path);
  }

  if (rotate.type == OffsetRotationType::kFixed) {
    path_position.tangent_in_degrees = 0;
  }

  transform.Translate(path_position.point.x() - origin_x,
                      path_position.point.y() - origin_y);
  transform.Rotate(path_position.tangent_in_degrees + rotate.angle);

  const LengthPoint& anchor = OffsetAnchor();
  if (!anchor.X().IsAuto()) {
    gfx::PointF anchor_point = PointForLengthPoint(anchor, bounding_box.size());
    anchor_point += bounding_box.OffsetFromOrigin();

    // Shift the origin back to transform-origin and then move it based on the
    // anchor.
    transform.Translate(origin_x - anchor_point.x(),
                        origin_y - anchor_point.y());
  }
}

bool ComputedStyle::CanRenderBorderImage() const {
  const StyleImage* border_image = BorderImage().GetImage();
  return border_image && border_image->CanRender() && border_image->IsLoaded();
}

const CounterDirectiveMap* ComputedStyle::GetCounterDirectives() const {
  return CounterDirectivesInternal().get();
}

const CounterDirectives ComputedStyle::GetCounterDirectives(
    const AtomicString& identifier) const {
  if (GetCounterDirectives()) {
    auto it = GetCounterDirectives()->find(identifier);
    if (it != GetCounterDirectives()->end()) {
      return it->value;
    }
  }
  return CounterDirectives();
}

Hyphenation* ComputedStyle::GetHyphenation() const {
  if (GetHyphens() != Hyphens::kAuto) {
    return nullptr;
  }
  if (const LayoutLocale* locale = GetFontDescription().Locale()) {
    return locale->GetHyphenation();
  }
  return nullptr;
}

Hyphenation* ComputedStyle::GetHyphenationWithLimits() const {
  if (Hyphenation* hyphenation = GetHyphenation()) {
    const StyleHyphenateLimitChars& limits = HyphenateLimitChars();
    hyphenation->SetLimits(limits.MinBeforeChars(), limits.MinAfterChars(),
                           limits.MinWordChars());
    return hyphenation;
  }
  return nullptr;
}

const AtomicString& ComputedStyle::HyphenString() const {
  const AtomicString& hyphenation_string = HyphenationString();
  if (!hyphenation_string.IsNull()) {
    return hyphenation_string;
  }

  // FIXME: This should depend on locale.
  DEFINE_STATIC_LOCAL(AtomicString, hyphen_minus_string,
                      (base::span_from_ref(uchar::kHyphenMinus)));
  DEFINE_STATIC_LOCAL(AtomicString, hyphen_string,
                      (base::span_from_ref(uchar::kHyphen)));
  const SimpleFontData* primary_font = GetFont()->PrimaryFont();
  DCHECK(primary_font);
  return primary_font && primary_font->GlyphForCharacter(uchar::kHyphen)
             ? hyphen_string
             : hyphen_minus_string;
}

ETextAlign ComputedStyle::GetTextAlign(bool is_last_line) const {
  if (!is_last_line) {
    return GetTextAlign();
  }

  // When this is the last line of a block, or the line ends with a forced line
  // break.
  // https://drafts.csswg.org/css-text-3/#propdef-text-align-last
  switch (TextAlignLast()) {
    case ETextAlignLast::kStart:
      return ETextAlign::kStart;
    case ETextAlignLast::kEnd:
      return ETextAlign::kEnd;
    case ETextAlignLast::kLeft:
      return ETextAlign::kLeft;
    case ETextAlignLast::kRight:
      return ETextAlign::kRight;
    case ETextAlignLast::kCenter:
      return ETextAlign::kCenter;
    case ETextAlignLast::kJustify:
      return ETextAlign::kJustify;
    case ETextAlignLast::kMatchParent:
      return ETextAlign::kMatchParent;
    case ETextAlignLast::kAuto:
      ETextAlign text_align = GetTextAlign();
      if (text_align == ETextAlign::kJustify) {
        return ETextAlign::kStart;
      }
      return text_align;
  }
  NOTREACHED();
}

// Unicode 11 introduced Georgian capital letters (U+1C90 - U+1CBA,
// U+1CB[D-F]), but virtually no font covers them. For now map them back
// to their lowercase counterparts (U+10D0 - U+10FA, U+10F[D-F]).
// https://www.unicode.org/charts/PDF/U10A0.pdf
// https://www.unicode.org/charts/PDF/U1C90.pdf
// See https://crbug.com/865427 .
// TODO(jshin): Make this platform-dependent. For instance, turn this
// off when CrOS gets new Georgian fonts covering capital letters.
// ( https://crbug.com/880144 ).
static String DisableNewGeorgianCapitalLetters(const String& text) {
  if (text.IsNull() || text.Is8Bit()) {
    return text;
  }
  unsigned length = text.length();
  const StringImpl& input = *(text.Impl());
  StringBuilder result;
  result.ReserveCapacity(length);
  // |input| must be well-formed UTF-16 so that there's no worry
  // about surrogate handling.
  for (unsigned i = 0; i < length; ++i) {
    UChar character = input[i];
    if (Character::IsModernGeorgianUppercase(character)) {
      result.Append(Character::LowercaseModernGeorgianUppercase(character));
    } else {
      result.Append(character);
    }
  }
  return result.ToString();
}

namespace {

UChar32 FullwidthVariant(UChar32 code_point) {
  // ASCII printable characters (U+0021..U+007E) to full-width (U+FF01..U+FF5E).
  if (code_point >= 0x21 && code_point <= 0x7E) {
    return code_point + 0xFEE0;
  }

  // Half-width Katakana (U+FF61..U+FF9F) to full-width Katakana.
  // Katakana characters are contiguous, so we can use direct array indexing.
  if (code_point >= 0xFF61 && code_point <= 0xFF9F) {
    auto kKatakanaTable = std::to_array<UChar32>(
        {0x3002, 0x300C, 0x300D, 0x3001, 0x30FB, 0x30F2, 0x30A1, 0x30A3,
         0x30A5, 0x30A7, 0x30A9, 0x30E3, 0x30E5, 0x30E7, 0x30C3, 0x30FC,
         0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, 0x30AB, 0x30AD, 0x30AF,
         0x30B1, 0x30B3, 0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD, 0x30BF,
         0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA, 0x30CB, 0x30CC, 0x30CD,
         0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x30DE, 0x30DF,
         0x30E0, 0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8, 0x30E9, 0x30EA,
         0x30EB, 0x30EC, 0x30ED, 0x30EF, 0x30F3, 0x3099, 0x309A});
    size_t index = code_point - 0xFF61;
    return kKatakanaTable[index];
  }

  // Half-width Hangul to full-width Hangul mapping.
  // Hangul characters have gaps, so we need a lookup table.
  if ((code_point >= 0xFFA0 && code_point <= 0xFFBE) ||
      (code_point >= 0xFFC2 && code_point <= 0xFFC7) ||
      (code_point >= 0xFFCA && code_point <= 0xFFCF) ||
      (code_point >= 0xFFD2 && code_point <= 0xFFD7) ||
      (code_point >= 0xFFDA && code_point <= 0xFFDC)) {
    static const struct {
      UChar32 halfwidth;
      UChar32 fullwidth;
    } kHangulTable[] = {
        {0xFFA0, 0x3164}, {0xFFA1, 0x3131}, {0xFFA2, 0x3132}, {0xFFA3, 0x3133},
        {0xFFA4, 0x3134}, {0xFFA5, 0x3135}, {0xFFA6, 0x3136}, {0xFFA7, 0x3137},
        {0xFFA8, 0x3138}, {0xFFA9, 0x3139}, {0xFFAA, 0x313A}, {0xFFAB, 0x313B},
        {0xFFAC, 0x313C}, {0xFFAD, 0x313D}, {0xFFAE, 0x313E}, {0xFFAF, 0x313F},
        {0xFFB0, 0x3140}, {0xFFB1, 0x3141}, {0xFFB2, 0x3142}, {0xFFB3, 0x3143},
        {0xFFB4, 0x3144}, {0xFFB5, 0x3145}, {0xFFB6, 0x3146}, {0xFFB7, 0x3147},
        {0xFFB8, 0x3148}, {0xFFB9, 0x3149}, {0xFFBA, 0x314A}, {0xFFBB, 0x314B},
        {0xFFBC, 0x314C}, {0xFFBD, 0x314D}, {0xFFBE, 0x314E}, {0xFFC2, 0x314F},
        {0xFFC3, 0x3150}, {0xFFC4, 0x3151}, {0xFFC5, 0x3152}, {0xFFC6, 0x3153},
        {0xFFC7, 0x3154}, {0xFFCA, 0x3155}, {0xFFCB, 0x3156}, {0xFFCC, 0x3157},
        {0xFFCD, 0x3158}, {0xFFCE, 0x3159}, {0xFFCF, 0x315A}, {0xFFD2, 0x315B},
        {0xFFD3, 0x315C}, {0xFFD4, 0x315D}, {0xFFD5, 0x315E}, {0xFFD6, 0x315F},
        {0xFFD7, 0x3160}, {0xFFDA, 0x3161}, {0xFFDB, 0x3162}, {0xFFDC, 0x3163},
    };
    for (const auto& entry : kHangulTable) {
      if (entry.halfwidth == code_point) {
        return entry.fullwidth;
      }
    }
  }

  // Special character mappings.
  switch (code_point) {
    case uchar::kSpace:
      return uchar::kIdeographicSpace;
    case 0x00A2:  // Cent sign
      return 0xFFE0;
    case 0x00A3:  // Pound sign
      return 0xFFE1;
    case 0x00AC:  // Not sign
      return 0xFFE2;
    case 0x00AF:  // Macron
      return 0xFFE3;
    case 0x00A6:  // Broken bar
      return 0xFFE4;
    case uchar::kYenSign:
      return 0xFFE5;
    case 0x20A9:  // Won sign
      return 0xFFE6;
    case 0x2985:  // Left white parenthesis
      return 0xFF5F;
    case 0x2986:  // Right white parenthesis
      return 0xFF60;
    case 0xFFE8:  // Halfwidth forms light vertical
      return 0x2502;
    case 0xFFE9:  // Halfwidth leftwards arrow
      return 0x2190;
    case 0xFFEA:  // Halfwidth upwards arrow
      return 0x2191;
    case 0xFFEB:  // Halfwidth rightwards arrow
      return 0x2192;
    case 0xFFEC:  // Halfwidth downwards arrow
      return 0x2193;
    case 0xFFED:  // Halfwidth black square
      return uchar::kBlackSquare;
    case 0xFFEE:  // Halfwidth white circle
      return uchar::kWhiteCircle;
  }

  return code_point;
}

String ApplyFullwidthTransform(const String& text,
                               TextOffsetMap* offset_map,
                               bool preserve_white_space) {
  StringBuilder result;
  result.ReserveCapacity(text.length());

  CodePointIterator begin(text);
  CodePointIterator end = CodePointIterator::End(text);
  for (auto it = begin; it != end; ++it) {
    UChar32 code_point = *it;

    // Surrogate pairs are not affected by full-width.
    if (!U_IS_BMP(code_point)) {
      result.Append(code_point);
      continue;
    }

    // Per CSS Text Level 3, spaces (U+0020) are transformed to ideographic
    // spaces (U+3000) only when white space is preserved. Spaces that will be
    // collapsed during white space processing should not be transformed.
    UChar32 transformed_char;
    if (code_point == uchar::kSpace && !preserve_white_space) {
      transformed_char = code_point;
    } else {
      transformed_char = FullwidthVariant(code_point);
    }
    result.Append(transformed_char);
  }

  String transformed_string = result.ReleaseString();
  if (offset_map) {
    offset_map->Append(text.length(), transformed_string.length());
  }
  return transformed_string;
}

String ApplyMathAutoTransform(const String& text, TextOffsetMap* offset_map) {
  if (text.length() != 1) {
    return text;
  }
  UChar character = text[0];
  UChar32 transformed_char = unicode::ItalicMathVariant(text[0]);
  if (transformed_char == static_cast<UChar32>(character)) {
    return text;
  }

  Vector<UChar> transformed_text(U16_LENGTH(transformed_char));
  int i = 0;
  U16_APPEND_UNSAFE(transformed_text, i, transformed_char);
  String transformed_string = String(transformed_text);
  if (offset_map) {
    offset_map->Append(text.length(), transformed_string.length());
  }
  return transformed_string;
}

}  // namespace

String ComputedStyle::ApplyTextTransform(const String& text,
                                         UChar previous_character,
                                         TextOffsetMap* offset_map) const {
  switch (TextTransform()) {
    case ETextTransform::kNone:
      return text;
    case ETextTransform::kCapitalize: {
      if (RuntimeEnabledFeatures::ICUCapitalizationEnabled()) {
        const LayoutLocale* locale = GetFontDescription().Locale();
        CaseMap case_map(locale ? locale->CaseMapLocale() : CaseMap::Locale());
        return case_map.ToTitle(text, offset_map, previous_character);
      }
      return Capitalize(text, previous_character);
    }
    case ETextTransform::kUppercase: {
      const LayoutLocale* locale = GetFontDescription().Locale();
      CaseMap case_map(locale ? locale->CaseMapLocale() : CaseMap::Locale());
      return DisableNewGeorgianCapitalLetters(
          case_map.ToUpper(text, offset_map));
    }
    case ETextTransform::kLowercase: {
      const LayoutLocale* locale = GetFontDescription().Locale();
      CaseMap case_map(locale ? locale->CaseMapLocale() : CaseMap::Locale());
      return case_map.ToLower(text, offset_map);
    }
    case ETextTransform::kFullWidth:
      return ApplyFullwidthTransform(text, offset_map,
                                     ShouldPreserveWhiteSpaces());
    case ETextTransform::kMathAuto:
      return ApplyMathAutoTransform(text, offset_map);
  }
  NOTREACHED();
}

const AtomicString& ComputedStyle::TextEmphasisMarkString() const {
  switch (GetTextEmphasisMark()) {
    case TextEmphasisMark::kNone:
      return g_null_atom;
    case TextEmphasisMark::kCustom:
      return TextEmphasisCustomMark();
    case TextEmphasisMark::kDot: {
      DEFINE_STATIC_LOCAL(AtomicString, filled_dot_string,
                          (base::span_from_ref(uchar::kBullet)));
      DEFINE_STATIC_LOCAL(AtomicString, open_dot_string,
                          (base::span_from_ref(uchar::kWhiteBullet)));
      return GetTextEmphasisFill() == TextEmphasisFill::kFilled
                 ? filled_dot_string
                 : open_dot_string;
    }
    case TextEmphasisMark::kCircle: {
      DEFINE_STATIC_LOCAL(AtomicString, filled_circle_string,
                          (base::span_from_ref(uchar::kBlackCircle)));
      DEFINE_STATIC_LOCAL(AtomicString, open_circle_string,
                          (base::span_from_ref(uchar::kWhiteCircle)));
      return GetTextEmphasisFill() == TextEmphasisFill::kFilled
                 ? filled_circle_string
                 : open_circle_string;
    }
    case TextEmphasisMark::kDoubleCircle: {
      DEFINE_STATIC_LOCAL(AtomicString, filled_double_circle_string,
                          (base::span_from_ref(uchar::kFisheye)));
      DEFINE_STATIC_LOCAL(AtomicString, open_double_circle_string,
                          (base::span_from_ref(uchar::kBullseye)));
      return GetTextEmphasisFill() == TextEmphasisFill::kFilled
                 ? filled_double_circle_string
                 : open_double_circle_string;
    }
    case TextEmphasisMark::kTriangle: {
      DEFINE_STATIC_LOCAL(
          AtomicString, filled_triangle_string,
          (base::span_from_ref(uchar::kBlackUpPointingTriangle)));
      DEFINE_STATIC_LOCAL(
          AtomicString, open_triangle_string,
          (base::span_from_ref(uchar::kWhiteUpPointingTriangle)));
      return GetTextEmphasisFill() == TextEmphasisFill::kFilled
                 ? filled_triangle_string
                 : open_triangle_string;
    }
    case TextEmphasisMark::kSesame: {
      DEFINE_STATIC_LOCAL(AtomicString, filled_sesame_string,
                          (base::span_from_ref(uchar::kSesameDot)));
      DEFINE_STATIC_LOCAL(AtomicString, open_sesame_string,
                          (base::span_from_ref(uchar::kWhiteSesameDot)));
      return GetTextEmphasisFill() == TextEmphasisFill::kFilled
                 ? filled_sesame_string
                 : open_sesame_string;
    }
    case TextEmphasisMark::kAuto:
      NOTREACHED();
  }

  NOTREACHED();
}

LineLogicalSide ComputedStyle::GetTextEmphasisLineLogicalSide() const {
  TextEmphasisPosition position = GetTextEmphasisPosition();
  if (RuntimeEnabledFeatures::TextEmphasisPositionAutoEnabled() &&
      position == TextEmphasisPosition::kAuto) {
    if (IsHorizontalWritingMode()) {
      return LineLogicalSide::kOver;
    }
    switch (GetWritingMode()) {
      case WritingMode::kVerticalRl:
      case WritingMode::kVerticalLr:
      case WritingMode::kSidewaysRl:
        return LineLogicalSide::kOver;
      case WritingMode::kSidewaysLr:
        return LineLogicalSide::kUnder;
      default:
        NOTREACHED();
    }
  }

  if (IsHorizontalWritingMode()) {
    return IsOver(position) ? LineLogicalSide::kOver : LineLogicalSide::kUnder;
  }
  if (GetWritingMode() != WritingMode::kSidewaysLr) {
    return IsRight(position) ? LineLogicalSide::kOver : LineLogicalSide::kUnder;
  }
  return IsLeft(position) ? LineLogicalSide::kOver : LineLogicalSide::kUnder;
}

FontBaseline ComputedStyle::GetFontBaseline() const {
  // CssDominantBaseline() always returns kAuto for non-SVG elements,
  // and never returns kUseScript, kNoChange, and kResetSize.
  // See StyleAdjuster::AdjustComputedStyle().
  switch (CssDominantBaseline()) {
    case EDominantBaseline::kAuto:
      break;
    case EDominantBaseline::kMiddle:
      return kXMiddleBaseline;
    case EDominantBaseline::kAlphabetic:
      return kAlphabeticBaseline;
    case EDominantBaseline::kHanging:
      return kHangingBaseline;
    case EDominantBaseline::kCentral:
      return kCentralBaseline;
    case EDominantBaseline::kTextBeforeEdge:
      return kTextOverBaseline;
    case EDominantBaseline::kTextAfterEdge:
      return kTextUnderBaseline;
    case EDominantBaseline::kIdeographic:
      return kIdeographicUnderBaseline;
    case EDominantBaseline::kMathematical:
      return kMathBaseline;

    case EDominantBaseline::kUseScript:
    case EDominantBaseline::kNoChange:
    case EDominantBaseline::kResetSize:
      NOTREACHED();
  }

  // Vertical flow (except 'text-orientation: sideways') uses ideographic
  // central baseline.
  // https://drafts.csswg.org/css-writing-modes-3/#text-baselines
  return !GetFontDescription().IsVerticalAnyUpright() ? kAlphabeticBaseline
                                                      : kCentralBaseline;
}

FontHeight ComputedStyle::GetFontHeight(FontBaseline baseline) const {
  if (const SimpleFontData* font_data = GetFont()->PrimaryFont()) {
    return font_data->GetFontMetrics().GetFontHeight(baseline);
  }
  return FontHeight();
}

bool ComputedStyle::TextDecorationVisualOverflowChanged(
    const ComputedStyle& o) const {
  const AppliedTextDecorationVector& applied_with_this =
      AppliedTextDecorations();
  const AppliedTextDecorationVector& applied_with_other =
      o.AppliedTextDecorations();
  if (applied_with_this.size() != applied_with_other.size()) {
    return true;
  }
  for (auto decoration_index = 0u; decoration_index < applied_with_this.size();
       ++decoration_index) {
    const AppliedTextDecoration& decoration_from_this =
        applied_with_this[decoration_index];
    const AppliedTextDecoration& decoration_from_other =
        applied_with_other[decoration_index];
    if (decoration_from_this.Thickness() != decoration_from_other.Thickness() ||
        decoration_from_this.UnderlineOffset() !=
            decoration_from_other.UnderlineOffset() ||
        decoration_from_this.Style() != decoration_from_other.Style() ||
        decoration_from_this.Lines() != decoration_from_other.Lines()) {
      return true;
    }
  }
  if (GetTextUnderlinePosition() != o.GetTextUnderlinePosition()) {
    return true;
  }

  return false;
}

TextDecorationLine ComputedStyle::TextDecorationsInEffect() const {
  TextDecorationLine decorations = GetTextDecorationLine();
  if (const auto* base_decorations = BaseTextDecorationData()) {
    for (const AppliedTextDecoration& decoration : *base_decorations) {
      decorations |= decoration.Lines();
    }
  }
  return decorations;
}

AppliedTextDecorationVector* ComputedStyle::EnsureAppliedTextDecorationsCache()
    const {
  DCHECK(IsDecoratingBox());

  if (!cached_data_ || !cached_data_->applied_text_decorations_) {
    AppliedTextDecorationVector* decorations =
        MakeGarbageCollected<AppliedTextDecorationVector>();

    if (const AppliedTextDecorationVector* base_decorations =
            BaseTextDecorationData()) {
      decorations->ReserveInitialCapacity(base_decorations->size() + 1u);
      *decorations = *base_decorations;
    }
    decorations->emplace_back(
        GetTextDecorationLine(), TextDecorationStyle(),
        VisitedDependentColor(GetCSSPropertyTextDecorationColor()),
        GetTextDecorationThickness(), TextUnderlineOffset());
    EnsureCachedData().applied_text_decorations_ = decorations;
  }

  return cached_data_->applied_text_decorations_.Get();
}

const AppliedTextDecorationVector& ComputedStyle::AppliedTextDecorations()
    const {
  DEFINE_STATIC_LOCAL(Persistent<AppliedTextDecorationVector>, empty,
                      (MakeGarbageCollected<AppliedTextDecorationVector>()));
  if (!HasAppliedTextDecorations()) {
    return *empty;
  }

  if (!IsDecoratingBox()) {
    const auto* base_decorations = BaseTextDecorationData();
    DCHECK(base_decorations);
    DCHECK_GE(base_decorations->size(), 1u);
    return *base_decorations;
  }

  return *EnsureAppliedTextDecorationsCache();
}

static bool HasInitialVariables(const StyleInitialData* initial_data) {
  return initial_data && initial_data->HasInitialVariables();
}

bool ComputedStyle::HasVariables() const {
  return !InheritedVariables().IsEmpty() ||
         !NonInheritedVariables().IsEmpty() ||
         HasInitialVariables(InitialData());
}

wtf_size_t ComputedStyle::GetVariableNamesCount() const {
  if (!HasVariables()) {
    return 0;
  }
  return GetVariableNames().size();
}

const Vector<AtomicString>& ComputedStyle::GetVariableNames() const {
  if (auto* cache = GetVariableNamesCache()) {
    return *cache;
  }

  Vector<AtomicString>& cache = EnsureVariableNamesCache();

  HashSet<AtomicString> names;
  if (auto* initial_data = InitialData()) {
    initial_data->CollectVariableNames(names);
  }
  InheritedVariables().CollectNames(names);
  NonInheritedVariables().CollectNames(names);
  cache.assign(names);

  return cache;
}

const StyleInheritedVariables& ComputedStyle::InheritedVariables() const {
  return InheritedVariablesInternal();
}

const StyleNonInheritedVariables& ComputedStyle::NonInheritedVariables() const {
  return NonInheritedVariablesInternal();
}

// static
const ComputedGridTrackList& ComputedStyle::ComputedGridTemplate(
    const Member<ComputedGridTrackList>& track_list) {
  if (track_list) {
    return *track_list;
  }
  // If `track_list` is null, that means it is the initial value.
  DEFINE_STATIC_LOCAL(
      Persistent<ComputedGridTrackList>, default_track_list,
      (MakeGarbageCollected<ComputedGridTrackList>(ComputedGridTrackList())));
  return *default_track_list;
}

bool ComputedStyle::HasPropertyDependingOnCurrentColor() const {
  for (CSSPropertyID property_id : kCSSIncludesCurrentColorProperties) {
    auto& property = CSSProperty::Get(property_id);
    DCHECK(property.IsLonghand());
    if (static_cast<const Longhand&>(property).IsAffectedByCurrentColor(
            *this)) {
      return true;
    }
  }
  return false;
}

template <typename T>
CSSVariableData* GetVariableDataInternal(const T& style_or_builder,
                                         const AtomicString& name,
                                         std::optional<bool> inherited_hint) {
  if (inherited_hint.value_or(true)) {
    if (auto data =
            style_or_builder.InheritedVariablesInternal().GetData(name)) {
      return *data;
    }
  }
  if (!inherited_hint.value_or(false)) {
    if (auto data =
            style_or_builder.NonInheritedVariablesInternal().GetData(name)) {
      return *data;
    }
  }
  if (StyleInitialData* initial_data = style_or_builder.InitialData()) {
    return initial_data->GetVariableData(name);
  }
  return nullptr;
}

template <typename T>
const CSSValue* GetVariableValue(
    const T& style_or_builder,
    const AtomicString& name,
    std::optional<bool> inherited_hint = std::nullopt) {
  if (inherited_hint.value_or(true)) {
    if (auto data = style_or_builder.InheritedVariables().GetValue(name)) {
      return *data;
    }
  }
  if (!inherited_hint.value_or(false)) {
    if (auto data = style_or_builder.NonInheritedVariables().GetValue(name)) {
      return *data;
    }
  }
  if (StyleInitialData* initial_data = style_or_builder.InitialData()) {
    return initial_data->GetVariableValue(name);
  }
  return nullptr;
}

CSSVariableData* ComputedStyle::GetVariableData(
    const AtomicString& name) const {
  return blink::GetVariableDataInternal(*this, name, std::nullopt);
}

CSSVariableData* ComputedStyle::GetVariableData(
    const AtomicString& name,
    bool is_inherited_property) const {
  return blink::GetVariableDataInternal(*this, name, is_inherited_property);
}

const CSSValue* ComputedStyle::GetVariableValue(
    const AtomicString& name) const {
  return blink::GetVariableValue(*this, name);
}

const CSSValue* ComputedStyle::GetVariableValue(
    const AtomicString& name,
    bool is_inherited_property) const {
  return blink::GetVariableValue(*this, name, is_inherited_property);
}

bool ComputedStyle::HasCustomScrollbarStyle(Element* element) const {
  if (!element) {
    return false;
  }

  // Ignore ::-webkit-scrollbar when the web setting to prefer default scrollbar
  // styling is true. The exception to this case is when 'display' is set to
  // 'none'.
  if (RuntimeEnabledFeatures::PreferDefaultScrollbarStylesEnabled() &&
      PrefersDefaultScrollbarStyles() && element &&
      !ScrollbarIsHiddenByCustomStyle(element)) {
    return false;
  }

  // Ignore non-standard ::-webkit-scrollbar when standard properties are in
  // use.
  return HasPseudoElementStyle(kPseudoIdScrollbar) &&
         !UsesStandardScrollbarStyle();
}

EScrollbarWidth ComputedStyle::UsedScrollbarWidth() const {
  if (PrefersDefaultScrollbarStyles() &&
      ScrollbarWidth() != EScrollbarWidth::kNone) {
    return EScrollbarWidth::kAuto;
  }

  return ScrollbarWidth();
}

StyleScrollbarColor* ComputedStyle::UsedScrollbarColor() const {
  if (PrefersDefaultScrollbarStyles()) {
    return nullptr;
  }

  return ScrollbarColor();
}

Length ComputedStyle::LineHeight() const {
  const Length& lh = LineHeightInternal();
  // Unlike getFontDescription().computedSize() and hence fontSize(), this is
  // recalculated on demand as we only store the specified line height.
  // FIXME: Should consider scaling the fixed part of any calc expressions
  // too, though this involves messily poking into CalcExpressionLength.
  if (lh.IsFixed()) {
    float multiplier = TextAutosizingMultiplier();
    return Length::Fixed(TextAutosizer::ComputeAutosizedFontSize(
        lh.Pixels(), multiplier, EffectiveZoom()));
  }

  return lh;
}

float ComputedStyle::ComputedLineHeight(const Length& lh, const Font& font) {
  // For "normal" line-height use the font's built-in spacing if available.
  if (lh.IsAuto()) {
    if (font.PrimaryFont()) {
      return font.PrimaryFont()->GetFontMetrics().LineSpacing();
    }
    return 0.0f;
  }

  if (lh.HasPercent()) {
    return MinimumValueForLength(
        lh, LayoutUnit(font.GetFontDescription().ComputedSize()));
  }

  DCHECK(lh.IsFixed());
  return lh.Pixels();
}

float ComputedStyle::ComputedLineHeight() const {
  return ComputedLineHeight(LineHeight(), *GetFont());
}

LayoutUnit ComputedStyle::ComputedLineHeightAsFixed(const Font& font) const {
  const Length& lh = LineHeight();

  // For "normal" line-height use the font's built-in spacing if available.
  if (lh.IsAuto()) {
    if (font.PrimaryFont()) {
      return font.PrimaryFont()->GetFontMetrics().FixedLineSpacing();
    }
    return LayoutUnit();
  }

  if (lh.HasPercent()) {
    return MinimumValueForLength(lh, ComputedFontSizeAsFixed(font));
  }

  DCHECK(lh.IsFixed());
  return LayoutUnit::FromFloatRound(lh.Pixels());
}

LayoutUnit ComputedStyle::ComputedLineHeightAsFixed() const {
  return ComputedLineHeightAsFixed(*GetFont());
}

StyleColor ComputedStyle::DecorationColorIncludingFallback(
    bool visited_link) const {
  StyleColor style_color = visited_link ? InternalVisitedTextDecorationColor()
                                        : TextDecorationColor();

  if (!style_color.IsCurrentColor()) {
    return style_color;
  }

  if (TextStrokeWidth()) {
    // Prefer stroke color if possible, but not if it's fully transparent.
    StyleColor text_stroke_style_color =
        visited_link ? InternalVisitedTextStrokeColor() : TextStrokeColor();
    if (!text_stroke_style_color.IsCurrentColor() &&
        !text_stroke_style_color.Resolve(blink::Color(), UsedColorScheme())
             .IsFullyTransparent()) {
      return text_stroke_style_color;
    }
  }

  return visited_link ? InternalVisitedTextFillColor() : TextFillColor();
}

bool ComputedStyle::HasBackground() const {
  // Ostensibly, we should call VisitedDependentColor() here,
  // but visited does not affect alpha (see VisitedDependentColor()
  // implementation).
  blink::Color color = GetCSSPropertyBackgroundColor().ColorIncludingFallback(
      false, *this,
      /*is_current_color=*/nullptr);
  if (!color.IsFullyTransparent()) {
    return true;
  }
  // When background color animation is running on the compositor thread, we
  // need to trigger repaint even if the background is transparent to collect
  // artifacts in order to run the animation on the compositor.
  if (RuntimeEnabledFeatures::CompositeBGColorAnimationEnabled() &&
      HasCurrentBackgroundColorAnimation()) {
    return true;
  }
  return HasBackgroundImage();
}

Color ComputedStyle::VisitedDependentColor(const Longhand& color_property,
                                           bool* is_current_color) const {
  DCHECK(!color_property.IsVisited());

  blink::Color unvisited_color =
      color_property.ColorIncludingFallback(false, *this, is_current_color);
  if (InsideLink() != EInsideLink::kInsideVisitedLink) {
    return unvisited_color;
  }

  // Properties that provide a GetVisitedProperty() must use the
  // ColorIncludingFallback function on that property.
  //
  // TODO(andruud): Simplify this when all properties support
  // GetVisitedProperty.
  const CSSProperty* visited_property = &color_property;
  if (const CSSProperty* visited = color_property.GetVisitedProperty()) {
    visited_property = visited;
  }

  // Overwrite is_current_color based on the visited color.
  blink::Color visited_color =
      To<Longhand>(*visited_property)
          .ColorIncludingFallback(true, *this, is_current_color);

  // Take the alpha from the unvisited color, but get the RGB values from the
  // visited color.
  //
  // Ideally we would set the |is_current_color| flag to true if the unvisited
  // color is ‘currentColor’, because the result depends on the unvisited alpha,
  // to tell the highlight painter to resolve the color again with a different
  // current color, but that’s not possible with the current interface.
  //
  // In reality, the highlight painter just throws away the whole color and
  // falls back to the layer or next layer or originating ‘color’, so setting
  // the flag when the unvisited color is ‘currentColor’ would break tests like
  // css/css-pseudo/selection-link-001 and css/css-pseudo/target-text-008.
  // TODO(dazabani@igalia.com) improve behaviour where unvisited is currentColor
  return Color::FromColorSpace(visited_color.GetColorSpace(),
                               visited_color.Param0(), visited_color.Param1(),
                               visited_color.Param2(), unvisited_color.Alpha());
}

blink::Color ComputedStyle::VisitedDependentGapColor(
    const StyleColor& gap_color,
    const ComputedStyle& style,
    bool is_column_rule) const {
  CHECK(RuntimeEnabledFeatures::CSSGapDecorationEnabled());
  blink::Color unvisited_gap_color;

  // `StyleColor::IsCurrentColor()` is used down the pipeline to determine if
  // `gap_color` is `currentColor`.
  if (ShouldForceColor(gap_color)) {
    unvisited_gap_color =
        GetInternalForcedCurrentColor(/*is_current_color=*/nullptr);
  } else {
    unvisited_gap_color = gap_color.Resolve(
        GetCurrentColor(), UsedColorScheme(), /*is_current_color=*/nullptr);
  }

  if (InsideLink() != EInsideLink::kInsideVisitedLink) {
    return unvisited_gap_color;
  }

  // For `row-rule-color`, :visited styling is not supported.
  if (!is_column_rule) {
    return unvisited_gap_color;
  }

  blink::Color visited_gap_color;
  if (ShouldForceColor(gap_color)) {
    visited_gap_color =
        GetInternalForcedVisitedCurrentColor(/*is_current_color=*/nullptr);
  } else {
    visited_gap_color =
        style.InternalVisitedColumnRuleColor().GetLegacyValue().Resolve(
            GetInternalVisitedCurrentColor(), UsedColorScheme(),
            /*is_current_color=*/nullptr);
  }

  return visited_gap_color;
}

blink::Color ComputedStyle::VisitedDependentContextFill(
    const SVGPaint& context_paint,
    const ComputedStyle& context_style) const {
  return VisitedDependentContextPaint(context_paint,
                                      context_style.InternalVisitedFillPaint());
}

blink::Color ComputedStyle::VisitedDependentContextStroke(
    const SVGPaint& context_paint,
    const ComputedStyle& context_style) const {
  return VisitedDependentContextPaint(
      context_paint, context_style.InternalVisitedStrokePaint());
}

blink::Color ComputedStyle::VisitedDependentContextPaint(
    const SVGPaint& context_paint,
    const SVGPaint& context_visited_paint) const {
  blink::Color unvisited_color =
      ShouldForceColor(context_paint.GetColor())
          ? GetInternalForcedCurrentColor(nullptr)
          : context_paint.GetColor().Resolve(GetCurrentColor(),
                                             UsedColorScheme(), nullptr);
  if (InsideLink() != EInsideLink::kInsideVisitedLink) {
    return unvisited_color;
  }

  if (!context_visited_paint.HasColor()) {
    return unvisited_color;
  }
  if (ShouldForceColor(context_visited_paint.GetColor())) {
    return GetInternalForcedVisitedCurrentColor(nullptr);
  }
  return context_visited_paint.GetColor().Resolve(
      GetInternalVisitedCurrentColor(), UsedColorScheme(), nullptr);
}

blink::Color ComputedStyle::ResolvedColor(const StyleColor& color,
                                          bool* is_current_color) const {
  bool visited_link = (InsideLink() == EInsideLink::kInsideVisitedLink);
  blink::Color current_color =
      visited_link ? GetInternalVisitedCurrentColor() : GetCurrentColor();
  return color.Resolve(current_color, UsedColorScheme(), is_current_color);
}

bool ComputedStyle::ColumnRuleEquivalent(
    const ComputedStyle& other_style) const {
  return ColumnRuleStyle() == other_style.ColumnRuleStyle() &&
         ColumnRuleWidth() == other_style.ColumnRuleWidth() &&
         VisitedDependentColor(GetCSSPropertyColumnRuleColor()) ==
             other_style.VisitedDependentColor(GetCSSPropertyColumnRuleColor());
}

TextEmphasisMark ComputedStyle::GetTextEmphasisMark() const {
  TextEmphasisMark mark = TextEmphasisMarkInternal();
  if (mark != TextEmphasisMark::kAuto) {
    return mark;
  }

  // https://drafts.csswg.org/css-text-decor/#propdef-text-emphasis-style
  // If only `filled` or `open` is specified, the shape keyword computes to
  // `circle` in horizontal typographic modes and `sesame` in vertical
  // typographic modes.
  if (IsHorizontalTypographicMode()) {
    return TextEmphasisMark::kDot;
  }

  return TextEmphasisMark::kSesame;
}

PhysicalBoxStrut ComputedStyle::ImageOutsets(
    const NinePieceImage& image) const {
  return {
      NinePieceImage::ComputeOutset(image.Outset().Top(), BorderTopWidth()),
      NinePieceImage::ComputeOutset(image.Outset().Right(), BorderRightWidth()),
      NinePieceImage::ComputeOutset(image.Outset().Bottom(),
                                    BorderBottomWidth()),
      NinePieceImage::ComputeOutset(image.Outset().Left(), BorderLeftWidth())};
}

bool ComputedStyle::BorderObscuresBackground() const {
  if (!HasBorder()) {
    return false;
  }

  // Bail if we have any border-image for now. We could look at the image alpha
  // to improve this.
  if (BorderImage().GetImage()) {
    return false;
  }

  BorderEdgeArray edges;
  GetBorderEdgeInfo(edges);

  for (unsigned int i = static_cast<unsigned>(BoxSide::kTop);
       i <= static_cast<unsigned>(BoxSide::kLeft); ++i) {
    const BorderEdge& curr_edge = edges[i];
    if (!curr_edge.ObscuresBackground()) {
      return false;
    }
  }

  return true;
}

PhysicalBoxStrut ComputedStyle::BoxDecorationOutsets() const {
  DCHECK(HasVisualOverflowingEffect());
  PhysicalBoxStrut outsets;

  if (const ShadowList* box_shadow = BoxShadow()) {
    outsets =
        PhysicalBoxStrut::Enclosing(box_shadow->RectOutsetsIncludingOriginal());
  }

  if (HasBorderImageOutsets()) {
    outsets.Unite(BorderImageOutsets());
  }

  if (HasMaskBoxImageOutsets()) {
    outsets.Unite(MaskBoxImageOutsets());
  }

  return outsets;
}

void ComputedStyle::GetBorderEdgeInfo(BorderEdgeArray& edges,
                                      PhysicalBoxSides sides_to_include) const {
  edges[static_cast<unsigned>(BoxSide::kTop)] = BorderEdge(
      BorderTopWidth(), VisitedDependentColor(GetCSSPropertyBorderTopColor()),
      BorderTopStyle(), sides_to_include.top);

  edges[static_cast<unsigned>(BoxSide::kRight)] =
      BorderEdge(BorderRightWidth(),
                 VisitedDependentColor(GetCSSPropertyBorderRightColor()),
                 BorderRightStyle(), sides_to_include.right);

  edges[static_cast<unsigned>(BoxSide::kBottom)] =
      BorderEdge(BorderBottomWidth(),
                 VisitedDependentColor(GetCSSPropertyBorderBottomColor()),
                 BorderBottomStyle(), sides_to_include.bottom);

  edges[static_cast<unsigned>(BoxSide::kLeft)] = BorderEdge(
      BorderLeftWidth(), VisitedDependentColor(GetCSSPropertyBorderLeftColor()),
      BorderLeftStyle(), sides_to_include.left);
}

void ComputedStyle::CopyChildDependentFlagsFrom(
    const ComputedStyle& other) const {
  if (other.ChildHasExplicitInheritance()) {
    SetChildHasExplicitInheritance();
  }
}

blink::Color ComputedStyle::GetCurrentColor(bool* is_current_color) const {
  DCHECK(!Color().IsCurrentColor());
  if (is_current_color) {
    *is_current_color = ColorIsCurrentColor();
  }
  return Color().Resolve(blink::Color(), UsedColorScheme());
}

blink::Color ComputedStyle::GetInternalVisitedCurrentColor(
    bool* is_current_color) const {
  DCHECK(!InternalVisitedColor().IsCurrentColor());
  if (is_current_color) {
    *is_current_color = InternalVisitedColorIsCurrentColor();
  }
  return InternalVisitedColor().Resolve(blink::Color(), UsedColorScheme());
}

blink::Color ComputedStyle::GetInternalForcedCurrentColor(
    bool* is_current_color) const {
  DCHECK(!InternalForcedColor().IsCurrentColor());
  if (Color().IsSystemColorIncludingDeprecated()) {
    return GetCurrentColor(is_current_color);
  }
  return InternalForcedColor().Resolve(blink::Color(), UsedColorScheme(),
                                       is_current_color);
}

blink::Color ComputedStyle::GetInternalForcedVisitedCurrentColor(
    bool* is_current_color) const {
  DCHECK(!InternalForcedVisitedColor().IsCurrentColor());
  if (InternalVisitedColor().IsSystemColorIncludingDeprecated()) {
    return GetInternalVisitedCurrentColor(is_current_color);
  }
  return InternalForcedVisitedColor().Resolve(blink::Color(), UsedColorScheme(),
                                              is_current_color);
}

bool ComputedStyle::ShadowListHasCurrentColor(const ShadowList* shadow_list) {
  return shadow_list &&
         std::ranges::any_of(shadow_list->Shadows(),
                             [](const ShadowData& shadow) {
                               return shadow.GetColor().DependsOnCurrentColor();
                             });
}

const AtomicString& ComputedStyle::ListStyleStringValue() const {
  if (!ListStyleType() || !ListStyleType()->IsString()) {
    return g_null_atom;
  }
  return ListStyleType()->GetStringValue();
}

bool ComputedStyle::MarkerShouldBeInside(
    const Element& parent,
    const DisplayStyle& marker_style) const {
  // https://w3c.github.io/csswg-drafts/css-lists/#list-style-position-outside
  // > If the list item is an inline box: this value is equivalent to inside.
  if (Display() == EDisplay::kInlineListItem ||
      ListStylePosition() == EListStylePosition::kInside) {
    return true;
  }
  return false;
}

std::optional<blink::Color> ComputedStyle::AccentColorResolved() const {
  const StyleAutoColor& auto_color = AccentColor();
  if (auto_color.IsAutoColor()) {
    return std::nullopt;
  }
  return auto_color.Resolve(GetCurrentColor(), UsedColorScheme());
}

std::optional<blink::Color> ComputedStyle::ScrollbarThumbColorResolved() const {
  if (const StyleScrollbarColor* scrollbar_color = UsedScrollbarColor()) {
    return scrollbar_color->GetThumbColor().Resolve(GetCurrentColor(),
                                                    UsedColorScheme());
  }
  return std::nullopt;
}

std::optional<blink::Color> ComputedStyle::ScrollbarTrackColorResolved() const {
  if (const StyleScrollbarColor* scrollbar_color = UsedScrollbarColor()) {
    return scrollbar_color->GetTrackColor().Resolve(GetCurrentColor(),
                                                    UsedColorScheme());
  }
  return std::nullopt;
}

bool ComputedStyle::ShouldApplyAnyContainment(const Element& element,
                                              const DisplayStyle& display_style,
                                              unsigned effective_containment) {
  DCHECK(IsA<HTMLBodyElement>(element) || IsA<HTMLHtmlElement>(element))
      << "Since elements can override the computed display for which box type "
         "to create, this method is not generally correct. Use "
         "LayoutObject::ShouldApplyAnyContainment if possible.";
  if (effective_containment & kContainsStyle) {
    return true;
  }
  if (!element.LayoutObjectIsNeeded(display_style)) {
    return false;
  }
  EDisplay display = display_style.Display();
  if (display == EDisplay::kInline) {
    return false;
  }
  if ((effective_containment & kContainsSize) &&
      (!IsDisplayTableType(display) || display == EDisplay::kTableCaption ||
       ShouldUseContentDataForElement(display_style.GetContentData()))) {
    return true;
  }
  return (effective_containment & (kContainsLayout | kContainsPaint)) &&
         (!IsDisplayTableType(display) || IsDisplayTableBox(display) ||
          display == EDisplay::kTableCell ||
          display == EDisplay::kTableCaption);
}

bool ComputedStyle::CanMatchSizeContainerQueries(const Element& element) const {
  return IsContainerForSizeContainerQueries() &&
         (!element.IsSVGElement() ||
          To<SVGElement>(element).IsOutermostSVGSVGElement());
}

bool ComputedStyle::IsInterleavingRoot(const ComputedStyle* style) {
  const ComputedStyle* unensured = ComputedStyle::NullifyEnsured(style);
  return unensured && (unensured->IsContainerForSizeContainerQueries() ||
                       unensured->GetPositionTryFallbacks() ||
                       unensured->HasAnchorFunctions());
}

bool ComputedStyle::ScrollbarIsHiddenByCustomStyle(Element* element) const {
  // It is necessary to check the cached styles because native input
  // controls are styled this way.
  const ComputedStyle* cached_scrollbar_style =
      GetCachedPseudoElementStyle(kPseudoIdScrollbar);
  if (cached_scrollbar_style &&
      cached_scrollbar_style->Display() == EDisplay::kNone) {
    return true;
  }

  if (!element) {
    return false;
  }

  const ComputedStyle* uncached_scrollbar_style =
      element->UncachedStyleForPseudoElement(
          StyleRequest(kPseudoIdScrollbar, StyleRequest::kForComputedStyle));
  return uncached_scrollbar_style &&
         uncached_scrollbar_style->Display() == EDisplay::kNone;
}

bool ComputedStyle::CalculateIsStackingContextWithoutContainment() const {
  // Force a stacking context for transform-style: preserve-3d. This happens
  // even if preserves-3d is ignored due to a 'grouping property' being
  // present which requires flattening. See:
  // ComputedStyle::HasGroupingPropertyForUsedTransformStyle3D().
  // This is legacy behavior that is left ambiguous in the official specs.
  // See https://crbug.com/663650 for more details.
  if (TransformStyle3D() == ETransformStyle3D::kPreserve3d) {
    return true;
  }
  if (ForcesStackingContext()) {
    return true;
  }
  if (StyleType() == kPseudoIdBackdrop) {
    return true;
  }
  if (HasTransformRelatedProperty()) {
    return true;
  }
  if (HasStackingGroupingProperty(BoxReflect())) {
    return true;
  }
  if (GetPosition() == EPosition::kFixed) {
    return true;
  }
  if (GetPosition() == EPosition::kSticky) {
    return true;
  }
  if (HasPropertyThatCreatesStackingContext(WillChangeProperties())) {
    return true;
  }
  if (ShouldCompositeForCurrentAnimations()) {
    // TODO(882625): This becomes unnecessary when will-change correctly takes
    // into account active animations.
    return true;
  }
  return false;
}

bool ComputedStyle::GapRuleColorIsTransparent(
    const GapDataList<StyleColor>& gap_rule_color) const {
  const blink::Color& current_color = GetCurrentColor();
  const mojom::blink::ColorScheme& color_scheme = UsedColorScheme();
  return std::ranges::all_of(
      gap_rule_color.GetGapDataList(),
      [&](const GapData<StyleColor>& gap_data) {
        // If it’s a simple value, just test it directly.
        if (!gap_data.IsRepeaterData()) {
          const StyleColor& v = gap_data.GetValue();
          return v.Resolve(current_color, color_scheme).IsFullyTransparent();
        }

        // Otherwise it’s a repeater: walk through its RepeatedValues(), and
        // only return true if all values are transparent.
        const auto* rep = gap_data.GetValueRepeater();
        return std::ranges::all_of(
            rep->RepeatedValues(), [&](const StyleColor& v) {
              return v.Resolve(current_color, color_scheme)
                  .IsFullyTransparent();
            });
      });
}

bool ComputedStyle::IsRenderedInTopLayer(const Element& element) const {
  return StyleType() == kPseudoIdBackdrop ||
         (element.IsInTopLayer() &&
          (!RuntimeEnabledFeatures::OverlayPropertyEnabled() ||
           Overlay() == EOverlay::kAuto));
}

bool ComputedStyle::ApplyControlFixedSize(const Node* node) const {
  if (FieldSizing() == EFieldSizing::kFixed) {
    return true;
  }
  if (!node) {
    return false;
  }
  const auto* control = DynamicTo<HTMLFormControlElement>(node);
  if (!control) {
    control = DynamicTo<HTMLFormControlElement>(node->OwnerShadowHost());
  }
  return control && control->GetAutofillState() != WebAutofillState::kNotFilled;
}

bool ComputedStyle::HasAnimationTrigger() const {
  CSSAnimationData* data = Animations();
  if (!data) {
    return false;
  }

  return std::any_of(data->TriggerAttachmentsList().begin(),
                     data->TriggerAttachmentsList().end(),
                     [](Member<StyleTriggerAttachmentVector> attachments_list) {
                       return attachments_list.Get();
                     });
}

bool ComputedStyle::HasBaseEffectiveAppearance() const {
  DCHECK(RuntimeEnabledFeatures::AppearanceBaseEnabled() ||
         EffectiveAppearance() != AppearanceValue::kBase);
  return EffectiveAppearance() == AppearanceValue::kBaseSelect ||
         EffectiveAppearance() == AppearanceValue::kBase;
}

ComputedStyleBuilder::ComputedStyleBuilder(const ComputedStyle& style)
    : ComputedStyleBuilderBase(style) {}

ComputedStyleBuilder::ComputedStyleBuilder(
    const ComputedStyle& initial_style,
    const ComputedStyle& parent_style,
    IsAtShadowBoundary is_at_shadow_boundary)
    : ComputedStyleBuilderBase(initial_style, parent_style) {
  // Even if surrounding content is user-editable, shadow DOM should act as a
  // single unit, and not necessarily be editable
  if (is_at_shadow_boundary == kAtShadowBoundary) {
    SetUserModify(initial_style.UserModify());
  }

  // TODO(crbug.com/1410068): Once `user-select` isn't inherited, we should
  // get rid of following if-statement.
  if (parent_style.UserSelect() == EUserSelect::kContain) {
    SetUserSelect(EUserSelect::kAuto);  // FIXME(sesse): Is this right?
  }

  // TODO(sesse): Why do we do this?
  SetBaseTextDecorationData(parent_style.AppliedTextDecorationData());
}

const ComputedStyle* ComputedStyleBuilder::TakeStyle() {
  return MakeGarbageCollected<ComputedStyle>(ComputedStyle::BuilderPassKey(),
                                             *this);
}

const ComputedStyle* ComputedStyleBuilder::CloneStyle() const {
  ResetAccess();
  has_own_animations_ = false;
  has_own_transitions_ = false;
  return MakeGarbageCollected<ComputedStyle>(ComputedStyle::BuilderPassKey(),
                                             *this);
}

void ComputedStyleBuilder::PropagateIndependentInheritedProperties(
    const ComputedStyle& parent_style) {
  ComputedStyleBuilderBase::PropagateIndependentInheritedProperties(
      parent_style);
  if (!HasVariableReference() && !HasVariableDeclaration() &&
      InheritedVariablesInternal() != parent_style.InheritedVariables()) {
    SetInheritedVariablesInternal(parent_style.InheritedVariablesInternal());
  }
}

void ComputedStyleBuilder::ClearBackgroundImage() {
  FillLayer* curr_child = &AccessBackgroundLayers();
  curr_child->SetImage(
      FillLayer::InitialFillImage(EFillLayerType::kBackground));
  for (curr_child = curr_child->Next(); curr_child;
       curr_child = curr_child->Next()) {
    curr_child->ClearImage();
  }
}

bool ComputedStyleBuilder::SetEffectiveZoom(float f) {
  // Clamp the effective zoom value to a smaller (but hopeful still large
  // enough) range, to avoid overflow in derived computations.
  float clamped_effective_zoom = ClampTo<float>(f, 1e-6, 1e6);
  if (EffectiveZoom() == clamped_effective_zoom) {
    return false;
  }
  SetEffectiveZoomInternal(clamped_effective_zoom);
  // Record UMA for the effective zoom in order to assess the relative
  // importance of sub-pixel behavior, and related features and bugs.
  // Clamp to a max of 400%, to make the histogram behave better at no
  // real cost to our understanding of the zooms in use.
  base::UmaHistogramSparse(
      "Blink.EffectiveZoom",
      std::clamp<float>(clamped_effective_zoom * 100, 0, 400));
  return true;
}

// Compute the FontOrientation from this style. It's derived from WritingMode
// and TextOrientation.
FontOrientation ComputedStyleBuilder::ComputeFontOrientation() const {
  // https://drafts.csswg.org/css-writing-modes/#propdef-text-orientation
  // > the property has no effect in horizontal typographic modes.
  if (IsHorizontalTypographicMode(GetWritingMode())) {
    return FontOrientation::kHorizontal;
  }
  switch (GetTextOrientation()) {
    case ETextOrientation::kMixed:
      return FontOrientation::kVerticalMixed;
    case ETextOrientation::kUpright:
      return FontOrientation::kVerticalUpright;
    case ETextOrientation::kSideways:
      return FontOrientation::kVerticalRotated;
    default:
      NOTREACHED();
  }
}

// Update FontOrientation in FontDescription if it is different. FontBuilder
// takes care of updating it, but if WritingMode or TextOrientation were
// changed after the style was constructed, this function synchronizes
// FontOrientation to match to this style.
void ComputedStyleBuilder::UpdateFontOrientation() {
  FontOrientation orientation = ComputeFontOrientation();
  if (GetFontDescription().Orientation() == orientation) {
    return;
  }
  FontDescription font_description = GetFontDescription();
  font_description.SetOrientation(orientation);
  SetFontDescription(font_description);
}

void ComputedStyleBuilder::SetTextAutosizingMultiplier(float multiplier) {
  if (TextAutosizingMultiplier() == multiplier) {
    return;
  }

  SetTextAutosizingMultiplierInternal(multiplier);

  float size = GetFontDescription().SpecifiedSize();

  DCHECK(std::isfinite(size));
  if (!std::isfinite(size) || size < 0) {
    size = 0;
  } else {
    size = std::min(kMaximumAllowedFontSize, size);
  }

  FontDescription desc(GetFontDescription());
  desc.SetSpecifiedSize(size);

  float computed_size = size * EffectiveZoom();

  float autosized_font_size = TextAutosizer::ComputeAutosizedFontSize(
      computed_size, multiplier, EffectiveZoom());
  desc.SetComputedSize(std::min(kMaximumAllowedFontSize, autosized_font_size));

  SetFontDescription(desc);
}

void ComputedStyleBuilder::SetUsedColorScheme(
    ColorSchemeFlags flags,
    mojom::blink::PreferredColorScheme preferred_color_scheme,
    bool force_dark) {
  bool prefers_dark =
      preferred_color_scheme == mojom::blink::PreferredColorScheme::kDark;
  bool has_dark = flags & static_cast<ColorSchemeFlags>(ColorSchemeFlag::kDark);
  bool has_light =
      flags & static_cast<ColorSchemeFlags>(ColorSchemeFlag::kLight);
  bool has_only = flags & static_cast<ColorSchemeFlags>(ColorSchemeFlag::kOnly);
  bool dark_scheme =
      // Dark scheme because the preferred scheme is dark and color-scheme
      // contains dark.
      (has_dark && prefers_dark) ||
      // Dark scheme because the the only recognized color-scheme is dark.
      (has_dark && !has_light) ||
      // Dark scheme because we have a dark color-scheme override for forced
      // darkening and no 'only' which opts out.
      (force_dark && !has_only) ||
      // Typically, forced darkening should be used with a dark preferred
      // color-scheme. This is to support the FORCE_DARK_ONLY behavior from
      // WebView where this combination is passed to the renderer.
      (force_dark && !prefers_dark);

  SetDarkColorScheme(dark_scheme);

  bool forced_scheme =
      // No dark in the color-scheme property, but we still forced it to dark.
      (!has_dark && dark_scheme) ||
      // Always use forced color-scheme for preferred light color-scheme with
      // forced darkening. The combination of preferred color-scheme of light
      // with a color-scheme property value of "light dark" chooses the light
      // color-scheme. Typically, forced darkening should be used with a dark
      // preferred color-scheme. This is to support the FORCE_DARK_ONLY
      // behavior from WebView where this combination is passed to the
      // renderer.
      (force_dark && !prefers_dark);

  SetColorSchemeForced(forced_scheme);

  const bool is_normal =
      flags == static_cast<ColorSchemeFlags>(ColorSchemeFlag::kNormal);
  SetColorSchemeFlagsIsNormal(is_normal);
}

StyleInheritedVariables& ComputedStyleBuilder::MutableInheritedVariables() {
  return MutableInheritedVariablesInternal();
}

StyleNonInheritedVariables&
ComputedStyleBuilder::MutableNonInheritedVariables() {
  return MutableNonInheritedVariablesInternal();
}

CSSVariableData* ComputedStyleBuilder::GetVariableData(
    const AtomicString& name,
    bool is_inherited_property) const {
  return blink::GetVariableDataInternal(*this, name, is_inherited_property);
}

void ComputedStyleBuilder::SetInheritedVariablesFrom(
    const ComputedStyle* style) {
  SetInheritedVariablesInternal(style->InheritedVariablesInternal());
}

void ComputedStyleBuilder::SetNonInheritedVariablesFrom(
    const ComputedStyle* style) {
  SetNonInheritedVariablesInternal(style->NonInheritedVariablesInternal());
}

STATIC_ASSERT_ENUM(cc::OverscrollBehavior::Type::kAuto,
                   EOverscrollBehavior::kAuto);
STATIC_ASSERT_ENUM(cc::OverscrollBehavior::Type::kContain,
                   EOverscrollBehavior::kContain);
STATIC_ASSERT_ENUM(cc::OverscrollBehavior::Type::kNone,
                   EOverscrollBehavior::kNone);

}  // namespace blink
