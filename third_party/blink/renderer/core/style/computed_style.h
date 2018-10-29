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
#include "third_party/blink/renderer/core/computed_style_base.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/style_auto_color.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/core/style/border_value.h"
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
#include "third_party/blink/renderer/platform/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
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
class FloatRoundedRect;
class Hyphenation;
class NinePieceImage;
class ShadowList;
class ShapeValue;
class StyleContentAlignmentData;
class StyleDifference;
class StyleImage;
class StyleInheritedVariables;
class StylePath;
class StyleResolver;
class StyleSelfAlignmentData;
class TransformationMatrix;

typedef Vector<scoped_refptr<ComputedStyle>, 4> PseudoStyleCache;

namespace CSSLonghand {

class BackgroundColor;
class BorderBottomColor;
class BorderLeftColor;
class BorderRightColor;
class BorderTopColor;
class CaretColor;
class Color;
class ColumnRuleColor;
class FloodColor;
class Fill;
class LightingColor;
class OutlineColor;
class StopColor;
class Stroke;
class TextDecorationColor;
class WebkitTapHighlightColor;
class WebkitTextEmphasisColor;
class WebkitTextFillColor;
class WebkitTextStrokeColor;

}  // namespace CSSLonghand

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
// include cached_pseudo_styles_ (for storing pseudo element styles), unique_
// (for style caching) and has_simple_underline_ (cached indicator flag of
// text-decoration). These are stored on ComputedStyle for two reasons:
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
  // Used by Web Animations CSS. Sets the color styles.
  friend class AnimatedStyleBuilder;
  // Used by Web Animations CSS. Gets visited and unvisited colors separately.
  friend class CSSAnimatableValueFactory;
  // Used by CSS animations. We can't allow them to animate based off visited
  // colors.
  friend class CSSPropertyEquality;

  // Accesses GetColor().
  friend class ComputedStyleUtils;
  // These get visited and unvisited colors separately.
  friend class CSSLonghand::BackgroundColor;
  friend class CSSLonghand::BorderBottomColor;
  friend class CSSLonghand::BorderLeftColor;
  friend class CSSLonghand::BorderRightColor;
  friend class CSSLonghand::BorderTopColor;
  friend class CSSLonghand::CaretColor;
  friend class CSSLonghand::Color;
  friend class CSSLonghand::ColumnRuleColor;
  friend class CSSLonghand::FloodColor;
  friend class CSSLonghand::Fill;
  friend class CSSLonghand::LightingColor;
  friend class CSSLonghand::OutlineColor;
  friend class CSSLonghand::StopColor;
  friend class CSSLonghand::Stroke;
  friend class CSSLonghand::TextDecorationColor;
  friend class CSSLonghand::WebkitTapHighlightColor;
  friend class CSSLonghand::WebkitTextEmphasisColor;
  friend class CSSLonghand::WebkitTextFillColor;
  friend class CSSLonghand::WebkitTextStrokeColor;
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

  // FIXME: When we stop resolving currentColor at style time, these can be
  // removed.
  friend class CSSToStyleMap;
  friend class FilterOperationResolver;
  friend class StyleBuilderConverter;
  friend class StyleResolverState;
  friend class StyleResolver;

 protected:
  // list of associated pseudo styles
  std::unique_ptr<PseudoStyleCache> cached_pseudo_styles_;

  DataRef<SVGComputedStyle> svg_style_;

 private:
  // TODO(sashab): Move these private members to the bottom of ComputedStyle.
  ALWAYS_INLINE ComputedStyle();
  ALWAYS_INLINE ComputedStyle(const ComputedStyle&);

  static scoped_refptr<ComputedStyle> CreateInitialStyle();
  // TODO(crbug.com/794841): Remove this. Initial style should not be mutable.
  CORE_EXPORT static ComputedStyle& MutableInitialStyle();

 public:
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

  // Computes how the style change should be propagated down the tree.
  static StyleRecalcChange StylePropagationDiff(const ComputedStyle* old_style,
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

  StyleDifference VisualInvalidationDiff(const Document&,
                                         const ComputedStyle&) const;

  CORE_EXPORT void InheritFrom(const ComputedStyle& inherit_parent,
                               IsAtShadowBoundary = kNotAtShadowBoundary);
  void CopyNonInheritedFromCached(const ComputedStyle&);

  PseudoId StyleType() const { return static_cast<PseudoId>(StyleTypeInternal()); }
  void SetStyleType(PseudoId style_type) { SetStyleTypeInternal(style_type); }

  const ComputedStyle* GetCachedPseudoStyle(PseudoId) const;
  const ComputedStyle* AddCachedPseudoStyle(scoped_refptr<ComputedStyle>);
  void RemoveCachedPseudoStyle(PseudoId);

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
  bool HasFixedAttachmentBackgroundImage() const {
    return BackgroundInternal().AnyLayerHasFixedAttachmentImage();
  }
  bool HasOnlyFixedAttachmentBackgroundImage() const {
    return BackgroundInternal().AnyLayerHasFixedAttachmentImage() &&
           !BackgroundInternal().AnyLayerHasLocalAttachmentImage() &&
           !BackgroundInternal().AnyLayerHasDefaultAttachment();
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
  void SetBorderImageSource(StyleImage*);

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

  // Border color properties.
  // border-left-color
  void SetBorderLeftColor(const StyleColor& color) {
    if (BorderLeftColor() != color) {
      SetBorderLeftColorInternal(color.Resolve(Color()));
      SetBorderLeftColorIsCurrentColor(color.IsCurrentColor());
    }
  }

  // border-right-color
  void SetBorderRightColor(const StyleColor& color) {
    if (BorderRightColor() != color) {
      SetBorderRightColorInternal(color.Resolve(Color()));
      SetBorderRightColorIsCurrentColor(color.IsCurrentColor());
    }
  }

  // border-top-color
  void SetBorderTopColor(const StyleColor& color) {
    if (BorderTopColor() != color) {
      SetBorderTopColorInternal(color.Resolve(Color()));
      SetBorderTopColorIsCurrentColor(color.IsCurrentColor());
    }
  }

  // border-bottom-color
  void SetBorderBottomColor(const StyleColor& color) {
    if (BorderBottomColor() != color) {
      SetBorderBottomColorInternal(color.Resolve(Color()));
      SetBorderBottomColorIsCurrentColor(color.IsCurrentColor());
    }
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
  void SetColumnCount(unsigned short c) {
    SetHasAutoColumnCountInternal(false);
    SetColumnCountInternal(clampTo<unsigned short>(c, 1));
  }
  void SetHasAutoColumnCount() {
    SetHasAutoColumnCountInternal(true);
    SetColumnCountInternal(ComputedStyleInitialValues::InitialColumnCount());
  }

  // column-rule-color (aka -webkit-column-rule-color)
  void SetColumnRuleColor(const StyleColor& c) {
    if (ColumnRuleColor() != c) {
      SetColumnRuleColorInternal(c.Resolve(Color()));
      SetColumnRuleColorIsCurrentColor(c.IsCurrentColor());
    }
  }

  // column-rule-width (aka -webkit-column-rule-width)
  unsigned short ColumnRuleWidth() const {
    if (ColumnRuleStyle() == EBorderStyle::kNone ||
        ColumnRuleStyle() == EBorderStyle::kHidden)
      return 0;
    return ColumnRuleWidthInternal().ToFloat();
  }
  void SetColumnRuleWidth(unsigned short w) {
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
    if (IsStackingContext() == other.IsStackingContext() ||
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
           OutlineColorIsCurrentColor() == other.OutlineColorIsCurrentColor() &&
           OutlineColor() == other.OutlineColor() &&
           OutlineStyle() == other.OutlineStyle() &&
           OutlineOffsetInternal() == other.OutlineOffsetInternal() &&
           OutlineStyleIsAuto() == other.OutlineStyleIsAuto();
  }

  // outline-color
  void SetOutlineColor(const StyleColor& v) {
    if (OutlineColor() != v) {
      SetOutlineColorInternal(v.Resolve(Color()));
      SetOutlineColorIsCurrentColor(v.IsCurrentColor());
    }
  }

  // outline-width
  unsigned short OutlineWidth() const {
    if (OutlineStyle() == EBorderStyle::kNone)
      return 0;
    // FIXME: Why is this stored as a float but converted to short?
    return OutlineWidthInternal().ToFloat();
  }
  void SetOutlineWidth(unsigned short v) {
    SetOutlineWidthInternal(LayoutUnit(v));
  }

  // outline-offset
  int OutlineOffset() const {
    if (OutlineStyle() == EBorderStyle::kNone)
      return 0;
    return OutlineOffsetInternal();
  }

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

  bool SetEffectiveZoom(float);

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

  // color
  void SetColor(const Color&);

  // line-height
  Length LineHeight() const;

  // List style properties.
  // list-style-image
  StyleImage* ListStyleImage() const;
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

  // -webkit-text-emphasis-color (aka -epub-text-emphasis-color)
  void SetTextEmphasisColor(const StyleColor& color) {
    SetTextEmphasisColorInternal(color.Resolve(Color()));
    SetTextEmphasisColorIsCurrentColorInternal(color.IsCurrentColor());
  }

  // -webkit-text-fill-color
  void SetTextFillColor(const StyleColor& color) {
    SetTextFillColorInternal(color.Resolve(Color()));
    SetTextFillColorIsCurrentColorInternal(color.IsCurrentColor());
  }

  // -webkit-text-stroke-color
  void SetTextStrokeColor(const StyleColor& color) {
    SetTextStrokeColorInternal(color.Resolve(Color()));
    SetTextStrokeColorIsCurrentColorInternal(color.IsCurrentColor());
  }

  // caret-color
  void SetCaretColor(const StyleAutoColor& color) {
    SetCaretColorInternal(color.Resolve(Color()));
    SetCaretColorIsCurrentColorInternal(color.IsCurrentColor());
    SetCaretColorIsAutoInternal(color.IsAutoColor());
  }

  // Font properties.
  CORE_EXPORT const Font& GetFont() const { return FontInternal(); }
  CORE_EXPORT void SetFont(const Font& font) { SetFontInternal(font); }
  CORE_EXPORT const FontDescription& GetFontDescription() const {
    return FontInternal().GetFontDescription();
  }
  CORE_EXPORT bool SetFontDescription(const FontDescription&);
  bool HasIdenticalAscentDescentAndLineGap(const ComputedStyle& other) const;

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
        SetTabSizeInternal(0);
      else
        SetTabSizeInternal(0.0f);
    } else {
      SetTabSizeInternal(t);
    }
  }

  // word-spacing
  float WordSpacing() const { return GetFontDescription().WordSpacing(); }
  void SetWordSpacing(float);

  // orphans
  void SetOrphans(short o) { SetOrphansInternal(clampTo<short>(o, 1)); }

  // widows
  void SetWidows(short w) { SetWidowsInternal(clampTo<short>(w, 1)); }

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
  void SetStopColor(const Color& c) { AccessSVGStyle().SetStopColor(c); }

  // flood-color
  void SetFloodColor(const Color& c) { AccessSVGStyle().SetFloodColor(c); }

  // lighting-color
  void SetLightingColor(const Color& c) {
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

  bool HasChildDependentFlags() const {
    return HasExplicitlyInheritedProperties();
  }
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

  // Variables.
  CORE_EXPORT StyleInheritedVariables* InheritedVariables() const;
  CORE_EXPORT StyleNonInheritedVariables* NonInheritedVariables() const;

  void SetVariable(const AtomicString&,
                   scoped_refptr<CSSVariableData>,
                   bool is_inherited_property);

  void SetRegisteredVariable(const AtomicString&,
                             const CSSValue*,
                             bool is_inherited_property);

  void RemoveVariable(const AtomicString&, bool is_inherited_property);

  // Handles both inherited and non-inherited variables
  CSSVariableData* GetVariable(const AtomicString&) const;

  CSSVariableData* GetVariable(const AtomicString&,
                               bool is_inherited_property) const;

  const CSSValue* GetRegisteredVariable(const AtomicString&,
                                        bool is_inherited_property) const;

  const CSSValue* GetRegisteredVariable(const AtomicString&) const;

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
  const Vector<String>& CallbackSelectors() const {
    return CallbackSelectorsInternal();
  }
  void AddCallbackSelector(const String& selector);

  // Non-property flags.
  CORE_EXPORT void SetTextAutosizingMultiplier(float);

  // Column utility functions.
  void ClearMultiCol();
  bool SpecifiesColumns() const {
    return !HasAutoColumnCount() || !HasAutoColumnWidth();
  }
  bool ColumnRuleIsTransparent() const {
    return !ColumnRuleColorIsCurrentColor() &&
           !ColumnRuleColorInternal().Alpha();
  }
  bool ColumnRuleEquivalent(const ComputedStyle& other_style) const;

  // Flex utility functions.
  bool IsColumnFlexDirection() const {
    return FlexDirection() == EFlexDirection::kColumn ||
           FlexDirection() == EFlexDirection::kColumnReverse;
  }
  bool IsReverseFlexDirection() const {
    return FlexDirection() == EFlexDirection::kRowReverse ||
           FlexDirection() == EFlexDirection::kColumnReverse;
  }
  bool HasBoxReflect() const { return BoxReflect(); }
  bool ReflectionDataEquivalent(const ComputedStyle& other) const {
    return DataEquivalent(BoxReflect(), other.BoxReflect());
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
    return (GridAutoFlowInternal() & kInternalAutoFlowDirectionRow) ==
           kInternalAutoFlowDirectionRow;
  }
  bool IsGridAutoFlowDirectionColumn() const {
    return (GridAutoFlowInternal() & kInternalAutoFlowDirectionColumn) ==
           kInternalAutoFlowDirectionColumn;
  }
  bool IsGridAutoFlowAlgorithmSparse() const {
    return (GridAutoFlowInternal() & kInternalAutoFlowAlgorithmSparse) ==
           kInternalAutoFlowAlgorithmSparse;
  }
  bool IsGridAutoFlowAlgorithmDense() const {
    return (GridAutoFlowInternal() & kInternalAutoFlowAlgorithmDense) ==
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
    return WillChangeProperties().Contains(CSSPropertyOpacity);
  }
  CORE_EXPORT bool HasWillChangeTransformHint() const;

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
  bool HasMargin() const {
    return !MarginLeft().IsZero() || !MarginRight().IsZero() ||
           !MarginTop().IsZero() || !MarginBottom().IsZero();
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
  bool HasPadding() const {
    return !PaddingLeft().IsZero() || !PaddingRight().IsZero() ||
           !PaddingTop().IsZero() || !PaddingBottom().IsZero();
  }
  void ResetPadding() {
    SetPaddingTop(Length(kFixed));
    SetPaddingBottom(Length(kFixed));
    SetPaddingLeft(Length(kFixed));
    SetPaddingRight(Length(kFixed));
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
           BorderLeftColor() == o.BorderLeftColor() &&
           BorderLeftColorIsCurrentColor() == o.BorderLeftColorIsCurrentColor();
  }
  bool BorderLeftEquals(const BorderValue& o) const {
    return BorderLeftWidthInternal().ToFloat() == o.Width() &&
           BorderLeftStyle() == o.Style() &&
           BorderLeftColor() == o.GetColor() &&
           BorderLeftColorIsCurrentColor() == o.ColorIsCurrentColor();
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
           BorderRightColor() == o.BorderRightColor() &&
           BorderRightColorIsCurrentColor() ==
               o.BorderRightColorIsCurrentColor();
  }
  bool BorderRightEquals(const BorderValue& o) const {
    return BorderRightWidthInternal().ToFloat() == o.Width() &&
           BorderRightStyle() == o.Style() &&
           BorderRightColor() == o.GetColor() &&
           BorderRightColorIsCurrentColor() == o.ColorIsCurrentColor();
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
           BorderTopColor() == o.BorderTopColor() &&
           BorderTopColorIsCurrentColor() == o.BorderTopColorIsCurrentColor();
  }
  bool BorderTopEquals(const BorderValue& o) const {
    return BorderTopWidthInternal().ToFloat() == o.Width() &&
           BorderTopStyle() == o.Style() && BorderTopColor() == o.GetColor() &&
           BorderTopColorIsCurrentColor() == o.ColorIsCurrentColor();
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
           BorderBottomColor() == o.BorderBottomColor() &&
           BorderBottomColorIsCurrentColor() ==
               o.BorderBottomColorIsCurrentColor();
  }
  bool BorderBottomEquals(const BorderValue& o) const {
    return BorderBottomWidthInternal().ToFloat() == o.Width() &&
           BorderBottomStyle() == o.Style() &&
           BorderBottomColor() == o.GetColor() &&
           BorderBottomColorIsCurrentColor() == o.ColorIsCurrentColor();
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

  bool BackgroundVisuallyEqual(const ComputedStyle& o) const {
    return BackgroundColorInternal() == o.BackgroundColorInternal() &&
           BackgroundInternal().VisuallyEqual(o.BackgroundInternal());
  }

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
    SetBorderTopColorInternal(0);
    SetBorderTopColorIsCurrentColor(true);
  }
  void ResetBorderRight() {
    SetBorderRightStyle(EBorderStyle::kNone);
    SetBorderRightWidth(3);
    SetBorderRightColorInternal(0);
    SetBorderRightColorIsCurrentColor(true);
  }
  void ResetBorderBottom() {
    SetBorderBottomStyle(EBorderStyle::kNone);
    SetBorderBottomWidth(3);
    SetBorderBottomColorInternal(0);
    SetBorderBottomColorIsCurrentColor(true);
  }
  void ResetBorderLeft() {
    SetBorderLeftStyle(EBorderStyle::kNone);
    SetBorderLeftWidth(3);
    SetBorderLeftColorInternal(0);
    SetBorderLeftColorIsCurrentColor(true);
  }

  void SetBorderRadius(const LengthSize& s) {
    SetBorderTopLeftRadius(s);
    SetBorderTopRightRadius(s);
    SetBorderBottomLeftRadius(s);
    SetBorderBottomRightRadius(s);
  }
  void SetBorderRadius(const IntSize& s) {
    SetBorderRadius(
        LengthSize(Length(s.Width(), kFixed), Length(s.Height(), kFixed)));
  }

  FloatRoundedRect GetRoundedBorderFor(
      const LayoutRect& border_rect,
      bool include_logical_left_edge = true,
      bool include_logical_right_edge = true) const;
  FloatRoundedRect GetRoundedInnerBorderFor(
      const LayoutRect& border_rect,
      bool include_logical_left_edge = true,
      bool include_logical_right_edge = true) const;
  FloatRoundedRect GetRoundedInnerBorderFor(
      const LayoutRect& border_rect,
      const LayoutRectOutsets& insets,
      bool include_logical_left_edge = true,
      bool include_logical_right_edge = true) const;

  bool CanRenderBorderImage() const;

  // Float utility functions.
  bool IsFloating() const { return Floating() != EFloat::kNone; }

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

  // Clip utility functions.
  const Length& ClipLeft() const { return Clip().Left(); }
  const Length& ClipRight() const { return Clip().Right(); }
  const Length& ClipTop() const { return Clip().Top(); }
  const Length& ClipBottom() const { return Clip().Bottom(); }

  // Offset utility functions.
  // Accessors for positioned object edges that take into account writing mode.
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
  bool IsDisplayFlexibleOrGridBox() const {
    return IsDisplayFlexibleBox(Display()) || IsDisplayGridBox(Display());
  }
  bool IsDisplayFlexibleBox() const { return IsDisplayFlexibleBox(Display()); }
  bool IsDisplayLayoutCustomBox() const {
    return IsDisplayLayoutCustomBox(Display());
  }

  bool IsDisplayTableType() const { return IsDisplayTableType(Display()); }

  // Isolation utility functions.
  bool HasIsolation() const { return Isolation() != EIsolation::kAuto; }

  // Content utility functions.
  bool HasContent() const { return GetContentData(); }

  // Cursor utility functions.
  CursorList* Cursors() const { return CursorDataInternal().Get(); }
  CORE_EXPORT void AddCursor(StyleImage*,
                             bool hot_spot_specified,
                             const IntPoint& hot_spot = IntPoint());
  void SetCursorList(CursorList*);
  void ClearCursorList();

  // Text decoration utility functions.
  void ApplyTextDecorations(const Color& parent_text_decoration_color,
                            bool override_existing_colors);
  void ClearAppliedTextDecorations();
  void RestoreParentTextDecorations(const ComputedStyle& parent_style);
  const Vector<AppliedTextDecoration>& AppliedTextDecorations() const;
  TextDecoration TextDecorationsInEffect() const;

  // Overflow utility functions.

  EOverflow OverflowInlineDirection() const {
    return IsHorizontalWritingMode() ? OverflowX() : OverflowY();
  }
  EOverflow OverflowBlockDirection() const {
    return IsHorizontalWritingMode() ? OverflowY() : OverflowX();
  }

  // It's sufficient to just check one direction, since it's illegal to have
  // visible on only one overflow value.
  bool IsOverflowVisible() const {
    DCHECK(OverflowX() != EOverflow::kVisible || OverflowX() == OverflowY());
    return OverflowX() == EOverflow::kVisible;
  }
  bool IsOverflowPaged() const {
    return OverflowY() == EOverflow::kWebkitPagedX ||
           OverflowY() == EOverflow::kWebkitPagedY;
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
           OverflowY() == EOverflow::kWebkitPagedY ||
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

  // Filter/transform utility functions.
  bool Has3DTransform() const {
    return Transform().Has3DOperation() ||
           (Translate() && Translate()->Z() != 0) ||
           (Rotate() && (Rotate()->X() != 0 || Rotate()->Y() != 0)) ||
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
    return HasGroupingProperty() ? ETransformStyle3D::kFlat
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
    return HasFilter() || HasBoxReflect();
  }

  // Returns |true| if opacity should be considered to have non-initial value
  // for the purpose of creating stacking contexts.
  bool HasNonInitialOpacity() const {
    return HasOpacity() || HasWillChangeOpacityHint() ||
           HasCurrentOpacityAnimation();
  }

  // Returns whether this style contains any grouping property as defined by
  // [css-transforms].  The main purpose of this is to adjust the used value of
  // transform-style property.
  // Note: We currently don't include every grouping property on the spec to
  // maintain backward compatibility.  [css-transforms]
  // https://drafts.csswg.org/css-transforms/#grouping-property-values
  bool HasGroupingProperty() const {
    return !IsOverflowVisible() || HasFilterInducingProperty() ||
           HasNonInitialOpacity();
  }

  // Return true if any transform related property (currently
  // transform/motionPath, transformStyle3D, perspective, or
  // will-change:transform) indicates that we are transforming.
  // will-change:transform should result in the same rendering behavior as
  // having a transform, including the creation of a containing block for fixed
  // position descendants.
  CORE_EXPORT bool HasTransformRelatedProperty() const {
    return HasTransform() || Preserves3D() || HasPerspective() ||
           HasWillChangeTransformHint() ||
           HasTransformAnimationWithForwardsOrBothFillMode();
  }

  // Paint utility functions.
  void AddPaintImage(StyleImage*);

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
  CORE_EXPORT void UpdateIsStackingContext(bool is_document_element,
                                           bool is_in_top_layer,
                                           bool is_svg_stacking);
  bool IsStacked() const {
    return IsStackingContext() || GetPosition() != EPosition::kStatic;
  }

  // Pseudo-styles
  bool HasAnyPublicPseudoStyles() const;
  bool HasPseudoStyle(PseudoId) const;
  void SetHasPseudoStyle(PseudoId);
  bool HasPseudoElementStyle() const;

  // Note: CanContainAbsolutePositionObjects should return true if
  // CanContainFixedPositionObjects.  We currently never use this value
  // directly, always OR'ing it with CanContainFixedPositionObjects.
  bool CanContainAbsolutePositionObjects() const {
    return GetPosition() != EPosition::kStatic;
  }
  bool CanContainFixedPositionObjects(bool is_document_element) const {
    return HasTransformRelatedProperty() ||
           // Filter establishes containing block for non-document elements:
           // https://drafts.fxtf.org/filter-effects-1/#FilterProperty
           (!is_document_element && HasFilter());
  }

  // Whitespace utility functions.
  static bool AutoWrap(EWhiteSpace ws) {
    // Nowrap and pre don't automatically wrap.
    return ws != EWhiteSpace::kNowrap && ws != EWhiteSpace::kPre;
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
    return ws != EWhiteSpace::kPre && ws != EWhiteSpace::kPreWrap;
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
    return WhiteSpace() == EWhiteSpace::kPreWrap ||
           GetLineBreak() == LineBreak::kAfterWhiteSpace;
  }

  bool BreakWords() const {
    return (WordBreak() == EWordBreak::kBreakWord ||
            OverflowWrap() == EOverflowWrap::kBreakWord) &&
           WhiteSpace() != EWhiteSpace::kPre &&
           WhiteSpace() != EWhiteSpace::kNowrap;
  }

  // Text direction utility functions.
  bool ShouldPlaceBlockDirectionScrollbarOnLogicalLeft() const {
    return !IsLeftToRightDirection() && IsHorizontalWritingMode();
  }
  bool HasInlinePaginationAxis() const {
    // If the pagination axis is parallel with the writing mode inline axis,
    // columns may be laid out along the inline axis, just like for regular
    // multicol. Otherwise, we need to lay out along the block axis.
    if (IsOverflowPaged()) {
      return (OverflowY() == EOverflow::kWebkitPagedX) ==
             IsHorizontalWritingMode();
    }
    return false;
  }

  // Border utility functions.
  bool BorderObscuresBackground() const;
  void GetBorderEdgeInfo(BorderEdge edges[],
                         bool include_logical_left_edge = true,
                         bool include_logical_right_edge = true) const;

  bool HasBoxDecorations() const {
    return HasBorderDecoration() || HasBorderRadius() || HasOutline() ||
           HasAppearance() || BoxShadow() || HasFilterInducingProperty() ||
           HasBackdropFilter() || Resize() != EResize::kNone;
  }

  // "Box decoration background" includes all box decorations and backgrounds
  // that are painted as the background of the object. It includes borders,
  // box-shadows, background-color and background-image, etc.
  bool HasBoxDecorationBackground() const {
    return HasBackground() || HasBorderDecoration() || HasAppearance() ||
           BoxShadow();
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
        VisitedLinkBackgroundColor().IsCurrentColor())
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

  // -webkit-appearance utility functions.
  bool HasAppearance() const { return Appearance() != kNoControlPart; }

  // Other utility functions.
  bool IsStyleAvailable() const;

  bool RequireTransformOrigin(ApplyTransformOrigin apply_origin,
                              ApplyMotionPath) const;

  InterpolationQuality GetInterpolationQuality() const;

  bool CanGeneratePseudoElement(PseudoId pseudo) const {
    if (!HasPseudoStyle(pseudo))
      return false;
    if (Display() == EDisplay::kNone)
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

 private:
  void SetVisitedLinkBackgroundColor(const StyleColor& v) {
    SetVisitedLinkBackgroundColorInternal(v);
  }
  void SetVisitedLinkBorderLeftColor(const StyleColor& v) {
    SetVisitedLinkBorderLeftColorInternal(v);
  }
  void SetVisitedLinkBorderRightColor(const StyleColor& v) {
    SetVisitedLinkBorderRightColorInternal(v);
  }
  void SetVisitedLinkBorderBottomColor(const StyleColor& v) {
    SetVisitedLinkBorderBottomColorInternal(v);
  }
  void SetVisitedLinkBorderTopColor(const StyleColor& v) {
    SetVisitedLinkBorderTopColorInternal(v);
  }
  void SetVisitedLinkOutlineColor(const StyleColor& v) {
    SetVisitedLinkOutlineColorInternal(v);
  }
  void SetVisitedLinkColumnRuleColor(const StyleColor& v) {
    SetVisitedLinkColumnRuleColorInternal(v);
  }
  void SetVisitedLinkTextDecorationColor(const StyleColor& v) {
    SetVisitedLinkTextDecorationColorInternal(v);
  }
  void SetVisitedLinkTextEmphasisColor(const StyleColor& color) {
    SetVisitedLinkTextEmphasisColorInternal(color.Resolve(Color()));
    SetVisitedLinkTextEmphasisColorIsCurrentColorInternal(
        color.IsCurrentColor());
  }
  void SetVisitedLinkTextFillColor(const StyleColor& color) {
    SetVisitedLinkTextFillColorInternal(color.Resolve(Color()));
    SetVisitedLinkTextFillColorIsCurrentColorInternal(color.IsCurrentColor());
  }
  void SetVisitedLinkTextStrokeColor(const StyleColor& color) {
    SetVisitedLinkTextStrokeColorInternal(color.Resolve(Color()));
    SetVisitedLinkTextStrokeColorIsCurrentColorInternal(color.IsCurrentColor());
  }
  void SetVisitedLinkCaretColor(const StyleAutoColor& color) {
    SetVisitedLinkCaretColorInternal(color.Resolve(Color()));
    SetVisitedLinkCaretColorIsCurrentColorInternal(color.IsCurrentColor());
    SetVisitedLinkCaretColorIsAutoInternal(color.IsAutoColor());
  }

  static bool IsDisplayBlockContainer(EDisplay display) {
    return display == EDisplay::kBlock || display == EDisplay::kListItem ||
           display == EDisplay::kInlineBlock ||
           display == EDisplay::kFlowRoot || display == EDisplay::kTableCell ||
           display == EDisplay::kTableCaption;
  }

  static bool IsDisplayFlexibleBox(EDisplay display) {
    return display == EDisplay::kFlex || display == EDisplay::kInlineFlex;
  }

  static bool IsDisplayGridBox(EDisplay display) {
    return display == EDisplay::kGrid || display == EDisplay::kInlineGrid;
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
  StyleColor BorderLeftColor() const {
    return BorderLeftColorIsCurrentColor()
               ? StyleColor::CurrentColor()
               : StyleColor(BorderLeftColorInternal());
  }
  StyleColor BorderRightColor() const {
    return BorderRightColorIsCurrentColor()
               ? StyleColor::CurrentColor()
               : StyleColor(BorderRightColorInternal());
  }
  StyleColor BorderTopColor() const {
    return BorderTopColorIsCurrentColor()
               ? StyleColor::CurrentColor()
               : StyleColor(BorderTopColorInternal());
  }
  StyleColor BorderBottomColor() const {
    return BorderBottomColorIsCurrentColor()
               ? StyleColor::CurrentColor()
               : StyleColor(BorderBottomColorInternal());
  }

  StyleColor BackgroundColor() const { return BackgroundColorInternal(); }
  StyleAutoColor CaretColor() const {
    if (CaretColorIsCurrentColorInternal())
      return StyleAutoColor::CurrentColor();
    if (CaretColorIsAutoInternal())
      return StyleAutoColor::AutoColor();
    return StyleAutoColor(CaretColorInternal());
  }
  Color GetColor() const;
  StyleColor ColumnRuleColor() const {
    return ColumnRuleColorIsCurrentColor()
               ? StyleColor::CurrentColor()
               : StyleColor(ColumnRuleColorInternal());
  }
  StyleColor OutlineColor() const {
    return OutlineColorIsCurrentColor() ? StyleColor::CurrentColor()
                                        : StyleColor(OutlineColorInternal());
  }
  StyleColor TextEmphasisColor() const {
    return TextEmphasisColorIsCurrentColorInternal()
               ? StyleColor::CurrentColor()
               : StyleColor(TextEmphasisColorInternal());
  }
  StyleColor TextFillColor() const {
    return TextFillColorIsCurrentColorInternal()
               ? StyleColor::CurrentColor()
               : StyleColor(TextFillColorInternal());
  }
  StyleColor TextStrokeColor() const {
    return TextStrokeColorIsCurrentColorInternal()
               ? StyleColor::CurrentColor()
               : StyleColor(TextStrokeColorInternal());
  }
  StyleAutoColor VisitedLinkCaretColor() const {
    if (VisitedLinkCaretColorIsCurrentColorInternal())
      return StyleAutoColor::CurrentColor();
    if (VisitedLinkCaretColorIsAutoInternal())
      return StyleAutoColor::AutoColor();
    return StyleAutoColor(VisitedLinkCaretColorInternal());
  }
  StyleColor VisitedLinkBackgroundColor() const {
    return VisitedLinkBackgroundColorInternal();
  }
  StyleColor VisitedLinkBorderLeftColor() const {
    return VisitedLinkBorderLeftColorInternal();
  }
  bool VisitedLinkBorderLeftColorHasNotChanged(
      const ComputedStyle& other) const {
    return (VisitedLinkBorderLeftColor() ==
                other.VisitedLinkBorderLeftColor() ||
            !BorderLeftWidth());
  }
  StyleColor VisitedLinkBorderRightColor() const {
    return VisitedLinkBorderRightColorInternal();
  }
  bool VisitedLinkBorderRightColorHasNotChanged(
      const ComputedStyle& other) const {
    return (VisitedLinkBorderRightColor() ==
                other.VisitedLinkBorderRightColor() ||
            !BorderRightWidth());
  }
  StyleColor VisitedLinkBorderBottomColor() const {
    return VisitedLinkBorderBottomColorInternal();
  }
  bool VisitedLinkBorderBottomColorHasNotChanged(
      const ComputedStyle& other) const {
    return (VisitedLinkBorderBottomColor() ==
                other.VisitedLinkBorderBottomColor() ||
            !BorderBottomWidth());
  }
  StyleColor VisitedLinkBorderTopColor() const {
    return VisitedLinkBorderTopColorInternal();
  }
  bool VisitedLinkBorderTopColorHasNotChanged(
      const ComputedStyle& other) const {
    return (VisitedLinkBorderTopColor() == other.VisitedLinkBorderTopColor() ||
            !BorderTopWidth());
  }
  StyleColor VisitedLinkOutlineColor() const {
    return VisitedLinkOutlineColorInternal();
  }
  bool VisitedLinkOutlineColorHasNotChanged(const ComputedStyle& other) const {
    return (VisitedLinkOutlineColor() == other.VisitedLinkOutlineColor() ||
            !OutlineWidth());
  }
  StyleColor VisitedLinkColumnRuleColor() const {
    return VisitedLinkColumnRuleColorInternal();
  }
  StyleColor VisitedLinkTextDecorationColor() const {
    return VisitedLinkTextDecorationColorInternal();
  }
  StyleColor VisitedLinkTextEmphasisColor() const {
    return VisitedLinkTextEmphasisColorIsCurrentColorInternal()
               ? StyleColor::CurrentColor()
               : StyleColor(VisitedLinkTextEmphasisColorInternal());
  }
  StyleColor VisitedLinkTextFillColor() const {
    return VisitedLinkTextFillColorIsCurrentColorInternal()
               ? StyleColor::CurrentColor()
               : StyleColor(VisitedLinkTextFillColorInternal());
  }
  StyleColor VisitedLinkTextStrokeColor() const {
    return VisitedLinkTextStrokeColorIsCurrentColorInternal()
               ? StyleColor::CurrentColor()
               : StyleColor(VisitedLinkTextStrokeColorInternal());
  }

  StyleColor DecorationColorIncludingFallback(bool visited_link) const;

  Color StopColor() const { return SvgStyle().StopColor(); }
  StyleColor FloodColor() const { return SvgStyle().FloodColor(); }
  StyleColor LightingColor() const { return SvgStyle().LightingColor(); }

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
  bool DiffNeedsPaintInvalidationSubtree(const ComputedStyle& other) const;
  bool DiffNeedsPaintInvalidationObject(const ComputedStyle& other) const;
  bool DiffNeedsPaintInvalidationObjectForPaintImage(
      const StyleImage&,
      const ComputedStyle& other) const;
  bool DiffNeedsVisualRectUpdate(const ComputedStyle& other) const;
  CORE_EXPORT void UpdatePropertySpecificDifferences(const ComputedStyle& other,
                                                     StyleDifference&) const;

  bool PropertiesEqual(const Vector<CSSPropertyID>& properties,
                       const ComputedStyle& other) const;
  bool CustomPropertiesEqual(const Vector<AtomicString>& properties,
                             const ComputedStyle& other) const;

  static bool ShadowListHasCurrentColor(const ShadowList*);

  StyleInheritedVariables& MutableInheritedVariables();
  StyleNonInheritedVariables& MutableNonInheritedVariables();

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
};

inline bool ComputedStyle::SetEffectiveZoom(float f) {
  // Clamp the effective zoom value to a smaller (but hopeful still large
  // enough) range, to avoid overflow in derived computations.
  float clamped_effective_zoom = clampTo<float>(f, 1e-6, 1e6);
  if (EffectiveZoom() == clamped_effective_zoom)
    return false;
  SetEffectiveZoomInternal(clamped_effective_zoom);
  return true;
}

inline bool ComputedStyle::HasAnyPublicPseudoStyles() const {
  return PseudoBitsInternal() != kPseudoIdNone;
}

inline bool ComputedStyle::HasPseudoStyle(PseudoId pseudo) const {
  DCHECK(pseudo >= kFirstPublicPseudoId);
  DCHECK(pseudo < kFirstInternalPseudoId);
  return (1 << (pseudo - kFirstPublicPseudoId)) & PseudoBitsInternal();
}

inline void ComputedStyle::SetHasPseudoStyle(PseudoId pseudo) {
  DCHECK(pseudo >= kFirstPublicPseudoId);
  DCHECK(pseudo < kFirstInternalPseudoId);
  // TODO: Fix up this code. It is hard to understand.
  SetPseudoBitsInternal(static_cast<PseudoId>(
      PseudoBitsInternal() | 1 << (pseudo - kFirstPublicPseudoId)));
}

inline bool ComputedStyle::HasPseudoElementStyle() const {
  return PseudoBitsInternal() & kElementPseudoIdMask;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COMPUTED_STYLE_H_
