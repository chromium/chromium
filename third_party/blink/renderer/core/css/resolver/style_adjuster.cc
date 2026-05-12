/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 */

#include "third_party/blink/renderer/core/css/resolver/style_adjuster.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"
#include "third_party/blink/renderer/core/html/forms/html_field_set_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_legend_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element.h"
#include "third_party/blink/renderer/core/html/html_frame_set_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_marquee_element.h"
#include "third_party/blink/renderer/core/html/html_meter_element.h"
#include "third_party/blink/renderer/core/html/html_olist_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/html_progress_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/html_ulist_element.h"
#include "third_party/blink/renderer/core/html/html_wbr_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_utils.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/list/list_marker.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/style_intrinsic_length.h"
#include "third_party/blink/renderer/core/svg/svg_foreign_object_element.h"
#include "third_party/blink/renderer/core/svg/svg_g_element.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_text_element.h"
#include "third_party/blink/renderer/core/svg/svg_tspan_element.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/transforms/transform_operations.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "ui/base/ui_base_features.h"

namespace blink {

using mojom::blink::FormControlType;

namespace {

bool IsOverflowClipOrVisible(EOverflow overflow) {
  return overflow == EOverflow::kClip || overflow == EOverflow::kVisible;
}

TouchAction AdjustTouchActionForElement(TouchAction touch_action,
                                        const ComputedStyleBuilder& builder,
                                        const ComputedStyle& parent_style,
                                        Element* element) {
  Element* document_element = element->GetDocument().documentElement();
  bool scrolls_overflow = builder.ScrollsOverflow();
  if (IsA<HTMLBodyElement>(element) &&
      element == element->GetDocument().FirstBodyElement()) {
    // Body scrolls overflow if html root overflow is not visible or the
    // propagation of overflow is stopped by containment.
    if (parent_style.IsOverflowVisibleAlongBothAxes()) {
      if (!parent_style.ShouldApplyAnyContainment(*document_element) &&
          !builder.ShouldApplyAnyContainment(*element)) {
        scrolls_overflow = false;
      }
    }
  }
  bool is_child_document =
      element == document_element && element->GetDocument().LocalOwner();
  if (scrolls_overflow || is_child_document) {
    return touch_action | TouchAction::kPan |
           TouchAction::kInternalPanXScrolls |
           TouchAction::kInternalNotWritable;
  }
  return touch_action;
}

bool HostIsInputFile(const Element* element) {
  if (!element || !element->IsInUserAgentShadowRoot()) {
    return false;
  }
  if (const Element* shadow_host = element->OwnerShadowHost()) {
    if (const auto* input = DynamicTo<HTMLInputElement>(shadow_host)) {
      return input->FormControlType() == FormControlType::kInputFile;
    }
  }
  return false;
}

// We need to avoid to inlinify children of <fieldset>, <audio>, and <video>.
// They create dedicated LayoutObjects, and assume only block children.
bool ShouldBeInlinified(const Element* element) {
  if (!element) {
    return true;
  }
  const Element* parent = FlatTreeTraversal::ParentElement(*element);
  for (; parent; parent = FlatTreeTraversal::ParentElement(*parent)) {
    const ComputedStyle* parent_style = parent->GetComputedStyle();
    if (!parent_style || parent_style->Display() != EDisplay::kContents) {
      break;
    }
  }
  return !IsA<HTMLFieldSetElement>(parent) && !IsA<HTMLMediaElement>(parent);
}

}  // namespace

void StyleAdjuster::AdjustStyleForSvgElement(
    const SVGElement& element,
    const SVGElement* styled_element,
    ComputedStyleBuilder& builder,
    const ComputedStyle& layout_parent_style) {
  if (builder.Display() != EDisplay::kNone) {
    // Disable some of text decoration properties.
    //
    // Note that SetFooBar() is more efficient than ResetFooBar() if the current
    // value is same as the reset value.
    builder.SetTextDecorationSkipInk(ETextDecorationSkipInk::kAuto);
    if (!RuntimeEnabledFeatures::SvgEnableTextDecorationCssStylingEnabled()) {
      builder.SetTextDecorationStyle(
          ETextDecorationStyle::kSolid);  // crbug.com/1246719
    }
    builder.SetTextDecorationThickness(TextDecorationThickness(Length::Auto()));
    builder.SetTextEmphasisMark(TextEmphasisMark::kNone);
    builder.SetTextUnderlineOffset(Length());  // crbug.com/1247912
    builder.SetTextUnderlinePosition(TextUnderlinePosition::kAuto);
  }

  // Only the root <svg> element in an SVG document fragment tree, honors the
  // CSS position, all other inner <svg> elements has to have position as static
  // as they don't follow CSS box model. This also includes when a <use> element
  // refers an <svg> root element, in that case, we need to consider the
  // styled_element itself to set its position CSS property.
  bool is_svg_root = styled_element->IsOutermostSVGSVGElement();
  if (!is_svg_root) {
    // Only the root <svg> element in an SVG document fragment tree honors css
    // position.
    builder.SetPosition(ComputedStyleInitialValues::InitialPosition());
  }

  if (builder.Display() == EDisplay::kContents &&
      (is_svg_root ||
       (!IsA<SVGSVGElement>(element) && !IsA<SVGGElement>(element) &&
        !IsA<SVGUseElement>(element) && !IsA<SVGTSpanElement>(element)))) {
    // According to the CSS Display spec[1], nested <svg> elements, <g>,
    // <use>, and <tspan> elements are not rendered and their children are
    // "hoisted". For other elements display:contents behaves as display:none.
    //
    // [1] https://drafts.csswg.org/css-display/#unbox-svg
    builder.SetDisplay(EDisplay::kNone);
  }

  // SVG text layout code expects us to be a block-level style element.
  if ((IsA<SVGForeignObjectElement>(element) || IsA<SVGTextElement>(element)) &&
      builder.IsDisplayInlineType()) {
    builder.SetDisplay(EDisplay::kBlock);
  }

  // Columns don't apply to svg text elements.
  if (IsA<SVGTextElement>(element)) {
    AdjustForSVGTextElement(builder);
  }

  // Copy DominantBaseline to CssDominantBaseline without 'no-change',
  // 'reset-size', and 'use-script'.
  auto baseline = builder.DominantBaseline();
  if (baseline == EDominantBaseline::kUseScript) {
    // TODO(fs): The dominant-baseline and the baseline-table components
    // are set by determining the predominant script of the character data
    // content.
    baseline = EDominantBaseline::kAlphabetic;
  } else if (baseline == EDominantBaseline::kNoChange ||
             baseline == EDominantBaseline::kResetSize) {
    baseline = layout_parent_style.CssDominantBaseline();
  }
  builder.SetCssDominantBaseline(baseline);
}

// https://drafts.csswg.org/css-display/#transformations
static EDisplay EquivalentBlockDisplay(EDisplay display) {
  switch (display) {
    case EDisplay::kFlowRootListItem:
    case EDisplay::kBlock:
    case EDisplay::kTable:
    case EDisplay::kWebkitBox:
    case EDisplay::kFlex:
    case EDisplay::kGrid:
    case EDisplay::kBlockMath:
    case EDisplay::kBlockRuby:
    case EDisplay::kListItem:
    case EDisplay::kFlowRoot:
    case EDisplay::kLayoutCustom:
    case EDisplay::kGridLanes:
      return display;
    case EDisplay::kInlineTable:
      return EDisplay::kTable;
    case EDisplay::kWebkitInlineBox:
      return EDisplay::kWebkitBox;
    case EDisplay::kInlineFlex:
      return EDisplay::kFlex;
    case EDisplay::kInlineGrid:
      return EDisplay::kGrid;
    case EDisplay::kMath:
      return EDisplay::kBlockMath;
    case EDisplay::kRuby:
      return EDisplay::kBlockRuby;
    case EDisplay::kInlineLayoutCustom:
      return EDisplay::kLayoutCustom;
    case EDisplay::kInlineListItem:
      return EDisplay::kListItem;
    case EDisplay::kInlineFlowRootListItem:
      return EDisplay::kFlowRootListItem;
    case EDisplay::kInlineGridLanes:
      return EDisplay::kGridLanes;

    case EDisplay::kContents:
    case EDisplay::kInline:
    case EDisplay::kInlineBlock:
    case EDisplay::kTableRowGroup:
    case EDisplay::kTableHeaderGroup:
    case EDisplay::kTableFooterGroup:
    case EDisplay::kTableRow:
    case EDisplay::kTableColumnGroup:
    case EDisplay::kTableColumn:
    case EDisplay::kTableCell:
    case EDisplay::kTableCaption:
    case EDisplay::kRubyText:
      return EDisplay::kBlock;
    case EDisplay::kNone:
      NOTREACHED();
  }
  NOTREACHED();
}

// https://drafts.csswg.org/css-display/#inlinify
static EDisplay EquivalentInlineDisplay(EDisplay display) {
  switch (display) {
    case EDisplay::kFlowRootListItem:
      return EDisplay::kInlineFlowRootListItem;
    case EDisplay::kBlock:
    case EDisplay::kFlowRoot:
      return EDisplay::kInlineBlock;
    case EDisplay::kTable:
      return EDisplay::kInlineTable;
    case EDisplay::kWebkitBox:
      return EDisplay::kWebkitInlineBox;
    case EDisplay::kFlex:
      return EDisplay::kInlineFlex;
    case EDisplay::kGrid:
      return EDisplay::kInlineGrid;
    case EDisplay::kGridLanes:
      return EDisplay::kInlineGridLanes;
    case EDisplay::kBlockMath:
      return EDisplay::kMath;
    case EDisplay::kBlockRuby:
      return EDisplay::kRuby;
    case EDisplay::kListItem:
      return EDisplay::kInlineListItem;
    case EDisplay::kLayoutCustom:
      return EDisplay::kInlineLayoutCustom;

    case EDisplay::kInlineFlex:
    case EDisplay::kInlineFlowRootListItem:
    case EDisplay::kInlineGrid:
    case EDisplay::kInlineLayoutCustom:
    case EDisplay::kInlineListItem:
    case EDisplay::kInlineGridLanes:
    case EDisplay::kInlineTable:
    case EDisplay::kMath:
    case EDisplay::kRuby:
    case EDisplay::kWebkitInlineBox:

    case EDisplay::kContents:
    case EDisplay::kInline:
    case EDisplay::kInlineBlock:
    case EDisplay::kTableRowGroup:
    case EDisplay::kTableHeaderGroup:
    case EDisplay::kTableFooterGroup:
    case EDisplay::kTableRow:
    case EDisplay::kTableColumnGroup:
    case EDisplay::kTableColumn:
    case EDisplay::kTableCell:
    case EDisplay::kTableCaption:
    case EDisplay::kRubyText:
      return display;

    case EDisplay::kNone:
      NOTREACHED();
  }
  NOTREACHED();
}

static bool IsOutermostSVGElement(const Element* element) {
  auto* svg_element = DynamicTo<SVGElement>(element);
  return svg_element && svg_element->IsOutermostSVGSVGElement();
}

static bool IsAtMediaUAShadowBoundary(const Element* element) {
  if (!element) {
    return false;
  }
  if (ContainerNode* parent = element->parentNode()) {
    if (auto* shadow_root = DynamicTo<ShadowRoot>(parent)) {
      return shadow_root->host().IsMediaElement();
    }
  }
  return false;
}

// CSS requires text-decoration to be reset at each DOM element for inline
// blocks, inline tables, floating elements, and absolute or relatively
// positioned elements. Outermost <svg> roots are considered to be atomic
// inline-level. Media elements have a special rendering where the media
// controls do not use a proper containing block model which means we need
// to manually stop text-decorations to apply to text inside media controls.
static bool StopPropagateTextDecorations(const ComputedStyleBuilder& builder,
                                         const Element* element) {
  return builder.IsAtomicInlineDisplayType() ||
         IsAtMediaUAShadowBoundary(element) || builder.IsFloating() ||
         builder.HasOutOfFlowPosition() || IsOutermostSVGElement(element) ||
         builder.Display() == EDisplay::kRubyText;
}

static bool LayoutParentStyleForcesZIndexToCreateStackingContext(
    const ComputedStyle& layout_parent_style) {
  return layout_parent_style.IsDisplayFlex() ||
         layout_parent_style.IsDisplayGrid() ||
         layout_parent_style.IsDisplayGridLanes();
}

void StyleAdjuster::AdjustStyleForEditing(ComputedStyleBuilder& builder,
                                          Element* element) {
  if (element && element->editContext()) {
    // If an element is associated with an EditContext, it should
    // become editable and should have -webkit-user-modify set to
    // read-write. This overrides any other values that have been
    // specified for contenteditable or -webkit-user-modify on that element.
    builder.SetUserModify(EUserModify::kReadWrite);
  }

  if (builder.UserModify() != EUserModify::kReadWritePlaintextOnly) {
    return;
  }
  // Collapsing whitespace is harmful in plain-text editing.
  if (builder.WhiteSpace() == EWhiteSpace::kNormal) {
    builder.SetWhiteSpace(EWhiteSpace::kPreWrap);
  } else if (builder.WhiteSpace() == EWhiteSpace::kNowrap) {
    builder.SetWhiteSpace(EWhiteSpace::kPre);
  } else if (builder.WhiteSpace() == EWhiteSpace::kPreLine) {
    builder.SetWhiteSpace(EWhiteSpace::kPreWrap);
  }
}

void StyleAdjuster::AdjustStyleForTextCombine(ComputedStyleBuilder& builder) {
  DCHECK_EQ(builder.Display(), EDisplay::kInlineBlock);
  // Set box sizes
  const Font* font = builder.GetFont();
  DCHECK(font->GetFontDescription().IsVerticalBaseline());
  const auto one_em = ComputedStyle::ComputedFontSizeAsFixed(*font);
  const auto line_height = builder.FontHeight();
  const auto size =
      LengthSize(Length::Fixed(line_height), Length::Fixed(one_em));
  builder.SetContainIntrinsicWidth(StyleIntrinsicLength(size.Width()));
  builder.SetContainIntrinsicHeight(StyleIntrinsicLength(size.Height()));
  builder.SetHeight(size.Height());
  builder.SetLineHeight(size.Height());
  builder.SetMaxHeight(size.Height());
  builder.SetMaxWidth(size.Width());
  builder.SetMinHeight(size.Height());
  builder.SetMinWidth(size.Width());
  builder.SetWidth(size.Width());
  AdjustStyleForCombinedText(builder);
}

void StyleAdjuster::AdjustStyleForCombinedText(ComputedStyleBuilder& builder) {
  builder.ResetTextCombine();
  builder.SetLetterSpacing(Length::Fixed(0.0f));
  builder.SetTextAlign(ETextAlign::kCenter);
  builder.SetTextDecorationLine(TextDecorationLine::kNone);
  builder.SetTextEmphasisMark(TextEmphasisMark::kNone);
  builder.SetVerticalAlign(EVerticalAlign::kMiddle);
  builder.SetWordBreak(EWordBreak::kKeepAll);
  builder.SetWordSpacing(/* 'normal' */ Length::Fixed(0.0f));
  builder.SetWritingMode(WritingMode::kHorizontalTb);

  builder.SetBaseTextDecorationData(nullptr);
  builder.ResetTextIndent();
  builder.UpdateFontOrientation();

#if DCHECK_IS_ON()
  DCHECK_EQ(builder.GetFont()->GetFontDescription().Orientation(),
            FontOrientation::kHorizontal);
  const ComputedStyle* cloned_style = builder.CloneStyle();
  LayoutTextCombine::AssertStyleIsValid(*cloned_style);
#endif
}

static void AdjustStyleForFirstLetter(ComputedStyleBuilder& builder,
                                      const ComputedStyle& parent_style) {
  if (builder.StyleType() != kPseudoIdFirstLetter) {
    return;
  }

  // Force inline display (except for floating first-letters).
  builder.SetDisplay(builder.IsFloating() ? EDisplay::kBlock
                                          : EDisplay::kInline);
  builder.SetContainerFont(parent_style.GetFont());
}

static void AdjustStyleForMarker(ComputedStyleBuilder& builder,
                                 const ComputedStyle& parent_style,
                                 const Element* parent_element) {
  if (builder.StyleType() != kPseudoIdMarker) {
    return;
  }

  if (parent_element->IsPseudoElement()) {
    parent_element = parent_element->parentElement();
  }

  if (parent_style.MarkerShouldBeInside(*parent_element,
                                        builder.GetDisplayStyle())) {
    Document& document = parent_element->GetDocument();
    auto margins =
        ListMarker::InlineMarginsForInside(document, builder, parent_style);
    LogicalToPhysicalSetter setter(builder.GetWritingDirection(), builder,
                                   &ComputedStyleBuilder::SetMarginTop,
                                   &ComputedStyleBuilder::SetMarginRight,
                                   &ComputedStyleBuilder::SetMarginBottom,
                                   &ComputedStyleBuilder::SetMarginLeft);
    setter.SetInlineStart(Length::Fixed(margins.first));
    setter.SetInlineEnd(Length::Fixed(margins.second));
  } else {
    // Outside list markers should generate a block container.
    builder.SetDisplay(EDisplay::kInlineBlock);

    // Do not break inside the marker, and honor the trailing spaces.
    builder.SetWhiteSpace(EWhiteSpace::kPre);

    // Compute margins for 'outside' during layout, because it requires the
    // layout size of the marker.
    // TODO(kojii): absolute position looks more reasonable, and maybe required
    // in some cases, but this is currently blocked by crbug.com/734554
    // builder.SetPosition(EPosition::kAbsolute);
  }
}

void StyleAdjuster::AdjustSliderContainerStyle(const Element& element,
                                               ComputedStyleBuilder& builder) {
  if (!IsHorizontalWritingMode(builder.GetWritingMode())) {
    builder.SetTouchAction(TouchAction::kPanX);
  } else if (RuntimeEnabledFeatures::
                 NonStandardAppearanceValueSliderVerticalEnabled() &&
             builder.Appearance() == AppearanceValue::kSliderVertical) {
    builder.SetTouchAction(TouchAction::kPanX);
    builder.SetWritingMode(WritingMode::kVerticalRl);
    // It's always in RTL because the slider value increases up even in LTR.
    builder.SetDirection(TextDirection::kRtl);
  } else {
    builder.SetTouchAction(TouchAction::kPanY);
    builder.SetWritingMode(WritingMode::kHorizontalTb);
    if (To<HTMLInputElement>(element.OwnerShadowHost())->DataList()) {
      builder.SetAlignSelf(StyleSelfAlignmentData(ItemPosition::kCenter,
                                                  OverflowAlignment::kUnsafe));
    }
  }
}

// static
void StyleAdjuster::AdjustStyleForHTMLElement(ComputedStyleBuilder& builder,
                                              HTMLElement& element) {
  switch (element.GetElementType()) {
    case ElementType::kHTMLImageElement: {
      auto& image = To<HTMLImageElement>(element);
      if (image.IsCollapsed() || builder.Display() == EDisplay::kContents) {
        builder.SetDisplay(EDisplay::kNone);
      }
      break;
    }

    case ElementType::kHTMLTableElement:
      // Tables never support the -webkit-* values for text-align and will reset
      // back to the default.
      if (builder.GetTextAlign() == ETextAlign::kWebkitLeft ||
          builder.GetTextAlign() == ETextAlign::kWebkitCenter ||
          builder.GetTextAlign() == ETextAlign::kWebkitRight) {
        builder.SetTextAlign(ETextAlign::kStart);
      }
      break;

    case ElementType::kHTMLFrameElement:
    case ElementType::kHTMLFrameSetElement:
      // Frames and framesets never honor position:relative or
      // position:absolute. This is necessary to fix a crash where a site tries
      // to position these objects. They also never honor display nor floating.
      builder.SetPosition(EPosition::kStatic);
      builder.SetDisplay(EDisplay::kBlock);
      builder.SetFloating(EFloat::kNone);
      break;

    case ElementType::kHTMLFencedFrameElement:
      // Force the CSS style `zoom` property to 1 so that the embedder cannot
      // communicate into the fenced frame by adjusting it, but still include
      // the page zoom factor in the effective zoom, which is safe because it
      // comes from user intervention. crbug.com/1285327
      builder.SetEffectiveZoom(
          element.GetDocument().GetStyleResolver().InitialZoom());
      break;

    case ElementType::kHTMLLegendElement:
      if (builder.Display() != EDisplay::kContents) {
        // Allow any blockified display value for legends. Note that according
        // to the spec, this shouldn't affect computed style (like we do here).
        // Instead, the display override should be determined during box
        // creation, and even then only be applied to the rendered legend inside
        // a fieldset. However, Blink determines the rendered legend during
        // layout instead of during layout object creation, and also generally
        // makes assumptions that the computed display value is the one to use.
        builder.SetDisplay(EquivalentBlockDisplay(builder.Display()));
      }
      break;

    case ElementType::kHTMLMarqueeElement:
      // For now, <marquee> requires an overflow clip to work properly.
      builder.SetOverflowX(EOverflow::kHidden);
      builder.SetOverflowY(EOverflow::kHidden);
      break;

    case ElementType::kHTMLTextAreaElement:
      // Textarea considers overflow visible as auto.
      builder.SetOverflowX(builder.OverflowX() == EOverflow::kVisible
                               ? EOverflow::kAuto
                               : builder.OverflowX());
      builder.SetOverflowY(builder.OverflowY() == EOverflow::kVisible
                               ? EOverflow::kAuto
                               : builder.OverflowY());

      // See https://drafts.csswg.org/css-display/#unbox-html
      if (builder.Display() == EDisplay::kContents) {
        builder.SetDisplay(EDisplay::kNone);
      }

      break;

    case ElementType::kHTMLEmbedElement:
    case ElementType::kHTMLObjectElement: {
      auto& html_plugin_element = To<HTMLPlugInElement>(element);
      builder.SetRequiresAcceleratedCompositingForExternalReasons(
          html_plugin_element.ShouldAccelerate());
      if (builder.Display() == EDisplay::kContents) {
        builder.SetDisplay(EDisplay::kNone);
      }
      break;
    }

    case ElementType::kHTMLBodyElement:
      if (element.GetDocument().FirstBodyElement() != element) {
        builder.SetIsSecondaryBodyElement();
      }
      break;

    case ElementType::kHTMLBRElement:
    case ElementType::kHTMLWBRElement:
    case ElementType::kHTMLMeterElement:
    case ElementType::kHTMLProgressElement:
    case ElementType::kHTMLCanvasElement:
    case ElementType::kHTMLAudioElement:
    case ElementType::kHTMLVideoElement:
    case ElementType::kHTMLInputElement:
    case ElementType::kHTMLSelectElement:
    case ElementType::kHTMLIFrameElement:
      // See https://drafts.csswg.org/css-display/#unbox-html
      if (builder.Display() == EDisplay::kContents) {
        builder.SetDisplay(EDisplay::kNone);
      }
      break;

    default:
      break;
  }
}

void StyleAdjuster::AdjustOverflow(ComputedStyleBuilder& builder,
                                   Element* element,
                                   Document& document) {
  DCHECK(builder.OverflowX() != EOverflow::kVisible ||
         builder.OverflowY() != EOverflow::kVisible);

  bool single_axis_scroller = false;

  bool overflow_is_clip_or_visible =
      IsOverflowClipOrVisible(builder.OverflowY()) &&
      IsOverflowClipOrVisible(builder.OverflowX());
  if (!overflow_is_clip_or_visible && builder.IsDisplayTable()) {
    // Tables only support overflow:hidden and overflow:visible and ignore
    // anything else, see https://drafts.csswg.org/css2/visufx.html#overflow. As
    // a table is not a block container box the rules for resolving conflicting
    // x and y values in CSS Overflow Module Level 3 do not apply. Arguably
    // overflow-x and overflow-y aren't allowed on tables but all UAs allow it.
    if (builder.OverflowX() != EOverflow::kHidden) {
      builder.SetOverflowX(EOverflow::kVisible);
    }
    if (builder.OverflowY() != EOverflow::kHidden) {
      builder.SetOverflowY(EOverflow::kVisible);
    }
    // If we are left with conflicting overflow values for the x and y axes on a
    // table then resolve both to OverflowVisible. This is interoperable
    // behaviour but is not specced anywhere.
    if (builder.OverflowX() == EOverflow::kVisible) {
      builder.SetOverflowY(EOverflow::kVisible);
    } else if (builder.OverflowY() == EOverflow::kVisible) {
      builder.SetOverflowX(EOverflow::kVisible);
    }
  } else if (!IsOverflowClipOrVisible(builder.OverflowY())) {
    // Values of 'clip' and 'visible' can only be used with 'clip' and
    // 'visible.' If they aren't, 'clip' and 'visible' is reset.
    if (builder.OverflowX() == EOverflow::kVisible) {
      builder.SetOverflowX(EOverflow::kAuto);
    } else if (builder.OverflowX() == EOverflow::kClip) {
      single_axis_scroller = true;
      if (!RuntimeEnabledFeatures::SingleAxisScrollContainersEnabled()) {
        builder.SetOverflowX(EOverflow::kHidden);
      }
    }
  } else if (!IsOverflowClipOrVisible(builder.OverflowX())) {
    // Values of 'clip' and 'visible' can only be used with 'clip' and
    // 'visible.' If they aren't, 'clip' and 'visible' is reset.
    if (builder.OverflowY() == EOverflow::kVisible) {
      builder.SetOverflowY(EOverflow::kAuto);
    } else if (builder.OverflowY() == EOverflow::kClip) {
      single_axis_scroller = true;
      if (!RuntimeEnabledFeatures::SingleAxisScrollContainersEnabled()) {
        builder.SetOverflowY(EOverflow::kHidden);
      }
    }
  }

  if (single_axis_scroller) {
    UseCounter::Count(document, WebFeature::kSingleAxisScroller);
  }

  if (element && !element->IsPseudoElement() &&
      (builder.OverflowX() == EOverflow::kClip ||
       builder.OverflowY() == EOverflow::kClip)) {
    UseCounter::Count(document, WebFeature::kOverflowClipAlongEitherAxis);
  }

  // overlay is a legacy alias of auto.
  // https://drafts.csswg.org/css-overflow-3/#valdef-overflow-auto
  if (builder.OverflowY() == EOverflow::kOverlay) {
    builder.SetOverflowY(EOverflow::kAuto);
  }
  if (builder.OverflowX() == EOverflow::kOverlay) {
    builder.SetOverflowX(EOverflow::kAuto);
  }
}

// https://github.com/WICG/html-in-canvas
// The `layoutsubtree` attribute ... causes the direct children of the <canvas>
// to have a stacking context and become a containing block for all descendants.
static bool ForceStackingAndContainingBlockForCanvasLayoutSubtree(
    const Element* element) {
  if (element && element->IsCanvasOrInCanvasSubtree() &&
      RuntimeEnabledFeatures::CanvasDrawElementEnabled(
          element->GetExecutionContext())) {
    if (const auto* canvas =
            DynamicTo<HTMLCanvasElement>(element->parentElement())) {
      return canvas->layoutSubtree();
    }
  }
  return false;
}

static bool IsCanvasWithDrawElements(const Element* element) {
  if (!element || !element->IsCanvasOrInCanvasSubtree() ||
      !RuntimeEnabledFeatures::CanvasDrawElementEnabled(
          element->GetExecutionContext())) {
    return false;
  }

  if (const auto* canvas = DynamicTo<HTMLCanvasElement>(element)) {
    return canvas->layoutSubtree();
  }

  return false;
}

void StyleAdjuster::AdjustStyleForDisplay(
    ComputedStyleBuilder& builder,
    const ComputedStyle& layout_parent_style,
    const Element* element,
    Document* document) {
  bool force_canvas_child_layout_subtree_styles =
      ForceStackingAndContainingBlockForCanvasLayoutSubtree(element);

  if ((layout_parent_style.BlockifiesChildren() && !HostIsInputFile(element)) ||
      force_canvas_child_layout_subtree_styles) {
    builder.SetIsInBlockifyingDisplay();
    if (builder.Display() != EDisplay::kContents) {
      builder.SetDisplay(EquivalentBlockDisplay(builder.Display()));
      if (!builder.HasOutOfFlowPosition()) {
        builder.SetIsFlexOrGridOrCustomItem();
      }
    }
    if (layout_parent_style.IsDisplayFlex() ||
        layout_parent_style.IsDisplayGrid() ||
        layout_parent_style.IsDisplayGridLanes() ||
        layout_parent_style.IsDisplayMath() ||
        force_canvas_child_layout_subtree_styles) {
      builder.SetIsInsideDisplayIgnoringFloatingChildren();
    }

    if (force_canvas_child_layout_subtree_styles) {
      builder.SetPosition(EPosition::kStatic);
      builder.SetContain(builder.Contain() | kContainsPaint);
    }
  }

  if (layout_parent_style.InlinifiesChildren() &&
      !builder.HasOutOfFlowPosition() && ShouldBeInlinified(element) &&
      !force_canvas_child_layout_subtree_styles) {
    if (builder.IsFloating()) {
      builder.SetFloating(EFloat::kNone);
      if (document) {
        document->AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                ConsoleMessage::Source::kRendering,
                ConsoleMessage::Level::kInfo,
                "`float` property is not supported correctly inside an element "
                "with `display: ruby` or `display: ruby-text`."),
            true);
      }
    }
    DCHECK(!builder.IsFloating());
    builder.SetIsInInlinifyingDisplay();
    builder.SetDisplay(EquivalentInlineDisplay(builder.Display()));
  }

  if (builder.StyleType() == kPseudoIdScrollMarkerGroup) {
    builder.SetDisplay(EquivalentBlockDisplay(builder.Display()));
  }

  if (builder.Display() == EDisplay::kBlock) {
    return;
  }

  // FIXME: Don't support this mutation for pseudo styles like first-letter or
  // first-line, since it's not completely clear how that should work.
  if (builder.Display() == EDisplay::kInline &&
      builder.StyleType() == kPseudoIdNone &&
      builder.GetWritingMode() != layout_parent_style.GetWritingMode()) {
    builder.SetDisplay(EDisplay::kInlineBlock);
  }

  // writing-mode does not apply to table row groups, table column groups, table
  // rows, and table columns.
  // TODO(crbug.com/736072): Borders specified with logical css properties will
  // not change to reflect new writing mode. ex: border-block-start.
  if (builder.Display() == EDisplay::kTableColumn ||
      builder.Display() == EDisplay::kTableColumnGroup ||
      builder.Display() == EDisplay::kTableFooterGroup ||
      builder.Display() == EDisplay::kTableHeaderGroup ||
      builder.Display() == EDisplay::kTableRow ||
      builder.Display() == EDisplay::kTableRowGroup) {
    // TODO(crbug.com/40527196): This effectively changes the *computed
    // value* of 'writing-mode', which is too late at this point.
    //
    // Note: if/when this is fixed, we can reinstate the NOTREACHED()
    // at the end of StyleCascade::ResolvePendingSubstitution().
    builder.SetWritingMode(layout_parent_style.GetWritingMode());
    builder.SetTextOrientation(layout_parent_style.GetTextOrientation());
    builder.UpdateFontOrientation();
  }

  // Blockify the child boxes of media elements. crbug.com/1379779.
  if (IsAtMediaUAShadowBoundary(element)) {
    builder.SetDisplay(EquivalentBlockDisplay(builder.Display()));
  }

  // display: -webkit-box when used with (-webkit)-line-clamp
  if (builder.BoxOrient() == EBoxOrient::kVertical &&
      (builder.WebkitLineClamp() != 0 ||
       builder.Continue() == EContinue::kCollapse ||
       builder.Continue() == EContinue::kWebkitLegacy)) {
    if (builder.Display() == EDisplay::kWebkitBox) {
      builder.SetDisplay(EDisplay::kFlowRoot);
      builder.SetIsSpecifiedDisplayWebkitBox();
    } else if (builder.Display() == EDisplay::kWebkitInlineBox) {
      builder.SetDisplay(EDisplay::kInlineBlock);
      builder.SetIsSpecifiedDisplayWebkitBox();
    }
  }
}

