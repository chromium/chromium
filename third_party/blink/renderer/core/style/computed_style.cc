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

#include "base/numerics/clamped_math.h"
#include "build/build_config.h"
#include "cc/input/overscroll_behavior.h"
#include "third_party/blink/renderer/core/animation/css/css_animation_data.h"
#include "third_party/blink/renderer/core/animation/css/css_transition_data.h"
#include "third_party/blink/renderer/core/css/css_paint_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_equality.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/ng/custom/layout_worklet.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/core/style/border_edge.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/computed_style_initial_values.h"
#include "third_party/blink/renderer/core/style/content_data.h"
#include "third_party/blink/renderer/core/style/cursor_data.h"
#include "third_party/blink/renderer/core/style/data_equivalency.h"
#include "third_party/blink/renderer/core/style/quotes_data.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/core/style/style_difference.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/core/style/style_generated_image.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/core/style/style_inherited_variables.h"
#include "third_party/blink/renderer/core/style/style_non_inherited_variables.h"
#include "third_party/blink/renderer/core/style/style_ray.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/text/capitalize.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/transforms/rotate_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/scale_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/translate_transform_operation.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/case_map.h"

namespace blink {

struct SameSizeAsBorderValue {
  RGBA32 color_;
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
    base::debug::Alias(&bitfields);
  }

 private:
  void* data_refs[7];
  unsigned bitfields[5];
};

struct SameSizeAsComputedStyle : public SameSizeAsComputedStyleBase,
                                 public RefCounted<SameSizeAsComputedStyle> {
  SameSizeAsComputedStyle() {
    base::debug::Alias(&own_ptrs);
    base::debug::Alias(&data_ref_svg_style);
  }

 private:
  void* own_ptrs[1];
  void* data_ref_svg_style;
};

// If this assert fails, it means that size of ComputedStyle has changed. Please
// check that you really *do* what to increase the size of ComputedStyle, then
// update the SameSizeAsComputedStyle struct to match the updated storage of
// ComputedStyle.
ASSERT_SIZE(ComputedStyle, SameSizeAsComputedStyle);

scoped_refptr<ComputedStyle> ComputedStyle::Create() {
  return base::AdoptRef(new ComputedStyle(InitialStyle()));
}

scoped_refptr<ComputedStyle> ComputedStyle::CreateInitialStyle() {
  return base::AdoptRef(new ComputedStyle());
}

ComputedStyle& ComputedStyle::MutableInitialStyle() {
  LEAK_SANITIZER_DISABLED_SCOPE;
  DEFINE_STATIC_REF(ComputedStyle, initial_style,
                    (ComputedStyle::CreateInitialStyle()));
  return *initial_style;
}

void ComputedStyle::InvalidateInitialStyle() {
  MutableInitialStyle().SetTapHighlightColor(
      ComputedStyleInitialValues::InitialTapHighlightColor());
}

scoped_refptr<ComputedStyle> ComputedStyle::CreateAnonymousStyleWithDisplay(
    const ComputedStyle& parent_style,
    EDisplay display) {
  scoped_refptr<ComputedStyle> new_style = ComputedStyle::Create();
  new_style->InheritFrom(parent_style);
  new_style->SetUnicodeBidi(parent_style.GetUnicodeBidi());
  new_style->SetDisplay(display);
  return new_style;
}

scoped_refptr<ComputedStyle>
ComputedStyle::CreateInheritedDisplayContentsStyleIfNeeded(
    const ComputedStyle& parent_style,
    const ComputedStyle& layout_parent_style) {
  if (parent_style.InheritedEqual(layout_parent_style))
    return nullptr;
  return ComputedStyle::CreateAnonymousStyleWithDisplay(parent_style,
                                                        EDisplay::kInline);
}

scoped_refptr<ComputedStyle> ComputedStyle::Clone(const ComputedStyle& other) {
  return base::AdoptRef(new ComputedStyle(other));
}

ALWAYS_INLINE ComputedStyle::ComputedStyle()
    : ComputedStyleBase(), RefCounted<ComputedStyle>() {
  svg_style_.Init();
}

ALWAYS_INLINE ComputedStyle::ComputedStyle(const ComputedStyle& o)
    : ComputedStyleBase(o),
      RefCounted<ComputedStyle>(),
      svg_style_(o.svg_style_) {}

static bool PseudoElementStylesEqual(const ComputedStyle& old_style,
                                     const ComputedStyle& new_style) {
  if (!old_style.HasAnyPseudoElementStyles() &&
      !new_style.HasAnyPseudoElementStyles())
    return true;
  for (PseudoId pseudo_id = kFirstPublicPseudoId;
       pseudo_id < kFirstInternalPseudoId;
       pseudo_id = static_cast<PseudoId>(pseudo_id + 1)) {
    if (!old_style.HasPseudoElementStyle(pseudo_id) &&
        !new_style.HasPseudoElementStyle(pseudo_id))
      continue;
    const ComputedStyle* new_pseudo_style =
        new_style.GetCachedPseudoElementStyle(pseudo_id);
    if (!new_pseudo_style)
      return false;
    const ComputedStyle* old_pseudo_style =
        old_style.GetCachedPseudoElementStyle(pseudo_id);
    if (old_pseudo_style && *old_pseudo_style != *new_pseudo_style)
      return false;
  }
  return true;
}

bool ComputedStyle::NeedsReattachLayoutTree(const ComputedStyle* old_style,
                                            const ComputedStyle* new_style) {
  if (old_style == new_style)
    return false;
  if (!old_style || !new_style)
    return true;
  if (old_style->Display() != new_style->Display())
    return true;
  if (old_style->HasPseudoElementStyle(kPseudoIdFirstLetter) !=
      new_style->HasPseudoElementStyle(kPseudoIdFirstLetter))
    return true;
  if (!old_style->ContentDataEquivalent(*new_style))
    return true;
  if (old_style->HasTextCombine() != new_style->HasTextCombine())
    return true;
  // line-clamping is currently only handled by LayoutDeprecatedFlexibleBox,
  // so that if line-clamping changes then the LayoutObject needs to be
  // recreated.
  if (RuntimeEnabledFeatures::WebkitBoxLayoutUsesFlexLayoutEnabled() &&
      (new_style->IsDeprecatedWebkitBox()) &&
      (old_style->HasLineClamp() != new_style->HasLineClamp() &&
       new_style->BoxOrient() == EBoxOrient::kVertical)) {
    return true;
  }
  // We need to perform a reattach if a "display: layout(foo)" has changed to a
  // "display: layout(bar)". This is because one custom layout could be
  // registered and the other may not, affecting the box-tree construction.
  if (old_style->DisplayLayoutCustomName() !=
      new_style->DisplayLayoutCustomName())
    return true;
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return false;

  // LayoutNG needs an anonymous inline wrapper if ::first-line is applied.
  // Also see |LayoutBlockFlow::NeedsAnonymousInlineWrapper()|.
  if (new_style->HasPseudoElementStyle(kPseudoIdFirstLine) &&
      !old_style->HasPseudoElementStyle(kPseudoIdFirstLine))
    return true;

  return false;
}

ComputedStyle::Difference ComputedStyle::ComputeDifference(
    const ComputedStyle* old_style,
    const ComputedStyle* new_style) {
  if (old_style == new_style)
    return Difference::kEqual;
  if (!old_style || !new_style)
    return Difference::kInherited;

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
  if (old_style.Display() != new_style.Display() &&
      old_style.BlockifiesChildren() != new_style.BlockifiesChildren())
    return Difference::kDisplayAffectingDescendantStyles;
  if (!old_style.NonIndependentInheritedEqual(new_style))
    return Difference::kInherited;
  if (!old_style.LoadingCustomFontsEqual(new_style) ||
      old_style.JustifyItems() != new_style.JustifyItems())
    return Difference::kInherited;
  bool non_inherited_equal = old_style.NonInheritedEqual(new_style);
  if (!non_inherited_equal && old_style.HasExplicitlyInheritedProperties()) {
    return Difference::kInherited;
  }
  if (!old_style.IndependentInheritedEqual(new_style))
    return Difference::kIndependentInherited;
  if (non_inherited_equal) {
    DCHECK(old_style == new_style);
    if (PseudoElementStylesEqual(old_style, new_style))
      return Difference::kEqual;
    return Difference::kPseudoElementStyle;
  }
  if (new_style.HasAnyPseudoElementStyles() ||
      old_style.HasAnyPseudoElementStyles())
    return Difference::kPseudoElementStyle;
  return Difference::kNonInherited;
}

