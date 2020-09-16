/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COMPUTED_STYLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COMPUTED_STYLE_H_

#include <memory>
#include "base/util/type_safety/pass_key.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/style_auto_color.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/layout/geometry/box_sides.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/style/border_value.h"
#include "third_party/blink/renderer/core/style/computed_style_base.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/computed_style_initial_values.h"
#include "third_party/blink/renderer/core/style/cursor_list.h"
#include "third_party/blink/renderer/core/style/data_ref.h"
#include "third_party/blink/renderer/core/style/svg_computed_style.h"
#include "third_party/blink/renderer/core/style/transform_origin.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect_outsets.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/geometry/length_box.h"
#include "third_party/blink/renderer/platform/geometry/length_point.h"
#include "third_party/blink/renderer/platform/geometry/length_size.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_direction_mode.h"
#include "third_party/blink/renderer/platform/text/writing_mode_utils.h"
#include "third_party/blink/renderer/platform/transforms/transform_operations.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/leak_annotations.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

using std::max;

class AppliedTextDecoration;
struct BorderEdge;
class ContentData;
class CounterDirectives;
class CSSAnimationData;
class FloodColor;
class CSSTransitionData;
class CSSVariableData;
class FilterOperations;
class Font;
class Hyphenation;
class LayoutTheme;
class NinePieceImage;
class ShadowList;
class ShapeValue;
class StyleAdjuster;
class StyleContentAlignmentData;
class StyleDifference;
class StyleImage;
class StyleInheritedVariables;
class StyleInitialData;
class StylePath;
class StyleResolver;
class StyleSelfAlignmentData;
class TransformationMatrix;

typedef Vector<scoped_refptr<const ComputedStyle>, 4> PseudoElementStyleCache;

namespace css_longhand {

class Appearance;
class BackgroundColor;
class BorderBottomColor;
class BorderLeftColor;
class BorderRightColor;
class BorderTopColor;
class CaretColor;
class Clear;
class Color;
class ColumnRuleColor;
class Fill;
class Float;
class FloodColor;
class InternalVisitedBackgroundColor;
class InternalVisitedBorderBottomColor;
class InternalVisitedBorderLeftColor;
class InternalVisitedBorderRightColor;
class InternalVisitedBorderTopColor;
class InternalVisitedCaretColor;
class InternalVisitedColor;
class InternalVisitedColumnRuleColor;
class InternalVisitedOutlineColor;
class InternalVisitedTextDecorationColor;
class InternalVisitedTextEmphasisColor;
class InternalVisitedTextFillColor;
class InternalVisitedTextStrokeColor;
class LightingColor;
class OutlineColor;
class Resize;
class StopColor;
class Stroke;
class TextDecorationColor;
class WebkitTapHighlightColor;
class WebkitTextEmphasisColor;
class WebkitTextFillColor;
class WebkitTextStrokeColor;

}  // namespace css_longhand