bool StyleAdjuster::IsEditableElement(Element* element,
                                      const ComputedStyleBuilder& builder) {
  if (builder.UserModify() != EUserModify::kReadOnly) {
    return true;
  }

  if (!element) {
    return false;
  }

  if (auto* textarea = DynamicTo<HTMLTextAreaElement>(*element)) {
    return !textarea->IsDisabledOrReadOnly();
  }

  if (auto* input = DynamicTo<HTMLInputElement>(*element)) {
    return !input->IsDisabledOrReadOnly() && input->IsTextField();
  }

  return false;
}

bool StyleAdjuster::IsPasswordFieldWithUnrevealedPassword(Element* element) {
  if (!element) {
    return false;
  }
  if (auto* input = DynamicTo<HTMLInputElement>(element)) {
    return input->FormControlType() == FormControlType::kInputPassword &&
           !input->ShouldRevealPassword();
  }
  return false;
}

void StyleAdjuster::AdjustEffectiveTouchAction(
    ComputedStyleBuilder& builder,
    const ComputedStyle& parent_style,
    Element* element,
    bool is_svg_root) {
  TouchAction inherited_action = parent_style.EffectiveTouchAction();

  if (!element) {
    builder.SetEffectiveTouchAction(TouchAction::kAuto & inherited_action);
    return;
  }

  // Touch actions are only supported by elements that support both the CSS
  // width and height properties.
  // See https://www.w3.org/TR/pointerevents/#the-touch-action-css-property.
  TouchAction element_touch_action = builder.GetTouchAction();
  bool ignore_touch_action_property;
  if (element_touch_action == TouchAction::kAuto) {
    // Fast path; the desired touch-action is already auto,
    // so don't go through all the checks below.
    ignore_touch_action_property = true;
  } else if (builder.IsDisplayTableRowOrColumnType()) {
    ignore_touch_action_property = true;
  } else if (IsA<HTMLImageElement>(element) || is_svg_root) {
    // Images and SVG roots support touch-action,
    // so leave the value alone.
    ignore_touch_action_property = false;
  } else if (IsA<HTMLCanvasElement>(element) &&
             element->GetExecutionContext() &&
             element->GetExecutionContext()->CanExecuteScripts(
                 kNotAboutToExecuteScript)) {
    // Replaced <canvas>, too.
    ignore_touch_action_property = false;
  } else {
    ignore_touch_action_property = builder.IsNonAtomicInlineDisplayType();
  }

  if (ignore_touch_action_property ||
      !element->LayoutObjectIsNeeded(builder.GetDisplayStyle())) {
    element_touch_action = TouchAction::kAuto;

    if (inherited_action == TouchAction::kAuto &&
        element != element->GetDocument().documentElement() &&
        !IsA<HTMLFrameOwnerElement>(element) &&
        !::features::IsSwipeToMoveCursorEnabled() &&
        !RuntimeEnabledFeatures::StylusHandwritingEnabled()) {
      // Fast path; both inherited and current value allow everything
      // (so no bits can be added), and none of the features that would
      // remove bits from inherited_action are active. Also, we don't
      // need to propagate any bits across frames. (We need to set it
      // explicitly even though kAuto is the default, in case StyleAdjuster
      // is called twice with different parameters.)
      //
      // If needed, we could loosen up this fast path (e.g., by moving
      // it to below the else and testing for element_touch_action == kAuto,
      // moving it further to below the is_child_document check, etc.),
      // but it seems that Clang is happier optimizing it this way
      // and it is hit in the vast majority of cases anyway.
      builder.SetEffectiveTouchAction(TouchAction::kAuto);
      return;
    }
  } else {
    // kInternalPanXScrolls is only for internal usage, GetTouchAction()
    // doesn't contain this bit. We set this bit when kPanX is set so it can be
    // cleared for eligible editable areas later on.
    if ((element_touch_action & TouchAction::kPanX) != TouchAction::kNone) {
      element_touch_action |= TouchAction::kInternalPanXScrolls;
    }

    // kInternalNotWritable is only for internal usage, GetTouchAction()
    // doesn't contain this bit. We set this bit when kPan is set so it can be
    // cleared for eligible non-password editable areas later on.
    if ((element_touch_action & TouchAction::kPan) != TouchAction::kNone) {
      element_touch_action |= TouchAction::kInternalNotWritable;
    }
  }

  bool is_child_document = element == element->GetDocument().documentElement();

  // Apply touch action inherited from parent frame.
  if (is_child_document && element->GetDocument().GetFrame()) {
    inherited_action &=
        TouchAction::kPan | TouchAction::kInternalPanXScrolls |
        TouchAction::kInternalNotWritable |
        element->GetDocument().GetFrame()->InheritedEffectiveTouchAction();
  }

  // The effective touch action is the intersection of the touch-action values
  // of the current element and all of its ancestors up to the one that
  // implements the gesture. Since panning is implemented by the scroller it is
  // re-enabled for scrolling elements.
  // The panning-restricted cancellation should also apply to iframes, so we
  // allow (panning & local touch action) on the first descendant element of a
  // iframe element.
  inherited_action = AdjustTouchActionForElement(inherited_action, builder,
                                                 parent_style, element);

  TouchAction enforced_by_policy = TouchAction::kNone;
  if (element->GetDocument().IsVerticalScrollEnforced()) {
    enforced_by_policy = TouchAction::kPanY;
  }
  if (::features::IsSwipeToMoveCursorEnabled() &&
      IsEditableElement(element, builder)) {
    element_touch_action &= ~TouchAction::kInternalPanXScrolls;
  }

  // TODO(crbug.com/40232387): Full style invalidation is needed when this
  // feature status changes at runtime as it affects the computed style.
  if (RuntimeEnabledFeatures::StylusHandwritingEnabled() &&
      (element_touch_action & TouchAction::kPan) == TouchAction::kPan &&
      IsEditableElement(element, builder) &&
      !IsPasswordFieldWithUnrevealedPassword(element)) {
    element_touch_action &= ~TouchAction::kInternalNotWritable;
  }

  // Apply the adjusted parent effective touch actions.
  builder.SetEffectiveTouchAction((element_touch_action & inherited_action) |
                                  enforced_by_policy);

  // Propagate touch action to child frames.
  if (auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(element)) {
    Frame* content_frame = frame_owner->ContentFrame();
    if (content_frame) {
      content_frame->SetInheritedEffectiveTouchAction(
          builder.EffectiveTouchAction());
    }
  }
}