void ComputedStyle::PropagateIndependentInheritedProperties(
    const ComputedStyle& parent_style) {
  ComputedStyleBase::PropagateIndependentInheritedProperties(parent_style);
}

StyleSelfAlignmentData ResolvedSelfAlignment(
    const StyleSelfAlignmentData& value,
    ItemPosition normal_value_behavior) {
  if (value.GetPosition() == ItemPosition::kLegacy ||
      value.GetPosition() == ItemPosition::kNormal ||
      value.GetPosition() == ItemPosition::kAuto)
    return {normal_value_behavior, OverflowAlignment::kDefault};
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
  if (!parent_style || AlignSelfPosition() != ItemPosition::kAuto)
    return ResolvedSelfAlignment(AlignSelf(), normal_value_behaviour);

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
  if (!parent_style || JustifySelfPosition() != ItemPosition::kAuto)
    return ResolvedSelfAlignment(JustifySelf(), normal_value_behaviour);

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

void ComputedStyle::InheritFrom(const ComputedStyle& inherit_parent,
                                IsAtShadowBoundary is_at_shadow_boundary) {
  EUserModify current_user_modify = UserModify();

  ComputedStyleBase::InheritFrom(inherit_parent, is_at_shadow_boundary);
  if (svg_style_ != inherit_parent.svg_style_)
    svg_style_.Access()->InheritFrom(*inherit_parent.svg_style_);

  if (is_at_shadow_boundary == kAtShadowBoundary) {
    // Even if surrounding content is user-editable, shadow DOM should act as a
    // single unit, and not necessarily be editable
    SetUserModify(current_user_modify);
  }
}

void ComputedStyle::CopyNonInheritedFromCached(const ComputedStyle& other) {
  DCHECK(MatchedPropertiesCache::IsStyleCacheable(other));

  ComputedStyleBase::CopyNonInheritedFromCached(other);

  // The flags are copied one-by-one because they contain
  // bunch of stuff other than real style data.
  // See comments for each skipped flag below.

  // Correctly set during selector matching:
  // m_styleType
  // m_pseudoBits

  // Set correctly while computing style for children:
  // m_explicitInheritance

  // The following flags are set during matching before we decide that we get a
  // match in the MatchedPropertiesCache which in turn calls this method. The
  // reason why we don't copy these flags is that they're already correctly set
  // and that they may differ between elements which have the same set of
  // matched properties. For instance, given the rule:
  //
  // :-webkit-any(:hover, :focus) { background-color: green }"
  //
  // A hovered element, and a focused element may use the same cached matched
  // properties here, but the affectedBy flags will be set differently based on
  // the matching order of the :-webkit-any components.
  //
  // m_emptyState
  // m_affectedByFocus
  // m_affectedByHover
  // m_affectedByActive
  // m_affectedByDrag
  // m_isLink

  if (svg_style_ != other.svg_style_)
    svg_style_.Access()->CopyNonInheritedFromCached(*other.svg_style_);
}

bool ComputedStyle::operator==(const ComputedStyle& o) const {
  return InheritedEqual(o) && NonInheritedEqual(o);
}

const ComputedStyle* ComputedStyle::GetCachedPseudoElementStyle(
    PseudoId pid) const {
  if (!cached_pseudo_element_styles_ || !cached_pseudo_element_styles_->size())
    return nullptr;

  if (StyleType() != kPseudoIdNone)
    return nullptr;

  for (const auto& pseudo_style : *cached_pseudo_element_styles_) {
    if (pseudo_style->StyleType() == pid)
      return pseudo_style.get();
  }

  return nullptr;
}

const ComputedStyle* ComputedStyle::AddCachedPseudoElementStyle(
    scoped_refptr<const ComputedStyle> pseudo) const {
  DCHECK(pseudo);
  DCHECK_GT(pseudo->StyleType(), kPseudoIdNone);

  const ComputedStyle* result = pseudo.get();

  if (!cached_pseudo_element_styles_)
    cached_pseudo_element_styles_ = std::make_unique<PseudoElementStyleCache>();

  cached_pseudo_element_styles_->push_back(std::move(pseudo));

  return result;
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
  return ComputedStyleBase::NonIndependentInheritedEqual(other) &&
         svg_style_->InheritedEqual(*other.svg_style_);
}

bool ComputedStyle::LoadingCustomFontsEqual(const ComputedStyle& other) const {
  return GetFont().LoadingCustomFonts() == other.GetFont().LoadingCustomFonts();
}

bool ComputedStyle::NonInheritedEqual(const ComputedStyle& other) const {
  // compare everything except the pseudoStyle pointer
  return ComputedStyleBase::NonInheritedEqual(other) &&
         svg_style_->NonInheritedEqual(*other.svg_style_);
}

bool ComputedStyle::InheritedDataShared(const ComputedStyle& other) const {
  // This is a fast check that only looks if the data structures are shared.
  return ComputedStyleBase::InheritedDataShared(other) &&
         svg_style_.Get() == other.svg_style_.Get();
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
  if (svg_style_.Get() != other.svg_style_.Get())
    diff = svg_style_->Diff(*other.svg_style_);

  if ((!diff.NeedsReshape() || !diff.NeedsFullLayout() ||
       !diff.NeedsFullPaintInvalidation()) &&
      DiffNeedsReshapeAndFullLayoutAndPaintInvalidation(*this, other)) {
    diff.SetNeedsReshape();
    diff.SetNeedsFullLayout();
    diff.SetNeedsPaintInvalidationObject();
  }

  if ((!diff.NeedsCollectInlines() || !diff.NeedsFullLayout() ||
       !diff.NeedsFullPaintInvalidation()) &&
      DiffNeedsCollectInlinesAndFullLayoutAndPaintInvalidation(*this, other)) {
    diff.SetNeedsCollectInlines();
    diff.SetNeedsFullLayout();
    diff.SetNeedsPaintInvalidationObject();
  }

  if ((!diff.NeedsFullLayout() || !diff.NeedsFullPaintInvalidation()) &&
      DiffNeedsFullLayoutAndPaintInvalidation(other)) {
    diff.SetNeedsFullLayout();
    diff.SetNeedsPaintInvalidationObject();
  }

  if (!diff.NeedsFullLayout() && DiffNeedsFullLayout(document, other))
    diff.SetNeedsFullLayout();

  if (!diff.NeedsFullLayout() && !MarginEqual(other)) {
    // Relative-positioned elements collapse their margins so need a full
    // layout.
    if (HasOutOfFlowPosition())
      diff.SetNeedsPositionedMovementLayout();
    else
      diff.SetNeedsFullLayout();
  }

  if (!diff.NeedsFullLayout() && GetPosition() != EPosition::kStatic &&
      !OffsetEqual(other)) {
    // Optimize for the case where a positioned layer is moving but not changing
    // size.
    if (DependenceOnContentHeightHasChanged(*this, other))
      diff.SetNeedsFullLayout();
    else
      diff.SetNeedsPositionedMovementLayout();
  }

  if (DiffNeedsPaintInvalidationSubtree(other))
    diff.SetNeedsPaintInvalidationSubtree();
  else
    AdjustDiffForNeedsPaintInvalidationObject(other, diff, document);

  if (DiffNeedsVisualRectUpdate(other))
    diff.SetNeedsVisualRectUpdate();

  UpdatePropertySpecificDifferences(other, diff);

  // The following condition needs to be at last, because it may depend on
  // conditions in diff computed above.
  if (ScrollAnchorDisablingPropertyChanged(other, diff))
    diff.SetScrollAnchorDisablingPropertyChanged();

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
  if (ComputedStyleBase::ScrollAnchorDisablingPropertyChanged(*this, other))
    return true;

  if (diff.TransformChanged())
    return true;

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

  if (ComputedStyleBase::DiffNeedsFullLayoutAndPaintInvalidation(*this, other))
    return true;

  if (IsDisplayTableType(Display())) {
    if (ComputedStyleBase::
            DiffNeedsFullLayoutAndPaintInvalidationDisplayTableType(*this,
                                                                    other))
      return true;

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
          other.BorderRightStyle() == EBorderStyle::kHidden)))
      return true;
  } else if (Display() == EDisplay::kListItem) {
    if (ComputedStyleBase::
            DiffNeedsFullLayoutAndPaintInvalidationDisplayListItem(*this,
                                                                   other))
      return true;
  }

  if ((Visibility() == EVisibility::kCollapse) !=
      (other.Visibility() == EVisibility::kCollapse))
    return true;

  // Movement of non-static-positioned object is special cased in
  // ComputedStyle::VisualInvalidationDiff().

  return false;
}

