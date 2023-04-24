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

#include "base/memory/values_equivalent.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/clamped_math.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "cc/input/overscroll_behavior.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom-blink.h"
#include "third_party/blink/renderer/core/animation/css/css_animation_data.h"
#include "third_party/blink/renderer/core/animation/css/css_transition_data.h"
#include "third_party/blink/renderer/core/css/css_paint_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_equality.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_progress_element.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/map_coordinates_flags.h"
#include "third_party/blink/renderer/core/layout/ng/custom/layout_worklet.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/core/style/border_edge.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/computed_style_initial_values.h"
#include "third_party/blink/renderer/core/style/content_data.h"
#include "third_party/blink/renderer/core/style/cursor_data.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/core/style/style_difference.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/core/style/style_generated_image.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/core/style/style_inherited_variables.h"
#include "third_party/blink/renderer/core/style/style_non_inherited_variables.h"
#include "third_party/blink/renderer/core/style/style_ray.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
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
#include "third_party/blink/renderer/platform/wtf/text/math_transform.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

struct SameSizeAsBorderValue {
  StyleColor color_;
  unsigned bitfield_;
};

ASSERT_SIZE(BorderValue, SameSizeAsBorderValue);

// Since different compilers/architectures pack ComputedStyle differently,
// re-create the same structure for an accurate size comparison.
//
// Keep a separate struct for ComputedStyleBase so that we can recreate the
// inheritance structure. Make sure the fields have the same access specifiers
// as in the "real" class since it can affect the layout. Reference the fields
// so that they are not seen as unused (-Wunused-private-field).
struct SameSizeAsComputedStyleBase {
  SameSizeAsComputedStyleBase() {
    base::debug::Alias(&data_refs);
    base::debug::Alias(&pointers);
    base::debug::Alias(&bitfields);
  }

 private:
  void* data_refs[8];
  void* pointers[1];
  unsigned bitfields[5];
};

struct SameSizeAsComputedStyle : public SameSizeAsComputedStyleBase,
                                 public RefCounted<SameSizeAsComputedStyle> {
  SameSizeAsComputedStyle() { base::debug::Alias(&own_ptrs); }

 private:
  void* own_ptrs[1];
};

// If this assert fails, it means that size of ComputedStyle has changed. Please
// check that you really *do* want to increase the size of ComputedStyle, then
// update the SameSizeAsComputedStyle struct to match the updated storage of
// ComputedStyle.
ASSERT_SIZE(ComputedStyle, SameSizeAsComputedStyle);

StyleCachedData& ComputedStyle::EnsureCachedData() const {
  if (!cached_data_) {
    cached_data_ = std::make_unique<StyleCachedData>();
  }
  return *cached_data_;
}

bool ComputedStyle::HasCachedPseudoElementStyles() const {
  return cached_data_ && cached_data_->pseudo_element_styles_ &&
         cached_data_->pseudo_element_styles_->size();
}

PseudoElementStyleCache* ComputedStyle::GetPseudoElementStyleCache() const {
  if (cached_data_) {
    return cached_data_->pseudo_element_styles_.get();
  }
  return nullptr;
}

PseudoElementStyleCache& ComputedStyle::EnsurePseudoElementStyleCache() const {
  if (!cached_data_ || !cached_data_->pseudo_element_styles_) {
    EnsureCachedData().pseudo_element_styles_ =
        std::make_unique<PseudoElementStyleCache>();
  }
  return *cached_data_->pseudo_element_styles_;
}

scoped_refptr<ComputedStyle> ComputedStyle::CreateInitialStyleSingleton() {
  return base::MakeRefCounted<ComputedStyle>(PassKey());
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

const ComputedStyle* ComputedStyle::AddCachedPositionFallbackStyle(
    scoped_refptr<const ComputedStyle> style,
    unsigned index) const {
  EnsurePositionFallbackStyleCache(index + 1)[index] = std::move(style);
  return (*cached_data_->position_fallback_styles_)[index].get();
}

const ComputedStyle* ComputedStyle::GetCachedPositionFallbackStyle(
    unsigned index) const {
  if (!cached_data_ || !cached_data_->position_fallback_styles_ ||
      index >= cached_data_->position_fallback_styles_->size()) {
    return nullptr;
  }
  return (*cached_data_->position_fallback_styles_)[index].get();
}

PositionFallbackStyleCache& ComputedStyle::EnsurePositionFallbackStyleCache(
    unsigned ensure_size) const {
  if (!cached_data_ || !cached_data_->position_fallback_styles_) {
    EnsureCachedData().position_fallback_styles_ =
        std::make_unique<PositionFallbackStyleCache>();
  }
  if (cached_data_->position_fallback_styles_->size() < ensure_size) {
    cached_data_->position_fallback_styles_->resize(ensure_size);
  }
  return *cached_data_->position_fallback_styles_;
}

ALWAYS_INLINE ComputedStyle::ComputedStyle()
    : ComputedStyleBase(), RefCounted<ComputedStyle>() {}

ALWAYS_INLINE ComputedStyle::ComputedStyle(const ComputedStyle& o)
    : ComputedStyleBase(o), RefCounted<ComputedStyle>() {}

ALWAYS_INLINE ComputedStyle::ComputedStyle(PassKey key) : ComputedStyle() {}

ALWAYS_INLINE ComputedStyle::ComputedStyle(PassKey key, const ComputedStyle& o)
    : ComputedStyle(o) {}

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
    if (UsesHighlightPseudoInheritance(pseudo_id)) {
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
      !new_style.IsContainerForSizeContainerQueries()) {
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
      (old_style.ScrollTimelineAxis() != new_style.ScrollTimelineAxis()) ||
      (old_style.ScrollTimelineAttachment() !=
       new_style.ScrollTimelineAttachment())) {
    return true;
  }
  if (!base::ValuesEquivalent(old_style.ViewTimelineName(),
                              new_style.ViewTimelineName()) ||
      (old_style.ViewTimelineAxis() != new_style.ViewTimelineAxis()) ||
      (old_style.ViewTimelineInset() != new_style.ViewTimelineInset()) ||
      (old_style.ViewTimelineAttachment() !=
       new_style.ViewTimelineAttachment())) {
    return true;
  }
  return false;
}