void StyleAdjuster::AdjustForForcedColorsMode(ComputedStyleBuilder& builder,
                                              Document& document) {
  if (!builder.InForcedColorsMode() ||
      builder.ForcedColorAdjust() != EForcedColorAdjust::kAuto) {
    return;
  }

  builder.SetTextShadow(ComputedStyleInitialValues::InitialTextShadow());
  builder.SetBoxShadow(ComputedStyleInitialValues::InitialBoxShadow());
  builder.SetColorScheme({AtomicString("light"), AtomicString("dark")});
  builder.SetScrollbarColor(
      ComputedStyleInitialValues::InitialScrollbarColor());
  if (builder.ShouldForceColor(builder.AccentColor())) {
    builder.SetAccentColor(ComputedStyleInitialValues::InitialAccentColor());
  }
  if (!builder.HasUrlBackgroundImage()) {
    builder.ClearBackgroundImage();
  }

  mojom::blink::ColorScheme color_scheme = mojom::blink::ColorScheme::kLight;
  if (document.GetStyleEngine().GetPreferredColorScheme() ==
      mojom::blink::PreferredColorScheme::kDark) {
    color_scheme = mojom::blink::ColorScheme::kDark;
  }
  const ui::ColorProvider* color_provider =
      document.GetColorProviderForPainting(color_scheme);
  auto can_expose_accent_color =
      document.IsInWebAppScope() && document.IsInitialProfile();

  // Re-resolve some internal forced color properties whose initial
  // values are system colors. This is necessary to ensure we get
  // the correct computed value from the color provider for the
  // system color when the theme changes.
  if (builder.InternalForcedBackgroundColor().IsSystemColor()) {
    builder.SetInternalForcedBackgroundColor(
        builder.InternalForcedBackgroundColor().ResolveSystemColor(
            color_scheme, color_provider, can_expose_accent_color));
  }
  // Per the CSS Color Adjustment specification [1]:
  // In forced-colors mode, if 'font-variant-emoji' computes to 'normal' or
  // 'unicode', emoji should be forced to render in their monochrome
  // (text-style) variant, if available.
  //
  // [1] https://www.w3.org/TR/css-color-adjust-1/#forced-colors-properties
  FontVariantEmoji variant = builder.GetFontDescription().VariantEmoji();
  if (RuntimeEnabledFeatures::EmojiMonochromeRenderingEnabled() &&
      (variant == kNormalVariantEmoji || variant == kUnicodeVariantEmoji)) {
    builder.SetFontVariantEmoji(kTextVariantEmoji);
  }
  if (builder.InternalForcedColor().IsSystemColor()) {
    builder.SetInternalForcedColor(
        builder.InternalForcedColor().ResolveSystemColor(
            color_scheme, color_provider, can_expose_accent_color));
  }
  if (builder.InternalForcedVisitedColor().IsSystemColor()) {
    builder.SetInternalForcedVisitedColor(
        builder.InternalForcedVisitedColor().ResolveSystemColor(
            color_scheme, color_provider, can_expose_accent_color));
  }
}