bool ComputedStyle::DiffNeedsFullLayout(const Document& document,
                                        const ComputedStyle& other) const {
  if (ComputedStyleBase::DiffNeedsFullLayout(*this, other))
    return true;

  if (IsDisplayLayoutCustomBox()) {
    if (DiffNeedsFullLayoutForLayoutCustom(document, other))
      return true;
  }

  if (DisplayLayoutCustomParentName()) {
    if (DiffNeedsFullLayoutForLayoutCustomChild(document, other))
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

  if (!worklet->GetDocumentDefinitionMap()->Contains(name))
    return false;

  const DocumentLayoutDefinition* definition =
      worklet->GetDocumentDefinitionMap()->at(name);
  if (definition == kInvalidDocumentLayoutDefinition)
    return false;

  if (!PropertiesEqual(definition->NativeInvalidationProperties(), other))
    return true;

  if (!CustomPropertiesEqual(definition->CustomInvalidationProperties(), other))
    return true;

  return false;
}

bool ComputedStyle::DiffNeedsFullLayoutForLayoutCustomChild(
    const Document& document,
    const ComputedStyle& other) const {
  LayoutWorklet* worklet = LayoutWorklet::From(*document.domWindow());
  const AtomicString& name = DisplayLayoutCustomParentName();

  if (!worklet->GetDocumentDefinitionMap()->Contains(name))
    return false;

  const DocumentLayoutDefinition* definition =
      worklet->GetDocumentDefinitionMap()->at(name);
  if (definition == kInvalidDocumentLayoutDefinition)
    return false;

  if (!PropertiesEqual(definition->ChildNativeInvalidationProperties(), other))
    return true;

  if (!CustomPropertiesEqual(definition->ChildCustomInvalidationProperties(),
                             other))
    return true;

  return false;
}

bool ComputedStyle::DiffNeedsPaintInvalidationSubtree(
    const ComputedStyle& other) const {
  return ComputedStyleBase::DiffNeedsPaintInvalidationSubtree(*this, other);
}

void ComputedStyle::AdjustDiffForNeedsPaintInvalidationObject(
    const ComputedStyle& other,
    StyleDifference& diff,
    const Document& document) const {
  if (ComputedStyleBase::DiffNeedsPaintInvalidationObject(*this, other) ||
      !BorderVisuallyEqual(other) || !RadiiEqual(other))
    diff.SetNeedsPaintInvalidationObject();

  AdjustDiffForBackgroundVisuallyEqual(other, diff);

  if (diff.NeedsPaintInvalidationObject())
    return;

  if (PaintImagesInternal()) {
    for (const auto& image : *PaintImagesInternal()) {
      DCHECK(image);
      if (DiffNeedsPaintInvalidationObjectForPaintImage(*image, other,
                                                        document)) {
        diff.SetNeedsPaintInvalidationObject();
        return;
      }
    }
  }
}

void ComputedStyle::AdjustDiffForBackgroundVisuallyEqual(
    const ComputedStyle& other,
    StyleDifference& diff) const {
  if (BackgroundColorInternal() != other.BackgroundColorInternal()) {
    diff.SetNeedsPaintInvalidationObject();
    if (BackgroundColorInternal().HasAlpha() !=
        other.BackgroundColorInternal().HasAlpha()) {
      diff.SetHasAlphaChanged();
      return;
    }
  }
  if (!BackgroundInternal().VisuallyEqual(other.BackgroundInternal())) {
    diff.SetNeedsPaintInvalidationObject();
    // Changes of background fill layers, such as images, may have
    // changed alpha.
    diff.SetHasAlphaChanged();
  }
}

bool ComputedStyle::DiffNeedsPaintInvalidationObjectForPaintImage(
    const StyleImage& image,
    const ComputedStyle& other,
    const Document& document) const {
  // https://crbug.com/835589: early exit when paint target is associated with
  // a link.
  if (InsideLink() != EInsideLink::kNotInsideLink)
    return false;

  CSSPaintValue* value = To<CSSPaintValue>(image.CssValue());

  // NOTE: If the invalidation properties vectors are null, we are invalid as
  // we haven't yet been painted (and can't provide the invalidation
  // properties yet).
  if (!value->NativeInvalidationProperties(document) ||
      !value->CustomInvalidationProperties(document))
    return true;

  if (!PropertiesEqual(*value->NativeInvalidationProperties(document), other))
    return true;

  if (!CustomPropertiesEqual(*value->CustomInvalidationProperties(document),
                             other))
    return true;

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
                                              other))
      return false;
  }

  return true;
}

bool ComputedStyle::CustomPropertiesEqual(
    const Vector<AtomicString>& properties,
    const ComputedStyle& other) const {
  // Short-circuit if neither of the styles have custom properties.
  if (!HasVariables() && !other.HasVariables())
    return true;

  for (const AtomicString& property_name : properties) {
    if (!DataEquivalent(GetVariableData(property_name),
                        other.GetVariableData(property_name))) {
      return false;
    }
    if (!DataEquivalent(GetVariableValue(property_name),
                        other.GetVariableValue(property_name))) {
      return false;
    }
  }

  return true;
}

// This doesn't include conditions needing layout or overflow recomputation
// which implies visual rect update.
bool ComputedStyle::DiffNeedsVisualRectUpdate(
    const ComputedStyle& other) const {
  // Visual rect is empty if visibility is hidden. Also need to update visual
  // rect of the resizer.
  return ComputedStyleBase::DiffNeedsVisualRectUpdate(*this, other);
}

void ComputedStyle::UpdatePropertySpecificDifferences(
    const ComputedStyle& other,
    StyleDifference& diff) const {
  if (ComputedStyleBase::UpdatePropertySpecificDifferencesZIndex(*this, other))
    diff.SetZIndexChanged();

  if (UpdatePropertySpecificDifferencesTransform(*this, other))
    diff.SetTransformChanged();

  if (ComputedStyleBase::UpdatePropertySpecificDifferencesOpacity(*this, other))
    diff.SetOpacityChanged();

  if (ComputedStyleBase::UpdatePropertySpecificDifferencesFilter(*this, other))
    diff.SetFilterChanged();

  if (ComputedStyleBase::
          UpdatePropertySpecificDifferencesNeedsRecomputeOverflow(*this, other))
    diff.SetNeedsRecomputeOverflow();

  if (ComputedStyleBase::UpdatePropertySpecificDifferencesBackdropFilter(*this,
                                                                         other))
    diff.SetBackdropFilterChanged();

  if (!diff.NeedsFullPaintInvalidation() &&
      ComputedStyleBase::UpdatePropertySpecificDifferencesTextDecorationOrColor(
          *this, other)) {
    diff.SetTextDecorationOrColorChanged();
  }

  if (ComputedStyleBase::UpdatePropertySpecificDifferencesMask(*this, other))
    diff.SetMaskChanged();

  bool has_clip = HasOutOfFlowPosition() && !HasAutoClip();
  bool other_has_clip = other.HasOutOfFlowPosition() && !other.HasAutoClip();
  if (has_clip != other_has_clip ||
      (has_clip && Clip() != other.Clip()))
    diff.SetCSSClipChanged();

  if (GetBlendMode() != other.GetBlendMode())
    diff.SetBlendModeChanged();

  if (HasCurrentTransformAnimation() != other.HasCurrentTransformAnimation() ||
      HasCurrentOpacityAnimation() != other.HasCurrentOpacityAnimation() ||
      HasCurrentFilterAnimation() != other.HasCurrentFilterAnimation() ||
      HasCurrentBackdropFilterAnimation() !=
          other.HasCurrentBackdropFilterAnimation() ||
      SubtreeWillChangeContents() != other.SubtreeWillChangeContents() ||
      BackfaceVisibility() != other.BackfaceVisibility() ||
      UsedTransformStyle3D() != other.UsedTransformStyle3D() ||
      ContainsPaint() != other.ContainsPaint() ||
      IsOverflowVisible() != other.IsOverflowVisible() ||
      WillChangeProperties() != other.WillChangeProperties()) {
    diff.SetCompositingReasonsChanged();
  }
}