// The transition to/from nullptr style can affect subsequent siblings
// if they reference a named timeline which appeared/disappeared.
static bool AffectsScrollAnimations(const ComputedStyle* old_style,
                                    const ComputedStyle* new_style) {
  if (old_style &&
      (old_style->ScrollTimelineName() || old_style->ViewTimelineName())) {
    return true;
  }
  if (new_style &&
      (new_style->ScrollTimelineName() || new_style->ViewTimelineName())) {
    return true;
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
  // line-clamping is currently only handled by LayoutDeprecatedFlexibleBox,
  // so that if line-clamping changes then the LayoutObject needs to be
  // recreated.
  if (old_style->IsDeprecatedFlexboxUsingFlexLayout() !=
      new_style->IsDeprecatedFlexboxUsingFlexLayout()) {
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
  if (UNLIKELY(IsA<HTMLLegendElement>(element) &&
               (old_style->IsFloating() != new_style->IsFloating() ||
                old_style->HasOutOfFlowPosition() !=
                    new_style->HasOutOfFlowPosition()))) {
    return true;
  }

  // We use LayoutNGTextCombine only for vertical writing mode.
  if (new_style->HasTextCombine() && old_style->IsHorizontalWritingMode() !=
                                         new_style->IsHorizontalWritingMode()) {
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
    if (AffectsScrollAnimations(old_style, new_style)) {
      return Difference::kSiblingDescendantAffecting;
    }
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
    return Difference::kSiblingDescendantAffecting;
  }
  if (old_style.Display() != new_style.Display() &&
      old_style.BlockifiesChildren() != new_style.BlockifiesChildren()) {
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
    ItemPosition normal_value_behavior) {
  if (value.GetPosition() == ItemPosition::kLegacy ||
      value.GetPosition() == ItemPosition::kNormal ||
      value.GetPosition() == ItemPosition::kAuto) {
    return {normal_value_behavior, OverflowAlignment::kDefault};
  }
  return value;
}

StyleSelfAlignmentData ComputedStyle::ResolvedAlignItems(
    ItemPosition normal_value_behaviour) const {
  // We will return the behaviour of 'normal' value if needed, which is specific
  // of each layout model.
  return ResolvedSelfAlignment(AlignItems(), normal_value_behaviour);
}

StyleSelfAlignmentData ComputedStyle::ResolvedAlignSelf(
    ItemPosition normal_value_behaviour,
    const ComputedStyle* parent_style) const {
  // We will return the behaviour of 'normal' value if needed, which is specific
  // of each layout model.
  if (!parent_style || AlignSelf().GetPosition() != ItemPosition::kAuto) {
    return ResolvedSelfAlignment(AlignSelf(), normal_value_behaviour);
  }

  // The 'auto' keyword computes to the parent's align-items computed value.
  return parent_style->ResolvedAlignItems(normal_value_behaviour);
}

StyleSelfAlignmentData ComputedStyle::ResolvedJustifyItems(
    ItemPosition normal_value_behaviour) const {
  // We will return the behaviour of 'normal' value if needed, which is specific
  // of each layout model.
  return ResolvedSelfAlignment(JustifyItems(), normal_value_behaviour);
}

StyleSelfAlignmentData ComputedStyle::ResolvedJustifySelf(
    ItemPosition normal_value_behaviour,
    const ComputedStyle* parent_style) const {
  // We will return the behaviour of 'normal' value if needed, which is specific
  // of each layout model.
  if (!parent_style || JustifySelf().GetPosition() != ItemPosition::kAuto) {
    return ResolvedSelfAlignment(JustifySelf(), normal_value_behaviour);
  }

  // The auto keyword computes to the parent's justify-items computed value.
  return parent_style->ResolvedJustifyItems(normal_value_behaviour);
}

StyleContentAlignmentData ResolvedContentAlignment(
    const StyleContentAlignmentData& value,
    const StyleContentAlignmentData& normal_behaviour) {
  return (value.GetPosition() == ContentPosition::kNormal &&
          value.Distribution() == ContentDistributionType::kDefault)
             ? normal_behaviour
             : value;
}

StyleContentAlignmentData ComputedStyle::ResolvedAlignContent(
    const StyleContentAlignmentData& normal_behaviour) const {
  // We will return the behaviour of 'normal' value if needed, which is specific
  // of each layout model.
  return ResolvedContentAlignment(AlignContent(), normal_behaviour);
}

StyleContentAlignmentData ComputedStyle::ResolvedJustifyContent(
    const StyleContentAlignmentData& normal_behaviour) const {
  // We will return the behaviour of 'normal' value if needed, which is specific
  // of each layout model.
  return ResolvedContentAlignment(JustifyContent(), normal_behaviour);
}

static inline ContentPosition ResolvedContentAlignmentPosition(
    const StyleContentAlignmentData& value,
    const StyleContentAlignmentData& normal_value_behavior) {
  return (value.GetPosition() == ContentPosition::kNormal &&
          value.Distribution() == ContentDistributionType::kDefault)
             ? normal_value_behavior.GetPosition()
             : value.GetPosition();
}

static inline ContentDistributionType ResolvedContentAlignmentDistribution(
    const StyleContentAlignmentData& value,
    const StyleContentAlignmentData& normal_value_behavior) {
  return (value.GetPosition() == ContentPosition::kNormal &&
          value.Distribution() == ContentDistributionType::kDefault)
             ? normal_value_behavior.Distribution()
             : value.Distribution();
}

ContentPosition ComputedStyle::ResolvedJustifyContentPosition(
    const StyleContentAlignmentData& normal_value_behavior) const {
  return ResolvedContentAlignmentPosition(JustifyContent(),
                                          normal_value_behavior);
}

ContentDistributionType ComputedStyle::ResolvedJustifyContentDistribution(
    const StyleContentAlignmentData& normal_value_behavior) const {
  return ResolvedContentAlignmentDistribution(JustifyContent(),
                                              normal_value_behavior);
}

ContentPosition ComputedStyle::ResolvedAlignContentPosition(
    const StyleContentAlignmentData& normal_value_behavior) const {
  return ResolvedContentAlignmentPosition(AlignContent(),
                                          normal_value_behavior);
}

ContentDistributionType ComputedStyle::ResolvedAlignContentDistribution(
    const StyleContentAlignmentData& normal_value_behavior) const {
  return ResolvedContentAlignmentDistribution(AlignContent(),
                                              normal_value_behavior);
}

bool ComputedStyle::operator==(const ComputedStyle& o) const {
  return InheritedEqual(o) && NonInheritedEqual(o);
}

const ComputedStyle* ComputedStyle::GetCachedPseudoElementStyle(
    PseudoId pseudo_id,
    const AtomicString& pseudo_argument) const {
  if (!HasCachedPseudoElementStyles()) {
    return nullptr;
  }

  for (const auto& pseudo_style : *GetPseudoElementStyleCache()) {
    if (pseudo_style->StyleType() == pseudo_id &&
        (!PseudoElementHasArguments(pseudo_id) ||
         pseudo_style->PseudoArgument() == pseudo_argument)) {
      return pseudo_style.get();
    }
  }

  return nullptr;
}

bool ComputedStyle::CachedPseudoElementStylesDependOnFontMetrics() const {
  if (!HasCachedPseudoElementStyles()) {
    return false;
  }

  DCHECK_EQ(StyleType(), kPseudoIdNone);

  for (const auto& pseudo_style : *GetPseudoElementStyleCache()) {
    if (pseudo_style->DependsOnFontMetrics()) {
      return true;
    }
  }

  return false;
}

const ComputedStyle* ComputedStyle::AddCachedPseudoElementStyle(
    scoped_refptr<const ComputedStyle> pseudo,
    PseudoId pseudo_id,
    const AtomicString& pseudo_argument) const {
  DCHECK(pseudo);

  // Confirm that the styles being cached are for the (PseudoId,argument) that
  // the caller intended (and presumably had checked was not present).
  DCHECK_EQ(static_cast<unsigned>(pseudo->StyleType()),
            static_cast<unsigned>(pseudo_id));
  DCHECK_EQ(pseudo->PseudoArgument(), pseudo_argument);

  // The pseudo style cache assumes that only one entry will be added for any
  // any given (PseudoId,argument). Adding more than one entry is a bug, even
  // if the styles being cached are equal.
  DCHECK(!GetCachedPseudoElementStyle(pseudo->StyleType(),
                                      pseudo->PseudoArgument()));

  const ComputedStyle* result = pseudo.get();

  EnsurePseudoElementStyleCache().push_back(std::move(pseudo));

  return result;
}

const ComputedStyle* ComputedStyle::ReplaceCachedPseudoElementStyle(
    scoped_refptr<const ComputedStyle> pseudo_style,
    PseudoId pseudo_id,
    const AtomicString& pseudo_argument) const {
  DCHECK(pseudo_style->StyleType() != kPseudoIdNone &&
         pseudo_style->StyleType() != kPseudoIdFirstLineInherited);
  if (HasCachedPseudoElementStyles()) {
    for (auto& cached_style : *GetPseudoElementStyleCache()) {
      if (cached_style->StyleType() == pseudo_id &&
          (!PseudoElementHasArguments(pseudo_id) ||
           cached_style->PseudoArgument() == pseudo_argument)) {
        SECURITY_CHECK(cached_style->IsEnsuredInDisplayNone());
        cached_style = pseudo_style;
        return pseudo_style.get();
      }
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
  if (auto* base_data = BaseData().get()) {
    return base_data->GetBaseComputedStyle();
  }
  return nullptr;
}

const CSSBitset* ComputedStyle::GetBaseImportantSet() const {
  if (auto* base_data = BaseData().get()) {
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

bool ComputedStyle::InheritedDataShared(const ComputedStyle& other) const {
  // This is a fast check that only looks if the data structures are shared.
  return ComputedStyleBase::InheritedDataShared(other);
}

static bool DependenceOnContentHeightHasChanged(const ComputedStyle& a,
                                                const ComputedStyle& b) {
  // If top or bottom become auto/non-auto then it means we either have to solve
  // height based on the content or stop doing so
  // (http://www.w3.org/TR/CSS2/visudet.html#abs-non-replaced-height)
  // - either way requires a layout.
  return a.LogicalTop().IsAuto() != b.LogicalTop().IsAuto() ||
         a.LogicalBottom().IsAuto() != b.LogicalBottom().IsAuto();
}

StyleDifference ComputedStyle::VisualInvalidationDiff(
    const Document& document,
    const ComputedStyle& other) const {
  // Note, we use .Get() on each DataRef below because DataRef::operator== will
  // do a deep compare, which is duplicate work when we're going to compare each
  // property inside this function anyway.

  StyleDifference diff;
  if (DiffNeedsReshapeAndFullLayoutAndPaintInvalidation(*this, other)) {
    diff.SetNeedsReshape();
    diff.SetNeedsFullLayout();
    diff.SetNeedsNormalPaintInvalidation();
  }

  if ((!diff.NeedsFullLayout() || !diff.NeedsNormalPaintInvalidation()) &&
      DiffNeedsFullLayoutAndPaintInvalidation(other)) {
    diff.SetNeedsFullLayout();
    diff.SetNeedsNormalPaintInvalidation();
  }

  if (!diff.NeedsFullLayout() && DiffNeedsFullLayout(document, other)) {
    diff.SetNeedsFullLayout();
  }

  if (!diff.NeedsFullLayout() && !MarginEqual(other)) {
    // Relative-positioned elements collapse their margins so need a full
    // layout.
    if (HasOutOfFlowPosition()) {
      diff.SetNeedsPositionedMovementLayout();
    } else {
      diff.SetNeedsFullLayout();
    }
  }

  if (!diff.NeedsFullLayout() && GetPosition() != EPosition::kStatic &&
      !OffsetEqual(other)) {
    // Optimize for the case where a positioned layer is moving but not changing
    // size.
    if (DependenceOnContentHeightHasChanged(*this, other)) {
      diff.SetNeedsFullLayout();
    } else {
      diff.SetNeedsPositionedMovementLayout();
    }
  }

  AdjustDiffForNeedsPaintInvalidation(other, diff, document);

  UpdatePropertySpecificDifferences(other, diff);

  // The following condition needs to be at last, because it may depend on
  // conditions in diff computed above.
  if (ScrollAnchorDisablingPropertyChanged(other, diff)) {
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

bool ComputedStyle::ScrollAnchorDisablingPropertyChanged(
    const ComputedStyle& other,
    const StyleDifference& diff) const {
  if (ComputedStyleBase::ScrollAnchorDisablingPropertyChanged(*this, other)) {
    return true;
  }

  if (diff.TransformChanged()) {
    return true;
  }

  return false;
}

bool ComputedStyle::DiffNeedsFullLayoutAndPaintInvalidation(
    const ComputedStyle& other) const {
  // FIXME: Not all cases in this method need both full layout and paint
  // invalidation.
  // Should move cases into DiffNeedsFullLayout() if
  // - don't need paint invalidation at all;
  // - or the layoutObject knows how to exactly invalidate paints caused by the
  //   layout change instead of forced full paint invalidation.

  if (ComputedStyleBase::DiffNeedsFullLayoutAndPaintInvalidation(*this,
                                                                 other)) {
    return true;
  }

  if (IsDisplayTableType(Display())) {
    if (ComputedStyleBase::
            DiffNeedsFullLayoutAndPaintInvalidationDisplayTableType(*this,
                                                                    other)) {
      return true;
    }

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
  } else if (IsDisplayListItem()) {
    if (ComputedStyleBase::
            DiffNeedsFullLayoutAndPaintInvalidationDisplayListItem(*this,
                                                                   other)) {
      return true;
    }
  }

  if ((Visibility() == EVisibility::kCollapse) !=
      (other.Visibility() == EVisibility::kCollapse)) {
    return true;
  }

  // Movement of non-static-positioned object is special cased in
  // ComputedStyle::VisualInvalidationDiff().

  return false;
}

bool ComputedStyle::DiffNeedsFullLayout(const Document& document,
                                        const ComputedStyle& other) const {
  if (ComputedStyleBase::DiffNeedsFullLayout(*this, other)) {
    return true;
  }

  if (IsDisplayLayoutCustomBox()) {
    if (DiffNeedsFullLayoutForLayoutCustom(document, other)) {
      return true;
    }
  }

  if (DisplayLayoutCustomParentName()) {
    if (DiffNeedsFullLayoutForLayoutCustomChild(document, other)) {
      return true;
    }
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

void ComputedStyle::AdjustDiffForNeedsPaintInvalidation(
    const ComputedStyle& other,
    StyleDifference& diff,
    const Document& document) const {
  if (ComputedStyleBase::DiffNeedsPaintInvalidation(*this, other) ||
      !BorderVisuallyEqual(other) || !RadiiEqual(other)) {
    diff.SetNeedsNormalPaintInvalidation();
  }

  AdjustDiffForClipPath(other, diff);
  AdjustDiffForBackgroundVisuallyEqual(other, diff);

  if (diff.NeedsNormalPaintInvalidation()) {
    return;
  }

  if (PaintImagesInternal()) {
    for (const auto& image : *PaintImagesInternal()) {
      DCHECK(image);
      if (DiffNeedsPaintInvalidationForPaintImage(*image, other, document)) {
        diff.SetNeedsNormalPaintInvalidation();
        return;
      }
    }
  }
}

void ComputedStyle::AdjustDiffForClipPath(const ComputedStyle& other,
                                          StyleDifference& diff) const {
  if (!this->ClipPathDataEquivalent(other)) {
    // Paint invalidation may not be necessary in the case of a composited
    // clip-path animation, so this dcision needs to be deferred until we know
    // whether the clip is being handled by the compositor or not
    diff.SetClipPathChanged();
  }
}

void ComputedStyle::AdjustDiffForBackgroundVisuallyEqual(
    const ComputedStyle& other,
    StyleDifference& diff) const {
  if (BackgroundColor() != other.BackgroundColor()) {
    // If the background color change is not due to a composited animation, then
    // paint invalidation is required; but we can defer the decision until we
    // know whether the color change will be rendered by the compositor.
    diff.SetBackgroundColorChanged();
  }
  // The rendered color may differ from the reported color for a link to prevent
  // leaking the visited status of a link.
  if (InternalVisitedBackgroundColor() !=
      other.InternalVisitedBackgroundColor()) {
    diff.SetBackgroundColorChanged();
  }

  if (!BackgroundInternal().VisuallyEqual(other.BackgroundInternal())) {
    diff.SetNeedsNormalPaintInvalidation();
    return;
  }
  // If the background image depends on currentColor
  // (e.g., background-image: linear-gradient(currentColor, #fff)), and the
  // color has changed, we need to recompute it even though VisuallyEqual()
  // thinks the old and new background styles are identical.
  if (BackgroundInternal().AnyLayerUsesCurrentColor() &&
      (GetCurrentColor() != other.GetCurrentColor() ||
       GetInternalVisitedCurrentColor() !=
           other.GetInternalVisitedCurrentColor())) {
    diff.SetNeedsNormalPaintInvalidation();
  }
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

void ComputedStyle::UpdatePropertySpecificDifferences(
    const ComputedStyle& other,
    StyleDifference& diff) const {
  if (ComputedStyleBase::UpdatePropertySpecificDifferencesZIndex(*this,
                                                                 other)) {
    diff.SetZIndexChanged();
  }

  if (ComputedStyleBase::UpdatePropertySpecificDifferencesTransform(*this,
                                                                    other)) {
    diff.SetTransformPropertyChanged();
  }

  if (ComputedStyleBase::UpdatePropertySpecificDifferencesOtherTransform(
          *this, other)) {
    diff.SetOtherTransformPropertyChanged();
  }

  if (ComputedStyleBase::UpdatePropertySpecificDifferencesOpacity(*this,
                                                                  other)) {
    diff.SetOpacityChanged();
  }

  if (ComputedStyleBase::UpdatePropertySpecificDifferencesFilter(*this,
                                                                 other)) {
    diff.SetFilterChanged();
  }

  if (ComputedStyleBase::
          UpdatePropertySpecificDifferencesNeedsRecomputeVisualOverflow(
              *this, other)) {
    diff.SetNeedsRecomputeVisualOverflow();
  }

  if (!diff.NeedsNormalPaintInvalidation() &&
      ComputedStyleBase::UpdatePropertySpecificDifferencesTextDecorationOrColor(
          *this, other)) {
    diff.SetTextDecorationOrColorChanged();
  }

  if (ComputedStyleBase::UpdatePropertySpecificDifferencesMask(*this, other)) {
    diff.SetMaskChanged();
  }

  bool has_clip = HasOutOfFlowPosition() && !HasAutoClip();
  bool other_has_clip = other.HasOutOfFlowPosition() && !other.HasAutoClip();
  if (has_clip != other_has_clip || (has_clip && Clip() != other.Clip())) {
    diff.SetCSSClipChanged();
  }

  if (GetBlendMode() != other.GetBlendMode()) {
    diff.SetBlendModeChanged();
  }

  if (HasCurrentTransformAnimation() != other.HasCurrentTransformAnimation() ||
      HasCurrentScaleAnimation() != other.HasCurrentScaleAnimation() ||
      HasCurrentRotateAnimation() != other.HasCurrentRotateAnimation() ||
      HasCurrentTranslateAnimation() != other.HasCurrentTranslateAnimation() ||
      HasCurrentOpacityAnimation() != other.HasCurrentOpacityAnimation() ||
      HasCurrentFilterAnimation() != other.HasCurrentFilterAnimation() ||
      HasCurrentBackdropFilterAnimation() !=
          other.HasCurrentBackdropFilterAnimation() ||
      SubtreeWillChangeContents() != other.SubtreeWillChangeContents() ||
      WillChangeScrollPosition() != other.WillChangeScrollPosition() ||
      WillChangeProperties() != other.WillChangeProperties() ||
      BackfaceVisibility() != other.BackfaceVisibility() ||
      UsedTransformStyle3D() != other.UsedTransformStyle3D() ||
      ContainsPaint() != other.ContainsPaint() ||
      IsOverflowVisibleAlongBothAxes() !=
          other.IsOverflowVisibleAlongBothAxes() ||
      !BackdropFilterDataEquivalent(other) ||
      PotentialCompositingReasonsFor3DTransformChanged(other)) {
    diff.SetCompositingReasonsChanged();
  }

  // TODO(crbug.com/1246826): Remove CompositablePaintAnimationChanged.
  if (RuntimeEnabledFeatures::CompositeBGColorAnimationEnabled() &&
      (HasCurrentBackgroundColorAnimation() !=
           other.HasCurrentBackgroundColorAnimation() ||
       other.CompositablePaintAnimationChanged())) {
    diff.SetCompositablePaintEffectChanged();
  }
}

bool ComputedStyle::HasCSSPaintImagesUsingCustomProperty(
    const AtomicString& custom_property_name,
    const Document& document) const {
  if (PaintImagesInternal()) {
    for (const auto& image : *PaintImagesInternal()) {
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

bool ComputedStyle::QuotesDataEquivalent(const ComputedStyle& other) const {
  return base::ValuesEquivalent(Quotes(), other.Quotes());
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
      case CSSPropertyID::kWebkitMask:
      case CSSPropertyID::kWebkitMaskBoxImage:
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
  return base::ranges::any_of(WillChangeProperties(),
                              IsWillChangeCompositingHintProperty);
}

bool ComputedStyle::HasWillChangeTransformHint() const {
  return base::ranges::any_of(WillChangeProperties(),
                              IsWillChangeTransformHintProperty);
}

bool ComputedStyle::HasWillChangeHintForAnyTransformProperty() const {
  return base::ranges::any_of(WillChangeProperties(),
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

  return kInterpolationDefault;
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

void ComputedStyle::ApplyTransform(
    gfx::Transform& result,
    const LayoutBox* box,
    const LayoutSize& border_box_size,
    ApplyTransformOperations apply_operations,
    ApplyTransformOrigin apply_origin,
    ApplyMotionPath apply_motion_path,
    ApplyIndependentTransformProperties apply_independent_transform_properties)
    const {
  ApplyTransform(result, box, gfx::RectF(gfx::SizeF(border_box_size)),
                 apply_operations, apply_origin, apply_motion_path,
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

bool ComputedStyle::HasFilters() const {
  return FilterInternal().Get() && !FilterInternal()->operations_.IsEmpty();
}

namespace {

gfx::SizeF GetReferenceBoxSize(const LayoutBox* box,
                               const gfx::RectF& bounding_box) {
  if (box) {
    if (const LayoutBlock* containing_block = box->ContainingBlock()) {
      // TODO(sakhapov): return based on <coord-box>.
      return gfx::SizeF(containing_block->BorderBoxRect().Size());
    }
  }
  return bounding_box.size();
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
gfx::PointF GetStartingPointOfThePath(const LayoutBox* box,
                                      const LengthPoint& offset_position,
                                      const gfx::SizeF& reference_box_size) {
  if (offset_position.X().IsAuto()) {
    return GetOffsetFromContainingBlock(box);
  }
  return PointForLengthPoint(offset_position, reference_box_size);
}

}  // namespace

PointAndTangent ComputedStyle::CalculatePointAndTangentOnRay(
    const LayoutBox* box,
    const gfx::RectF& bounding_box) const {
  const auto& ray = To<StyleRay>(*OffsetPath());
  const gfx::SizeF reference_box_size = GetReferenceBoxSize(box, bounding_box);
  const gfx::PointF starting_point =
      GetStartingPointOfThePath(box, OffsetPosition(), reference_box_size);
  const float ray_length =
      ray.CalculateRayPathLength(starting_point, reference_box_size);
  float path_length = FloatValueForLength(OffsetDistance(), ray_length);
  return ray.PointAndNormalAtLength(path_length);
}

PointAndTangent ComputedStyle::CalculatePointAndTangentOnPath() const {
  float zoom = EffectiveZoom();
  const StylePath& path = To<StylePath>(*OffsetPath());
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
      path.GetPath().PointAndNormalAtLength(computed_distance);
  path_position.point.Scale(zoom, zoom);
  return path_position;
}

void ComputedStyle::ApplyMotionPathTransform(float origin_x,
                                             float origin_y,
                                             const LayoutBox* box,
                                             const gfx::RectF& bounding_box,
                                             gfx::Transform& transform) const {
  // TODO(ericwilligers): crbug.com/638055 Apply offset-position.
  const BasicShape* path = OffsetPath();
  if (!path) {
    return;
  }

  const LengthPoint& anchor = OffsetAnchor();
  const LengthPoint& position = OffsetPosition();
  const StyleOffsetRotation& rotate = OffsetRotate();

  float origin_shift_x = 0;
  float origin_shift_y = 0;
  // If the offset-position and offset-anchor properties are not yet enabled,
  // they will have the default value, auto.
  gfx::PointF anchor_point(origin_x, origin_y);
  if (!position.X().IsAuto() || !anchor.X().IsAuto()) {
    anchor_point = PointForLengthPoint(anchor, bounding_box.size());
    anchor_point += bounding_box.OffsetFromOrigin();

    // Shift the origin from transform-origin to offset-anchor.
    origin_shift_x = anchor_point.x() - origin_x;
    origin_shift_y = anchor_point.y() - origin_y;
  }

  PointAndTangent path_position;
  switch (path->GetType()) {
    case BasicShape::kStylePathType:
      path_position = CalculatePointAndTangentOnPath();
      break;
    case BasicShape::kStyleRayType:
      path_position = CalculatePointAndTangentOnRay(box, bounding_box);
      break;
    default:
      NOTREACHED();
      break;
  }

  if (rotate.type == OffsetRotationType::kFixed) {
    path_position.tangent_in_degrees = 0;
  }

  transform.Translate(
      path_position.point.x() - anchor_point.x() + origin_shift_x,
      path_position.point.y() - anchor_point.y() + origin_shift_y);
  transform.Rotate(path_position.tangent_in_degrees + rotate.angle);

  if (!position.X().IsAuto() || !anchor.X().IsAuto()) {
    // Shift the origin back to transform-origin.
    transform.Translate(-origin_shift_x, -origin_shift_y);
  }
}

bool ComputedStyle::TextShadowDataEquivalent(const ComputedStyle& other) const {
  return base::ValuesEquivalent(TextShadow(), other.TextShadow());
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

AtomicString ComputedStyle::LocaleForLineBreakIterator() const {
  LineBreakIteratorMode mode = LineBreakIteratorMode::kDefault;
  switch (GetLineBreak()) {
    case LineBreak::kAuto:
    case LineBreak::kAfterWhiteSpace:
    case LineBreak::kAnywhere:
      return Locale();
    case LineBreak::kNormal:
      mode = LineBreakIteratorMode::kNormal;
      break;
    case LineBreak::kStrict:
      mode = LineBreakIteratorMode::kStrict;
      break;
    case LineBreak::kLoose:
      mode = LineBreakIteratorMode::kLoose;
      break;
  }
  if (const LayoutLocale* locale = GetFontDescription().Locale()) {
    return locale->LocaleWithBreakKeyword(mode);
  }
  return Locale();
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
                      (&kHyphenMinusCharacter, 1));
  DEFINE_STATIC_LOCAL(AtomicString, hyphen_string, (&kHyphenCharacter, 1));
  const SimpleFontData* primary_font = GetFont().PrimaryFont();
  DCHECK(primary_font);
  return primary_font && primary_font->GlyphForCharacter(kHyphenCharacter)
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
    case ETextAlignLast::kAuto:
      ETextAlign text_align = GetTextAlign();
      if (text_align == ETextAlign::kJustify) {
        return ETextAlign::kStart;
      }
      return text_align;
  }
  NOTREACHED();
  return GetTextAlign();
}

bool ComputedStyle::ShouldUseTextIndent(bool is_first_line) const {
  return is_first_line;
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

static void ApplyMathAutoTransform(String* text) {
  if (text->length() != 1) {
    return;
  }
  UChar character = (*text)[0];
  UChar32 transformed_char = ItalicMathVariant((*text)[0]);
  if (transformed_char == static_cast<UChar32>(character)) {
    return;
  }

  Vector<UChar> transformed_text(U16_LENGTH(transformed_char));
  int i = 0;
  U16_APPEND_UNSAFE(transformed_text, i, transformed_char);
  *text = String(transformed_text);
}

}  // namespace

void ComputedStyle::ApplyTextTransform(String* text,
                                       UChar previous_character) const {
  switch (TextTransform()) {
    case ETextTransform::kNone:
      return;
    case ETextTransform::kCapitalize:
      *text = Capitalize(*text, previous_character);
      return;
    case ETextTransform::kUppercase: {
      const LayoutLocale* locale = GetFontDescription().Locale();
      CaseMap case_map(locale ? locale->CaseMapLocale() : CaseMap::Locale());
      *text = DisableNewGeorgianCapitalLetters(case_map.ToUpper(*text));
      return;
    }
    case ETextTransform::kLowercase: {
      const LayoutLocale* locale = GetFontDescription().Locale();
      CaseMap case_map(locale ? locale->CaseMapLocale() : CaseMap::Locale());
      *text = case_map.ToLower(*text);
      return;
    }
    case ETextTransform::kMathAuto:
      ApplyMathAutoTransform(text);
      return;
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
                          (&kBulletCharacter, 1));
      DEFINE_STATIC_LOCAL(AtomicString, open_dot_string,
                          (&kWhiteBulletCharacter, 1));
      return GetTextEmphasisFill() == TextEmphasisFill::kFilled
                 ? filled_dot_string
                 : open_dot_string;
    }
    case TextEmphasisMark::kCircle: {
      DEFINE_STATIC_LOCAL(AtomicString, filled_circle_string,
                          (&kBlackCircleCharacter, 1));
      DEFINE_STATIC_LOCAL(AtomicString, open_circle_string,
                          (&kWhiteCircleCharacter, 1));
      return GetTextEmphasisFill() == TextEmphasisFill::kFilled
                 ? filled_circle_string
                 : open_circle_string;
    }
    case TextEmphasisMark::kDoubleCircle: {
      DEFINE_STATIC_LOCAL(AtomicString, filled_double_circle_string,
                          (&kFisheyeCharacter, 1));
      DEFINE_STATIC_LOCAL(AtomicString, open_double_circle_string,
                          (&kBullseyeCharacter, 1));
      return GetTextEmphasisFill() == TextEmphasisFill::kFilled
                 ? filled_double_circle_string
                 : open_double_circle_string;
    }
    case TextEmphasisMark::kTriangle: {
      DEFINE_STATIC_LOCAL(AtomicString, filled_triangle_string,
                          (&kBlackUpPointingTriangleCharacter, 1));
      DEFINE_STATIC_LOCAL(AtomicString, open_triangle_string,
                          (&kWhiteUpPointingTriangleCharacter, 1));
      return GetTextEmphasisFill() == TextEmphasisFill::kFilled
                 ? filled_triangle_string
                 : open_triangle_string;
    }
    case TextEmphasisMark::kSesame: {
      DEFINE_STATIC_LOCAL(AtomicString, filled_sesame_string,
                          (&kSesameDotCharacter, 1));
      DEFINE_STATIC_LOCAL(AtomicString, open_sesame_string,
                          (&kWhiteSesameDotCharacter, 1));
      return GetTextEmphasisFill() == TextEmphasisFill::kFilled
                 ? filled_sesame_string
                 : open_sesame_string;
    }
    case TextEmphasisMark::kAuto:
      NOTREACHED();
      return g_null_atom;
  }

  NOTREACHED();
  return g_null_atom;
}

LineLogicalSide ComputedStyle::GetTextEmphasisLineLogicalSide() const {
  TextEmphasisPosition position = GetTextEmphasisPosition();
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
      break;
  }

  // Vertical flow (except 'text-orientation: sideways') uses ideographic
  // central baseline.
  // https://drafts.csswg.org/css-writing-modes-3/#text-baselines
  return !GetFontDescription().IsVerticalAnyUpright() ? kAlphabeticBaseline
                                                      : kCentralBaseline;
}

FontHeight ComputedStyle::GetFontHeight(FontBaseline baseline) const {
  if (const SimpleFontData* font_data = GetFont().PrimaryFont()) {
    return font_data->GetFontMetrics().GetFontHeight(baseline);
  }
  NOTREACHED();
  return FontHeight();
}

bool ComputedStyle::TextDecorationVisualOverflowEqual(
    const ComputedStyle& o) const {
  const Vector<AppliedTextDecoration, 1>& applied_with_this =
      AppliedTextDecorations();
  const Vector<AppliedTextDecoration, 1>& applied_with_other =
      o.AppliedTextDecorations();
  if (applied_with_this.size() != applied_with_other.size()) {
    return false;
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
      return false;
    }
  }
  if (GetTextUnderlinePosition() != o.GetTextUnderlinePosition()) {
    return false;
  }

  return true;
}

TextDecorationLine ComputedStyle::TextDecorationsInEffect() const {
  TextDecorationLine decorations = GetTextDecorationLine();
  if (const auto& base_decorations = BaseTextDecorationDataInternal()) {
    for (const AppliedTextDecoration& decoration : base_decorations->data) {
      decorations |= decoration.Lines();
    }
  }
  return decorations;
}

base::RefCountedData<Vector<AppliedTextDecoration, 1>>*
ComputedStyle::EnsureAppliedTextDecorationsCache() const {
  DCHECK(IsDecoratingBox());

  if (!cached_data_ || !cached_data_->applied_text_decorations_) {
    using DecorationsVector = Vector<AppliedTextDecoration, 1>;
    DecorationsVector decorations;
    if (const auto& base_decorations = BaseTextDecorationDataInternal()) {
      decorations.ReserveInitialCapacity(base_decorations->data.size() + 1u);
      decorations = base_decorations->data;
    }
    decorations.emplace_back(
        GetTextDecorationLine(), TextDecorationStyle(),
        VisitedDependentColor(GetCSSPropertyTextDecorationColor()),
        GetTextDecorationThickness(), TextUnderlineOffset());
    EnsureCachedData().applied_text_decorations_ =
        base::MakeRefCounted<base::RefCountedData<DecorationsVector>>(
            std::move(decorations));
  }

  return cached_data_->applied_text_decorations_.get();
}

const Vector<AppliedTextDecoration, 1>& ComputedStyle::AppliedTextDecorations()
    const {
  if (!HasAppliedTextDecorations()) {
    using DecorationsVector = Vector<AppliedTextDecoration, 1>;
    DEFINE_STATIC_LOCAL(DecorationsVector, empty, ());
    return empty;
  }

  if (!IsDecoratingBox()) {
    const auto& base_decorations = BaseTextDecorationDataInternal();
    DCHECK(base_decorations);
    DCHECK_GE(base_decorations->data.size(), 1u);
    return base_decorations->data;
  }

  return EnsureAppliedTextDecorationsCache()->data;
}

static bool HasInitialVariables(const StyleInitialData* initial_data) {
  return initial_data && initial_data->HasInitialVariables();
}

bool ComputedStyle::HasVariables() const {
  return InheritedVariables() || NonInheritedVariables() ||
         HasInitialVariables(InitialData().get());
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
  if (auto* initial_data = InitialData().get()) {
    initial_data->CollectVariableNames(names);
  }
  if (auto* inherited_variables = InheritedVariables()) {
    inherited_variables->CollectNames(names);
  }
  if (auto* non_inherited_variables = NonInheritedVariables()) {
    non_inherited_variables->CollectNames(names);
  }
  cache.assign(names);

  return cache;
}

const StyleInheritedVariables* ComputedStyle::InheritedVariables() const {
  return InheritedVariablesInternal().get();
}

const StyleNonInheritedVariables* ComputedStyle::NonInheritedVariables() const {
  return NonInheritedVariablesInternal().get();
}

namespace {

template <typename T>
CSSVariableData* GetVariableData(
    const T& style_or_builder,
    const AtomicString& name,
    absl::optional<bool> inherited_hint = absl::nullopt) {
  if (inherited_hint.value_or(true) && style_or_builder.InheritedVariables()) {
    if (auto data = style_or_builder.InheritedVariables()->GetData(name)) {
      return *data;
    }
  }
  if (!inherited_hint.value_or(false) &&
      style_or_builder.NonInheritedVariables()) {
    if (auto data = style_or_builder.NonInheritedVariables()->GetData(name)) {
      return *data;
    }
  }
  if (StyleInitialData* initial_data = style_or_builder.InitialData().get()) {
    return initial_data->GetVariableData(name);
  }
  return nullptr;
}

template <typename T>
const CSSValue* GetVariableValue(
    const T& style_or_builder,
    const AtomicString& name,
    absl::optional<bool> inherited_hint = absl::nullopt) {
  if (inherited_hint.value_or(true) && style_or_builder.InheritedVariables()) {
    if (auto data = style_or_builder.InheritedVariables()->GetValue(name)) {
      return *data;
    }
  }
  if (!inherited_hint.value_or(false) &&
      style_or_builder.NonInheritedVariables()) {
    if (auto data = style_or_builder.NonInheritedVariables()->GetValue(name)) {
      return *data;
    }
  }
  if (StyleInitialData* initial_data = style_or_builder.InitialData().get()) {
    return initial_data->GetVariableValue(name);
  }
  return nullptr;
}

}  // namespace

CSSVariableData* ComputedStyle::GetVariableData(
    const AtomicString& name) const {
  return blink::GetVariableData(*this, name);
}

CSSVariableData* ComputedStyle::GetVariableData(
    const AtomicString& name,
    bool is_inherited_property) const {
  return blink::GetVariableData(*this, name, is_inherited_property);
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

bool ComputedStyle::HasIdenticalAscentDescentAndLineGap(
    const ComputedStyle& other) const {
  const SimpleFontData* font_data = GetFont().PrimaryFont();
  const SimpleFontData* other_font_data = other.GetFont().PrimaryFont();
  return font_data && other_font_data &&
         font_data->GetFontMetrics().HasIdenticalAscentDescentAndLineGap(
             other_font_data->GetFontMetrics());
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
        lh.Value(), multiplier, EffectiveZoom()));
  }

  return lh;
}

float ComputedStyle::ComputedLineHeight(const Length& lh, const Font& font) {
  // Negative value means the line height is not set. Use the font's built-in
  // spacing, if available.
  if (lh.IsNegative() && font.PrimaryFont()) {
    return font.PrimaryFont()->GetFontMetrics().LineSpacing();
  }

  if (lh.IsPercentOrCalc()) {
    return MinimumValueForLength(
        lh, LayoutUnit(font.GetFontDescription().ComputedSize()));
  }

  return lh.Value();
}

float ComputedStyle::ComputedLineHeight() const {
  return ComputedLineHeight(LineHeight(), GetFont());
}

LayoutUnit ComputedStyle::ComputedLineHeightAsFixed(const Font& font) const {
  const Length& lh = LineHeight();

  // Negative value means the line height is not set. Use the font's built-in
  // spacing, if available.
  if (lh.IsNegative() && font.PrimaryFont()) {
    return font.PrimaryFont()->GetFontMetrics().FixedLineSpacing();
  }

  if (lh.IsPercentOrCalc()) {
    return MinimumValueForLength(lh, ComputedFontSizeAsFixed(font));
  }

  return LayoutUnit::FromFloatFloor(lh.Value());
}

LayoutUnit ComputedStyle::ComputedLineHeightAsFixed() const {
  return ComputedLineHeightAsFixed(GetFont());
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
        text_stroke_style_color.Resolve(blink::Color(), UsedColorScheme())
            .Alpha()) {
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
  if (color.Alpha()) {
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
                               visited_color.Param2(),
                               unvisited_color.FloatAlpha());
}

blink::Color ComputedStyle::ResolvedColor(const StyleColor& color,
                                          bool* is_current_color) const {
  bool visited_link = (InsideLink() == EInsideLink::kInsideVisitedLink);
  blink::Color current_color =
      visited_link ? GetInternalVisitedCurrentColor() : GetCurrentColor();
  return color.Resolve(current_color, UsedColorScheme(), is_current_color);
}

bool ComputedStyle::StrokeDashArrayDataEquivalent(
    const ComputedStyle& other) const {
  return StrokeDashArray()->data == other.StrokeDashArray()->data;
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

  if (IsHorizontalWritingMode()) {
    return TextEmphasisMark::kDot;
  }

  return TextEmphasisMark::kSesame;
}

NGPhysicalBoxStrut ComputedStyle::ImageOutsets(
    const NinePieceImage& image) const {
  return {NinePieceImage::ComputeOutset(image.Outset().Top(),
                                        BorderTopWidth().ToInt()),
          NinePieceImage::ComputeOutset(image.Outset().Right(),
                                        BorderRightWidth().ToInt()),
          NinePieceImage::ComputeOutset(image.Outset().Bottom(),
                                        BorderBottomWidth().ToInt()),
          NinePieceImage::ComputeOutset(image.Outset().Left(),
                                        BorderLeftWidth().ToInt())};
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

  BorderEdge edges[4];
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

NGPhysicalBoxStrut ComputedStyle::BoxDecorationOutsets() const {
  DCHECK(HasVisualOverflowingEffect());
  NGPhysicalBoxStrut outsets;

  if (const ShadowList* box_shadow = BoxShadow()) {
    outsets = NGPhysicalBoxStrut::Enclosing(
        box_shadow->RectOutsetsIncludingOriginal());
  }

  if (HasBorderImageOutsets()) {
    outsets.Unite(BorderImageOutsets());
  }

  if (HasMaskBoxImageOutsets()) {
    outsets.Unite(MaskBoxImageOutsets());
  }

  return outsets;
}

void ComputedStyle::GetBorderEdgeInfo(BorderEdge edges[],
                                      PhysicalBoxSides sides_to_include) const {
  edges[static_cast<unsigned>(BoxSide::kTop)] =
      BorderEdge(BorderTopWidth().ToInt(),
                 VisitedDependentColor(GetCSSPropertyBorderTopColor()),
                 BorderTopStyle(), sides_to_include.top);

  edges[static_cast<unsigned>(BoxSide::kRight)] =
      BorderEdge(BorderRightWidth().ToInt(),
                 VisitedDependentColor(GetCSSPropertyBorderRightColor()),
                 BorderRightStyle(), sides_to_include.right);

  edges[static_cast<unsigned>(BoxSide::kBottom)] =
      BorderEdge(BorderBottomWidth().ToInt(),
                 VisitedDependentColor(GetCSSPropertyBorderBottomColor()),
                 BorderBottomStyle(), sides_to_include.bottom);

  edges[static_cast<unsigned>(BoxSide::kLeft)] =
      BorderEdge(BorderLeftWidth().ToInt(),
                 VisitedDependentColor(GetCSSPropertyBorderLeftColor()),
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
                                       is_current_color,
                                       /* is_forced_color */ true);
}

blink::Color ComputedStyle::GetInternalForcedVisitedCurrentColor(
    bool* is_current_color) const {
  DCHECK(!InternalForcedVisitedColor().IsCurrentColor());
  if (InternalVisitedColor().IsSystemColorIncludingDeprecated()) {
    return GetInternalVisitedCurrentColor(is_current_color);
  }
  return InternalForcedVisitedColor().Resolve(blink::Color(), UsedColorScheme(),
                                              is_current_color,
                                              /* is_forced_color */ true);
}

bool ComputedStyle::ShadowListHasCurrentColor(const ShadowList* shadow_list) {
  return shadow_list &&
         base::ranges::any_of(shadow_list->Shadows(),
                              [](const ShadowData& shadow) {
                                return shadow.GetColor().IsCurrentColor();
                              });
}

const AtomicString& ComputedStyle::ListStyleStringValue() const {
  if (!ListStyleType() || !ListStyleType()->IsString()) {
    return g_null_atom;
  }
  return ListStyleType()->GetStringValue();
}

bool ComputedStyle::MarkerShouldBeInside(const Node& parent_node) const {
  // https://w3c.github.io/csswg-drafts/css-lists/#list-style-position-outside
  // > If the list item is an inline box: this value is equivalent to inside.
  if (Display() == EDisplay::kInlineListItem) {
    return true;
  }
  return ListStylePosition() == EListStylePosition::kInside ||
         (IsA<HTMLLIElement>(parent_node) && !IsInsideListElement());
}

absl::optional<blink::Color> ComputedStyle::AccentColorResolved() const {
  const StyleAutoColor& auto_color = AccentColor();
  if (auto_color.IsAutoColor()) {
    return absl::nullopt;
  }
  return auto_color.Resolve(GetCurrentColor(), UsedColorScheme());
}

static const int kPaintOrderBitwidth = 2;

static unsigned PaintOrderSequence(EPaintOrderType first,
                                   EPaintOrderType second,
                                   EPaintOrderType third) {
  return (((third << kPaintOrderBitwidth) | second) << kPaintOrderBitwidth) |
         first;
}

EPaintOrderType ComputedStyle::PaintOrderType(unsigned index) const {
  unsigned pt = 0;
  DCHECK(index < ((1 << kPaintOrderBitwidth) - 1));
  switch (PaintOrder()) {
    case kPaintOrderNormal:
    case kPaintOrderFillStrokeMarkers:
      pt = PaintOrderSequence(PT_FILL, PT_STROKE, PT_MARKERS);
      break;
    case kPaintOrderFillMarkersStroke:
      pt = PaintOrderSequence(PT_FILL, PT_MARKERS, PT_STROKE);
      break;
    case kPaintOrderStrokeFillMarkers:
      pt = PaintOrderSequence(PT_STROKE, PT_FILL, PT_MARKERS);
      break;
    case kPaintOrderStrokeMarkersFill:
      pt = PaintOrderSequence(PT_STROKE, PT_MARKERS, PT_FILL);
      break;
    case kPaintOrderMarkersFillStroke:
      pt = PaintOrderSequence(PT_MARKERS, PT_FILL, PT_STROKE);
      break;
    case kPaintOrderMarkersStrokeFill:
      pt = PaintOrderSequence(PT_MARKERS, PT_STROKE, PT_FILL);
      break;
  }

  pt =
      (pt >> (kPaintOrderBitwidth * index)) & ((1u << kPaintOrderBitwidth) - 1);
  return static_cast<EPaintOrderType>(pt);
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
  return unensured && unensured->IsContainerForSizeContainerQueries();
}

bool ComputedStyle::CalculateIsStackingContextWithoutContainment() const {
  // Force a stacking context for transform-style: preserve-3d. This happens
  // even if preserves-3d is ignored due to a 'grouping property' being present
  // which requires flattening. See:
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

bool ComputedStyle::IsInTopLayer(const Element& element) const {
  return element.IsInTopLayer() && Overlay() == EOverlay::kAuto;
}

ComputedStyleBuilder::ComputedStyleBuilder(const ComputedStyle& style) {
  style_ = base::AdoptRef(new ComputedStyle(style));
  SetStyleBase(*style_);
}

scoped_refptr<const ComputedStyle> ComputedStyleBuilder::CloneStyle() const {
  DCHECK(style_);
  ResetAccess();
  return base::AdoptRef(new ComputedStyle(*style_));
}

void ComputedStyleBuilder::PropagateIndependentInheritedProperties(
    const ComputedStyle& parent_style) {
  ComputedStyleBuilderBase::PropagateIndependentInheritedProperties(
      parent_style);
  if (!HasVariableReference() && !HasVariableDeclaration() &&
      (InheritedVariablesInternal().get() !=
       parent_style.InheritedVariables())) {
    MutableInheritedVariablesInternal() =
        parent_style.InheritedVariablesInternal();
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

StyleHighlightData& ComputedStyleBuilder::MutableHighlightData() {
  scoped_refptr<StyleHighlightData>& data = MutableHighlightDataInternal();
  if (!data) {
    data = StyleHighlightData::Create();
  } else if (!data->HasOneRef()) {
    data = data->Copy();
  }
  return *data;
}

// Compute the FontOrientation from this style. It's derived from WritingMode
// and TextOrientation.
FontOrientation ComputedStyleBuilder::ComputeFontOrientation() const {
  if (blink::IsHorizontalWritingMode(GetWritingMode())) {
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
      return FontOrientation::kVerticalMixed;
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
}

CSSVariableData* ComputedStyleBuilder::GetVariableData(
    const AtomicString& name,
    bool is_inherited_property) const {
  return blink::GetVariableData(*this, name, is_inherited_property);
}

StyleInheritedVariables& ComputedStyleBuilder::MutableInheritedVariables() {
  scoped_refptr<StyleInheritedVariables>& variables =
      MutableInheritedVariablesInternal();
  if (!variables) {
    variables = StyleInheritedVariables::Create();
  } else if (!variables->HasOneRef()) {
    variables = variables->Copy();
  }
  return *variables;
}

StyleNonInheritedVariables&
ComputedStyleBuilder::MutableNonInheritedVariables() {
  std::unique_ptr<StyleNonInheritedVariables>& variables =
      MutableNonInheritedVariablesInternal();
  if (!variables) {
    variables = std::make_unique<StyleNonInheritedVariables>();
  }
  return *variables;
}

STATIC_ASSERT_ENUM(cc::OverscrollBehavior::Type::kAuto,
                   EOverscrollBehavior::kAuto);
STATIC_ASSERT_ENUM(cc::OverscrollBehavior::Type::kContain,
                   EOverscrollBehavior::kContain);
STATIC_ASSERT_ENUM(cc::OverscrollBehavior::Type::kNone,
                   EOverscrollBehavior::kNone);

CORE_EXPORT ComputedStyle* ComputedStyle::freelist_ = nullptr;

}  // namespace blink