void StyleAdjuster::AdjustForSVGTextElement(ComputedStyleBuilder& builder) {
  // TODO(mstensho): We only need to reset the properties that may actually
  // enable multicol here. As of multicol level 1, that's just `column-count`
  // and `column-width`. Once speccing of level 2 `column-wrap` and
  // `column-height` is done, these may also become such properties, though.
  builder.SetColumnGap(ComputedStyleInitialValues::InitialColumnGap());
  builder.SetColumnWidthInternal(
      ComputedStyleInitialValues::InitialColumnWidth());
  builder.SetColumnHeightInternal(
      ComputedStyleInitialValues::InitialColumnHeight());
  builder.SetColumnRuleStyle(
      ComputedStyleInitialValues::InitialColumnRuleStyle());
  builder.SetColumnRuleWidthInternal(
      ComputedStyleInitialValues::InitialColumnRuleWidth());
  builder.SetColumnRuleColor(
      ComputedStyleInitialValues::InitialColumnRuleColor());
  builder.SetInternalVisitedColumnRuleColor(
      ComputedStyleInitialValues::InitialInternalVisitedColumnRuleColor());
  builder.SetColumnCountInternal(
      ComputedStyleInitialValues::InitialColumnCount());
  builder.SetHasAutoColumnCountInternal(
      ComputedStyleInitialValues::InitialHasAutoColumnCount());
  builder.SetHasAutoColumnWidthInternal(
      ComputedStyleInitialValues::InitialHasAutoColumnWidth());
  builder.SetHasAutoColumnHeightInternal(
      ComputedStyleInitialValues::InitialHasAutoColumnHeight());
  builder.ResetColumnFill();
  builder.ResetColumnWrap();
  builder.ResetColumnSpan();
}