void ComputedStyle::AddPaintImage(StyleImage* image) {
  if (!MutablePaintImagesInternal()) {
    SetPaintImagesInternal(std::make_unique<PaintImages>());
  }
  MutablePaintImagesInternal()->push_back(image);
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

void ComputedStyle::AddCursor(StyleImage* image,
                              bool hot_spot_specified,
                              const IntPoint& hot_spot) {
  if (!CursorDataInternal())
    SetCursorDataInternal(MakeGarbageCollected<CursorList>());
  MutableCursorDataInternal()->push_back(
      CursorData(image, hot_spot_specified, hot_spot));
}

void ComputedStyle::SetCursorList(CursorList* other) {
  SetCursorDataInternal(other);
}

bool ComputedStyle::QuotesDataEquivalent(const ComputedStyle& other) const {
  return DataEquivalent(Quotes(), other.Quotes());
}

void ComputedStyle::ClearCursorList() {
  if (CursorDataInternal())
    SetCursorDataInternal(nullptr);
}

static bool HasPropertyThatCreatesStackingContext(
    const Vector<CSSPropertyID>& properties) {
  for (CSSPropertyID property : properties) {
    switch (property) {
      case CSSPropertyID::kOpacity:
      case CSSPropertyID::kTransform:
      case CSSPropertyID::kAliasWebkitTransform:
      case CSSPropertyID::kTransformStyle:
      case CSSPropertyID::kAliasWebkitTransformStyle:
      case CSSPropertyID::kPerspective:
      case CSSPropertyID::kAliasWebkitPerspective:
      case CSSPropertyID::kTranslate:
      case CSSPropertyID::kRotate:
      case CSSPropertyID::kScale:
      case CSSPropertyID::kOffsetPath:
      case CSSPropertyID::kOffsetPosition:
      case CSSPropertyID::kWebkitMask:
      case CSSPropertyID::kWebkitMaskBoxImage:
      case CSSPropertyID::kClipPath:
      case CSSPropertyID::kAliasWebkitClipPath:
      case CSSPropertyID::kWebkitBoxReflect:
      case CSSPropertyID::kFilter:
      case CSSPropertyID::kAliasWebkitFilter:
      case CSSPropertyID::kBackdropFilter:
      case CSSPropertyID::kZIndex:
      case CSSPropertyID::kPosition:
      case CSSPropertyID::kMixBlendMode:
      case CSSPropertyID::kIsolation:
        return true;
      default:
        break;
    }
  }
  return false;
}

void ComputedStyle::UpdateIsStackingContext(bool is_document_element,
                                            bool is_in_top_layer,
                                            bool is_svg_stacking) {
  if (IsStackingContext())
    return;

  // Force a stacking context for transform-style: preserve-3d. This happens
  // even if preserves-3d is ignored due to a 'grouping property' being present
  // which requires flattening. See ComputedStyle::UsedTransformStyle3D() and
  // ComputedStyle::HasGroupingProperty().
  // This is legacy behavior that is left ambiguous in the official specs.
  // See https://crbug.com/663650 for more details."
  if (TransformStyle3D() == ETransformStyle3D::kPreserve3d) {
    SetIsStackingContext(true);
    return;
  }

  if (is_document_element || is_in_top_layer || is_svg_stacking ||
      StyleType() == kPseudoIdBackdrop || HasOpacity() ||
      HasTransformRelatedProperty() || HasMask() || ClipPath() ||
      BoxReflect() || HasFilterInducingProperty() || HasBackdropFilter() ||
      HasBlendMode() || HasIsolation() || HasViewportConstrainedPosition() ||
      GetPosition() == EPosition::kSticky ||
      HasPropertyThatCreatesStackingContext(WillChangeProperties()) ||
      /* TODO(882625): This becomes unnecessary when will-change correctly takes
      into account active animations. */
      ShouldCompositeForCurrentAnimations() || ContainsPaint() ||
      ContainsLayout()) {
    SetIsStackingContext(true);
  }
}

void ComputedStyle::AddCallbackSelector(const String& selector) {
  if (!CallbackSelectorsInternal().Contains(selector))
    MutableCallbackSelectorsInternal().push_back(selector);
}

void ComputedStyle::SetContent(ContentData* content_data) {
  SetContentInternal(content_data);
}

static bool IsWillChangeTransformHintProperty(CSSPropertyID property) {
  switch (property) {
    case CSSPropertyID::kTransform:
    case CSSPropertyID::kAliasWebkitTransform:
    case CSSPropertyID::kPerspective:
    case CSSPropertyID::kTranslate:
    case CSSPropertyID::kScale:
    case CSSPropertyID::kRotate:
    case CSSPropertyID::kOffsetPath:
    case CSSPropertyID::kOffsetPosition:
      return true;
    default:
      break;
  }
  return false;
}

static bool IsWillChangeCompositingHintProperty(CSSPropertyID property) {
  if (IsWillChangeTransformHintProperty(property))
    return true;
  switch (property) {
    case CSSPropertyID::kOpacity:
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
  const auto& properties = WillChangeProperties();
  return std::any_of(properties.begin(), properties.end(),
                     IsWillChangeCompositingHintProperty);
}

bool ComputedStyle::HasWillChangeTransformHint() const {
  const auto& properties = WillChangeProperties();
  return std::any_of(properties.begin(), properties.end(),
                     IsWillChangeTransformHintProperty);
}

bool ComputedStyle::RequireTransformOrigin(
    ApplyTransformOrigin apply_origin,
    ApplyMotionPath apply_motion_path) const {
  // transform-origin brackets the transform with translate operations.
  // Optimize for the case where the only transform is a translation, since the
  // transform-origin is irrelevant in that case.
  if (apply_origin != kIncludeTransformOrigin)
    return false;

  if (apply_motion_path == kIncludeMotionPath)
    return true;

  for (const auto& operation : Transform().Operations()) {
    TransformOperation::OperationType type = operation->GetType();
    if (type != TransformOperation::kTranslateX &&
        type != TransformOperation::kTranslateY &&
        type != TransformOperation::kTranslate &&
        type != TransformOperation::kTranslateZ &&
        type != TransformOperation::kTranslate3D)
      return true;
  }

  return Scale() || Rotate();
}

InterpolationQuality ComputedStyle::GetInterpolationQuality() const {
  if (ImageRendering() == EImageRendering::kPixelated)
    return kInterpolationNone;

  if (ImageRendering() == EImageRendering::kWebkitOptimizeContrast)
    return kInterpolationLow;

  return kInterpolationDefault;
}

void ComputedStyle::LoadDeferredImages(Document& document) const {
  if (HasBackgroundImage()) {
    for (const FillLayer* background_layer = &BackgroundLayers();
         background_layer; background_layer = background_layer->Next()) {
      if (StyleImage* image = background_layer->GetImage()) {
        if (image->IsImageResource() && image->IsLazyloadPossiblyDeferred())
          To<StyleFetchedImage>(image)->LoadDeferredImage(document);
      }
    }
  }
}

void ComputedStyle::ApplyTransform(
    TransformationMatrix& result,
    const LayoutSize& border_box_size,
    ApplyTransformOrigin apply_origin,
    ApplyMotionPath apply_motion_path,
    ApplyIndependentTransformProperties apply_independent_transform_properties)
    const {
  ApplyTransform(result, FloatRect(FloatPoint(), FloatSize(border_box_size)),
                 apply_origin, apply_motion_path,
                 apply_independent_transform_properties);
}

void ComputedStyle::ApplyTransform(
    TransformationMatrix& result,
    const FloatRect& bounding_box,
    ApplyTransformOrigin apply_origin,
    ApplyMotionPath apply_motion_path,
    ApplyIndependentTransformProperties apply_independent_transform_properties)
    const {
  if (!HasOffset())
    apply_motion_path = kExcludeMotionPath;
  bool apply_transform_origin =
      RequireTransformOrigin(apply_origin, apply_motion_path);

  float origin_x = 0;
  float origin_y = 0;
  float origin_z = 0;

  const FloatSize& box_size = bounding_box.Size();
  if (apply_transform_origin ||
      // We need to calculate originX and originY for applying motion path.
      apply_motion_path == kIncludeMotionPath) {
    origin_x = FloatValueForLength(TransformOriginX(), box_size.Width()) +
               bounding_box.X();
    origin_y = FloatValueForLength(TransformOriginY(), box_size.Height()) +
               bounding_box.Y();
    if (apply_transform_origin) {
      origin_z = TransformOriginZ();
      result.Translate3d(origin_x, origin_y, origin_z);
    }
  }

  if (apply_independent_transform_properties ==
      kIncludeIndependentTransformProperties) {
    if (Translate())
      Translate()->Apply(result, box_size);

    if (Rotate())
      Rotate()->Apply(result, box_size);

    if (Scale())
      Scale()->Apply(result, box_size);
  }

  if (apply_motion_path == kIncludeMotionPath)
    ApplyMotionPathTransform(origin_x, origin_y, bounding_box, result);

  for (const auto& operation : Transform().Operations())
    operation->Apply(result, box_size);

  if (apply_transform_origin) {
    result.Translate3d(-origin_x, -origin_y, -origin_z);
  }
}

bool ComputedStyle::HasFilters() const {
  return FilterInternal().Get() && !FilterInternal()->operations_.IsEmpty();
}

void ComputedStyle::ApplyMotionPathTransform(
    float origin_x,
    float origin_y,
    const FloatRect& bounding_box,
    TransformationMatrix& transform) const {
  // TODO(ericwilligers): crbug.com/638055 Apply offset-position.
  if (!OffsetPath()) {
    return;
  }
  const LengthPoint& position = OffsetPosition();
  const LengthPoint& anchor = OffsetAnchor();
  const Length& distance = OffsetDistance();
  const BasicShape* path = OffsetPath();
  const StyleOffsetRotation& rotate = OffsetRotate();

  FloatPoint point;
  float angle;
  if (path->GetType() == BasicShape::kStyleRayType) {
    // TODO(ericwilligers): crbug.com/641245 Support <size> for ray paths.
    float float_distance = FloatValueForLength(distance, 0);

    angle = To<StyleRay>(*path).Angle() - 90;
    point.SetX(float_distance * cos(deg2rad(angle)));
    point.SetY(float_distance * sin(deg2rad(angle)));
  } else {
    float zoom = EffectiveZoom();
    const StylePath& motion_path = To<StylePath>(*path);
    float path_length = motion_path.length();
    float float_distance =
        FloatValueForLength(distance, path_length * zoom) / zoom;
    float computed_distance;
    if (motion_path.IsClosed() && path_length > 0) {
      computed_distance = fmod(float_distance, path_length);
      if (computed_distance < 0)
        computed_distance += path_length;
    } else {
      computed_distance = clampTo<float>(float_distance, 0, path_length);
    }

    motion_path.GetPath().PointAndNormalAtLength(computed_distance, point,
                                                 angle);

    point.Scale(zoom, zoom);
  }

  if (rotate.type == OffsetRotationType::kFixed)
    angle = 0;

  float origin_shift_x = 0;
  float origin_shift_y = 0;
  // If the offset-position and offset-anchor properties are not yet enabled,
  // they will have the default value, auto.
  FloatPoint anchor_point(origin_x, origin_y);
  if (!position.X().IsAuto() || !anchor.X().IsAuto()) {
    anchor_point = FloatPointForLengthPoint(anchor, bounding_box.Size());
    anchor_point += bounding_box.Location();

    // Shift the origin from transform-origin to offset-anchor.
    origin_shift_x = anchor_point.X() - origin_x;
    origin_shift_y = anchor_point.Y() - origin_y;
  }

  transform.Translate(point.X() - anchor_point.X() + origin_shift_x,
                      point.Y() - anchor_point.Y() + origin_shift_y);
  transform.Rotate(angle + rotate.angle);

  if (!position.X().IsAuto() || !anchor.X().IsAuto())
    // Shift the origin back to transform-origin.
    transform.Translate(-origin_shift_x, -origin_shift_y);
}

bool ComputedStyle::TextShadowDataEquivalent(const ComputedStyle& other) const {
  return DataEquivalent(TextShadow(), other.TextShadow());
}

StyleImage* ComputedStyle::ListStyleImage() const {
  return ListStyleImageInternal();
}
void ComputedStyle::SetListStyleImage(StyleImage* v) {
  SetListStyleImageInternal(v);
}

Color ComputedStyle::GetColor() const {
  return ColorInternal();
}
void ComputedStyle::SetColor(const Color& v) {
  SetIsColorInternalText(false);
  SetColorInternal(v);
}
void ComputedStyle::ResolveInternalTextColor(const Color& v) {
  SetColorInternal(v);
}

static FloatRoundedRect::Radii CalcRadiiFor(const LengthSize& top_left,
                                            const LengthSize& top_right,
                                            const LengthSize& bottom_left,
                                            const LengthSize& bottom_right,
                                            FloatSize size) {
  return FloatRoundedRect::Radii(FloatSizeForLengthSize(top_left, size),
                                 FloatSizeForLengthSize(top_right, size),
                                 FloatSizeForLengthSize(bottom_left, size),
                                 FloatSizeForLengthSize(bottom_right, size));
}

FloatRoundedRect ComputedStyle::GetRoundedBorderFor(
    const LayoutRect& border_rect,
    bool include_logical_left_edge,
    bool include_logical_right_edge) const {
  FloatRoundedRect rounded_rect(PixelSnappedIntRect(border_rect));
  if (HasBorderRadius()) {
    FloatRoundedRect::Radii radii = CalcRadiiFor(
        BorderTopLeftRadius(), BorderTopRightRadius(), BorderBottomLeftRadius(),
        BorderBottomRightRadius(), FloatSize(border_rect.Size()));
    rounded_rect.IncludeLogicalEdges(radii, IsHorizontalWritingMode(),
                                     include_logical_left_edge,
                                     include_logical_right_edge);
    rounded_rect.ConstrainRadii();
  }
  return rounded_rect;
}

FloatRoundedRect ComputedStyle::GetRoundedInnerBorderFor(
    const LayoutRect& border_rect,
    bool include_logical_left_edge,
    bool include_logical_right_edge) const {
  bool horizontal = IsHorizontalWritingMode();

  int left_width = (!horizontal || include_logical_left_edge)
                       ? roundf(BorderLeftWidth())
                       : 0;
  int right_width = (!horizontal || include_logical_right_edge)
                        ? roundf(BorderRightWidth())
                        : 0;
  int top_width =
      (horizontal || include_logical_left_edge) ? roundf(BorderTopWidth()) : 0;
  int bottom_width = (horizontal || include_logical_right_edge)
                         ? roundf(BorderBottomWidth())
                         : 0;

  return GetRoundedInnerBorderFor(
      border_rect,
      LayoutRectOutsets(-top_width, -right_width, -bottom_width, -left_width),
      include_logical_left_edge, include_logical_right_edge);
}

FloatRoundedRect ComputedStyle::GetRoundedInnerBorderFor(
    const LayoutRect& border_rect,
    const LayoutRectOutsets& insets,
    bool include_logical_left_edge,
    bool include_logical_right_edge) const {
  LayoutRect inner_rect(border_rect);
  inner_rect.Expand(insets);
  LayoutSize inner_rect_size = inner_rect.Size();
  inner_rect_size.ClampNegativeToZero();
  inner_rect.SetSize(inner_rect_size);

  FloatRoundedRect rounded_rect(PixelSnappedIntRect(inner_rect));

  if (HasBorderRadius()) {
    FloatRoundedRect::Radii radii = GetRoundedBorderFor(border_rect).GetRadii();
    // Insets use negative values.
    radii.Shrink(-insets.Top().ToFloat(), -insets.Bottom().ToFloat(),
                 -insets.Left().ToFloat(), -insets.Right().ToFloat());
    rounded_rect.IncludeLogicalEdges(radii, IsHorizontalWritingMode(),
                                     include_logical_left_edge,
                                     include_logical_right_edge);
  }
  return rounded_rect;
}

bool ComputedStyle::CanRenderBorderImage() const {
  if (!HasBorderDecoration())
    return false;

  StyleImage* border_image = BorderImage().GetImage();
  return border_image && border_image->CanRender() && border_image->IsLoaded();
}

const CounterDirectiveMap* ComputedStyle::GetCounterDirectives() const {
  return CounterDirectivesInternal().get();
}

CounterDirectiveMap& ComputedStyle::AccessCounterDirectives() {
  std::unique_ptr<CounterDirectiveMap>& map =
      MutableCounterDirectivesInternal();
  if (!map)
    map = std::make_unique<CounterDirectiveMap>();
  return *map;
}

const CounterDirectives ComputedStyle::GetCounterDirectives(
    const AtomicString& identifier) const {
  if (const CounterDirectiveMap* directives = GetCounterDirectives())
    return directives->at(identifier);
  return CounterDirectives();
}

void ComputedStyle::ClearIncrementDirectives() {
  if (!GetCounterDirectives())
    return;

  // This makes us copy even if we may not be removing any items.
  CounterDirectiveMap& map = AccessCounterDirectives();
  typedef CounterDirectiveMap::iterator Iterator;

  Iterator end = map.end();
  for (Iterator it = map.begin(); it != end; ++it)
    it->value.ClearIncrement();
}

void ComputedStyle::ClearResetDirectives() {
  if (!GetCounterDirectives())
    return;

  // This makes us copy even if we may not be removing any items.
  CounterDirectiveMap& map = AccessCounterDirectives();
  typedef CounterDirectiveMap::iterator Iterator;

  Iterator end = map.end();
  for (Iterator it = map.begin(); it != end; ++it)
    it->value.ClearReset();
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
  if (const LayoutLocale* locale = GetFontDescription().Locale())
    return locale->LocaleWithBreakKeyword(mode);
  return Locale();
}

Hyphenation* ComputedStyle::GetHyphenation() const {
  return GetHyphens() == Hyphens::kAuto
             ? GetFontDescription().LocaleOrDefault().GetHyphenation()
             : nullptr;
}

const AtomicString& ComputedStyle::HyphenString() const {
  const AtomicString& hyphenation_string = HyphenationString();
  if (!hyphenation_string.IsNull())
    return hyphenation_string;

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
  if (!is_last_line)
    return GetTextAlign();

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
      if (text_align == ETextAlign::kJustify)
        return ETextAlign::kStart;
      return text_align;
  }
  NOTREACHED();
  return GetTextAlign();
}

bool ComputedStyle::ShouldUseTextIndent(bool is_first_line,
                                        bool is_after_forced_break) const {
  bool should_use =
      is_first_line || (is_after_forced_break &&
                        GetTextIndentLine() != TextIndentLine::kFirstLine);
  return GetTextIndentType() == TextIndentType::kNormal ? should_use
                                                        : !should_use;
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
  if (text.IsNull() || text.Is8Bit())
    return text;
  unsigned length = text.length();
  const StringImpl& input = *(text.Impl());
  StringBuilder result;
  result.ReserveCapacity(length);
  // |input| must be well-formed UTF-16 so that there's no worry
  // about surrogate handling.
  for (unsigned i = 0; i < length; ++i) {
    UChar character = input[i];
    if (Character::IsModernGeorgianUppercase(character))
      result.Append(Character::LowercaseModernGeorgianUppercase(character));
    else
      result.Append(character);
  }
  return result.ToString();
}

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
    return position == TextEmphasisPosition::kOverRight ||
                   position == TextEmphasisPosition::kOverLeft
               ? LineLogicalSide::kOver
               : LineLogicalSide::kUnder;
  }
  if (GetWritingMode() != WritingMode::kSidewaysLr) {
    return position == TextEmphasisPosition::kOverRight ||
                   position == TextEmphasisPosition::kUnderRight
               ? LineLogicalSide::kOver
               : LineLogicalSide::kUnder;
  }
  return position == TextEmphasisPosition::kOverLeft ||
                 position == TextEmphasisPosition::kUnderLeft
             ? LineLogicalSide::kOver
             : LineLogicalSide::kUnder;
}