// ComputedStyle stores the computed value [1] for every CSS property on an
// element and provides the interface between the style engine and the rest of
// Blink. It acts as a container where the computed value of every CSS property
// can be stored and retrieved:
//
//   auto style = ComputedStyle::Create();
//   style->SetDisplay(EDisplay::kNone); //'display' keyword property
//   style->Display();
//
// In addition to storing the computed value of every CSS property,
// ComputedStyle also contains various internal style information. Examples
// include cached_pseudo_element_styles_ (for storing pseudo element styles) and
// has_simple_underline_ (cached indicator flag of text-decoration). These are
// stored on ComputedStyle for two reasons:
//
//  1) They share the same lifetime as ComputedStyle, so it is convenient to
//  store them in the same object rather than a separate object that have to be
//  passed around as well.
//
//  2) Many of these data members can be packed as bit fields, so we use less
//  memory by packing them in this object with other bit fields.
//
// STORAGE:
//
// ComputedStyle is optimized for memory and performance. The data is not
// actually stored directly in ComputedStyle, but rather in a generated parent
// class ComputedStyleBase. This separation of concerns allows us to optimise
// the memory layout without affecting users of ComputedStyle. ComputedStyle
// inherits from ComputedStyleBase. For more about the memory layout, there is
// documentation in ComputedStyleBase and make_computed_style_base.py.
//
// INTERFACE:
//
// For most CSS properties, ComputedStyle provides a consistent interface which
// includes a getter, setter, and resetter (that resets the computed value to
// its initial value). Exceptions include vertical-align, which has a separate
// set of accessors for its length and its keyword components. Apart from
// accessors, ComputedStyle also has a wealth of helper functions.
//
// Because ComputedStyleBase defines simple accessors to every CSS property,
// ComputedStyle inherits these and so they are not redeclared in this file.
// This means that the interface to ComputedStyle is split between this file and
// ComputedStyleBase.h.
//
// [1] https://developer.mozilla.org/en-US/docs/Web/CSS/computed_value
//
// NOTE:
//
// Currently, some properties are stored in ComputedStyle and some in
// ComputedStyleBase. Eventually, the storage of all properties (except SVG
// ones) will be in ComputedStyleBase.
//
// Since this class is huge, do not mark all of it CORE_EXPORT.  Instead,
// export only the methods you need below.
class ComputedStyle : public ComputedStyleBase,
                      public RefCounted<ComputedStyle> {
  // Needed to allow access to private/protected getters of fields to allow diff
  // generation
  friend class ComputedStyleBase;
  // Used by CSS animations. We can't allow them to animate based off visited
  // colors.
  friend class CSSPropertyEquality;

  // Accesses GetColor().
  friend class ComputedStyleUtils;
  // These get visited and unvisited colors separately.
  friend class css_longhand::BackgroundColor;
  friend class css_longhand::BorderBottomColor;
  friend class css_longhand::BorderLeftColor;
  friend class css_longhand::BorderRightColor;
  friend class css_longhand::BorderTopColor;
  friend class css_longhand::CaretColor;
  friend class css_longhand::Clear;
  friend class css_longhand::Color;
  friend class css_longhand::ColumnRuleColor;
  friend class css_longhand::Fill;
  friend class css_longhand::Float;
  friend class css_longhand::FloodColor;
  friend class css_longhand::InternalVisitedBackgroundColor;
  friend class css_longhand::InternalVisitedBorderBottomColor;
  friend class css_longhand::InternalVisitedBorderLeftColor;
  friend class css_longhand::InternalVisitedBorderRightColor;
  friend class css_longhand::InternalVisitedBorderTopColor;
  friend class css_longhand::InternalVisitedCaretColor;
  friend class css_longhand::InternalVisitedColor;
  friend class css_longhand::InternalVisitedColumnRuleColor;
  friend class css_longhand::InternalVisitedOutlineColor;
  friend class css_longhand::InternalVisitedTextDecorationColor;
  friend class css_longhand::InternalVisitedTextEmphasisColor;
  friend class css_longhand::InternalVisitedTextFillColor;
  friend class css_longhand::InternalVisitedTextStrokeColor;
  friend class css_longhand::LightingColor;
  friend class css_longhand::OutlineColor;
  friend class css_longhand::Resize;
  friend class css_longhand::StopColor;
  friend class css_longhand::Stroke;
  friend class css_longhand::TextDecorationColor;
  friend class css_longhand::WebkitTapHighlightColor;
  friend class css_longhand::WebkitTextEmphasisColor;
  friend class css_longhand::WebkitTextFillColor;
  friend class css_longhand::WebkitTextStrokeColor;
  // Access to private Appearance() and HasAppearance().
  friend class LayoutTheme;
  friend class StyleAdjuster;
  friend class StyleCascade;
  friend class css_longhand::Appearance;
  // Editing has to only reveal unvisited info.
  friend class ApplyStyleCommand;
  // Editing has to only reveal unvisited info.
  friend class EditingStyle;
  // Needs to be able to see visited and unvisited colors for devtools.
  friend class ComputedStyleCSSValueMapping;
  // Sets color styles
  friend class StyleBuilderFunctions;
  // Saves Border/Background information for later comparison.
  friend class CachedUAStyle;
  // Accesses visited and unvisited colors.
  friend class ColorPropertyFunctions;
  // Edits the background for media controls.
  friend class StyleAdjuster;
  // Access to private SetFontInternal().
  friend class FontBuilder;

  // FIXME: When we stop resolving currentColor at style time, these can be
  // removed.
  friend class CSSToStyleMap;
  friend class FilterOperationResolver;
  friend class StyleBuilderConverter;
  friend class StyleResolverState;
  friend class StyleResolver;

 protected:
  // This cache stores ComputedStyles for pseudo elements originating from this
  // ComputedStyle's element. Pseudo elements which are represented by
  // PseudoElement in DOM store the ComputedStyle on those elements, so this
  // cache is for:
  //
  // 1. Pseudo elements which do not generate a PseudoElement internally like
  //    ::first-line and ::selection.
  //
  // 2. Pseudo element style requested from getComputedStyle() where the element
  //    currently doesn't generate a PseudoElement. E.g.:
  //
  //    <style>
  //      #div::before { color: green /* no content property! */}
  //    </style>
  //    <div id=div></div>
  //    <script>
  //      getComputedStyle(div, "::before").color // still green.
  //    </script>
  mutable std::unique_ptr<PseudoElementStyleCache>
      cached_pseudo_element_styles_;

  DataRef<SVGComputedStyle> svg_style_;

 private:
  // TODO(sashab): Move these private members to the bottom of ComputedStyle.
  ALWAYS_INLINE ComputedStyle();
  ALWAYS_INLINE ComputedStyle(const ComputedStyle&);

  static scoped_refptr<ComputedStyle> CreateInitialStyle();
  // TODO(crbug.com/794841): Remove this. Initial style should not be mutable.
  CORE_EXPORT static ComputedStyle& MutableInitialStyle();

 public:
  using PassKey = util::PassKey<ComputedStyle>;

  ALWAYS_INLINE ComputedStyle(PassKey, const ComputedStyle&);
  ALWAYS_INLINE explicit ComputedStyle(PassKey);

  CORE_EXPORT static scoped_refptr<ComputedStyle> Create();
  static scoped_refptr<ComputedStyle> CreateAnonymousStyleWithDisplay(
      const ComputedStyle& parent_style,
      EDisplay);
  static scoped_refptr<ComputedStyle>
  CreateInheritedDisplayContentsStyleIfNeeded(
      const ComputedStyle& parent_style,
      const ComputedStyle& layout_parent_style);
  CORE_EXPORT static scoped_refptr<ComputedStyle> Clone(const ComputedStyle&);
  static const ComputedStyle& InitialStyle() { return MutableInitialStyle(); }
  static void InvalidateInitialStyle();

  // Find out how two ComputedStyles differ. Used for figuring out if style
  // recalc needs to propagate style changes down the tree. The constants are
  // listed in increasing severity. E.g. kInherited also means we need to update
  // pseudo elements (kPseudoElementStyle).
  enum class Difference {
    // The ComputedStyle objects have the same computed style. The might have
    // some different extra flags which means we still need to replace the old
    // with the new instance.
    kEqual,
    // Non-inherited properties differ which means we need to apply visual
    // difference changes to the layout tree through LayoutObject::SetStyle().
    kNonInherited,
    // Pseudo element style is different which means we have to update pseudo
    // element existence and computed style.
    kPseudoElementStyle,
    // Inherited properties are different which means we need to recalc style
    // for children. Only independent properties changed which means we can
    // inherit by cloning the exiting ComputedStyle for children an set modified
    // properties directly without re-matching rules.
    kIndependentInherited,
    // Inherited properties are different which means we need to recalc style
    // for children.
    kInherited,
    // Display type changes for flex/grid/custom layout affects computed style
    // adjustments for descendants. For instance flex/grid items are blockified
    // at computed style time and such items can be arbitrarily deep down the
    // flat tree in the presence of display:contents.
    kDisplayAffectingDescendantStyles,
  };
  CORE_EXPORT static Difference ComputeDifference(
      const ComputedStyle* old_style,
      const ComputedStyle* new_style);

  // Returns true if the ComputedStyle change requires a LayoutObject re-attach.
  static bool NeedsReattachLayoutTree(const Element& element,
                                      const ComputedStyle* old_style,
                                      const ComputedStyle* new_style);

  // Copies the values of any independent inherited properties from the parent
  // that are not explicitly set in this style.
  void PropagateIndependentInheritedProperties(
      const ComputedStyle& parent_style);

  ContentPosition ResolvedJustifyContentPosition(
      const StyleContentAlignmentData& normal_value_behavior) const;
  ContentDistributionType ResolvedJustifyContentDistribution(
      const StyleContentAlignmentData& normal_value_behavior) const;
  ContentPosition ResolvedAlignContentPosition(
      const StyleContentAlignmentData& normal_value_behavior) const;
  ContentDistributionType ResolvedAlignContentDistribution(
      const StyleContentAlignmentData& normal_value_behavior) const;
  StyleSelfAlignmentData ResolvedAlignItems(
      ItemPosition normal_value_behaviour) const;
  StyleSelfAlignmentData ResolvedAlignSelf(
      ItemPosition normal_value_behaviour,
      const ComputedStyle* parent_style = nullptr) const;
  StyleContentAlignmentData ResolvedAlignContent(
      const StyleContentAlignmentData& normal_behaviour) const;
  StyleSelfAlignmentData ResolvedJustifyItems(
      ItemPosition normal_value_behaviour) const;
  StyleSelfAlignmentData ResolvedJustifySelf(
      ItemPosition normal_value_behaviour,
      const ComputedStyle* parent_style = nullptr) const;
  StyleContentAlignmentData ResolvedJustifyContent(
      const StyleContentAlignmentData& normal_behaviour) const;

  CORE_EXPORT StyleDifference
  VisualInvalidationDiff(const Document&, const ComputedStyle&) const;

  CORE_EXPORT void InheritFrom(const ComputedStyle& inherit_parent,
                               IsAtShadowBoundary = kNotAtShadowBoundary);
  void CopyNonInheritedFromCached(const ComputedStyle&);

  PseudoId StyleType() const {
    return static_cast<PseudoId>(StyleTypeInternal());
  }
  void SetStyleType(PseudoId style_type) { SetStyleTypeInternal(style_type); }

  const ComputedStyle* GetCachedPseudoElementStyle(PseudoId) const;
  const ComputedStyle* AddCachedPseudoElementStyle(
      scoped_refptr<const ComputedStyle>) const;
  void ClearCachedPseudoElementStyles() const {
    if (cached_pseudo_element_styles_)
      cached_pseudo_element_styles_->clear();
  }

  /**
   * ComputedStyle properties
   *
   * Each property stored in ComputedStyle is made up of fields. Fields have
   * getters and setters. A field is preferably a basic data type or enum,
   * but can be any type. A set of fields should be preceded by the property
   * the field is stored for.
   *
   * Field method naming should be done like so:
   *   // name-of-property
   *   int nameOfProperty() const;
   *   void SetNameOfProperty(int);
   * If the property has multiple fields, add the field name to the end of the
   * method name.
   *
   * Avoid nested types by splitting up fields where possible, e.g.:
   *  int getBorderTopWidth();
   *  int getBorderBottomWidth();
   *  int getBorderLeftWidth();
   *  int getBorderRightWidth();
   * is preferable to:
   *  BorderWidths getBorderWidths();
   *
   * Utility functions should go in a separate section at the end of the
   * class, and be kept to a minimum.
   */

  const FilterOperations& BackdropFilter() const {
    DCHECK(BackdropFilterInternal().Get());
    return BackdropFilterInternal()->operations_;
  }
  FilterOperations& MutableBackdropFilter() {
    DCHECK(BackdropFilterInternal().Get());
    return MutableBackdropFilterInternal()->operations_;
  }
  // For containing blocks, use |HasNonInitialBackdropFilter()| which includes
  // will-change: backdrop-filter.
  bool HasBackdropFilter() const {
    DCHECK(BackdropFilterInternal().Get());
    return !BackdropFilterInternal()->operations_.Operations().IsEmpty();
  }
  void SetBackdropFilter(const FilterOperations& ops) {
    DCHECK(BackdropFilterInternal().Get());
    if (BackdropFilterInternal()->operations_ != ops)
      MutableBackdropFilterInternal()->operations_ = ops;
  }
  bool BackdropFilterDataEquivalent(const ComputedStyle& o) const {
    return DataEquivalent(BackdropFilterInternal(), o.BackdropFilterInternal());
  }

  // filter (aka -webkit-filter)
  FilterOperations& MutableFilter() {
    DCHECK(FilterInternal().Get());
    return MutableFilterInternal()->operations_;
  }
  const FilterOperations& Filter() const {
    DCHECK(FilterInternal().Get());
    return FilterInternal()->operations_;
  }
  // For containing blocks, use |HasNonInitialFilter()| which includes
  // will-change: filter.
  bool HasFilter() const {
    DCHECK(FilterInternal().Get());
    return !FilterInternal()->operations_.Operations().IsEmpty();
  }
  void SetFilter(const FilterOperations& v) {
    DCHECK(FilterInternal().Get());
    if (FilterInternal()->operations_ != v)
      MutableFilterInternal()->operations_ = v;
  }
  bool FilterDataEquivalent(const ComputedStyle& o) const {
    return DataEquivalent(FilterInternal(), o.FilterInternal());
  }


  // background-image
  bool HasBackgroundImage() const {
    return BackgroundInternal().AnyLayerHasImage();
  }
  bool HasUrlBackgroundImage() const {
    return BackgroundInternal().AnyLayerHasUrlImage();
  }
  bool HasFixedAttachmentBackgroundImage() const {
    return BackgroundInternal().AnyLayerHasFixedAttachmentImage();
  }

  // background-clip
  EFillBox BackgroundClip() const {
    return static_cast<EFillBox>(BackgroundInternal().Clip());
  }

  // Returns true if the Element should stick to the viewport bottom as the URL
  // bar hides.
  bool IsFixedToBottom() const { return !Bottom().IsAuto() && Top().IsAuto(); }

  // Border properties.
  // border-image-slice
  const LengthBox& BorderImageSlices() const {
    return BorderImage().ImageSlices();
  }
  void SetBorderImageSlices(const LengthBox&);

  // border-image-source
  StyleImage* BorderImageSource() const { return BorderImage().GetImage(); }
  CORE_EXPORT void SetBorderImageSource(StyleImage*);

  // border-image-width
  const BorderImageLengthBox& BorderImageWidth() const {
    return BorderImage().BorderSlices();
  }
  void SetBorderImageWidth(const BorderImageLengthBox&);

  // border-image-outset
  const BorderImageLengthBox& BorderImageOutset() const {
    return BorderImage().Outset();
  }
  void SetBorderImageOutset(const BorderImageLengthBox&);

  // Border width properties.
  float BorderTopWidth() const {
    if (BorderTopStyle() == EBorderStyle::kNone ||
        BorderTopStyle() == EBorderStyle::kHidden)
      return 0;
    return BorderTopWidthInternal().ToFloat();
  }
  void SetBorderTopWidth(float v) { SetBorderTopWidthInternal(LayoutUnit(v)); }
  bool BorderTopNonZero() const {
    return BorderTopWidth() && (BorderTopStyle() != EBorderStyle::kNone);
  }

  // border-bottom-width
  float BorderBottomWidth() const {
    if (BorderBottomStyle() == EBorderStyle::kNone ||
        BorderBottomStyle() == EBorderStyle::kHidden)
      return 0;
    return BorderBottomWidthInternal().ToFloat();
  }
  void SetBorderBottomWidth(float v) {
    SetBorderBottomWidthInternal(LayoutUnit(v));
  }
  bool BorderBottomNonZero() const {
    return BorderBottomWidth() && (BorderBottomStyle() != EBorderStyle::kNone);
  }

  // border-left-width
  float BorderLeftWidth() const {
    if (BorderLeftStyle() == EBorderStyle::kNone ||
        BorderLeftStyle() == EBorderStyle::kHidden)
      return 0;
    return BorderLeftWidthInternal().ToFloat();
  }
  void SetBorderLeftWidth(float v) {
    SetBorderLeftWidthInternal(LayoutUnit(v));
  }
  bool BorderLeftNonZero() const {
    return BorderLeftWidth() && (BorderLeftStyle() != EBorderStyle::kNone);
  }

  // border-right-width
  float BorderRightWidth() const {
    if (BorderRightStyle() == EBorderStyle::kNone ||
        BorderRightStyle() == EBorderStyle::kHidden)
      return 0;
    return BorderRightWidthInternal().ToFloat();
  }
  void SetBorderRightWidth(float v) {
    SetBorderRightWidthInternal(LayoutUnit(v));
  }
  bool BorderRightNonZero() const {
    return BorderRightWidth() && (BorderRightStyle() != EBorderStyle::kNone);
  }

  // box-shadow (aka -webkit-box-shadow)
  bool BoxShadowDataEquivalent(const ComputedStyle& other) const {
    return DataEquivalent(BoxShadow(), other.BoxShadow());
  }

  // clip
  void SetClip(const LengthBox& box) {
    SetHasAutoClipInternal(false);
    SetClipInternal(box);
  }
  void SetHasAutoClip() {
    SetHasAutoClipInternal(true);
    SetClipInternal(ComputedStyleInitialValues::InitialClip());
  }

  // Column properties.
  // column-count (aka -webkit-column-count)
  void SetColumnCount(uint16_t c) {
    SetHasAutoColumnCountInternal(false);
    SetColumnCountInternal(clampTo<uint16_t>(c, 1));
  }
  void SetHasAutoColumnCount() {
    SetHasAutoColumnCountInternal(true);
    SetColumnCountInternal(ComputedStyleInitialValues::InitialColumnCount());
  }

  // column-rule-width (aka -webkit-column-rule-width)
  uint16_t ColumnRuleWidth() const {
    if (ColumnRuleStyle() == EBorderStyle::kNone ||
        ColumnRuleStyle() == EBorderStyle::kHidden)
      return 0;
    return ColumnRuleWidthInternal().ToUnsigned();
  }
  void SetColumnRuleWidth(uint16_t w) {
    SetColumnRuleWidthInternal(LayoutUnit(w));
  }

  // column-width (aka -webkit-column-width)
  void SetColumnWidth(float f) {
    SetHasAutoColumnWidthInternal(false);
    SetColumnWidthInternal(f);
  }
  void SetHasAutoColumnWidth() {
    SetHasAutoColumnWidthInternal(true);
    SetColumnWidthInternal(0);
  }

  // content
  ContentData* GetContentData() const { return ContentInternal().Get(); }
  void SetContent(ContentData*);

  // -webkit-line-clamp
  bool HasLineClamp() const { return LineClamp() > 0; }

  // -webkit-box-ordinal-group
  void SetBoxOrdinalGroup(unsigned og) {
    SetBoxOrdinalGroupInternal(
        std::min(std::numeric_limits<unsigned>::max() - 1, og));
  }

  // opacity (aka -webkit-opacity)
  void SetOpacity(float f) {
    float v = clampTo<float>(f, 0, 1);
    SetOpacityInternal(v);
  }

  bool OpacityChangedStackingContext(const ComputedStyle& other) const {
    // We only need do layout for opacity changes if adding or losing opacity
    // could trigger a change
    // in us being a stacking context.
    if (IsStackingContextWithoutContainment() ==
            other.IsStackingContextWithoutContainment() ||
        HasOpacity() == other.HasOpacity()) {
      // FIXME: We would like to use SimplifiedLayout here, but we can't quite
      // do that yet.  We need to make sure SimplifiedLayout can operate
      // correctly on LayoutInlines (we will need to add a
      // selfNeedsSimplifiedLayout bit in order to not get confused and taint
      // every line).  In addition we need to solve the floating object issue
      // when layers come and go. Right now a full layout is necessary to keep
      // floating object lists sane.
      return true;
    }
    return false;
  }

  // order (aka -webkit-order)
  // We restrict the smallest value to int min + 2 because we use int min and
  // int min + 1 as special values in a hash set.
  void SetOrder(int o) {
    SetOrderInternal(max(std::numeric_limits<int>::min() + 2, o));
  }

  // Outline properties.

  bool OutlineVisuallyEqual(const ComputedStyle& other) const {
    if (OutlineStyle() == EBorderStyle::kNone &&
        other.OutlineStyle() == EBorderStyle::kNone)
      return true;
    return OutlineWidthInternal() == other.OutlineWidthInternal() &&
           OutlineColor() == other.OutlineColor() &&
           OutlineStyle() == other.OutlineStyle() &&
           OutlineOffset() == other.OutlineOffset() &&
           OutlineStyleIsAuto() == other.OutlineStyleIsAuto();
  }

  // outline-width
  float OutlineWidth() const {
    if (OutlineStyle() == EBorderStyle::kNone)
      return 0;
    return OutlineWidthInternal();
  }
  void SetOutlineWidth(float v) { SetOutlineWidthInternal(LayoutUnit(v)); }
  // TODO(rego): This is a temporal method that will be removed once we start
  // using the float OutlineWidth() in the painting code.
  uint16_t OutlineWidthInt() const {
    if (OutlineStyle() == EBorderStyle::kNone)
      return 0;
    return OutlineWidthInternal().ToUnsigned();
  }

  // outline-offset
  int16_t OutlineOffsetInt() const { return OutlineOffset().ToInt(); }

  // -webkit-perspective-origin-x
  const Length& PerspectiveOriginX() const { return PerspectiveOrigin().X(); }
  void SetPerspectiveOriginX(const Length& v) {
    SetPerspectiveOrigin(LengthPoint(v, PerspectiveOriginY()));
  }

  // -webkit-perspective-origin-y
  const Length& PerspectiveOriginY() const { return PerspectiveOrigin().Y(); }
  void SetPerspectiveOriginY(const Length& v) {
    SetPerspectiveOrigin(LengthPoint(PerspectiveOriginX(), v));
  }

  // Transform properties.
  // -webkit-transform-origin-x
  const Length& TransformOriginX() const { return GetTransformOrigin().X(); }
  void SetTransformOriginX(const Length& v) {
    SetTransformOrigin(
        TransformOrigin(v, TransformOriginY(), TransformOriginZ()));
  }

  // -webkit-transform-origin-y
  const Length& TransformOriginY() const { return GetTransformOrigin().Y(); }
  void SetTransformOriginY(const Length& v) {
    SetTransformOrigin(
        TransformOrigin(TransformOriginX(), v, TransformOriginZ()));
  }

  // -webkit-transform-origin-z
  float TransformOriginZ() const { return GetTransformOrigin().Z(); }
  void SetTransformOriginZ(float f) {
    SetTransformOrigin(
        TransformOrigin(TransformOriginX(), TransformOriginY(), f));
  }

  // Scroll properties.
  // scroll-padding-block-start
  const Length& ScrollPaddingBlockStart() const {
    return IsHorizontalWritingMode() ? ScrollPaddingTop() : ScrollPaddingLeft();
  }
  void SetScrollPaddingBlockStart(const Length& v) {
    if (IsHorizontalWritingMode())
      SetScrollPaddingTop(v);
    else
      SetScrollPaddingLeft(v);
  }

  // scroll-padding-block-end
  const Length& ScrollPaddingBlockEnd() const {
    return IsHorizontalWritingMode() ? ScrollPaddingBottom()
                                     : ScrollPaddingRight();
  }
  void SetScrollPaddingBlockEnd(const Length& v) {
    if (IsHorizontalWritingMode())
      SetScrollPaddingBottom(v);
    else
      SetScrollPaddingRight(v);
  }

  // scroll-padding-inline-start
  const Length& ScrollPaddingInlineStart() const {
    return IsHorizontalWritingMode() ? ScrollPaddingLeft() : ScrollPaddingTop();
  }
  void SetScrollPaddingInlineStart(const Length& v) {
    if (IsHorizontalWritingMode())
      SetScrollPaddingLeft(v);
    else
      SetScrollPaddingTop(v);
  }

  // scroll-padding-inline-end
  const Length& ScrollPaddingInlineEnd() const {
    return IsHorizontalWritingMode() ? ScrollPaddingRight()
                                     : ScrollPaddingBottom();
  }
  void SetScrollPaddingInlineEnd(const Length& v) {
    if (IsHorizontalWritingMode())
      SetScrollPaddingRight(v);
    else
      SetScrollPaddingBottom(v);
  }

  // scroll-margin-block-start
  float ScrollMarginBlockStart() const {
    return IsHorizontalWritingMode() ? ScrollMarginTop() : ScrollMarginLeft();
  }
  void SetScrollMarginBlockStart(float v) {
    if (IsHorizontalWritingMode())
      SetScrollMarginTop(v);
    else
      SetScrollMarginLeft(v);
  }

  // scroll-margin-block-end
  float ScrollMarginBlockEnd() const {
    return IsHorizontalWritingMode() ? ScrollMarginBottom()
                                     : ScrollMarginRight();
  }
  void SetScrollMarginBlockEnd(float v) {
    if (IsHorizontalWritingMode())
      SetScrollMarginBottom(v);
    else
      SetScrollMarginRight(v);
  }

  // scroll-margin-inline-start
  float ScrollMarginInlineStart() const {
    return IsHorizontalWritingMode() ? ScrollMarginLeft() : ScrollMarginTop();
  }
  void SetScrollMarginInlineStart(float v) {
    if (IsHorizontalWritingMode())
      SetScrollMarginLeft(v);
    else
      SetScrollMarginTop(v);
  }

  // scroll-margin-inline-end
  float ScrollMarginInlineEnd() const {
    return IsHorizontalWritingMode() ? ScrollMarginRight()
                                     : ScrollMarginBottom();
  }
  void SetScrollMarginInlineEnd(float v) {
    if (IsHorizontalWritingMode())
      SetScrollMarginRight(v);
    else
      SetScrollMarginBottom(v);
  }

  // scrollbar-gutter
  inline bool ScrollbarGutterIsAuto() const {
    return ScrollbarGutter() == kScrollbarGutterAuto;
  }
  inline bool ScrollbarGutterIsStable() const {
    return ScrollbarGutter() & kScrollbarGutterStable;
  }
  inline bool ScrollbarGutterIsAlways() const {
    return ScrollbarGutter() & kScrollbarGutterAlways;
  }
  inline bool ScrollbarGutterIsBoth() const {
    return ScrollbarGutter() & kScrollbarGutterBoth;
  }
  inline bool ScrollbarGutterIsForce() const {
    return ScrollbarGutter() & kScrollbarGutterForce;
  }

  // shape-image-threshold (aka -webkit-shape-image-threshold)
  void SetShapeImageThreshold(float shape_image_threshold) {
    float clamped_shape_image_threshold =
        clampTo<float>(shape_image_threshold, 0, 1);
    SetShapeImageThresholdInternal(clamped_shape_image_threshold);
  }

  // shape-outside (aka -webkit-shape-outside)
  ShapeValue* ShapeOutside() const { return ShapeOutsideInternal().Get(); }
  bool ShapeOutsideDataEquivalent(const ComputedStyle& other) const {
    return DataEquivalent(ShapeOutside(), other.ShapeOutside());
  }

  // touch-action
  TouchAction GetEffectiveTouchAction() const {
    return EffectiveTouchActionInternal();
  }
  void SetEffectiveTouchAction(TouchAction t) {
    return SetEffectiveTouchActionInternal(t);
  }

  // vertical-align
  EVerticalAlign VerticalAlign() const { return static_cast<EVerticalAlign>(VerticalAlignInternal()); }
  void SetVerticalAlign(EVerticalAlign v) { SetVerticalAlignInternal(static_cast<unsigned>(v)); }
  void SetVerticalAlignLength(const Length& length) {
    SetVerticalAlignInternal(static_cast<unsigned>(EVerticalAlign::kLength));
    SetVerticalAlignLengthInternal(length);
  }

  // z-index
  void SetZIndex(int v) {
    SetHasAutoZIndexInternal(false);
    SetZIndexInternal(v);
  }
  void SetHasAutoZIndex() {
    SetHasAutoZIndexInternal(true);
    SetZIndexInternal(0);
  }
  // This returns the z-index if it applies (i.e. positioned element or grid or
  // flex children), and 0 otherwise. Note that for most situations,
  // `EffectiveZIndex()` is what the code should use to determine how to stack
  // the element. `ZIndex()` is still available and returns the value as
  // specified in style (used for e.g. style comparisons and computed style
  // reporting)
  int EffectiveZIndex() const { return EffectiveZIndexZero() ? 0 : ZIndex(); }

  CORE_EXPORT bool SetEffectiveZoom(float);

  // -webkit-clip-path
  bool ClipPathDataEquivalent(const ComputedStyle& other) const {
    return DataEquivalent(ClipPath(), other.ClipPath());
  }

  // Mask properties.
  // -webkit-mask-box-image-outset
  bool HasMaskBoxImageOutsets() const {
    return MaskBoxImageInternal().HasImage() && MaskBoxImageOutset().NonZero();
  }
  LayoutRectOutsets MaskBoxImageOutsets() const {
    return ImageOutsets(MaskBoxImageInternal());
  }
  const BorderImageLengthBox& MaskBoxImageOutset() const {
    return MaskBoxImageInternal().Outset();
  }
  void SetMaskBoxImageOutset(const BorderImageLengthBox& outset) {
    MutableMaskBoxImageInternal().SetOutset(outset);
  }

  // -webkit-mask-box-image-slice
  const LengthBox& MaskBoxImageSlices() const {
    return MaskBoxImageInternal().ImageSlices();
  }
  void SetMaskBoxImageSlices(const LengthBox& slices) {
    MutableMaskBoxImageInternal().SetImageSlices(slices);
  }

  // -webkit-mask-box-image-source
  StyleImage* MaskBoxImageSource() const {
    return MaskBoxImageInternal().GetImage();
  }
  void SetMaskBoxImageSource(StyleImage* v) {
    MutableMaskBoxImageInternal().SetImage(v);
  }

  // -webkit-mask-box-image-width
  const BorderImageLengthBox& MaskBoxImageWidth() const {
    return MaskBoxImageInternal().BorderSlices();
  }
  void SetMaskBoxImageWidth(const BorderImageLengthBox& slices) {
    MutableMaskBoxImageInternal().SetBorderSlices(slices);
  }

  // Inherited properties.

  // line-height
  Length LineHeight() const;

  // List style properties.
  // list-style-image
  CORE_EXPORT StyleImage* ListStyleImage() const;
  void SetListStyleImage(StyleImage*);

  // quotes
  bool QuotesDataEquivalent(const ComputedStyle&) const;

  // text-shadow
  bool TextShadowDataEquivalent(const ComputedStyle&) const;

  // Text emphasis properties.
  TextEmphasisMark GetTextEmphasisMark() const;
  void SetTextEmphasisMark(TextEmphasisMark mark) {
    SetTextEmphasisMarkInternal(mark);
  }
  const AtomicString& TextEmphasisMarkString() const;
  LineLogicalSide GetTextEmphasisLineLogicalSide() const;

  // Font properties.
  CORE_EXPORT const Font& GetFont() const { return FontInternal(); }
  CORE_EXPORT void SetFont(const Font& font) { SetFontInternal(font); }
  CORE_EXPORT const FontDescription& GetFontDescription() const {
    return FontInternal().GetFontDescription();
  }
  CORE_EXPORT bool SetFontDescription(const FontDescription&);
  bool HasIdenticalAscentDescentAndLineGap(const ComputedStyle& other) const;
  bool HasFontRelativeUnits() const {
    return HasEmUnits() || HasRemUnits() || HasGlyphRelativeUnits();
  }

  // If true, the ComputedStyle must be recalculated when fonts are updated.
  bool DependsOnFontMetrics() const {
    return HasGlyphRelativeUnits() || HasFontSizeAdjust();
  }
  bool CachedPseudoElementStylesDependOnFontMetrics() const;

  // font-size
  int FontSize() const { return GetFontDescription().ComputedPixelSize(); }
  CORE_EXPORT float SpecifiedFontSize() const {
    return GetFontDescription().SpecifiedSize();
  }
  CORE_EXPORT float ComputedFontSize() const {
    return GetFontDescription().ComputedSize();
  }
  LayoutUnit ComputedFontSizeAsFixed() const {
    return LayoutUnit::FromFloatRound(GetFontDescription().ComputedSize());
  }

  // font-size-adjust
  float FontSizeAdjust() const { return GetFontDescription().SizeAdjust(); }
  bool HasFontSizeAdjust() const {
    return GetFontDescription().HasSizeAdjust();
  }

  // font-weight
  CORE_EXPORT FontSelectionValue GetFontWeight() const {
    return GetFontDescription().Weight();
  }

  // font-stretch
  FontSelectionValue GetFontStretch() const {
    return GetFontDescription().Stretch();
  }

  // Child is aligned to the parent by matching the parent’s dominant baseline
  // to the same baseline in the child.
  FontBaseline GetFontBaseline() const;

  FontHeight GetFontHeight(FontBaseline baseline) const;
  FontHeight GetFontHeight() const { return GetFontHeight(GetFontBaseline()); }

  // Compute FontOrientation from this style. It is derived from WritingMode and
  // TextOrientation.
  FontOrientation ComputeFontOrientation() const;

  // Update FontOrientation in FontDescription if it is different. FontBuilder
  // takes care of updating it, but if WritingMode or TextOrientation were
  // changed after the style was constructed, this function synchronizes
  // FontOrientation to match to this style.
  void UpdateFontOrientation();

  // -webkit-locale
  const AtomicString& Locale() const {
    return LayoutLocale::LocaleString(GetFontDescription().Locale());
  }
  AtomicString LocaleForLineBreakIterator() const;

  // FIXME: Remove letter-spacing/word-spacing and replace them with respective
  // FontBuilder calls.  letter-spacing
  float LetterSpacing() const { return GetFontDescription().LetterSpacing(); }
  void SetLetterSpacing(float);

  // tab-size
  void SetTabSize(const TabSize& t) {
    if (t.GetPixelSize(1) < 0) {
      if (t.IsSpaces())
        SetTabSizeInternal(TabSize(0, TabSizeValueType::kSpace));
      else
        SetTabSizeInternal(TabSize(0, TabSizeValueType::kLength));
    } else {
      SetTabSizeInternal(t);
    }
  }

  // word-spacing
  float WordSpacing() const { return GetFontDescription().WordSpacing(); }
  void SetWordSpacing(float);

  // orphans
  void SetOrphans(int16_t o) { SetOrphansInternal(clampTo<int16_t>(o, 1)); }

  // widows
  void SetWidows(int16_t w) { SetWidowsInternal(clampTo<int16_t>(w, 1)); }

  // SVG properties.
  const SVGComputedStyle& SvgStyle() const { return *svg_style_.Get(); }
  SVGComputedStyle& AccessSVGStyle() { return *svg_style_.Access(); }

  // baseline-shift
  EBaselineShift BaselineShift() const { return SvgStyle().BaselineShift(); }
  const Length& BaselineShiftValue() const {
    return SvgStyle().BaselineShiftValue();
  }
  void SetBaselineShiftValue(const Length& value) {
    SVGComputedStyle& svg_style = AccessSVGStyle();
    svg_style.SetBaselineShift(BS_LENGTH);
    svg_style.SetBaselineShiftValue(value);
  }

  // cx
  void SetCx(const Length& cx) { AccessSVGStyle().SetCx(cx); }

  // cy
  void SetCy(const Length& cy) { AccessSVGStyle().SetCy(cy); }

  // d
  void SetD(scoped_refptr<StylePath> d) { AccessSVGStyle().SetD(std::move(d)); }

  // x
  void SetX(const Length& x) { AccessSVGStyle().SetX(x); }

  // y
  void SetY(const Length& y) { AccessSVGStyle().SetY(y); }

  // r
  void SetR(const Length& r) { AccessSVGStyle().SetR(r); }

  // rx
  void SetRx(const Length& rx) { AccessSVGStyle().SetRx(rx); }

  // ry
  void SetRy(const Length& ry) { AccessSVGStyle().SetRy(ry); }

  // fill-opacity
  float FillOpacity() const { return SvgStyle().FillOpacity(); }
  void SetFillOpacity(float f) { AccessSVGStyle().SetFillOpacity(f); }

  // stop-color
  void SetStopColor(const StyleColor& c) { AccessSVGStyle().SetStopColor(c); }

  // flood-color
  void SetFloodColor(const StyleColor& c) { AccessSVGStyle().SetFloodColor(c); }

  // lighting-color
  void SetLightingColor(const StyleColor& c) {
    AccessSVGStyle().SetLightingColor(c);
  }

  // flood-opacity
  float FloodOpacity() const { return SvgStyle().FloodOpacity(); }
  void SetFloodOpacity(float f) { AccessSVGStyle().SetFloodOpacity(f); }

  // stop-opacity
  float StopOpacity() const { return SvgStyle().StopOpacity(); }
  void SetStopOpacity(float f) { AccessSVGStyle().SetStopOpacity(f); }

  // stroke-dasharray
  SVGDashArray* StrokeDashArray() const { return SvgStyle().StrokeDashArray(); }
  void SetStrokeDashArray(scoped_refptr<SVGDashArray> array) {
    AccessSVGStyle().SetStrokeDashArray(std::move(array));
  }

  // stroke-dashoffset
  const Length& StrokeDashOffset() const {
    return SvgStyle().StrokeDashOffset();
  }
  void SetStrokeDashOffset(const Length& d) {
    AccessSVGStyle().SetStrokeDashOffset(d);
  }

  // stroke-miterlimit
  float StrokeMiterLimit() const { return SvgStyle().StrokeMiterLimit(); }
  void SetStrokeMiterLimit(float f) { AccessSVGStyle().SetStrokeMiterLimit(f); }

  // stroke-opacity
  float StrokeOpacity() const { return SvgStyle().StrokeOpacity(); }
  void SetStrokeOpacity(float f) { AccessSVGStyle().SetStrokeOpacity(f); }

  // stroke-width
  const UnzoomedLength& StrokeWidth() const { return SvgStyle().StrokeWidth(); }
  void SetStrokeWidth(const UnzoomedLength& w) {
    AccessSVGStyle().SetStrokeWidth(w);
  }

  // Comparison operators
  // FIXME: Replace callers of operator== wth a named method instead, e.g.
  // inheritedEquals().
  CORE_EXPORT bool operator==(const ComputedStyle& other) const;
  bool operator!=(const ComputedStyle& other) const {
    return !(*this == other);
  }

  bool InheritedEqual(const ComputedStyle&) const;
  bool NonInheritedEqual(const ComputedStyle&) const;
  inline bool IndependentInheritedEqual(const ComputedStyle&) const;
  inline bool NonIndependentInheritedEqual(const ComputedStyle&) const;
  bool LoadingCustomFontsEqual(const ComputedStyle&) const;
  bool InheritedDataShared(const ComputedStyle&) const;

  bool HasChildDependentFlags() const { return ChildHasExplicitInheritance(); }
  void CopyChildDependentFlagsFrom(const ComputedStyle&);

  // Counters.
  const CounterDirectiveMap* GetCounterDirectives() const;
  CounterDirectiveMap& AccessCounterDirectives();
  const CounterDirectives GetCounterDirectives(
      const AtomicString& identifier) const;
  bool CounterDirectivesEqual(const ComputedStyle& other) const {
    // If the counter directives change, trigger a relayout to re-calculate
    // counter values and rebuild the counter node tree.
    return DataEquivalent(CounterDirectivesInternal().get(),
                          other.CounterDirectivesInternal().get());
  }
  void ClearIncrementDirectives();
  void ClearResetDirectives();
  void ClearSetDirectives();

  bool IsDeprecatedWebkitBox() const {
    return Display() == EDisplay::kWebkitBox ||
           Display() == EDisplay::kWebkitInlineBox;
  }
  bool IsDeprecatedFlexboxUsingFlexLayout() const {
    return IsDeprecatedWebkitBox() &&
           !IsDeprecatedWebkitBoxWithVerticalLineClamp();
  }
  bool IsDeprecatedWebkitBoxWithVerticalLineClamp() const {
    return IsDeprecatedWebkitBox() && BoxOrient() == EBoxOrient::kVertical &&
           HasLineClamp();
  }

  // Variables.
  bool HasVariables() const;
  CORE_EXPORT HashSet<AtomicString> GetVariableNames() const;
  CORE_EXPORT StyleInheritedVariables* InheritedVariables() const;
  CORE_EXPORT StyleNonInheritedVariables* NonInheritedVariables() const;

  CORE_EXPORT void SetVariableData(const AtomicString&,
                                   scoped_refptr<CSSVariableData>,
                                   bool is_inherited_property);
  CORE_EXPORT void SetVariableValue(const AtomicString&,
                                    const CSSValue*,
                                    bool is_inherited_property);

  // Handles both inherited and non-inherited variables
  CORE_EXPORT CSSVariableData* GetVariableData(const AtomicString&) const;
  CSSVariableData* GetVariableData(const AtomicString&,
                                   bool is_inherited_property) const;

  const CSSValue* GetVariableValue(const AtomicString&) const;
  const CSSValue* GetVariableValue(const AtomicString&,
                                   bool is_inherited_property) const;

  // Animations.
  CSSAnimationData& AccessAnimations();
  const CSSAnimationData* Animations() const {
    return AnimationsInternal().get();
  }

  // Transitions.
  const CSSTransitionData* Transitions() const {
    return TransitionsInternal().get();
  }
  CSSTransitionData& AccessTransitions();

  // Callback selectors.
  void AddCallbackSelector(const String& selector);

  // Non-property flags.
  CORE_EXPORT void SetTextAutosizingMultiplier(float);

  // Column utility functions.
  void ClearMultiCol();
  bool SpecifiesColumns() const {
    return !HasAutoColumnCount() || !HasAutoColumnWidth();
  }
  bool ColumnRuleIsTransparent() const {
    return !ColumnRuleColorInternal()
                .Resolve(GetCurrentColor(), UsedColorScheme())
                .Alpha();
  }
  bool ColumnRuleEquivalent(const ComputedStyle& other_style) const;
  bool HasColumnRule() const {
    if (LIKELY(!SpecifiesColumns()))
      return false;
    return ColumnRuleWidth() && !ColumnRuleIsTransparent() &&
           BorderStyleIsVisible(ColumnRuleStyle());
  }

  // Flex utility functions.
  bool ResolvedIsColumnFlexDirection() const {
    if (IsDeprecatedWebkitBox())
      return BoxOrient() == EBoxOrient::kVertical;
    return FlexDirection() == EFlexDirection::kColumn ||
           FlexDirection() == EFlexDirection::kColumnReverse;
  }
  bool ResolvedIsColumnReverseFlexDirection() const {
    if (IsDeprecatedWebkitBox()) {
      return BoxOrient() == EBoxOrient::kVertical &&
             BoxDirection() == EBoxDirection::kReverse;
    }
    return FlexDirection() == EFlexDirection::kColumnReverse;
  }
  bool ResolvedIsRowReverseFlexDirection() const {
    if (IsDeprecatedWebkitBox()) {
      return BoxOrient() == EBoxOrient::kHorizontal &&
             BoxDirection() == EBoxDirection::kReverse;
    }
    return FlexDirection() == EFlexDirection::kRowReverse;
  }
  bool HasBoxReflect() const { return BoxReflect(); }
  bool ReflectionDataEquivalent(const ComputedStyle& other) const {
    return DataEquivalent(BoxReflect(), other.BoxReflect());
  }
  float ResolvedFlexGrow(const ComputedStyle& box_style) const {
    if (box_style.IsDeprecatedWebkitBox())
      return BoxFlex() > 0 ? BoxFlex() : 0.0f;
    return FlexGrow();
  }
  float ResolvedFlexShrink(const ComputedStyle& box_style) const {
    if (box_style.IsDeprecatedWebkitBox())
      return BoxFlex() > 0 ? BoxFlex() : 0.0f;
    return FlexShrink();
  }

  // Mask utility functions.
  bool HasMask() const {
    return MaskInternal().AnyLayerHasImage() ||
           MaskBoxImageInternal().HasImage();
  }
  StyleImage* MaskImage() const { return MaskInternal().GetImage(); }
  FillLayer& AccessMaskLayers() { return MutableMaskInternal(); }
  const FillLayer& MaskLayers() const { return MaskInternal(); }
  const NinePieceImage& MaskBoxImage() const { return MaskBoxImageInternal(); }
  bool MaskBoxImageSlicesFill() const { return MaskBoxImageInternal().Fill(); }
  void AdjustMaskLayers() {
    if (MaskLayers().Next()) {
      AccessMaskLayers().CullEmptyLayers();
      AccessMaskLayers().FillUnsetProperties();
    }
  }
  void SetMaskBoxImage(const NinePieceImage& b) { SetMaskBoxImageInternal(b); }
  void SetMaskBoxImageSlicesFill(bool fill) {
    MutableMaskBoxImageInternal().SetFill(fill);
  }

  // Text-combine utility functions.
  bool HasTextCombine() const { return TextCombine() != ETextCombine::kNone; }

  // Grid utility functions.
  GridAutoFlow GetGridAutoFlow() const { return GridAutoFlowInternal(); }
  bool IsGridAutoFlowDirectionRow() const {
    return (GridAutoFlowInternal() &
            static_cast<int>(kInternalAutoFlowDirectionRow)) ==
           kInternalAutoFlowDirectionRow;
  }
  bool IsGridAutoFlowDirectionColumn() const {
    return (GridAutoFlowInternal() &
            static_cast<int>(kInternalAutoFlowDirectionColumn)) ==
           kInternalAutoFlowDirectionColumn;
  }
  bool IsGridAutoFlowAlgorithmSparse() const {
    return (GridAutoFlowInternal() &
            static_cast<int>(kInternalAutoFlowAlgorithmSparse)) ==
           kInternalAutoFlowAlgorithmSparse;
  }
  bool IsGridAutoFlowAlgorithmDense() const {
    return (GridAutoFlowInternal() &
            static_cast<int>(kInternalAutoFlowAlgorithmDense)) ==
           kInternalAutoFlowAlgorithmDense;
  }

  // align-content utility functions.
  ContentPosition AlignContentPosition() const {
    return AlignContent().GetPosition();
  }
  ContentDistributionType AlignContentDistribution() const {
    return AlignContent().Distribution();
  }
  OverflowAlignment AlignContentOverflowAlignment() const {
    return AlignContent().Overflow();
  }
  void SetAlignContentPosition(ContentPosition position) {
    MutableAlignContentInternal().SetPosition(position);
  }
  void SetAlignContentDistribution(ContentDistributionType distribution) {
    MutableAlignContentInternal().SetDistribution(distribution);
  }
  void SetAlignContentOverflow(OverflowAlignment overflow) {
    MutableAlignContentInternal().SetOverflow(overflow);
  }

  // justify-content utility functions.
  ContentPosition JustifyContentPosition() const {
    return JustifyContent().GetPosition();
  }
  ContentDistributionType JustifyContentDistribution() const {
    return JustifyContent().Distribution();
  }
  OverflowAlignment JustifyContentOverflowAlignment() const {
    return JustifyContent().Overflow();
  }
  void SetJustifyContentPosition(ContentPosition position) {
    MutableJustifyContentInternal().SetPosition(position);
  }
  void SetJustifyContentDistribution(ContentDistributionType distribution) {
    MutableJustifyContentInternal().SetDistribution(distribution);
  }
  void SetJustifyContentOverflow(OverflowAlignment overflow) {
    MutableJustifyContentInternal().SetOverflow(overflow);
  }

  // align-items utility functions.
  ItemPosition AlignItemsPosition() const { return AlignItems().GetPosition(); }
  OverflowAlignment AlignItemsOverflowAlignment() const {
    return AlignItems().Overflow();
  }
  void SetAlignItemsPosition(ItemPosition position) {
    MutableAlignItemsInternal().SetPosition(position);
  }
  void SetAlignItemsOverflow(OverflowAlignment overflow) {
    MutableAlignItemsInternal().SetOverflow(overflow);
  }

  // justify-items utility functions.
  ItemPosition JustifyItemsPosition() const {
    return JustifyItems().GetPosition();
  }
  OverflowAlignment JustifyItemsOverflowAlignment() const {
    return JustifyItems().Overflow();
  }
  ItemPositionType JustifyItemsPositionType() const {
    return JustifyItems().PositionType();
  }
  void SetJustifyItemsPosition(ItemPosition position) {
    MutableJustifyItemsInternal().SetPosition(position);
  }
  void SetJustifyItemsOverflow(OverflowAlignment overflow) {
    MutableJustifyItemsInternal().SetOverflow(overflow);
  }
  void SetJustifyItemsPositionType(ItemPositionType position_type) {
    MutableJustifyItemsInternal().SetPositionType(position_type);
  }

  // align-self utility functions.
  ItemPosition AlignSelfPosition() const { return AlignSelf().GetPosition(); }
  OverflowAlignment AlignSelfOverflowAlignment() const {
    return AlignSelf().Overflow();
  }
  void SetAlignSelfPosition(ItemPosition position) {
    MutableAlignSelfInternal().SetPosition(position);
  }
  void SetAlignSelfOverflow(OverflowAlignment overflow) {
    MutableAlignSelfInternal().SetOverflow(overflow);
  }

  // justify-self utility functions.
  ItemPosition JustifySelfPosition() const {
    return JustifySelf().GetPosition();
  }
  OverflowAlignment JustifySelfOverflowAlignment() const {
    return JustifySelf().Overflow();
  }
  void SetJustifySelfPosition(ItemPosition position) {
    MutableJustifySelfInternal().SetPosition(position);
  }
  void SetJustifySelfOverflow(OverflowAlignment overflow) {
    MutableJustifySelfInternal().SetOverflow(overflow);
  }

  // Writing mode utility functions.
  WritingDirectionMode GetWritingDirection() const {
    return {GetWritingMode(), Direction()};
  }
  bool IsHorizontalWritingMode() const {
    return blink::IsHorizontalWritingMode(GetWritingMode());
  }
  bool IsFlippedLinesWritingMode() const {
    return blink::IsFlippedLinesWritingMode(GetWritingMode());
  }
  bool IsFlippedBlocksWritingMode() const {
    return blink::IsFlippedBlocksWritingMode(GetWritingMode());
  }

  // Will-change utility functions.
  bool HasWillChangeCompositingHint() const;
  bool HasWillChangeOpacityHint() const {
    return WillChangeProperties().Contains(CSSPropertyID::kOpacity);
  }
  bool HasWillChangeTransformHint() const;
  bool HasWillChangeFilterHint() const {
    return WillChangeProperties().Contains(CSSPropertyID::kFilter) ||
           WillChangeProperties().Contains(CSSPropertyID::kAliasWebkitFilter);
  }
  bool HasWillChangeBackdropFilterHint() const {
    return WillChangeProperties().Contains(CSSPropertyID::kBackdropFilter);
  }

  // Hyphen utility functions.
  Hyphenation* GetHyphenation() const;
  const AtomicString& HyphenString() const;

  // text-align utility functions.
  using ComputedStyleBase::GetTextAlign;
  ETextAlign GetTextAlign(bool is_last_line) const;

  // text-indent utility functions.
  bool ShouldUseTextIndent(bool is_first_line,
                           bool is_after_forced_break) const;

  // text-transform utility functions.
  void ApplyTextTransform(String*, UChar previous_character = ' ') const;

  // Line-height utility functions.
  const Length& SpecifiedLineHeight() const { return LineHeightInternal(); }
  int ComputedLineHeight() const;
  LayoutUnit ComputedLineHeightAsFixed() const;

  // Width/height utility functions.
  const Length& LogicalWidth() const {
    return IsHorizontalWritingMode() ? Width() : Height();
  }
  const Length& LogicalHeight() const {
    return IsHorizontalWritingMode() ? Height() : Width();
  }
  void SetLogicalWidth(const Length& v) {
    if (IsHorizontalWritingMode()) {
      SetWidth(v);
    } else {
      SetHeight(v);
    }
  }

  void SetLogicalHeight(const Length& v) {
    if (IsHorizontalWritingMode()) {
      SetHeight(v);
    } else {
      SetWidth(v);
    }
  }
  const Length& LogicalMaxWidth() const {
    return IsHorizontalWritingMode() ? MaxWidth() : MaxHeight();
  }
  const Length& LogicalMaxHeight() const {
    return IsHorizontalWritingMode() ? MaxHeight() : MaxWidth();
  }
  const Length& LogicalMinWidth() const {
    return IsHorizontalWritingMode() ? MinWidth() : MinHeight();
  }
  const Length& LogicalMinHeight() const {
    return IsHorizontalWritingMode() ? MinHeight() : MinWidth();
  }

  // Margin utility functions.
  void SetMarginTop(const Length& v) {
    if (MarginTop() != v) {
      if (!v.IsZero())
        SetMayHaveMargin();
      MutableMarginTopInternal() = v;
    }
  }
  void SetMarginRight(const Length& v) {
    if (MarginRight() != v) {
      if (!v.IsZero())
        SetMayHaveMargin();
      MutableMarginRightInternal() = v;
    }
  }
  void SetMarginBottom(const Length& v) {
    if (MarginBottom() != v) {
      if (!v.IsZero())
        SetMayHaveMargin();
      MutableMarginBottomInternal() = v;
    }
  }
  void SetMarginLeft(const Length& v) {
    if (MarginLeft() != v) {
      if (!v.IsZero())
        SetMayHaveMargin();
      MutableMarginLeftInternal() = v;
    }
  }
  bool HasMarginBeforeQuirk() const { return MarginBefore().Quirk(); }
  bool HasMarginAfterQuirk() const { return MarginAfter().Quirk(); }
  const Length& MarginBefore() const { return MarginBeforeUsing(*this); }
  const Length& MarginAfter() const { return MarginAfterUsing(*this); }
  const Length& MarginStart() const { return MarginStartUsing(*this); }
  const Length& MarginEnd() const { return MarginEndUsing(*this); }
  const Length& MarginOver() const {
    return PhysicalMarginToLogical(*this).Over();
  }
  const Length& MarginUnder() const {
    return PhysicalMarginToLogical(*this).Under();
  }
  const Length& MarginStartUsing(const ComputedStyle& other) const {
    return PhysicalMarginToLogical(other).Start();
  }
  const Length& MarginEndUsing(const ComputedStyle& other) const {
    return PhysicalMarginToLogical(other).End();
  }
  const Length& MarginBeforeUsing(const ComputedStyle& other) const {
    return PhysicalMarginToLogical(other).Before();
  }
  const Length& MarginAfterUsing(const ComputedStyle& other) const {
    return PhysicalMarginToLogical(other).After();
  }
  void SetMarginStart(const Length&);
  void SetMarginEnd(const Length&);
  bool MarginEqual(const ComputedStyle& other) const {
    return MarginTop() == other.MarginTop() &&
           MarginLeft() == other.MarginLeft() &&
           MarginRight() == other.MarginRight() &&
           MarginBottom() == other.MarginBottom();
  }

  // Padding utility functions.
  void SetPaddingTop(const Length& v) {
    if (PaddingTop() != v) {
      if (!v.IsZero())
        SetMayHavePadding();
      MutablePaddingTopInternal() = v;
    }
  }
  void SetPaddingRight(const Length& v) {
    if (PaddingRight() != v) {
      if (!v.IsZero())
        SetMayHavePadding();
      MutablePaddingRightInternal() = v;
    }
  }
  void SetPaddingBottom(const Length& v) {
    if (PaddingBottom() != v) {
      if (!v.IsZero())
        SetMayHavePadding();
      MutablePaddingBottomInternal() = v;
    }
  }
  void SetPaddingLeft(const Length& v) {
    if (PaddingLeft() != v) {
      if (!v.IsZero())
        SetMayHavePadding();
      MutablePaddingLeftInternal() = v;
    }
  }

  const Length& PaddingBefore() const {
    return PhysicalPaddingToLogical().Before();
  }
  const Length& PaddingAfter() const {
    return PhysicalPaddingToLogical().After();
  }
  const Length& PaddingStart() const {
    return PhysicalPaddingToLogical().Start();
  }
  const Length& PaddingEnd() const { return PhysicalPaddingToLogical().End(); }
  const Length& PaddingOver() const {
    return PhysicalPaddingToLogical().Over();
  }
  const Length& PaddingUnder() const {
    return PhysicalPaddingToLogical().Under();
  }
  void ResetPadding() {
    SetPaddingTop(Length::Fixed());
    SetPaddingBottom(Length::Fixed());
    SetPaddingLeft(Length::Fixed());
    SetPaddingRight(Length::Fixed());
  }
  void SetPadding(const LengthBox& b) {
    SetPaddingTop(b.top_);
    SetPaddingBottom(b.bottom_);
    SetPaddingLeft(b.left_);
    SetPaddingRight(b.right_);
  }
  bool PaddingEqual(const ComputedStyle& other) const {
    return PaddingTop() == other.PaddingTop() &&
           PaddingLeft() == other.PaddingLeft() &&
           PaddingRight() == other.PaddingRight() &&
           PaddingBottom() == other.PaddingBottom();
  }
  bool PaddingEqual(const LengthBox& other) const {
    return PaddingTop() == other.Top() && PaddingLeft() == other.Left() &&
           PaddingRight() == other.Right() && PaddingBottom() == other.Bottom();
  }

  // Border utility functions
  LayoutRectOutsets ImageOutsets(const NinePieceImage&) const;
  bool HasBorderImageOutsets() const {
    return BorderImage().HasImage() && BorderImage().Outset().NonZero();
  }
  LayoutRectOutsets BorderImageOutsets() const {
    return ImageOutsets(BorderImage());
  }
  bool BorderImageSlicesFill() const { return BorderImage().Fill(); }

  void SetBorderImageSlicesFill(bool);
  const BorderValue BorderLeft() const {
    return BorderValue(BorderLeftStyle(), BorderLeftColor(),
                       BorderLeftWidthInternal().ToFloat());
  }
  const BorderValue BorderRight() const {
    return BorderValue(BorderRightStyle(), BorderRightColor(),
                       BorderRightWidthInternal().ToFloat());
  }
  const BorderValue BorderTop() const {
    return BorderValue(BorderTopStyle(), BorderTopColor(),
                       BorderTopWidthInternal().ToFloat());
  }
  const BorderValue BorderBottom() const {
    return BorderValue(BorderBottomStyle(), BorderBottomColor(),
                       BorderBottomWidthInternal().ToFloat());
  }

  bool BorderSizeEquals(const ComputedStyle& o) const {
    return BorderLeftWidth() == o.BorderLeftWidth() &&
           BorderTopWidth() == o.BorderTopWidth() &&
           BorderRightWidth() == o.BorderRightWidth() &&
           BorderBottomWidth() == o.BorderBottomWidth();
  }

  BorderValue BorderBeforeUsing(const ComputedStyle& other) const {
    return PhysicalBorderToLogical(other).Before();
  }
  BorderValue BorderAfterUsing(const ComputedStyle& other) const {
    return PhysicalBorderToLogical(other).After();
  }
  BorderValue BorderStartUsing(const ComputedStyle& other) const {
    return PhysicalBorderToLogical(other).Start();
  }
  BorderValue BorderEndUsing(const ComputedStyle& other) const {
    return PhysicalBorderToLogical(other).End();
  }

  BorderValue BorderBefore() const { return BorderBeforeUsing(*this); }
  BorderValue BorderAfter() const { return BorderAfterUsing(*this); }
  BorderValue BorderStart() const { return BorderStartUsing(*this); }
  BorderValue BorderEnd() const { return BorderEndUsing(*this); }

  float BorderAfterWidth() const {
    return PhysicalBorderWidthToLogical().After();
  }
  float BorderBeforeWidth() const {
    return PhysicalBorderWidthToLogical().Before();
  }
  float BorderEndWidth() const { return PhysicalBorderWidthToLogical().End(); }
  float BorderStartWidth() const {
    return PhysicalBorderWidthToLogical().Start();
  }
  float BorderOverWidth() const {
    return PhysicalBorderWidthToLogical().Over();
  }
  float BorderUnderWidth() const {
    return PhysicalBorderWidthToLogical().Under();
  }

  EBorderStyle BorderAfterStyle() const {
    return PhysicalBorderStyleToLogical().After();
  }
  EBorderStyle BorderBeforeStyle() const {
    return PhysicalBorderStyleToLogical().Before();
  }
  EBorderStyle BorderEndStyle() const {
    return PhysicalBorderStyleToLogical().End();
  }
  EBorderStyle BorderStartStyle() const {
    return PhysicalBorderStyleToLogical().Start();
  }

  bool HasBorderFill() const {
    return BorderImage().HasImage() && BorderImage().Fill();
  }
  bool HasBorder() const {
    return BorderLeftNonZero() || BorderRightNonZero() || BorderTopNonZero() ||
           BorderBottomNonZero();
  }
  bool HasBorderDecoration() const { return HasBorder() || HasBorderFill(); }
  bool HasBorderRadius() const {
    if (!BorderTopLeftRadius().Width().IsZero())
      return true;
    if (!BorderTopRightRadius().Width().IsZero())
      return true;
    if (!BorderBottomLeftRadius().Width().IsZero())
      return true;
    if (!BorderBottomRightRadius().Width().IsZero())
      return true;
    return false;
  }
  bool HasBorderColorReferencingCurrentColor() const {
    return (BorderLeftNonZero() && BorderLeftColor().IsCurrentColor()) ||
           (BorderRightNonZero() && BorderRightColor().IsCurrentColor()) ||
           (BorderTopNonZero() && BorderTopColor().IsCurrentColor()) ||
           (BorderBottomNonZero() && BorderBottomColor().IsCurrentColor());
  }

  bool RadiiEqual(const ComputedStyle& o) const {
    return BorderTopLeftRadius() == o.BorderTopLeftRadius() &&
           BorderTopRightRadius() == o.BorderTopRightRadius() &&
           BorderBottomLeftRadius() == o.BorderBottomLeftRadius() &&
           BorderBottomRightRadius() == o.BorderBottomRightRadius();
  }

  bool BorderLeftEquals(const ComputedStyle& o) const {
    return BorderLeftWidthInternal() == o.BorderLeftWidthInternal() &&
           BorderLeftStyle() == o.BorderLeftStyle() &&
           BorderLeftColor() == o.BorderLeftColor();
  }
  bool BorderLeftEquals(const BorderValue& o) const {
    return BorderLeftWidthInternal().ToFloat() == o.Width() &&
           BorderLeftStyle() == o.Style() && BorderLeftColor() == o.GetColor();
  }

  bool BorderLeftVisuallyEqual(const ComputedStyle& o) const {
    if (BorderLeftStyle() == EBorderStyle::kNone &&
        o.BorderLeftStyle() == EBorderStyle::kNone)
      return true;
    if (BorderLeftStyle() == EBorderStyle::kHidden &&
        o.BorderLeftStyle() == EBorderStyle::kHidden)
      return true;
    return BorderLeftEquals(o);
  }

  bool BorderRightEquals(const ComputedStyle& o) const {
    return BorderRightWidthInternal() == o.BorderRightWidthInternal() &&
           BorderRightStyle() == o.BorderRightStyle() &&
           BorderRightColor() == o.BorderRightColor();
  }
  bool BorderRightEquals(const BorderValue& o) const {
    return BorderRightWidthInternal().ToFloat() == o.Width() &&
           BorderRightStyle() == o.Style() &&
           BorderRightColor() == o.GetColor();
  }

  bool BorderRightVisuallyEqual(const ComputedStyle& o) const {
    if (BorderRightStyle() == EBorderStyle::kNone &&
        o.BorderRightStyle() == EBorderStyle::kNone)
      return true;
    if (BorderRightStyle() == EBorderStyle::kHidden &&
        o.BorderRightStyle() == EBorderStyle::kHidden)
      return true;
    return BorderRightEquals(o);
  }

  bool BorderTopVisuallyEqual(const ComputedStyle& o) const {
    if (BorderTopStyle() == EBorderStyle::kNone &&
        o.BorderTopStyle() == EBorderStyle::kNone)
      return true;
    if (BorderTopStyle() == EBorderStyle::kHidden &&
        o.BorderTopStyle() == EBorderStyle::kHidden)
      return true;
    return BorderTopEquals(o);
  }

  bool BorderTopEquals(const ComputedStyle& o) const {
    return BorderTopWidthInternal() == o.BorderTopWidthInternal() &&
           BorderTopStyle() == o.BorderTopStyle() &&
           BorderTopColor() == o.BorderTopColor();
  }
  bool BorderTopEquals(const BorderValue& o) const {
    return BorderTopWidthInternal().ToFloat() == o.Width() &&
           BorderTopStyle() == o.Style() && BorderTopColor() == o.GetColor();
  }

  bool BorderBottomVisuallyEqual(const ComputedStyle& o) const {
    if (BorderBottomStyle() == EBorderStyle::kNone &&
        o.BorderBottomStyle() == EBorderStyle::kNone)
      return true;
    if (BorderBottomStyle() == EBorderStyle::kHidden &&
        o.BorderBottomStyle() == EBorderStyle::kHidden)
      return true;
    return BorderBottomEquals(o);
  }

  bool BorderBottomEquals(const ComputedStyle& o) const {
    return BorderBottomWidthInternal() == o.BorderBottomWidthInternal() &&
           BorderBottomStyle() == o.BorderBottomStyle() &&
           BorderBottomColor() == o.BorderBottomColor();
  }
  bool BorderBottomEquals(const BorderValue& o) const {
    return BorderBottomWidthInternal().ToFloat() == o.Width() &&
           BorderBottomStyle() == o.Style() &&
           BorderBottomColor() == o.GetColor();
  }

  bool BorderEquals(const ComputedStyle& o) const {
    return BorderLeftEquals(o) && BorderRightEquals(o) && BorderTopEquals(o) &&
           BorderBottomEquals(o) && BorderImage() == o.BorderImage();
  }

  bool BorderVisuallyEqual(const ComputedStyle& o) const {
    return BorderLeftVisuallyEqual(o) && BorderRightVisuallyEqual(o) &&
           BorderTopVisuallyEqual(o) && BorderBottomVisuallyEqual(o) &&
           BorderImage() == o.BorderImage();
  }

  bool BorderVisualOverflowEqual(const ComputedStyle& o) const {
    return BorderImage().Outset() == o.BorderImage().Outset();
  }

  CORE_EXPORT void AdjustDiffForBackgroundVisuallyEqual(
      const ComputedStyle& o,
      StyleDifference& diff) const;

  void ResetBorder() {
    ResetBorderImage();
    ResetBorderTop();
    ResetBorderRight();
    ResetBorderBottom();
    ResetBorderLeft();
    ResetBorderTopLeftRadius();
    ResetBorderTopRightRadius();
    ResetBorderBottomLeftRadius();
    ResetBorderBottomRightRadius();
  }

  void ResetBorderTop() {
    SetBorderTopStyle(EBorderStyle::kNone);
    SetBorderTopWidth(3);
    SetBorderTopColorInternal(StyleColor::CurrentColor());
  }
  void ResetBorderRight() {
    SetBorderRightStyle(EBorderStyle::kNone);
    SetBorderRightWidth(3);
    SetBorderRightColorInternal(StyleColor::CurrentColor());
  }
  void ResetBorderBottom() {
    SetBorderBottomStyle(EBorderStyle::kNone);
    SetBorderBottomWidth(3);
    SetBorderBottomColorInternal(StyleColor::CurrentColor());
  }
  void ResetBorderLeft() {
    SetBorderLeftStyle(EBorderStyle::kNone);
    SetBorderLeftWidth(3);
    SetBorderLeftColorInternal(StyleColor::CurrentColor());
  }

  void SetBorderRadius(const LengthSize& s) {
    SetBorderTopLeftRadius(s);
    SetBorderTopRightRadius(s);
    SetBorderBottomLeftRadius(s);
    SetBorderBottomRightRadius(s);
  }
  void SetBorderRadius(const IntSize& s) {
    SetBorderRadius(
        LengthSize(Length::Fixed(s.Width()), Length::Fixed(s.Height())));
  }

  bool CanRenderBorderImage() const;

  // Float utility functions.
  bool IsFloating() const { return FloatingInternal() != EFloat::kNone; }
  EFloat UnresolvedFloating() const { return FloatingInternal(); }

  EFloat Floating(const ComputedStyle& cb_style) const {
    return Floating(cb_style.Direction());
  }

  EFloat Floating(TextDirection cb_direction) const {
    const EFloat value = FloatingInternal();
    switch (value) {
      case EFloat::kInlineStart:
      case EFloat::kInlineEnd: {
        return IsLtr(cb_direction) == (value == EFloat::kInlineStart)
                   ? EFloat::kLeft
                   : EFloat::kRight;
      }
      default:
        return value;
    }
  }

  // Mix-blend-mode utility functions.
  bool HasBlendMode() const { return GetBlendMode() != BlendMode::kNormal; }

  // Motion utility functions.
  bool HasOffset() const {
    return !OffsetPosition().X().IsAuto() || OffsetPath();
  }

  // Direction utility functions.
  bool IsLeftToRightDirection() const {
    return Direction() == TextDirection::kLtr;
  }

  // Perspective utility functions.
  bool HasPerspective() const { return Perspective() > 0; }

  // Outline utility functions.
  // HasOutline is insufficient to determine whether Node has an outline.
  // Use NGOutlineUtils::HasPaintedOutline instead.
  bool HasOutline() const {
    return OutlineWidth() > 0 && OutlineStyle() > EBorderStyle::kHidden;
  }
  CORE_EXPORT int OutlineOutsetExtent() const;
  CORE_EXPORT float GetOutlineStrokeWidthForFocusRing() const;
  bool HasOutlineWithCurrentColor() const {
    return HasOutline() && OutlineColor().IsCurrentColor();
  }

  // Position utility functions.
  bool HasOutOfFlowPosition() const {
    return GetPosition() == EPosition::kAbsolute ||
           GetPosition() == EPosition::kFixed;
  }
  bool HasInFlowPosition() const {
    return GetPosition() == EPosition::kRelative ||
           GetPosition() == EPosition::kSticky;
  }
  bool HasViewportConstrainedPosition() const {
    return GetPosition() == EPosition::kFixed;
  }
  bool HasStickyConstrainedPosition() const {
    return GetPosition() == EPosition::kSticky &&
           (!Top().IsAuto() || !Left().IsAuto() || !Right().IsAuto() ||
            !Bottom().IsAuto());
  }

  // Clear utility functions.
  bool HasClear() const { return ClearInternal() != EClear::kNone; }
  EClear UnresolvedClear() const { return ClearInternal(); }

  EClear Clear(const ComputedStyle& cb_style) const {
    return Clear(cb_style.Direction());
  }

  EClear Clear(TextDirection cb_direction) const {
    const EClear value = ClearInternal();
    switch (value) {
      case EClear::kInlineStart:
      case EClear::kInlineEnd: {
        return IsLtr(cb_direction) == (value == EClear::kInlineStart)
                   ? EClear::kLeft
                   : EClear::kRight;
      }
      default:
        return value;
    }
  }

  // Clip utility functions.
  const Length& ClipLeft() const { return Clip().Left(); }
  const Length& ClipRight() const { return Clip().Right(); }
  const Length& ClipTop() const { return Clip().Top(); }
  const Length& ClipBottom() const { return Clip().Bottom(); }

  // Offset utility functions.
  // Accessors for positioned object edges that take into account writing mode.
  const Length& LogicalInlineStart() const {
    return PhysicalBoundsToLogical().InlineStart();
  }
  const Length& LogicalInlineEnd() const {
    return PhysicalBoundsToLogical().InlineEnd();
  }
  const Length& LogicalLeft() const {
    return PhysicalBoundsToLogical().LineLeft();
  }
  const Length& LogicalRight() const {
    return PhysicalBoundsToLogical().LineRight();
  }
  const Length& LogicalTop() const {
    return PhysicalBoundsToLogical().Before();
  }
  const Length& LogicalBottom() const {
    return PhysicalBoundsToLogical().After();
  }
  bool OffsetEqual(const ComputedStyle& other) const {
    return Left() == other.Left() && Right() == other.Right() &&
           Top() == other.Top() && Bottom() == other.Bottom();
  }

  // Whether or not a positioned element requires normal flow x/y to be computed
  // to determine its position.
  bool HasAutoLeftAndRight() const {
    return Left().IsAuto() && Right().IsAuto();
  }
  bool HasAutoTopAndBottom() const {
    return Top().IsAuto() && Bottom().IsAuto();
  }
  bool HasStaticInlinePosition(bool horizontal) const {
    return horizontal ? HasAutoLeftAndRight() : HasAutoTopAndBottom();
  }
  bool HasStaticBlockPosition(bool horizontal) const {
    return horizontal ? HasAutoTopAndBottom() : HasAutoLeftAndRight();
  }

  // Content utility functions.
  bool ContentDataEquivalent(const ComputedStyle& other) const {
    return DataEquivalent(GetContentData(), other.GetContentData());
  }

  // Contain utility functions.
  bool ContainsPaint() const { return Contain() & kContainsPaint; }
  bool ContainsStyle() const { return Contain() & kContainsStyle; }
  bool ContainsLayout() const { return Contain() & kContainsLayout; }
  bool ContainsSize() const { return Contain() & kContainsSize; }

  // Display utility functions.
  bool IsDisplayReplacedType() const {
    return IsDisplayReplacedType(Display());
  }
  bool IsDisplayInlineType() const { return IsDisplayInlineType(Display()); }
  bool IsOriginalDisplayInlineType() const {
    return IsDisplayInlineType(OriginalDisplay());
  }
  bool IsDisplayBlockContainer() const {
    return IsDisplayBlockContainer(Display());
  }
  bool IsDisplayTableBox() const { return IsDisplayTableBox(Display()); }
  bool IsDisplayFlexibleOrGridBox() const {
    return IsDisplayFlexibleBox(Display()) || IsDisplayGridBox(Display());
  }
  bool IsDisplayLayoutCustomBox() const {
    return IsDisplayLayoutCustomBox(Display());
  }

  bool IsDisplayTableType() const { return IsDisplayTableType(Display()); }

  bool IsDisplayMathType() const { return IsDisplayMathBox(Display()); }

  bool BlockifiesChildren() const {
    return IsDisplayFlexibleOrGridBox() || IsDisplayMathType() ||
           IsDisplayLayoutCustomBox() ||
           (Display() == EDisplay::kContents && IsInBlockifyingDisplay());
  }

  // Isolation utility functions.
  bool HasIsolation() const { return Isolation() != EIsolation::kAuto; }

  // Content utility functions.
  bool ContentBehavesAsNormal() const {
    switch (StyleType()) {
      case kPseudoIdMarker:
        return !GetContentData();
      default:
        return !GetContentData() || GetContentData()->IsNone();
    }
  }
  bool ContentPreventsBoxGeneration() const {
    switch (StyleType()) {
      case kPseudoIdBefore:
      case kPseudoIdAfter:
        return ContentBehavesAsNormal();
      case kPseudoIdMarker:
        return GetContentData() && GetContentData()->IsNone();
      default:
        return false;
    }
  }

  // Cursor utility functions.
  CursorList* Cursors() const { return CursorDataInternal().Get(); }
  CORE_EXPORT void AddCursor(StyleImage*,
                             bool hot_spot_specified,
                             const IntPoint& hot_spot = IntPoint());
  void SetCursorList(CursorList*);
  void ClearCursorList();

  // Resize utility functions.
  bool HasResize() const { return ResizeInternal() != EResize::kNone; }
  EResize UnresolvedResize() const { return ResizeInternal(); }

  EResize Resize(const ComputedStyle& cb_style) const {
    EResize value = ResizeInternal();
    switch (value) {
      case EResize::kBlock:
      case EResize::kInline: {
        return ::blink::IsHorizontalWritingMode(cb_style.GetWritingMode()) ==
                       (value == EResize::kBlock)
                   ? EResize::kVertical
                   : EResize::kHorizontal;
      }
      default:
        return value;
    }
  }

  // Text decoration utility functions.
  bool TextDecorationVisualOverflowEqual(const ComputedStyle& o) const;
  void ApplyTextDecorations(const Color& parent_text_decoration_color,
                            bool override_existing_colors);
  void ClearAppliedTextDecorations();
  void RestoreParentTextDecorations(const ComputedStyle& parent_style);
  CORE_EXPORT const Vector<AppliedTextDecoration>& AppliedTextDecorations()
      const;
  CORE_EXPORT TextDecoration TextDecorationsInEffect() const;

  // Overflow utility functions.

  EOverflow OverflowInlineDirection() const {
    return IsHorizontalWritingMode() ? OverflowX() : OverflowY();
  }
  EOverflow OverflowBlockDirection() const {
    return IsHorizontalWritingMode() ? OverflowY() : OverflowX();
  }

  // Returns true if 'overflow' is 'visible' along both axes. When 'clip' is
  // used the other axis may be 'visible'. In other words, if one axis is
  // 'visible' the other axis is not necessarily 'visible.'
  bool IsOverflowVisibleAlongBothAxes() const {
    // Overflip clip and overflow visible may be used along different axis.
    return OverflowX() == EOverflow::kVisible &&
           OverflowY() == EOverflow::kVisible;
  }

  // An overflow value of visible or clip is not a scroll container, all other
  // values result in a scroll container. Also note that if visible or clip is
  // set on one axis, then the other axis must also be visible or clip. For
  // example, "overflow-x: clip; overflow-y: visible" is allowed, but
  // "overflow-x: clip; overflow-y: hidden" is not.
  bool IsScrollContainer() const {
    return OverflowX() != EOverflow::kVisible &&
           OverflowX() != EOverflow::kClip;
  }

  bool IsDisplayTableRowOrColumnType() const {
    return Display() == EDisplay::kTableRow ||
           Display() == EDisplay::kTableRowGroup ||
           Display() == EDisplay::kTableColumn ||
           Display() == EDisplay::kTableColumnGroup;
  }

  bool HasAutoHorizontalScroll() const {
    return OverflowX() == EOverflow::kAuto ||
           OverflowX() == EOverflow::kOverlay;
  }

  bool HasAutoVerticalScroll() const {
    return OverflowY() == EOverflow::kAuto ||
           OverflowY() == EOverflow::kOverlay;
  }

  bool ScrollsOverflowX() const {
    return OverflowX() == EOverflow::kScroll || HasAutoHorizontalScroll();
  }

  bool ScrollsOverflowY() const {
    return OverflowY() == EOverflow::kScroll || HasAutoVerticalScroll();
  }

  bool ScrollsOverflow() const {
    return ScrollsOverflowX() || ScrollsOverflowY();
  }

  // Visibility utility functions.
  bool VisibleToHitTesting() const {
    return Visibility() == EVisibility::kVisible &&
           PointerEvents() != EPointerEvents::kNone;
  }

  // Animation utility functions.
  bool ShouldCompositeForCurrentAnimations() const {
    return HasCurrentOpacityAnimation() || HasCurrentTransformAnimation() ||
           HasCurrentFilterAnimation() || HasCurrentBackdropFilterAnimation();
  }
  bool IsRunningAnimationOnCompositor() const {
    return IsRunningOpacityAnimationOnCompositor() ||
           IsRunningTransformAnimationOnCompositor() ||
           IsRunningFilterAnimationOnCompositor() ||
           IsRunningBackdropFilterAnimationOnCompositor();
  }

  // Opacity utility functions.
  bool HasOpacity() const { return Opacity() < 1.0f; }

  // Table layout utility functions.
  bool IsFixedTableLayout() const {
    return TableLayout() == ETableLayout::kFixed && !LogicalWidth().IsAuto();
  }

  // Returns true if the computed style contains a 3D transform operation. This
  // can be individual operations from the transform property, or individual
  // values from translate/rotate/scale properties. Perspective is omitted since
  // it does not, by itself, specify a 3D transform.
  bool Has3DTransformOperation() const {
    return Transform().HasNonPerspective3DOperation() ||
           (Translate() && Translate()->Z() != 0) ||
           (Rotate() && (Rotate()->X() != 0 || Rotate()->Y() != 0)) ||
           (Scale() && Scale()->Z() != 1);
  }
  // Returns true if the computed style contains a 3D transform operation with a
  // non-trivial component in the Z axis. This can be individual operations from
  // the transform property, or individual values from translate/rotate/scale
  // properties. Perspective is omitted since it does not, by itself, specify a
  // 3D transform.
  bool HasNonTrivial3DTransformOperation() const {
    return Transform().HasNonTrivial3DComponent() ||
           (Translate() && Translate()->Z() != 0) ||
           (Rotate() && Rotate()->Angle() != 0 &&
            (Rotate()->X() != 0 || Rotate()->Y() != 0)) ||
           (Scale() && Scale()->Z() != 1);
  }
  bool HasTransform() const {
    return HasTransformOperations() || HasOffset() ||
           HasCurrentTransformAnimation() || Translate() || Rotate() || Scale();
  }
  bool HasTransformOperations() const {
    return !Transform().Operations().IsEmpty();
  }
  ETransformStyle3D UsedTransformStyle3D() const {
    return HasGroupingPropertyForUsedTransformStyle3D()
               ? ETransformStyle3D::kFlat
               : TransformStyle3D();
  }
  // Returns whether the transform operations for |otherStyle| differ from the
  // operations for this style instance. Note that callers may want to also
  // check hasTransform(), as it is possible for two styles to have matching
  // transform operations but differ in other transform-impacting style
  // respects.
  bool TransformDataEquivalent(const ComputedStyle& other) const {
    return !DiffTransformData(*this, other);
  }
  bool Preserves3D() const {
    return UsedTransformStyle3D() != ETransformStyle3D::kFlat;
  }
  enum ApplyTransformOrigin {
    kIncludeTransformOrigin,
    kExcludeTransformOrigin
  };
  enum ApplyMotionPath { kIncludeMotionPath, kExcludeMotionPath };
  enum ApplyIndependentTransformProperties {
    kIncludeIndependentTransformProperties,
    kExcludeIndependentTransformProperties
  };
  void ApplyTransform(TransformationMatrix&,
                      const LayoutSize& border_box_data_size,
                      ApplyTransformOrigin,
                      ApplyMotionPath,
                      ApplyIndependentTransformProperties) const;
  void ApplyTransform(TransformationMatrix&,
                      const FloatRect& bounding_box,
                      ApplyTransformOrigin,
                      ApplyMotionPath,
                      ApplyIndependentTransformProperties) const;

  bool HasFilters() const;

  // Returns |true| if any property that renders using filter operations is
  // used (including, but not limited to, 'filter' and 'box-reflect').
  bool HasFilterInducingProperty() const {
    return HasNonInitialFilter() || HasBoxReflect();
  }

  // Returns |true| if filter should be considered to have non-initial value
  // for the purposes of containing blocks.
  bool HasNonInitialFilter() const {
    return HasFilter() || HasWillChangeFilterHint();
  }

  // Returns |true| if backdrop-filter should be considered to have non-initial
  // value for the purposes of containing blocks.
  bool HasNonInitialBackdropFilter() const {
    return HasBackdropFilter() || HasWillChangeBackdropFilterHint();
  }

  // Returns |true| if opacity should be considered to have non-initial value
  // for the purpose of creating stacking contexts.
  bool HasNonInitialOpacity() const {
    return HasOpacity() || HasWillChangeOpacityHint() ||
           HasCurrentOpacityAnimation();
  }

  // Returns whether this style contains any grouping property as defined by
  // https://drafts.csswg.org/css-transforms-2/#grouping-property-values.
  //
  // |has_box_reflection| is a parameter instead of checking |BoxReflect()|
  // because box reflection styles only apply for some objects (see:
  // |LayoutObject::HasReflection()|).
  bool HasGroupingProperty(bool has_box_reflection) const {
    if (HasStackingGroupingProperty(has_box_reflection))
      return true;
    // TODO(pdr): Also check for overflow because the spec requires "overflow:
    // any value other than visible or clip."
    if (!HasAutoClip() && HasOutOfFlowPosition())
      return true;
    return false;
  }

  // This is the subset of grouping properties (see: |HasGroupingProperty|) that
  // also create stacking contexts.
  bool HasStackingGroupingProperty(bool has_box_reflection) const {
    if (HasNonInitialOpacity())
      return true;
    if (HasNonInitialFilter())
      return true;
    if (has_box_reflection)
      return true;
    if (ClipPath())
      return true;
    if (HasIsolation())
      return true;
    if (HasMask())
      return true;
    if (HasBlendMode())
      return true;
    if (HasNonInitialBackdropFilter())
      return true;
    return false;
  }

  // Grouping requires creating a flattened representation of the descendant
  // elements before they can be applied, and therefore force the element to
  // have a used style of flat for preserve-3d.
  // Until |RuntimeEnabledFeatures::TransformInteropEnabled()| launches, the
  // approach is different from the spec to maintain backwards compatibility.
  // TODO(chrishtr): replace this with |HasGroupingProperty()|.
  CORE_EXPORT bool HasGroupingPropertyForUsedTransformStyle3D() const {
    if (RuntimeEnabledFeatures::TransformInteropEnabled()) {
      return HasGroupingProperty(BoxReflect()) ||
             !IsOverflowVisibleAlongBothAxes();
    }
    return !IsOverflowVisibleAlongBothAxes() || HasFilterInducingProperty() ||
           HasNonInitialOpacity();
  }

  // Return true if any transform related property (currently
  // transform/motionPath, transformStyle3D, perspective, or
  // will-change:transform) indicates that we are transforming.
  // will-change:transform should result in the same rendering behavior as
  // having a transform, including the creation of a containing block for fixed
  // position descendants.
  bool HasTransformRelatedProperty() const {
    return HasTransform() || Preserves3D() || HasPerspective() ||
           HasWillChangeTransformHint();
  }

  // Paint utility functions.
  CORE_EXPORT void AddPaintImage(StyleImage*);

  // Returns true if any property has an <image> value that is a CSS paint
  // function that is using a given custom property.
  bool HasCSSPaintImagesUsingCustomProperty(
      const AtomicString& custom_property_name,
      const Document&) const;

  // FIXME: reflections should belong to this helper function but they are
  // currently handled through their self-painting layers. So the layout code
  // doesn't account for them.
  bool HasVisualOverflowingEffect() const {
    return BoxShadow() || HasBorderImageOutsets() || HasOutline() ||
           HasMaskBoxImageOutsets();
  }

  // Stacking contexts and positioned elements[1] are stacked (sorted in
  // negZOrderList
  // and posZOrderList) in their enclosing stacking contexts.
  //
  // [1] According to CSS2.1, Appendix E.2.8
  // (https://www.w3.org/TR/CSS21/zindex.html),
  // positioned elements with 'z-index: auto' are "treated as if it created a
  // new stacking context" and z-ordered together with other elements with
  // 'z-index: 0'.  The difference of them from normal stacking contexts is that
  // they don't determine the stacking of the elements underneath them.  (Note:
  // There are also other elements treated as stacking context during painting,
  // but not managed in stacks. See ObjectPainter::PaintAllPhasesAtomically().)
  CORE_EXPORT void UpdateIsStackingContextWithoutContainment(
      bool is_document_element,
      bool is_in_top_layer,
      bool is_svg_stacking);
  bool IsStackedWithoutContainment() const {
    return IsStackingContextWithoutContainment() ||
           GetPosition() != EPosition::kStatic;
  }

  // Pseudo element styles.
  bool HasAnyPseudoElementStyles() const;
  bool HasPseudoElementStyle(PseudoId) const;
  void SetHasPseudoElementStyle(PseudoId);

  // Note: CanContainAbsolutePositionObjects should return true if
  // CanContainFixedPositionObjects.  We currently never use this value
  // directly, always OR'ing it with CanContainFixedPositionObjects.
  bool CanContainAbsolutePositionObjects() const {
    return GetPosition() != EPosition::kStatic;
  }

  // Whitespace utility functions.
  static bool Is(EWhiteSpace a, EWhiteSpace b) {
    return static_cast<unsigned>(a) & static_cast<unsigned>(b);
  }
  static bool IsNot(EWhiteSpace a, EWhiteSpace b) { return !Is(a, b); }
  static bool AutoWrap(EWhiteSpace ws) {
    // Nowrap and pre don't automatically wrap.
    return IsNot(ws, EWhiteSpace::kNowrap | EWhiteSpace::kPre);
  }

  bool AutoWrap() const { return AutoWrap(WhiteSpace()); }

  static bool PreserveNewline(EWhiteSpace ws) {
    // Normal and nowrap do not preserve newlines.
    return ws != EWhiteSpace::kNormal && ws != EWhiteSpace::kNowrap;
  }

  bool PreserveNewline() const { return PreserveNewline(WhiteSpace()); }

  static bool BorderStyleIsVisible(EBorderStyle style) {
    return style != EBorderStyle::kNone && style != EBorderStyle::kHidden;
  }

  static bool CollapseWhiteSpace(EWhiteSpace ws) {
    // Pre and prewrap do not collapse whitespace.
    return IsNot(ws, EWhiteSpace::kPre | EWhiteSpace::kPreWrap |
                         EWhiteSpace::kBreakSpaces);
  }

  bool CollapseWhiteSpace() const { return CollapseWhiteSpace(WhiteSpace()); }

  bool IsCollapsibleWhiteSpace(UChar c) const {
    switch (c) {
      case ' ':
      case '\t':
        return CollapseWhiteSpace();
      case '\n':
        return !PreserveNewline();
    }
    return false;
  }
  bool BreakOnlyAfterWhiteSpace() const {
    return Is(WhiteSpace(),
              EWhiteSpace::kPreWrap | EWhiteSpace::kBreakSpaces) ||
           GetLineBreak() == LineBreak::kAfterWhiteSpace;
  }

  bool BreakWords() const {
    return (WordBreak() == EWordBreak::kBreakWord ||
            OverflowWrap() != EOverflowWrap::kNormal) &&
           IsNot(WhiteSpace(), EWhiteSpace::kPre | EWhiteSpace::kNowrap);
  }

  // Text direction utility functions.
  bool ShouldPlaceBlockDirectionScrollbarOnLogicalLeft() const {
    return !IsLeftToRightDirection() && IsHorizontalWritingMode();
  }

  // Border utility functions.
  bool BorderObscuresBackground() const;
  void GetBorderEdgeInfo(
      BorderEdge edges[],
      PhysicalBoxSides sides_to_include = PhysicalBoxSides()) const;

  bool HasBoxDecorations() const {
    return HasBorderDecoration() || HasBorderRadius() || HasOutline() ||
           HasEffectiveAppearance() || BoxShadow() ||
           HasFilterInducingProperty() || HasNonInitialBackdropFilter() ||
           HasResize();
  }

  // "Box decoration background" includes all box decorations and backgrounds
  // that are painted as the background of the object. It includes borders,
  // box-shadows, background-color and background-image, etc.
  bool HasBoxDecorationBackground() const {
    return HasBackground() || HasBorderDecoration() ||
           HasEffectiveAppearance() || BoxShadow();
  }

  LayoutRectOutsets BoxDecorationOutsets() const;

  // Background utility functions.
  FillLayer& AccessBackgroundLayers() { return MutableBackgroundInternal(); }
  const FillLayer& BackgroundLayers() const { return BackgroundInternal(); }
  void AdjustBackgroundLayers() {
    if (BackgroundLayers().Next()) {
      AccessBackgroundLayers().CullEmptyLayers();
      AccessBackgroundLayers().FillUnsetProperties();
    }
  }
  bool HasBackgroundRelatedColorReferencingCurrentColor() const {
    if (BackgroundColor().IsCurrentColor() ||
        InternalVisitedBackgroundColor().IsCurrentColor())
      return true;
    if (!BoxShadow())
      return false;
    return ShadowListHasCurrentColor(BoxShadow());
  }
  bool HasBackground() const {
    Color color = VisitedDependentColor(GetCSSPropertyBackgroundColor());
    if (color.Alpha())
      return true;
    return HasBackgroundImage();
  }

  // Color utility functions.
  CORE_EXPORT Color
  VisitedDependentColor(const CSSProperty& color_property) const;

  // Helper for resolving a StyleColor which may contain currentColor or a
  // system color keyword. This is intended for cases such as SVG <paint> where
  // a given property consists of a StyleColor plus additional information. For
  // <color> properties, prefer VisitedDependentColor() or
  // Longhand::ColorIncludingFallback() instead.
  Color ResolvedColor(const StyleColor& color) const;

  // -webkit-appearance utility functions.
  bool HasEffectiveAppearance() const {
    return EffectiveAppearance() != kNoControlPart;
  }
  bool IsCheckboxOrRadioPart() const {
    return HasEffectiveAppearance() &&
           (EffectiveAppearance() == kCheckboxPart ||
            EffectiveAppearance() == kRadioPart);
  }

  // Other utility functions.
  bool RequireTransformOrigin(ApplyTransformOrigin apply_origin,
                              ApplyMotionPath) const;

  InterpolationQuality GetInterpolationQuality() const;

  bool CanGeneratePseudoElement(PseudoId pseudo) const {
    if (Display() == EDisplay::kNone)
      return false;
    if (IsEnsuredInDisplayNone())
      return false;
    if (pseudo == kPseudoIdMarker)
      return Display() == EDisplay::kListItem;
    if (!HasPseudoElementStyle(pseudo))
      return false;
    if (Display() != EDisplay::kContents)
      return true;
    // For display: contents elements, we still need to generate ::before and
    // ::after, but the rest of the pseudo-elements should only be used for
    // elements with an actual layout object.
    return pseudo == kPseudoIdBefore || pseudo == kPseudoIdAfter;
  }

  // Load the images of CSS properties that were deferred by LazyLoad.
  void LoadDeferredImages(Document&) const;

  enum ColorScheme ComputedColorScheme() const {
    return DarkColorScheme() ? ColorScheme::kDark : ColorScheme::kLight;
  }

  enum ColorScheme UsedColorScheme() const {
    return RuntimeEnabledFeatures::CSSColorSchemeUARenderingEnabled()
               ? ComputedColorScheme()
               : ColorScheme::kLight;
  }

  enum ColorScheme UsedColorSchemeForInitialColors() const {
    return ComputedColorScheme();
  }

  StyleColor InitialColorForColorScheme() const {
    // TODO(crbug.com/1046753, crbug.com/929098): The initial value of the color
    // property should be canvastext, but since we do not yet ship color-scheme
    // aware system colors, we use this method instead. This should be replaced
    // by default_value:"canvastext" in css_properties.json5.
    return StyleColor(DarkColorScheme() ? Color::kWhite : Color::kBlack);
  }

  bool GeneratesMarkerImage() const {
    return Display() == EDisplay::kListItem && ListStyleImage() &&
           !ListStyleImage()->ErrorOccurred();
  }

  base::Optional<LogicalSize> LogicalAspectRatio() const {
    if (!AspectRatio())
      return base::nullopt;
    IntSize ratio = *AspectRatio();
    if (!IsHorizontalWritingMode())
      ratio = ratio.TransposedSize();
    return LogicalSize(LayoutUnit(ratio.Width()), LayoutUnit(ratio.Height()));
  }

 private:
  EClear Clear() const { return ClearInternal(); }
  EFloat Floating() const { return FloatingInternal(); }
  EResize Resize() const { return ResizeInternal(); }

  void SetInternalVisitedColor(const StyleColor& v) {
    SetInternalVisitedColorInternal(v);
  }
  void SetInternalVisitedBackgroundColor(const StyleColor& v) {
    SetInternalVisitedBackgroundColorInternal(v);
  }
  void SetInternalVisitedBorderLeftColor(const StyleColor& v) {
    SetInternalVisitedBorderLeftColorInternal(v);
  }
  void SetInternalVisitedBorderRightColor(const StyleColor& v) {
    SetInternalVisitedBorderRightColorInternal(v);
  }
  void SetInternalVisitedBorderBottomColor(const StyleColor& v) {
    SetInternalVisitedBorderBottomColorInternal(v);
  }
  void SetInternalVisitedBorderTopColor(const StyleColor& v) {
    SetInternalVisitedBorderTopColorInternal(v);
  }
  void SetInternalVisitedOutlineColor(const StyleColor& v) {
    SetInternalVisitedOutlineColorInternal(v);
  }
  void SetInternalVisitedColumnRuleColor(const StyleColor& v) {
    SetInternalVisitedColumnRuleColorInternal(v);
  }
  void SetInternalVisitedTextDecorationColor(const StyleColor& v) {
    SetInternalVisitedTextDecorationColorInternal(v);
  }
  void SetInternalVisitedTextEmphasisColor(const StyleColor& color) {
    SetInternalVisitedTextEmphasisColorInternal(color);
  }
  void SetInternalVisitedTextFillColor(const StyleColor& color) {
    SetInternalVisitedTextFillColorInternal(color);
  }
  void SetInternalVisitedTextStrokeColor(const StyleColor& color) {
    SetInternalVisitedTextStrokeColorInternal(color);
  }

  static bool IsDisplayBlockContainer(EDisplay display) {
    return display == EDisplay::kBlock || display == EDisplay::kListItem ||
           display == EDisplay::kInlineBlock ||
           display == EDisplay::kFlowRoot || display == EDisplay::kTableCell ||
           display == EDisplay::kTableCaption;
  }

  static bool IsDisplayTableBox(EDisplay display) {
    return display == EDisplay::kTable || display == EDisplay::kInlineTable;
  }

  static bool IsDisplayFlexibleBox(EDisplay display) {
    return display == EDisplay::kFlex || display == EDisplay::kInlineFlex;
  }

  static bool IsDisplayGridBox(EDisplay display) {
    return display == EDisplay::kGrid || display == EDisplay::kInlineGrid;
  }

  static bool IsDisplayMathBox(EDisplay display) {
    return display == EDisplay::kMath || display == EDisplay::kInlineMath;
  }

  static bool IsDisplayLayoutCustomBox(EDisplay display) {
    return display == EDisplay::kLayoutCustom ||
           display == EDisplay::kInlineLayoutCustom;
  }

  static bool IsDisplayReplacedType(EDisplay display) {
    return display == EDisplay::kInlineBlock ||
           display == EDisplay::kWebkitInlineBox ||
           display == EDisplay::kInlineFlex ||
           display == EDisplay::kInlineTable ||
           display == EDisplay::kInlineGrid ||
           display == EDisplay::kInlineMath ||
           display == EDisplay::kInlineLayoutCustom;
  }

  static bool IsDisplayInlineType(EDisplay display) {
    return display == EDisplay::kInline || IsDisplayReplacedType(display);
  }

  static bool IsDisplayTableType(EDisplay display) {
    return display == EDisplay::kTable || display == EDisplay::kInlineTable ||
           display == EDisplay::kTableRowGroup ||
           display == EDisplay::kTableHeaderGroup ||
           display == EDisplay::kTableFooterGroup ||
           display == EDisplay::kTableRow ||
           display == EDisplay::kTableColumnGroup ||
           display == EDisplay::kTableColumn ||
           display == EDisplay::kTableCell ||
           display == EDisplay::kTableCaption;
  }

  // Color accessors are all private to make sure callers use
  // VisitedDependentColor instead to access them.
  const StyleColor& BorderLeftColor() const {
    return BorderLeftColorInternal();
  }
  const StyleColor& BorderRightColor() const {
    return BorderRightColorInternal();
  }
  const StyleColor& BorderTopColor() const { return BorderTopColorInternal(); }
  const StyleColor& BorderBottomColor() const {
    return BorderBottomColorInternal();
  }

  const StyleColor& BackgroundColor() const {
    return BackgroundColorInternal();
  }
  const StyleAutoColor& CaretColor() const { return CaretColorInternal(); }
  const StyleColor& GetColor() const { return ColorInternal(); }
  const StyleColor& ColumnRuleColor() const {
    return ColumnRuleColorInternal();
  }
  const StyleColor& OutlineColor() const { return OutlineColorInternal(); }
  const StyleColor& TextDecorationColor() const {
    return TextDecorationColorInternal();
  }
  const StyleColor& TextEmphasisColor() const {
    return TextEmphasisColorInternal();
  }
  const StyleColor& TextFillColor() const { return TextFillColorInternal(); }
  const StyleColor& TextStrokeColor() const {
    return TextStrokeColorInternal();
  }
  const StyleColor& InternalVisitedColor() const {
    return InternalVisitedColorInternal();
  }
  const StyleAutoColor& InternalVisitedCaretColor() const {
    return InternalVisitedCaretColorInternal();
  }
  const StyleColor& InternalVisitedBackgroundColor() const {
    return InternalVisitedBackgroundColorInternal();
  }
  const StyleColor& InternalVisitedBorderLeftColor() const {
    return InternalVisitedBorderLeftColorInternal();
  }
  bool InternalVisitedBorderLeftColorHasNotChanged(
      const ComputedStyle& other) const {
    return (InternalVisitedBorderLeftColor() ==
                other.InternalVisitedBorderLeftColor() ||
            !BorderLeftWidth());
  }
  const StyleColor& InternalVisitedBorderRightColor() const {
    return InternalVisitedBorderRightColorInternal();
  }
  bool InternalVisitedBorderRightColorHasNotChanged(
      const ComputedStyle& other) const {
    return (InternalVisitedBorderRightColor() ==
                other.InternalVisitedBorderRightColor() ||
            !BorderRightWidth());
  }
  const StyleColor& InternalVisitedBorderBottomColor() const {
    return InternalVisitedBorderBottomColorInternal();
  }
  bool InternalVisitedBorderBottomColorHasNotChanged(
      const ComputedStyle& other) const {
    return (InternalVisitedBorderBottomColor() ==
                other.InternalVisitedBorderBottomColor() ||
            !BorderBottomWidth());
  }
  const StyleColor& InternalVisitedBorderTopColor() const {
    return InternalVisitedBorderTopColorInternal();
  }
  bool InternalVisitedBorderTopColorHasNotChanged(
      const ComputedStyle& other) const {
    return (InternalVisitedBorderTopColor() ==
                other.InternalVisitedBorderTopColor() ||
            !BorderTopWidth());
  }
  const StyleColor& InternalVisitedOutlineColor() const {
    return InternalVisitedOutlineColorInternal();
  }
  bool InternalVisitedOutlineColorHasNotChanged(
      const ComputedStyle& other) const {
    return (InternalVisitedOutlineColor() ==
                other.InternalVisitedOutlineColor() ||
            !OutlineWidth());
  }
  const StyleColor& InternalVisitedColumnRuleColor() const {
    return InternalVisitedColumnRuleColorInternal();
  }
  const StyleColor& InternalVisitedTextDecorationColor() const {
    return InternalVisitedTextDecorationColorInternal();
  }
  const StyleColor& InternalVisitedTextEmphasisColor() const {
    return InternalVisitedTextEmphasisColorInternal();
  }
  const StyleColor& InternalVisitedTextFillColor() const {
    return InternalVisitedTextFillColorInternal();
  }
  const StyleColor& InternalVisitedTextStrokeColor() const {
    return InternalVisitedTextStrokeColorInternal();
  }

  StyleColor DecorationColorIncludingFallback(bool visited_link) const;

  const StyleColor& StopColor() const { return SvgStyle().StopColor(); }
  const StyleColor& FloodColor() const { return SvgStyle().FloodColor(); }
  const StyleColor& LightingColor() const { return SvgStyle().LightingColor(); }

  // Appearance accessors are private to make sure callers use
  // EffectiveAppearance in almost all cases.
  ControlPart Appearance() const { return AppearanceInternal(); }
  bool HasAppearance() const { return Appearance() != kNoControlPart; }

  void AddAppliedTextDecoration(const AppliedTextDecoration&);
  void OverrideTextDecorationColors(Color propagated_color);
  void ApplyMotionPathTransform(float origin_x,
                                float origin_y,
                                const FloatRect& bounding_box,
                                TransformationMatrix&) const;

  bool ScrollAnchorDisablingPropertyChanged(const ComputedStyle& other,
                                            const StyleDifference&) const;
  bool DiffNeedsFullLayoutAndPaintInvalidation(
      const ComputedStyle& other) const;
  bool DiffNeedsFullLayout(const Document&, const ComputedStyle& other) const;
  bool DiffNeedsFullLayoutForLayoutCustom(const Document&,
                                          const ComputedStyle& other) const;
  bool DiffNeedsFullLayoutForLayoutCustomChild(
      const Document&,
      const ComputedStyle& other) const;
  void AdjustDiffForNeedsPaintInvalidation(const ComputedStyle& other,
                                           StyleDifference&,
                                           const Document&) const;
  bool DiffNeedsPaintInvalidationForPaintImage(const StyleImage&,
                                               const ComputedStyle& other,
                                               const Document&) const;
  bool DiffNeedsVisualRectUpdate(const ComputedStyle& other) const;
  CORE_EXPORT void UpdatePropertySpecificDifferences(const ComputedStyle& other,
                                                     StyleDifference&) const;

  bool PropertiesEqual(const Vector<CSSPropertyID>& properties,
                       const ComputedStyle& other) const;
  CORE_EXPORT bool CustomPropertiesEqual(const Vector<AtomicString>& properties,
                                         const ComputedStyle& other) const;

  Color GetCurrentColor() const;
  Color GetInternalVisitedCurrentColor() const;

  static bool ShadowListHasCurrentColor(const ShadowList*);

  StyleInheritedVariables& MutableInheritedVariables();
  StyleNonInheritedVariables& MutableNonInheritedVariables();

  CORE_EXPORT void SetInitialData(scoped_refptr<StyleInitialData>);

  PhysicalToLogical<const Length&> PhysicalMarginToLogical(
      const ComputedStyle& other) const {
    return PhysicalToLogical<const Length&>(
        other.GetWritingMode(), other.Direction(), MarginTop(), MarginRight(),
        MarginBottom(), MarginLeft());
  }

  PhysicalToLogical<const Length&> PhysicalPaddingToLogical() const {
    return PhysicalToLogical<const Length&>(GetWritingMode(), Direction(),
                                            PaddingTop(), PaddingRight(),
                                            PaddingBottom(), PaddingLeft());
  }

  PhysicalToLogical<BorderValue> PhysicalBorderToLogical(
      const ComputedStyle& other) const {
    return PhysicalToLogical<BorderValue>(
        other.GetWritingMode(), other.Direction(), BorderTop(), BorderRight(),
        BorderBottom(), BorderLeft());
  }

  PhysicalToLogical<float> PhysicalBorderWidthToLogical() const {
    return PhysicalToLogical<float>(GetWritingMode(), Direction(),
                                    BorderTopWidth(), BorderRightWidth(),
                                    BorderBottomWidth(), BorderLeftWidth());
  }

  PhysicalToLogical<EBorderStyle> PhysicalBorderStyleToLogical() const {
    return PhysicalToLogical<EBorderStyle>(
        GetWritingMode(), Direction(), BorderTopStyle(), BorderRightStyle(),
        BorderBottomStyle(), BorderLeftStyle());
  }

  PhysicalToLogical<const Length&> PhysicalBoundsToLogical() const {
    return PhysicalToLogical<const Length&>(GetWritingMode(), Direction(),
                                            Top(), Right(), Bottom(), Left());
  }

  static Difference ComputeDifferenceIgnoringInheritedFirstLineStyle(
      const ComputedStyle& old_style,
      const ComputedStyle& new_style);

  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesRespectsTransformAnimation);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesCompositingReasonsTransforom);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesCompositingReasonsOpacity);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesCompositingReasonsFilter);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesCompositingReasonsBackdropFilter);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesCompositingReasonsInlineTransform);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesCompositingReasonsBackfaceVisibility);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesCompositingReasonsWillChange);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesCompositingReasonsUsedStylePreserve3D);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesCompositingReasonsOverflow);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      UpdatePropertySpecificDifferencesCompositingReasonsContainsPaint);
  FRIEND_TEST_ALL_PREFIXES(ComputedStyleTest,
                           UpdatePropertySpecificDifferencesHasAlpha);
  FRIEND_TEST_ALL_PREFIXES(ComputedStyleTest, CustomPropertiesEqual_Values);
  FRIEND_TEST_ALL_PREFIXES(ComputedStyleTest, CustomPropertiesEqual_Data);
  FRIEND_TEST_ALL_PREFIXES(ComputedStyleTest, InitialVariableNames);
  FRIEND_TEST_ALL_PREFIXES(ComputedStyleTest,
                           InitialAndInheritedAndNonInheritedVariableNames);
  FRIEND_TEST_ALL_PREFIXES(StyleCascadeTest, ForcedVisitedBackgroundColor);
  FRIEND_TEST_ALL_PREFIXES(
      ComputedStyleTest,
      TextDecorationEqualDoesNotRequireRecomputeInkOverflow);
  FRIEND_TEST_ALL_PREFIXES(ComputedStyleTest,
                           TextDecorationNotEqualRequiresRecomputeInkOverflow);
};

inline bool ComputedStyle::HasAnyPseudoElementStyles() const {
  return !!PseudoBitsInternal();
}

inline bool ComputedStyle::HasPseudoElementStyle(PseudoId pseudo) const {
  DCHECK(pseudo >= kFirstPublicPseudoId);
  DCHECK(pseudo < kFirstInternalPseudoId);
  return (1 << (pseudo - kFirstPublicPseudoId)) & PseudoBitsInternal();
}

inline void ComputedStyle::SetHasPseudoElementStyle(PseudoId pseudo) {
  DCHECK(pseudo >= kFirstPublicPseudoId);
  DCHECK(pseudo < kFirstInternalPseudoId);
  // TODO: Fix up this code. It is hard to understand.
  SetPseudoBitsInternal(static_cast<PseudoId>(
      PseudoBitsInternal() | 1 << (pseudo - kFirstPublicPseudoId)));
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COMPUTED_STYLE_H_