void StyleAdjuster::AdjustComputedStyle(StyleResolverState& state,
                                        Element* element) {
  DCHECK(state.LayoutParentStyle());
  DCHECK(state.ParentStyle());
  ComputedStyleBuilder& builder = state.StyleBuilder();
  const ComputedStyle& parent_style = *state.ParentStyle();
  const ComputedStyle& layout_parent_style = *state.LayoutParentStyle();

  auto* html_element = DynamicTo<HTMLElement>(element);
  if (html_element &&
      (builder.Display() != EDisplay::kNone ||
       element->LayoutObjectIsNeeded(builder.GetDisplayStyle()))) {
    AdjustStyleForHTMLElement(builder, *html_element);
  }

  bool is_document_element =
      element && element->GetDocument().documentElement() == element;
  bool is_in_top_layer = false;
  if (!is_document_element && element) {
    if (RuntimeEnabledFeatures::OverlayPropertyEnabled()) {
      if (builder.Overlay() == EOverlay::kAuto) {
        if (RuntimeEnabledFeatures::OverlayGlobalRuleRemovalEnabled()) {
          is_in_top_layer = element->IsInTopLayer();
        } else {
          is_in_top_layer = true;
        }
      }
    } else {
      is_in_top_layer = element->IsRenderedInTopLayer();
    }
  }

  if (builder.Display() != EDisplay::kNone) {
    // https://drafts.csswg.org/css-position-4/#top-styling
    // Elements in the top layer must be out-of-flow positioned.
    // Root elements that are in the top layer should just be left alone
    // because the fullscreen.css doesn't apply any style to them.
    //
    // Similarly, overscroll-position elements must be out of flow positioned
    // with a box.
    if (is_in_top_layer || builder.StyleType() == kPseudoIdBackdrop ||
        builder.InternalOverscrollPosition() ==
            EInternalOverscrollPosition::kAuto) {
      if (!builder.HasOutOfFlowPosition()) {
        builder.SetPosition(EPosition::kAbsolute);
      }
      if (builder.Display() == EDisplay::kContents) {
        // See crbug.com/1240701 for more details.
        // https://fullscreen.spec.whatwg.org/#new-stacking-layer
        // If its specified display property is contents, it computes to block.
        builder.SetDisplay(EDisplay::kBlock);
      }
    }

    // Absolute/fixed positioned elements, floating elements and the document
    // element need block-like outside display.
    if (is_document_element ||
        (builder.Display() != EDisplay::kContents &&
         (builder.HasOutOfFlowPosition() || builder.IsFloating()))) {
      builder.SetDisplay(EquivalentBlockDisplay(builder.Display()));
    }

    // math display values on non-MathML elements compute to flow display
    // values.
    if (!IsA<MathMLElement>(element) && builder.IsDisplayMath()) {
      builder.SetDisplay(builder.Display() == EDisplay::kBlockMath
                             ? EDisplay::kBlock
                             : EDisplay::kInline);
    }

    AdjustStyleForMarker(builder, parent_style, &state.GetElement());

    if (builder.StyleType() != kPseudoIdScrollMarker) {
      AdjustStyleForDisplay(builder, layout_parent_style, element,
                            element ? &element->GetDocument() : nullptr);
    }

    if (builder.StyleType() == kPseudoIdScrollMarkerGroup) {
      // A scroll marker group always needs layout containment, since it
      // modifies its layout box structure during layout. Only in-flow
      // positioned scroll marker groups need size containment, though, since
      // the size of out-of-flow positioned scroll marker groups don't affect
      // anything on the outside (which is precisely why we DO need it for
      // in-flow groups).
      unsigned containment = builder.Contain() | kContainsLayout;
      if (!builder.HasOutOfFlowPosition()) {
        containment |= kContainsSize;
      }
      builder.SetContain(containment);
    }

    // If this is a child of a LayoutCustom, we need the name of the parent
    // layout function for invalidation purposes.
    if (layout_parent_style.IsDisplayLayoutCustom()) {
      builder.SetDisplayLayoutCustomParentName(
          layout_parent_style.DisplayLayoutCustomName());
    }

    // The root element of the main frame has no backdrop, so don't allow
    // it to have a backdrop filter either.
    if (is_document_element && builder.HasBackdropFilter() &&
        element->GetDocument().IsInMainFrame()) {
      builder.SetBackdropFilter(FilterOperations());
    }
    if (builder.InternalOverscrollArea() != EInternalOverscrollArea::kNone) {
      // TODO(crbug.com/467112943): Layout containment is currently forced to
      // ensure that the container of the overscroll areas actually contains
      // the overscroll areas. However, requiring layout containment is
      // overly restrictive to the child content that can be used within
      // the scroller. We should remove this requirement while ensure they are
      // layout children of the container element.
      builder.SetContain(builder.Contain() | kContainsLayout);
    }
  }

  // We don't adjust the first letter style earlier because we may change the
  // display setting above (including in AdjustStyleForHTMLElement()),
  // and this needs to override those changes.
  AdjustStyleForFirstLetter(builder, parent_style);

  builder.SetForcesStackingContext(false);

  // z-index is only applicable if positioned, or if a flex/grid/etc item.
  if (builder.GetPosition() != EPosition::kStatic ||
      LayoutParentStyleForcesZIndexToCreateStackingContext(
          layout_parent_style) ||
      ForceStackingAndContainingBlockForCanvasLayoutSubtree(element)) {
    builder.SetAllowsZIndex(true);
    if (!builder.HasAutoZIndex()) {
      builder.SetForcesStackingContext(true);
    }
  }

  bool is_replaced_normal_flow_video =
      RuntimeEnabledFeatures::StackingContextIsNotStackedEnabled() &&
      IsA<HTMLVideoElement>(element) &&
      builder.GetPosition() == EPosition::kStatic &&
      element->FastHasAttribute(html_names::kControlsAttr);

  if (is_document_element || is_replaced_normal_flow_video ||
      (element && IsA<SVGForeignObjectElement>(*element)) || is_in_top_layer ||
      builder.StyleType() == kPseudoIdBackdrop ||
      builder.StyleType() == kPseudoIdViewTransition ||
      IsCanvasWithDrawElements(element)) {
    builder.SetForcesStackingContext(true);
  }

  if (builder.OverflowX() != EOverflow::kVisible ||
      builder.OverflowY() != EOverflow::kVisible) {
    AdjustOverflow(builder, element ? element : state.GetPseudoElement(),
                   state.GetDocument());
  }

  // Highlight pseudos propagate decorations with inheritance only.
  if (StopPropagateTextDecorations(builder, element) ||
      state.IsForHighlight()) {
    builder.SetBaseTextDecorationData(nullptr);
  } else {
    builder.SetBaseTextDecorationData(
        layout_parent_style.AppliedTextDecorationData());
  }

  // The computed value of currentColor for highlight pseudos is the
  // color that would have been used if no highlights were applied,
  // i.e. the originating element's color.
  if (state.IsForHighlight() && state.OriginatingElementStyle()) {
    const ComputedStyle* originating_style = state.OriginatingElementStyle();
    if (builder.ColorIsCurrentColor()) {
      builder.SetColor(originating_style->Color());
    }
    if (builder.InternalVisitedColorIsCurrentColor()) {
      builder.SetInternalVisitedColor(
          originating_style->InternalVisitedColor());
    }
  }

  // Cull out any useless layers and also repeat patterns into additional
  // layers.
  builder.AdjustBackgroundLayers();
  builder.AdjustMaskLayers();

  // A subset of CSS properties should be forced at computed value time:
  // https://drafts.csswg.org/css-color-adjust-1/#forced-colors-properties.
  AdjustForForcedColorsMode(builder, state.GetDocument());

  if (element && IsSliderContainer(*element)) {
    // NOTE: This needs to come before AdjustEffectiveTouchAction().
    AdjustSliderContainerStyle(*element, builder);
  }

  AdjustStyleForEditing(builder, element);

  if (auto* svg_element = DynamicTo<SVGElement>(element); svg_element) {
    auto* styled_element = DynamicTo<SVGElement>(state.GetStyledElement());
    AdjustStyleForSvgElement(*svg_element, styled_element, builder,
                             layout_parent_style);
  } else if (IsA<MathMLElement>(element)) {
    if (builder.Display() == EDisplay::kContents) {
      // https://drafts.csswg.org/css-display/#unbox-mathml
      builder.SetDisplay(EDisplay::kNone);
    }
  }

  // If this node is sticky it marks the creation of a sticky subtree, which we
  // must track to properly handle document lifecycle in some cases.
  //
  // It is possible that this node is already in a sticky subtree (i.e. we have
  // nested sticky nodes) - in that case the bit will already be set via
  // inheritance from the ancestor and there is no harm to setting it again.
  if (builder.GetPosition() == EPosition::kSticky) {
    builder.SetSubtreeIsSticky(true);
  }

  // If the inherited value of justify-items includes the 'legacy'
  // keyword (plus 'left', 'right' or 'center'), 'legacy' computes to
  // the the inherited value.  Otherwise, 'auto' computes to 'normal'.
  if (parent_style.JustifyItems().PositionType() == ItemPositionType::kLegacy &&
      builder.JustifyItems().GetPosition() == ItemPosition::kLegacy) {
    builder.SetJustifyItems(parent_style.JustifyItems());
  }

  AdjustEffectiveTouchAction(builder, parent_style, element,
                             IsOutermostSVGElement(element));

  if (element && element->IsInShadowTree()) {
    const AtomicString& pseudo_id = element->ShadowPseudoId();
    if (!pseudo_id.IsNull()) {
      if (!builder.TextOverflow().IsClip() &&
          (pseudo_id == shadow_element_names::kPseudoInputPlaceholder ||
           pseudo_id == shadow_element_names::kPseudoInternalInputSuggested)) {
        TextControlElement* text_control =
            ToTextControl(element->OwnerShadowHost());
        DCHECK(text_control);
        // TODO(futhark@chromium.org): We force clipping text overflow for
        // focused input elements since we don't want to render ellipsis
        // during editing. We should do this as a general solution which also
        // includes contenteditable elements being edited. The computed style
        // should not change, but
        // LayoutBlockFlow::ShouldTruncateOverflowingText() should instead
        // return false when text is being edited inside that block.
        // https://crbug.com/814954
        builder.SetTextOverflow(text_control->ValueForTextOverflow());
      }
    }
  }

  if (builder.ContentVisibility() == EContentVisibility::kAuto) {
    builder.SetContainIntrinsicSizeAuto();
  }
}