CSSAnimationData& ComputedStyle::AccessAnimations() {
  if (!AnimationsInternal())
    SetAnimationsInternal(std::make_unique<CSSAnimationData>());
  return *AnimationsInternal();
}

CSSTransitionData& ComputedStyle::AccessTransitions() {
  if (!TransitionsInternal())
    SetTransitionsInternal(std::make_unique<CSSTransitionData>());
  return *TransitionsInternal();
}

FontBaseline ComputedStyle::GetFontBaseline() const {
  // TODO(kojii): Incorporate 'dominant-baseline' when we support it.
  // https://www.w3.org/TR/css-inline-3/#dominant-baseline-property

  // Vertical flow (except 'text-orientation: sideways') uses ideographic
  // baseline. https://drafts.csswg.org/css-writing-modes-3/#intro-baselines
  return !GetFontDescription().IsVerticalAnyUpright() ? kAlphabeticBaseline
                                                      : kIdeographicBaseline;
}

FontOrientation ComputedStyle::ComputeFontOrientation() const {
  if (IsHorizontalWritingMode())
    return FontOrientation::kHorizontal;

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

void ComputedStyle::UpdateFontOrientation() {
  FontOrientation orientation = ComputeFontOrientation();
  if (GetFontDescription().Orientation() == orientation)
    return;
  FontSelector* current_font_selector = GetFont().GetFontSelector();
  FontDescription font_description = GetFontDescription();
  font_description.SetOrientation(orientation);
  SetFontDescription(font_description);
  GetFont().Update(current_font_selector);
}

TextDecoration ComputedStyle::TextDecorationsInEffect() const {
  if (HasSimpleUnderlineInternal())
    return TextDecoration::kUnderline;
  if (!AppliedTextDecorationsInternal())
    return TextDecoration::kNone;

  TextDecoration decorations = TextDecoration::kNone;

  const Vector<AppliedTextDecoration>& applied = AppliedTextDecorations();

  for (const AppliedTextDecoration& decoration : applied)
    decorations |= decoration.Lines();

  return decorations;
}

const Vector<AppliedTextDecoration>& ComputedStyle::AppliedTextDecorations()
    const {
  if (HasSimpleUnderlineInternal()) {
    DEFINE_STATIC_LOCAL(
        Vector<AppliedTextDecoration>, underline,
        (1, AppliedTextDecoration(
                TextDecoration::kUnderline, ETextDecorationStyle::kSolid,
                VisitedDependentColor(GetCSSPropertyTextDecorationColor()))));
    // Since we only have one of these in memory, just update the color before
    // returning.
    underline.at(0).SetColor(
        VisitedDependentColor(GetCSSPropertyTextDecorationColor()));
    return underline;
  }
  if (!AppliedTextDecorationsInternal()) {
    DEFINE_STATIC_LOCAL(Vector<AppliedTextDecoration>, empty, ());
    return empty;
  }

  return AppliedTextDecorationsInternal()->data;
}

static bool HasInitialVariables(const StyleInitialData* initial_data) {
  return initial_data && initial_data->HasInitialVariables();
}

bool ComputedStyle::HasVariables() const {
  return InheritedVariables() || NonInheritedVariables() ||
         HasInitialVariables(InitialDataInternal().get());
}

StyleInheritedVariables* ComputedStyle::InheritedVariables() const {
  return InheritedVariablesInternal().get();
}

StyleNonInheritedVariables* ComputedStyle::NonInheritedVariables() const {
  return NonInheritedVariablesInternal().get();
}

StyleInheritedVariables& ComputedStyle::MutableInheritedVariables() {
  scoped_refptr<StyleInheritedVariables>& variables =
      MutableInheritedVariablesInternal();
  if (!variables)
    variables = StyleInheritedVariables::Create();
  else if (!variables->HasOneRef())
    variables = variables->Copy();
  return *variables;
}

StyleNonInheritedVariables& ComputedStyle::MutableNonInheritedVariables() {
  std::unique_ptr<StyleNonInheritedVariables>& variables =
      MutableNonInheritedVariablesInternal();
  if (!variables)
    variables = std::make_unique<StyleNonInheritedVariables>();
  return *variables;
}

void ComputedStyle::SetInitialData(scoped_refptr<StyleInitialData> data) {
  MutableInitialDataInternal() = std::move(data);
}

void ComputedStyle::SetVariableData(const AtomicString& name,
                                    scoped_refptr<CSSVariableData> value,
                                    bool is_inherited_property) {
  if (is_inherited_property)
    MutableInheritedVariables().SetData(name, std::move(value));
  else
    MutableNonInheritedVariables().SetData(name, std::move(value));
}

void ComputedStyle::SetVariableValue(const AtomicString& name,
                                     const CSSValue* value,
                                     bool is_inherited_property) {
  if (is_inherited_property)
    MutableInheritedVariables().SetValue(name, value);
  else
    MutableNonInheritedVariables().SetValue(name, value);
}

static CSSVariableData* GetInitialVariableData(
    const AtomicString& name,
    const StyleInitialData* initial_data) {
  if (!initial_data)
    return nullptr;
  return initial_data->GetVariableData(name);
}

CSSVariableData* ComputedStyle::GetVariableData(
    const AtomicString& name) const {
  if (InheritedVariables()) {
    if (auto data = InheritedVariables()->GetData(name))
      return *data;
  }
  if (NonInheritedVariables()) {
    if (auto data = NonInheritedVariables()->GetData(name))
      return *data;
  }
  return GetInitialVariableData(name, InitialDataInternal().get());
}

CSSVariableData* ComputedStyle::GetVariableData(
    const AtomicString& name,
    bool is_inherited_property) const {
  if (is_inherited_property) {
    if (InheritedVariables()) {
      if (auto data = InheritedVariables()->GetData(name))
        return *data;
    }
  } else {
    if (NonInheritedVariables()) {
      if (auto data = NonInheritedVariables()->GetData(name))
        return *data;
    }
  }
  return GetInitialVariableData(name, InitialDataInternal().get());
}

static const CSSValue* GetInitialVariableValue(
    const AtomicString& name,
    const StyleInitialData* initial_data) {
  if (!initial_data)
    return nullptr;
  return initial_data->GetVariableValue(name);
}

const CSSValue* ComputedStyle::GetVariableValue(
    const AtomicString& name) const {
  if (InheritedVariables()) {
    if (auto value = InheritedVariables()->GetValue(name))
      return *value;
  }
  if (NonInheritedVariables()) {
    if (auto value = NonInheritedVariables()->GetValue(name))
      return *value;
  }
  return GetInitialVariableValue(name, InitialDataInternal().get());
}

const CSSValue* ComputedStyle::GetVariableValue(
    const AtomicString& name,
    bool is_inherited_property) const {
  if (is_inherited_property) {
    if (InheritedVariables()) {
      if (auto value = InheritedVariables()->GetValue(name))
        return *value;
    }
  } else {
    if (NonInheritedVariables()) {
      if (auto value = NonInheritedVariables()->GetValue(name))
        return *value;
    }
  }
  return GetInitialVariableValue(name, InitialDataInternal().get());
}

bool ComputedStyle::SetFontDescription(const FontDescription& v) {
  if (FontInternal().GetFontDescription() != v) {
    SetFontInternal(Font(v));
    return true;
  }
  return false;
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

int ComputedStyle::ComputedLineHeight() const {
  const Length& lh = LineHeight();

  // Negative value means the line height is not set. Use the font's built-in
  // spacing, if avalible.
  if (lh.IsNegative() && GetFont().PrimaryFont())
    return GetFont().PrimaryFont()->GetFontMetrics().LineSpacing();

  if (lh.IsPercentOrCalc())
    return MinimumValueForLength(lh, LayoutUnit(ComputedFontSize())).ToInt();

  return std::min(lh.Value(), LayoutUnit::Max().ToFloat());
}

LayoutUnit ComputedStyle::ComputedLineHeightAsFixed() const {
  const Length& lh = LineHeight();

  // Negative value means the line height is not set. Use the font's built-in
  // spacing, if avalible.
  if (lh.IsNegative() && GetFont().PrimaryFont())
    return GetFont().PrimaryFont()->GetFontMetrics().FixedLineSpacing();

  if (lh.IsPercentOrCalc()) {
    return LayoutUnit(
        MinimumValueForLength(lh, ComputedFontSizeAsFixed()).ToInt());
  }

  return LayoutUnit(floorf(lh.Value()));
}

void ComputedStyle::SetWordSpacing(float word_spacing) {
  FontSelector* current_font_selector = GetFont().GetFontSelector();
  FontDescription desc(GetFontDescription());
  desc.SetWordSpacing(word_spacing);
  SetFontDescription(desc);
  GetFont().Update(current_font_selector);
}

void ComputedStyle::SetLetterSpacing(float letter_spacing) {
  FontSelector* current_font_selector = GetFont().GetFontSelector();
  FontDescription desc(GetFontDescription());
  desc.SetLetterSpacing(letter_spacing);
  SetFontDescription(desc);
  GetFont().Update(current_font_selector);
}

void ComputedStyle::SetTextAutosizingMultiplier(float multiplier) {
  SetTextAutosizingMultiplierInternal(multiplier);

  float size = SpecifiedFontSize();

  DCHECK(std::isfinite(size));
  if (!std::isfinite(size) || size < 0)
    size = 0;
  else
    size = std::min(kMaximumAllowedFontSize, size);

  FontSelector* current_font_selector = GetFont().GetFontSelector();
  FontDescription desc(GetFontDescription());
  desc.SetSpecifiedSize(size);

  float computed_size = size * EffectiveZoom();

  float autosized_font_size = TextAutosizer::ComputeAutosizedFontSize(
      computed_size, multiplier, EffectiveZoom());
  desc.SetComputedSize(std::min(kMaximumAllowedFontSize, autosized_font_size));

  SetFontDescription(desc);
  GetFont().Update(current_font_selector);
}

void ComputedStyle::AddAppliedTextDecoration(
    const AppliedTextDecoration& decoration) {
  scoped_refptr<AppliedTextDecorationList>& list =
      MutableAppliedTextDecorationsInternal();

  if (!list)
    list = base::MakeRefCounted<AppliedTextDecorationList>();
  else if (!list->HasOneRef())
    list = base::MakeRefCounted<AppliedTextDecorationList>(list->data);

  list->data.push_back(decoration);
}

void ComputedStyle::OverrideTextDecorationColors(Color override_color) {
  scoped_refptr<AppliedTextDecorationList>& list =
      MutableAppliedTextDecorationsInternal();
  DCHECK(list);
  if (!list->HasOneRef())
    list = base::MakeRefCounted<AppliedTextDecorationList>(list->data);

  for (AppliedTextDecoration& decoration : list->data)
    decoration.SetColor(override_color);
}

void ComputedStyle::ApplyTextDecorations(
    const Color& parent_text_decoration_color,
    bool override_existing_colors) {
  if (GetTextDecoration() == TextDecoration::kNone &&
      !HasSimpleUnderlineInternal() && !AppliedTextDecorationsInternal())
    return;

  // If there are any color changes or decorations set by this element, stop
  // using m_hasSimpleUnderline.
  Color current_text_decoration_color =
      VisitedDependentColor(GetCSSPropertyTextDecorationColor());
  if (HasSimpleUnderlineInternal() &&
      (GetTextDecoration() != TextDecoration::kNone ||
       current_text_decoration_color != parent_text_decoration_color)) {
    SetHasSimpleUnderlineInternal(false);
    AddAppliedTextDecoration(AppliedTextDecoration(
        TextDecoration::kUnderline, ETextDecorationStyle::kSolid,
        parent_text_decoration_color));
  }
  if (override_existing_colors && AppliedTextDecorationsInternal())
    OverrideTextDecorationColors(current_text_decoration_color);
  if (GetTextDecoration() == TextDecoration::kNone)
    return;
  DCHECK(!HasSimpleUnderlineInternal());
  // To save memory, we don't use AppliedTextDecoration objects in the common
  // case of a single simple underline of currentColor.
  TextDecoration decoration_lines = GetTextDecoration();
  ETextDecorationStyle decoration_style = TextDecorationStyle();
  bool is_simple_underline = decoration_lines == TextDecoration::kUnderline &&
                             decoration_style == ETextDecorationStyle::kSolid &&
                             TextDecorationColor().IsCurrentColor();
  if (is_simple_underline && !AppliedTextDecorationsInternal()) {
    SetHasSimpleUnderlineInternal(true);
    return;
  }

  AddAppliedTextDecoration(AppliedTextDecoration(
      decoration_lines, decoration_style, current_text_decoration_color));
}

void ComputedStyle::ClearAppliedTextDecorations() {
  SetHasSimpleUnderlineInternal(false);

  if (AppliedTextDecorationsInternal())
    SetAppliedTextDecorationsInternal(nullptr);
}

void ComputedStyle::RestoreParentTextDecorations(
    const ComputedStyle& parent_style) {
  SetHasSimpleUnderlineInternal(parent_style.HasSimpleUnderlineInternal());
  if (AppliedTextDecorationsInternal() !=
      parent_style.AppliedTextDecorationsInternal()) {
    SetAppliedTextDecorationsInternal(scoped_refptr<AppliedTextDecorationList>(
        parent_style.AppliedTextDecorationsInternal()));
  }
}

void ComputedStyle::ClearMultiCol() {
  SetColumnGap(ComputedStyleInitialValues::InitialColumnGap());
  SetColumnWidthInternal(ComputedStyleInitialValues::InitialColumnWidth());
  SetColumnRuleStyle(ComputedStyleInitialValues::InitialColumnRuleStyle());
  SetColumnRuleWidthInternal(
      LayoutUnit(ComputedStyleInitialValues::InitialColumnRuleWidth()));
  SetColumnRuleColorInternal(
      ComputedStyleInitialValues::InitialColumnRuleColor());
  SetColumnRuleColorIsCurrentColor(
      ComputedStyleInitialValues::InitialColumnRuleColorIsCurrentColor());
  SetInternalVisitedColumnRuleColorInternal(
      ComputedStyleInitialValues::InitialInternalVisitedColumnRuleColor());
  SetColumnCountInternal(ComputedStyleInitialValues::InitialColumnCount());
  SetHasAutoColumnCountInternal(
      ComputedStyleInitialValues::InitialHasAutoColumnCount());
  SetHasAutoColumnWidthInternal(
      ComputedStyleInitialValues::InitialHasAutoColumnWidth());
  ResetColumnFill();
  ResetColumnSpan();
}

StyleColor ComputedStyle::DecorationColorIncludingFallback(
    bool visited_link) const {
  StyleColor style_color = visited_link ? InternalVisitedTextDecorationColor()
                                        : TextDecorationColor();

  if (!style_color.IsCurrentColor())
    return style_color;

  if (TextStrokeWidth()) {
    // Prefer stroke color if possible, but not if it's fully transparent.
    StyleColor text_stroke_style_color =
        visited_link ? InternalVisitedTextStrokeColor() : TextStrokeColor();
    if (!text_stroke_style_color.IsCurrentColor() &&
        text_stroke_style_color.GetColor().Alpha())
      return text_stroke_style_color;
  }

  return visited_link ? InternalVisitedTextFillColor() : TextFillColor();
}

Color ComputedStyle::VisitedDependentColor(
    const CSSProperty& color_property) const {
  DCHECK(!color_property.IsVisited());

  Color unvisited_color =
      To<Longhand>(color_property).ColorIncludingFallback(false, *this);
  if (InsideLink() != EInsideLink::kInsideVisitedLink)
    return unvisited_color;

  // Properties that provide a GetVisitedProperty() must use the
  // ColorIncludingFallback function on that property.
  //
  // TODO(andruud): Simplify this when all properties support
  // GetVisitedProperty.
  const CSSProperty* visited_property = &color_property;
  if (const CSSProperty* visited = color_property.GetVisitedProperty())
    visited_property = visited;

  Color visited_color =
      To<Longhand>(*visited_property).ColorIncludingFallback(true, *this);

  // Take the alpha from the unvisited color, but get the RGB values from the
  // visited color.
  return Color(visited_color.Red(), visited_color.Green(), visited_color.Blue(),
               unvisited_color.Alpha());
}

void ComputedStyle::SetMarginStart(const Length& margin) {
  if (IsHorizontalWritingMode()) {
    if (IsLeftToRightDirection())
      SetMarginLeft(margin);
    else
      SetMarginRight(margin);
  } else {
    if (IsLeftToRightDirection())
      SetMarginTop(margin);
    else
      SetMarginBottom(margin);
  }
}

void ComputedStyle::SetMarginEnd(const Length& margin) {
  if (IsHorizontalWritingMode()) {
    if (IsLeftToRightDirection())
      SetMarginRight(margin);
    else
      SetMarginLeft(margin);
  } else {
    if (IsLeftToRightDirection())
      SetMarginBottom(margin);
    else
      SetMarginTop(margin);
  }
}

int ComputedStyle::OutlineOutsetExtent() const {
  if (!HasOutline())
    return 0;
  if (OutlineStyleIsAuto()) {
    return GraphicsContext::FocusRingOutsetExtent(
        OutlineOffset(), std::ceil(GetOutlineStrokeWidthForFocusRing()),
        LayoutTheme::GetTheme().IsFocusRingOutset());
  }
  return base::ClampAdd(OutlineWidth(), OutlineOffset()).Max(0);
}

float ComputedStyle::GetOutlineStrokeWidthForFocusRing() const {
#if defined(OS_MACOSX)
  return OutlineWidth();
#else
  if (LayoutTheme::GetTheme().IsFocusRingOutset()) {
    return OutlineWidth();
  }
  // Draw an outline with thickness in proportion to the zoom level, but never
  // so narrow that it becomes invisible.
  return std::max(EffectiveZoom(), 1.f);
#endif
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
  if (mark != TextEmphasisMark::kAuto)
    return mark;

  if (IsHorizontalWritingMode())
    return TextEmphasisMark::kDot;

  return TextEmphasisMark::kSesame;
}

LayoutRectOutsets ComputedStyle::ImageOutsets(
    const NinePieceImage& image) const {
  return LayoutRectOutsets(
      NinePieceImage::ComputeOutset(image.Outset().Top(), BorderTopWidth()),
      NinePieceImage::ComputeOutset(image.Outset().Right(), BorderRightWidth()),
      NinePieceImage::ComputeOutset(image.Outset().Bottom(),
                                    BorderBottomWidth()),
      NinePieceImage::ComputeOutset(image.Outset().Left(), BorderLeftWidth()));
}

void ComputedStyle::SetBorderImageSource(StyleImage* image) {
  if (BorderImage().GetImage() == image)
    return;
  MutableBorderImageInternal().SetImage(image);
}

void ComputedStyle::SetBorderImageSlices(const LengthBox& slices) {
  if (BorderImage().ImageSlices() == slices)
    return;
  MutableBorderImageInternal().SetImageSlices(slices);
}

void ComputedStyle::SetBorderImageSlicesFill(bool fill) {
  if (BorderImage().Fill() == fill)
    return;
  MutableBorderImageInternal().SetFill(fill);
}

void ComputedStyle::SetBorderImageWidth(const BorderImageLengthBox& slices) {
  if (BorderImage().BorderSlices() == slices)
    return;
  MutableBorderImageInternal().SetBorderSlices(slices);
}

void ComputedStyle::SetBorderImageOutset(const BorderImageLengthBox& outset) {
  if (BorderImage().Outset() == outset)
    return;
  MutableBorderImageInternal().SetOutset(outset);
}

bool ComputedStyle::BorderObscuresBackground() const {
  if (!HasBorder())
    return false;

  // Bail if we have any border-image for now. We could look at the image alpha
  // to improve this.
  if (BorderImage().GetImage())
    return false;

  BorderEdge edges[4];
  GetBorderEdgeInfo(edges);

  for (unsigned int i = static_cast<unsigned>(BoxSide::kTop);
       i <= static_cast<unsigned>(BoxSide::kLeft); ++i) {
    const BorderEdge& curr_edge = edges[i];
    if (!curr_edge.ObscuresBackground())
      return false;
  }

  return true;
}

LayoutRectOutsets ComputedStyle::BoxDecorationOutsets() const {
  DCHECK(HasVisualOverflowingEffect());
  LayoutRectOutsets outsets;

  if (const ShadowList* box_shadow = BoxShadow())
    outsets = LayoutRectOutsets(box_shadow->RectOutsetsIncludingOriginal());

  if (HasBorderImageOutsets())
    outsets.Unite(BorderImageOutsets());

  if (HasMaskBoxImageOutsets())
    outsets.Unite(MaskBoxImageOutsets());

  return outsets;
}

void ComputedStyle::GetBorderEdgeInfo(BorderEdge edges[],
                                      bool include_logical_left_edge,
                                      bool include_logical_right_edge) const {
  bool horizontal = IsHorizontalWritingMode();

  edges[static_cast<unsigned>(BoxSide::kTop)] = BorderEdge(
      BorderTopWidth(), VisitedDependentColor(GetCSSPropertyBorderTopColor()),
      BorderTopStyle(), horizontal || include_logical_left_edge);

  edges[static_cast<unsigned>(BoxSide::kRight)] =
      BorderEdge(BorderRightWidth(),
                 VisitedDependentColor(GetCSSPropertyBorderRightColor()),
                 BorderRightStyle(), !horizontal || include_logical_right_edge);

  edges[static_cast<unsigned>(BoxSide::kBottom)] =
      BorderEdge(BorderBottomWidth(),
                 VisitedDependentColor(GetCSSPropertyBorderBottomColor()),
                 BorderBottomStyle(), horizontal || include_logical_right_edge);

  edges[static_cast<unsigned>(BoxSide::kLeft)] = BorderEdge(
      BorderLeftWidth(), VisitedDependentColor(GetCSSPropertyBorderLeftColor()),
      BorderLeftStyle(), !horizontal || include_logical_left_edge);
}

void ComputedStyle::CopyChildDependentFlagsFrom(const ComputedStyle& other) {
  if (other.HasExplicitlyInheritedProperties())
    SetHasExplicitlyInheritedProperties();
}

bool ComputedStyle::ShadowListHasCurrentColor(const ShadowList* shadow_list) {
  if (!shadow_list)
    return false;
  return std::any_of(shadow_list->Shadows().begin(),
                     shadow_list->Shadows().end(),
                     [](const ShadowData& shadow) {
                       return shadow.GetColor().IsCurrentColor();
                     });
}

STATIC_ASSERT_ENUM(cc::OverscrollBehavior::kOverscrollBehaviorTypeAuto,
                   EOverscrollBehavior::kAuto);
STATIC_ASSERT_ENUM(cc::OverscrollBehavior::kOverscrollBehaviorTypeContain,
                   EOverscrollBehavior::kContain);
STATIC_ASSERT_ENUM(cc::OverscrollBehavior::kOverscrollBehaviorTypeNone,
                   EOverscrollBehavior::kNone);

}  // namespace blink