void StyleAdjuster::RunUncacheableStyleAdjustment(
    ComputedStyleBuilder& builder,
    Element& element,
    const Element* element_or_pseudo_element,
    const Element* styled_element) {
  // Elements are almost never view transition scopes (i.e., we get an
  // early-out), so it's just as cheap to do the logic here as in
  // AdjustComputedStyle().
  if (element.GetDocument().GetViewTransitionsIfExists()) {
    if (const ViewTransition* view_transition =
            ViewTransitionUtils::GetTransition(element);
        view_transition && view_transition->Scope() == &element) {
      bool is_document_element =
          element.GetDocument().documentElement() == element;
      if (!is_document_element) {
        builder.SetContain(builder.Contain() | kContainsLayout);
        builder.SetViewTransitionScope(EViewTransitionScope::kAll);
      }
      builder.SetForcesStackingContext(true);
    }

    // We need to use styled element here to ensure coverage for
    // pseudo-elements.
    if (styled_element &&
        ViewTransitionUtils::IsViewTransitionElementExcludingRootFromSupplement(
            *styled_element)) {
      builder.SetElementIsViewTransitionParticipant();
    }
  }

  // The layout theme has its own style adjustment, mostly related to
  // the appearance property (although it can also modify display,
  // seemingly for historical reasons).
  if (builder.Appearance() == AppearanceValue::kNone ||
      !element_or_pseudo_element) {
    builder.SetEffectiveAppearance(AppearanceValue::kNone);
  } else {
    LayoutTheme::GetTheme().AdjustStyle(*element_or_pseudo_element, builder);
  }

  bool is_media_control =
      element.ShadowPseudoId().starts_with("-webkit-media-controls");
  if (is_media_control && !builder.HasEffectiveAppearance()) {
    // For compatibility reasons if the element is a media control and the
    // -webkit-appearance is none then we should clear the background image.
    builder.MutableBackgroundInternal().ClearImage();
  }

  if (builder.HasBaseAppearance() &&
      element.SupportsBaseAppearance(builder.Appearance())) {
    builder.SetInBaseAppearance(true);
  }
  if (builder.InBaseAppearance() && !builder.HasBaseAppearance()) {
    // Don't allow base appearance to be inherited to elements which actually
    // support the appearance property.
    if (element.SupportsBaseAppearance(AppearanceValue::kBase) ||
        element.SupportsBaseAppearance(AppearanceValue::kBaseSelect)) {
      builder.SetInBaseAppearance(false);
    }
  }

  if (element.HasCustomStyleCallbacks()) {
    element.AdjustStyle(base::PassKey<StyleAdjuster>(), builder);
  }
}

bool StyleAdjuster::IsCacheCompatible(
    const ComputedStyle& parent_style_a,
    const ComputedStyle& layout_parent_style_a,
    const ComputedStyle& parent_style_b,
    const ComputedStyle& layout_parent_style_b) {
  if (parent_style_a.JustifyItems() != parent_style_b.JustifyItems()) {
    // There are special inheritance rules for justify-items
    // that can affect the computed values in the child.
    return false;
  }
  if (&layout_parent_style_a == &layout_parent_style_b) {
    // Short-circuit the rest below.
    return true;
  }
  if (layout_parent_style_a.Display() != layout_parent_style_b.Display() ||
      layout_parent_style_a.IsInBlockifyingDisplay() !=
          layout_parent_style_b.IsInBlockifyingDisplay() ||
      layout_parent_style_a.IsInInlinifyingDisplay() !=
          layout_parent_style_b.IsInInlinifyingDisplay()) {
    // The layout parent style's display style affects blockification,
    // stacking context, and many other things.
    return false;
  }
  if (layout_parent_style_a.GetWritingMode() !=
      layout_parent_style_b.GetWritingMode()) {
    // Changing writing-mode from the layout parent can move
    // inline to inline-block. Also, it can be simply copied down
    // to the element.
    return false;
  }
  if (layout_parent_style_a.GetTextOrientation() !=
      layout_parent_style_b.GetTextOrientation()) {
    // text-orientation can also be copied from the layout parent
    // to the element.
    return false;
  }
  if (layout_parent_style_a.CssDominantBaseline() !=
      layout_parent_style_b.CssDominantBaseline()) {
    // And similarly, CssDominantBaseline().
    return false;
  }
  if (!base::ValuesEquivalent(
          layout_parent_style_a.AppliedTextDecorationData(),
          layout_parent_style_b.AppliedTextDecorationData())) {
    // Applied text decorations are normally (but not always)
    // copied down from the layout parent.
    return false;
  }
  return true;
}

StyleAdjuster::ElementTypeForCache StyleAdjuster::GetElementTypeCacheKey(
    const ComputedStyle& layout_parent_style,
    const Element& element) {
  // NOTE: As described in the .h file, kIsNotElement is a special value
  // for “cannot cache style adjustment for this element”.

  // Has special handling for top-layer, blockification, stacking context
  // and many other things.
  bool is_document_element = element.GetDocument().documentElement() == element;
  if (is_document_element) {
    return {ElementType::kIsNotElement};
  }

  // Has special handling in AdjustStyleForEditing().
  if (element.editContext()) {
    return {ElementType::kIsNotElement};
  }

  // Has special handling in a number of places (including depending on
  // parents' layoutSubtree() status).
  if (element.IsCanvasOrInCanvasSubtree()) {
    return {ElementType::kIsNotElement};
  }

  // Some UA shadow elements have special handling of background and
  // text-overflow, as well as special slider container style.
  // (It is possible that we could loosen this check a bit.)
  if (!element.ShadowPseudoId().IsNull()) {
    return {ElementType::kIsNotElement};
  }

  // If the layout parent style inlinifies children, we need to do a tree walk
  // to find out if our (non-replaced) parent is a field set of media element
  // (see ShouldBeInlinified()).
  if (layout_parent_style.InlinifiesChildren()) {
    return {ElementType::kIsNotElement};
  }

  // UA media shadow boundaries have special rules around blockification
  // and text decorations.
  if (IsAtMediaUAShadowBoundary(&element)) {
    return {ElementType::kIsNotElement};
  }

  // Depending on feature flags, these could make IsRenderedInTopLayer()
  // return true, which influences display and others.
  if (element.FastHasAttribute(html_names::kPopoverAttr) ||
      IsA<HTMLDialogElement>(element)) {
    return {ElementType::kIsNotElement};
  }

  // Depending on feature flags, can affect a bunch of things like
  // display and position.
  if (element.IsInTopLayer()) {
    return {ElementType::kIsNotElement};
  }

  switch (element.GetElementType()) {
    case ElementType::kHTMLCanvasElement:
      // <canvas> has special handling for touch-action and stacking contexts
      // depending on whether it has layoutSubtree() or not, and also
      // CanExecuteScripts(). It seems rare enough that we don't bother checking
      // the properties on the elements, and just exclude all canvas elements.
      return {ElementType::kIsNotElement};

    case ElementType::kHTMLTextAreaElement:
      // Have special handling based on HTML attributes setting them to disabled
      // or read-only. Also, special handling of overflow and display, but this
      // is moot, as we exclude them anyway.
      return {ElementType::kIsNotElement};

    case ElementType::kHTMLVideoElement:
      // <video> can have special stacking behavior depending on the
      // controls="" attribute.
      return {ElementType::kIsNotElement};

    case ElementType::kHTMLBodyElement:
      // The first <body> element is treated differently from the others,
      // both for touch-action and for setting a ComputedStyle flag
      // (forcing style invalidation).
      if (element.GetDocument().FirstBodyElement() != element) {
        return {ElementType::kIsNotElement};
      }
      return {element.GetElementType()};

    case ElementType::kHTMLImageElement:
      if (To<HTMLImageElement>(element).IsCollapsed()) {
        // Has special display handling.
        return {ElementType::kIsNotElement};
      }
      return {element.GetElementType()};

    case ElementType::kHTMLFrameElement:
    case ElementType::kHTMLIFrameElement:
    case ElementType::kHTMLFencedFrameElement:
      // HTMLFrameOwnerElement descendants have special touch-action behavior.
      // There is also special handling of position and zoom, but this is moot,
      // as we exclude them anyway.
      return {ElementType::kIsNotElement};

    case ElementType::kHTMLFrameSetElement:
      // Special handling of position.
      return {element.GetElementType()};

    case ElementType::kHTMLInputElement: {
      // Text fields have special handling based on HTML attributes setting them
      // to disabled or read-only (and also unrevealed passwords).
      // <input type="file"> also has special handling. But we cannot take out
      // all inputs; they are so common (e.g. <input type="hidden">).
      const HTMLInputElement& input = To<HTMLInputElement>(element);
      if (input.FormControlType() == FormControlType::kInputFile ||
          input.FormControlType() == FormControlType::kInputPassword) {
        return {ElementType::kIsNotElement};
      }
      if (input.IsDisabledOrReadOnly()) {
        return {ElementType::kIsNotElement};
      }

      // There's various other handling of <input> that is different from
      // other types, but none that distinguish different inputs from each
      // other, so we can cache these.
      return {ElementType::kHTMLInputElement};
    }

    case ElementType::kHTMLTableElement:
      // Special handling of text-align.
      return {element.GetElementType()};

    case ElementType::kHTMLLegendElement:
      // Special handling of display.
      return {element.GetElementType()};

    case ElementType::kHTMLMarqueeElement:
      // Special handling of overflow.
      return {element.GetElementType()};

    case ElementType::kHTMLEmbedElement:
    case ElementType::kHTMLObjectElement:
      // Special handling of display, and sets an acceleration flag.
      return {ElementType::kHTMLEmbedElement};

    case ElementType::kHTMLBRElement:
    case ElementType::kHTMLWBRElement:
    case ElementType::kHTMLMeterElement:
    case ElementType::kHTMLProgressElement:
    case ElementType::kHTMLAudioElement:
    case ElementType::kHTMLSelectElement:
      // Special handling of display: contents. (Shared with a couple
      // of others, like kHTMLCanvasElement, that are already handled
      // specially.)
      return {ElementType::kHTMLBRElement};

    // SVG and MathML have special handling.
    case ElementType::kMathMLElement:
    case ElementType::kMathMLFractionElement:
    case ElementType::kMathMLOperatorElement:
    case ElementType::kMathMLPaddedElement:
    case ElementType::kMathMLRadicalElement:
    case ElementType::kMathMLRowElement:
    case ElementType::kMathMLScriptsElement:
    case ElementType::kMathMLSpaceElement:
    case ElementType::kMathMLTableCellElement:
    case ElementType::kMathMLTokenElement:
    case ElementType::kMathMLUnderOverElement:
      return {ElementType::kMathMLElement};

    case ElementType::kSVGSVGElement:
      if (!To<SVGSVGElement>(element).IsOutermostSVGSVGElement()) {
        // <svg> within <svg> has special handling, and as this seems
        // to be rare, we simply exclude them as a special case.
        return {ElementType::kIsNotElement};
      } else {
        // Even outermost SVG elements have special display handling.
        return {element.GetElementType()};
      }

    // SVG <use> elements can refer to other SVG elements (so they can e.g.
    // behave as a non-outermost <svg> element), and generally have
    // unpredictable behavior. (It is possible that this is too conservative.)
    case ElementType::kSVGUseElement:
      return {ElementType::kIsNotElement};

    // <g> and <tspan> have special display handling (but the same one).
    case ElementType::kSVGGElement:
    case ElementType::kSVGTSpanElement:
      return {ElementType::kSVGGElement};

    // <foreignObject> has special (different) display handling,
    // and forces a stacking context.
    case ElementType::kSVGForeignObjectElement:
      return {element.GetElementType()};

    // <text>, too, and its own special rules about columns.
    case ElementType::kSVGTextElement:
      return {element.GetElementType()};

    // The rest of the SVG elements are generally handled the same;
    // they're SVG elements, but not different from each other.
    case ElementType::kSVGAElement:
    case ElementType::kSVGAnimateElement:
    case ElementType::kSVGAnimateMotionElement:
    case ElementType::kSVGAnimateTransformElement:
    case ElementType::kSVGCircleElement:
    case ElementType::kSVGClipPathElement:
    case ElementType::kSVGDefsElement:
    case ElementType::kSVGDescElement:
    case ElementType::kSVGEllipseElement:
    case ElementType::kSVGFEBlendElement:
    case ElementType::kSVGFEColorMatrixElement:
    case ElementType::kSVGFEComponentTransferElement:
    case ElementType::kSVGFECompositeElement:
    case ElementType::kSVGFEConvolveMatrixElement:
    case ElementType::kSVGFEDiffuseLightingElement:
    case ElementType::kSVGFEDisplacementMapElement:
    case ElementType::kSVGFEDistantLightElement:
    case ElementType::kSVGFEDropShadowElement:
    case ElementType::kSVGFEFloodElement:
    case ElementType::kSVGFEFuncAElement:
    case ElementType::kSVGFEFuncBElement:
    case ElementType::kSVGFEFuncGElement:
    case ElementType::kSVGFEFuncRElement:
    case ElementType::kSVGFEGaussianBlurElement:
    case ElementType::kSVGFEImageElement:
    case ElementType::kSVGFEMergeElement:
    case ElementType::kSVGFEMergeNodeElement:
    case ElementType::kSVGFEMorphologyElement:
    case ElementType::kSVGFEOffsetElement:
    case ElementType::kSVGFEPointLightElement:
    case ElementType::kSVGFESpecularLightingElement:
    case ElementType::kSVGFESpotLightElement:
    case ElementType::kSVGFETileElement:
    case ElementType::kSVGFETurbulenceElement:
    case ElementType::kSVGFilterElement:
    case ElementType::kSVGImageElement:
    case ElementType::kSVGLinearGradientElement:
    case ElementType::kSVGLineElement:
    case ElementType::kSVGMarkerElement:
    case ElementType::kSVGMaskElement:
    case ElementType::kSVGMetadataElement:
    case ElementType::kSVGMPathElement:
    case ElementType::kSVGPathElement:
    case ElementType::kSVGPatternElement:
    case ElementType::kSVGPolygonElement:
    case ElementType::kSVGPolylineElement:
    case ElementType::kSVGRadialGradientElement:
    case ElementType::kSVGRectElement:
    case ElementType::kSVGScriptElement:
    case ElementType::kSVGSetElement:
    case ElementType::kSVGStopElement:
    case ElementType::kSVGStyleElement:
    case ElementType::kSVGSwitchElement:
    case ElementType::kSVGSymbolElement:
    case ElementType::kSVGTextPathElement:
    case ElementType::kSVGTitleElement:
    case ElementType::kSVGUnknownElement:
    case ElementType::kSVGViewElement:
      return {ElementType::kSVGAElement};

    // None of the others are treated differently from each other,
    // so we can collapse them into one for better caching.
    case ElementType::kIsNotElement:
    case ElementType::kHTMLAnchorElement:
    case ElementType::kHTMLAreaElement:
    case ElementType::kHTMLBaseElement:
    case ElementType::kHTMLBDIElement:
    case ElementType::kHTMLButtonElement:
    case ElementType::kHTMLCredentialElement:
    case ElementType::kHTMLDataElement:
    case ElementType::kHTMLDataListElement:
    case ElementType::kHTMLDetailsElement:
    case ElementType::kHTMLDialogElement:
    case ElementType::kHTMLDirectoryElement:
    case ElementType::kHTMLDivElement:
    case ElementType::kHTMLDListElement:
    case ElementType::kHTMLElement:
    case ElementType::kHTMLFieldSetElement:
    case ElementType::kHTMLFontElement:
    case ElementType::kHTMLFormElement:
    case ElementType::kHTMLGeolocationElement:
    case ElementType::kHTMLHeadElement:
    case ElementType::kHTMLHeadingElement:
    case ElementType::kHTMLHRElement:
    case ElementType::kHTMLHtmlElement:
    case ElementType::kHTMLInstallElement:
    case ElementType::kHTMLLabelElement:
    case ElementType::kHTMLLIElement:
    case ElementType::kHTMLLinkElement:
    case ElementType::kHTMLLoginElement:
    case ElementType::kHTMLMapElement:
    case ElementType::kHTMLMenuBarElement:
    case ElementType::kHTMLMenuElement:
    case ElementType::kHTMLMenuItemElement:
    case ElementType::kHTMLMenuListElement:
    case ElementType::kHTMLMetaElement:
    case ElementType::kHTMLModElement:
    case ElementType::kHTMLNoEmbedElement:
    case ElementType::kHTMLNoScriptElement:
    case ElementType::kHTMLOListElement:
    case ElementType::kHTMLOptGroupElement:
    case ElementType::kHTMLOptionElement:
    case ElementType::kHTMLOutputElement:
    case ElementType::kHTMLParagraphElement:
    case ElementType::kHTMLParamElement:
    case ElementType::kHTMLPictureElement:
    case ElementType::kHTMLPreElement:
    case ElementType::kHTMLQuoteElement:
    case ElementType::kHTMLScriptElement:
    case ElementType::kHTMLSearchElement:
    case ElementType::kHTMLSelectedContentElement:
    case ElementType::kHTMLSlotElement:
    case ElementType::kHTMLSourceElement:
    case ElementType::kHTMLSpanElement:
    case ElementType::kHTMLStyleElement:
    case ElementType::kHTMLSummaryElement:
    case ElementType::kHTMLTableCaptionElement:
    case ElementType::kHTMLTableCellElement:
    case ElementType::kHTMLTableColElement:
    case ElementType::kHTMLTableRowElement:
    case ElementType::kHTMLTableSectionElement:
    case ElementType::kHTMLTemplateElement:
    case ElementType::kHTMLTimeElement:
    case ElementType::kHTMLTitleElement:
    case ElementType::kHTMLTrackElement:
    case ElementType::kHTMLUListElement:
    case ElementType::kHTMLUnknownElement:
    case ElementType::kHTMLUserMediaElement:
      return {ElementType::kHTMLDivElement};

      // Don't add a default here; new SVG/MathML elements need to be different
      // from new HTML elements, even if the StyleAdjuster does not otherwise
      // care about them.
  }
}

}  // namespace blink
