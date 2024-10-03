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
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
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
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
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
  if (element == element->GetDocument().FirstBodyElement()) {
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

void AdjustStyleForSvgElement(const SVGElement& element,
                              ComputedStyleBuilder& builder) {
  // Disable some of text decoration properties.
  //
  // Note that SetFooBar() is more efficient than ResetFooBar() if the current
  // value is same as the reset value.
  builder.SetTextDecorationSkipInk(ETextDecorationSkipInk::kAuto);
  builder.SetTextDecorationStyle(
      ETextDecorationStyle::kSolid);  // crbug.com/1246719
  builder.SetTextDecorationThickness(TextDecorationThickness(Length::Auto()));
  builder.SetTextEmphasisMark(TextEmphasisMark::kNone);
  builder.SetTextUnderlineOffset(Length());  // crbug.com/1247912
  builder.SetTextUnderlinePosition(TextUnderlinePosition::kAuto);
}

bool ElementForcesStackingContext(Element* element) {
  if (!element) {
    return false;
  }
  if (element == element->GetDocument().documentElement()) {
    return true;
  }
  if (IsA<SVGForeignObjectElement>(*element)) {
    return true;
  }
  return false;
}

}  // namespace

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
    case EDisplay::kMasonry:
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
    case EDisplay::kInlineMasonry:
      return EDisplay::kMasonry;

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
      NOTREACHED_IN_MIGRATION();
      return display;
  }
  NOTREACHED_IN_MIGRATION();
  return EDisplay::kBlock;
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
    case EDisplay::kMasonry:
      return EDisplay::kInlineMasonry;
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
    case EDisplay::kInlineMasonry:
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
      NOTREACHED_IN_MIGRATION();
      return display;
  }
  NOTREACHED_IN_MIGRATION();
  return EDisplay::kBlock;
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
  return builder.IsDisplayReplacedType() ||
         IsAtMediaUAShadowBoundary(element) || builder.IsFloating() ||
         builder.HasOutOfFlowPosition() || IsOutermostSVGElement(element) ||
         builder.Display() == EDisplay::kRubyText;
}

static bool LayoutParentStyleForcesZIndexToCreateStackingContext(
    const ComputedStyle& layout_parent_style) {
  return layout_parent_style.IsDisplayFlexibleOrGridBox();
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
  const Font& font = builder.GetFont();
  DCHECK(font.GetFontDescription().IsVerticalBaseline());
  const auto one_em = ComputedStyle::ComputedFontSizeAsFixed(builder.GetFont());
  const auto line_height = builder.FontHeight();
  const auto size =
      LengthSize(Length::Fixed(line_height), Length::Fixed(one_em));
  builder.SetContainIntrinsicWidth(StyleIntrinsicLength(false, size.Width()));
  builder.SetContainIntrinsicHeight(StyleIntrinsicLength(false, size.Height()));
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
  builder.SetLetterSpacing(0.0f);
  builder.SetTextAlign(ETextAlign::kCenter);
  builder.SetTextDecorationLine(TextDecorationLine::kNone);
  builder.SetTextEmphasisMark(TextEmphasisMark::kNone);
  builder.SetVerticalAlign(EVerticalAlign::kMiddle);
  builder.SetWordBreak(EWordBreak::kKeepAll);
  builder.SetWordSpacing(0.0f);
  builder.SetWritingMode(WritingMode::kHorizontalTb);

  builder.SetBaseTextDecorationData(nullptr);
  builder.ResetTextIndent();
  builder.UpdateFontOrientation();

#if DCHECK_IS_ON()
  DCHECK_EQ(builder.GetFont().GetFontDescription().Orientation(),
            FontOrientation::kHorizontal);
  const ComputedStyle* cloned_style = builder.CloneStyle();
  LayoutTextCombine::AssertStyleIsValid(*cloned_style);
#endif
}

static void AdjustStyleForFirstLetter(ComputedStyleBuilder& builder) {
  if (builder.StyleType() != kPseudoIdFirstLetter) {
    return;
  }

  // Force inline display (except for floating first-letters).
  builder.SetDisplay(builder.IsFloating() ? EDisplay::kBlock
                                          : EDisplay::kInline);
}

static void AdjustStyleForMarker(ComputedStyleBuilder& builder,
                                 const ComputedStyle& parent_style,
                                 const Element& parent_element) {
  if (builder.StyleType() != kPseudoIdMarker) {
    return;
  }

  if (parent_style.MarkerShouldBeInside(parent_element,
                                        builder.GetDisplayStyle())) {
    Document& document = parent_element.GetDocument();
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

static void AdjustStyleForHTMLElement(ComputedStyleBuilder& builder,
                                      HTMLElement& element) {
  // <div> and <span> are the most common elements on the web, we skip all the
  // work for them.
  if (IsA<HTMLDivElement>(element) || IsA<HTMLSpanElement>(element)) {
    return;
  }

  if (auto* image = DynamicTo<HTMLImageElement>(element)) {
    if (image->IsCollapsed() || builder.Display() == EDisplay::kContents) {
      builder.SetDisplay(EDisplay::kNone);
    }
    return;
  }

  if (IsA<HTMLTableElement>(element)) {
    // Tables never support the -webkit-* values for text-align and will reset
    // back to the default.
    if (builder.GetTextAlign() == ETextAlign::kWebkitLeft ||
        builder.GetTextAlign() == ETextAlign::kWebkitCenter ||
        builder.GetTextAlign() == ETextAlign::kWebkitRight) {
      builder.SetTextAlign(ETextAlign::kStart);
    }
    return;
  }

  if (IsA<HTMLFrameElement>(element) || IsA<HTMLFrameSetElement>(element)) {
    // Frames and framesets never honor position:relative or position:absolute.
    // This is necessary to fix a crash where a site tries to position these
    // objects. They also never honor display nor floating.
    builder.SetPosition(EPosition::kStatic);
    builder.SetDisplay(EDisplay::kBlock);
    builder.SetFloating(EFloat::kNone);
    return;
  }

  if (IsA<HTMLFrameElementBase>(element)) {
    if (builder.Display() == EDisplay::kContents) {
      builder.SetDisplay(EDisplay::kNone);
      return;
    }
    return;
  }

  if (IsA<HTMLFencedFrameElement>(element)) {
    // Force the CSS style `zoom` property to 1 so that the embedder cannot
    // communicate into the fenced frame by adjusting it, but still include
    // the page zoom factor in the effective zoom, which is safe because it
    // comes from user intervention. crbug.com/1285327
    builder.SetEffectiveZoom(
        element.GetDocument().GetStyleResolver().InitialZoom());
  }

  if (IsA<HTMLLegendElement>(element) &&
      builder.Display() != EDisplay::kContents) {
    // Allow any blockified display value for legends. Note that according to
    // the spec, this shouldn't affect computed style (like we do here).
    // Instead, the display override should be determined during box creation,
    // and even then only be applied to the rendered legend inside a
    // fieldset. However, Blink determines the rendered legend during layout
    // instead of during layout object creation, and also generally makes
    // assumptions that the computed display value is the one to use.
    builder.SetDisplay(EquivalentBlockDisplay(builder.Display()));
    return;
  }

  if (IsA<HTMLMarqueeElement>(element)) {
    // For now, <marquee> requires an overflow clip to work properly.
    builder.SetOverflowX(EOverflow::kHidden);
    builder.SetOverflowY(EOverflow::kHidden);
    return;
  }

  if (IsA<HTMLTextAreaElement>(element)) {
    // Textarea considers overflow visible as auto.
    builder.SetOverflowX(builder.OverflowX() == EOverflow::kVisible
                             ? EOverflow::kAuto
                             : builder.OverflowX());
    builder.SetOverflowY(builder.OverflowY() == EOverflow::kVisible
                             ? EOverflow::kAuto
                             : builder.OverflowY());
    if (builder.Display() == EDisplay::kContents) {
      builder.SetDisplay(EDisplay::kNone);
    }
    return;
  }

  if (auto* html_plugin_element = DynamicTo<HTMLPlugInElement>(element)) {
    builder.SetRequiresAcceleratedCompositingForExternalReasons(
        html_plugin_element->ShouldAccelerate());
    if (builder.Display() == EDisplay::kContents) {
      builder.SetDisplay(EDisplay::kNone);
    }
    return;
  }

  if (IsA<HTMLUListElement>(element) || IsA<HTMLOListElement>(element)) {
    builder.SetIsInsideListElement();
    return;
  }

  if (builder.Display() == EDisplay::kContents) {
    // See https://drafts.csswg.org/css-display/#unbox-html
    // Some of these elements are handled with other adjustments above.
    if (IsA<HTMLBRElement>(element) || IsA<HTMLWBRElement>(element) ||
        IsA<HTMLMeterElement>(element) || IsA<HTMLProgressElement>(element) ||
        IsA<HTMLCanvasElement>(element) || IsA<HTMLMediaElement>(element) ||
        IsA<HTMLInputElement>(element) || IsA<HTMLTextAreaElement>(element) ||
        IsA<HTMLSelectElement>(element)) {
      builder.SetDisplay(EDisplay::kNone);
    }
  }

  if (IsA<HTMLBodyElement>(element) &&
      element.GetDocument().FirstBodyElement() != element) {
    builder.SetIsSecondaryBodyElement();
  }
}

void StyleAdjuster::AdjustOverflow(ComputedStyleBuilder& builder,
                                   Element* element) {
  DCHECK(builder.OverflowX() != EOverflow::kVisible ||
         builder.OverflowY() != EOverflow::kVisible);

  bool overflow_is_clip_or_visible =
      IsOverflowClipOrVisible(builder.OverflowY()) &&
      IsOverflowClipOrVisible(builder.OverflowX());
  if (!overflow_is_clip_or_visible && builder.IsDisplayTableBox()) {
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
      builder.SetOverflowX(EOverflow::kHidden);
    }
  } else if (!IsOverflowClipOrVisible(builder.OverflowX())) {
    // Values of 'clip' and 'visible' can only be used with 'clip' and
    // 'visible.' If they aren't, 'clip' and 'visible' is reset.
    if (builder.OverflowY() == EOverflow::kVisible) {
      builder.SetOverflowY(EOverflow::kAuto);
    } else if (builder.OverflowY() == EOverflow::kClip) {
      builder.SetOverflowY(EOverflow::kHidden);
    }
  }

  if (element && !element->IsPseudoElement() &&
      (builder.OverflowX() == EOverflow::kClip ||
       builder.OverflowY() == EOverflow::kClip)) {
    UseCounter::Count(element->GetDocument(),
                      WebFeature::kOverflowClipAlongEitherAxis);
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

// g-issues.chromium.org/issues/349835587
// https://github.com/WICG/canvas-place-element
static bool IsCanvasPlacedElement(const Element* element) {
  if (RuntimeEnabledFeatures::CanvasPlaceElementEnabled() && element) {
    // Only want to do the different layout if placeElement has been called.
    if (const auto* canvas =
            DynamicTo<HTMLCanvasElement>(element->parentElement())) {
      return canvas->HasPlacedElements();
    }
  }

  return false;
}

static void AdjustStyleForDisplay(ComputedStyleBuilder& builder,
                                  const ComputedStyle& layout_parent_style,
                                  const Element* element,
                                  Document* document) {
  bool is_canvas_placed_element = IsCanvasPlacedElement(element);

  if ((layout_parent_style.BlockifiesChildren() && !HostIsInputFile(element)) ||
      is_canvas_placed_element) {
    builder.SetIsInBlockifyingDisplay();
    if (builder.Display() != EDisplay::kContents) {
      builder.SetDisplay(EquivalentBlockDisplay(builder.Display()));
      if (!builder.HasOutOfFlowPosition()) {
        builder.SetIsFlexOrGridOrCustomItem();
      }
    }
    if (layout_parent_style.IsDisplayFlexibleOrGridBox() ||
        layout_parent_style.IsDisplayMathType() || is_canvas_placed_element) {
      builder.SetIsInsideDisplayIgnoringFloatingChildren();
    }

    if (is_canvas_placed_element) {
      builder.SetPosition(EPosition::kStatic);
    }
  }

  // We need to avoid to inlinify children of a <fieldset>, which creates a
  // dedicated LayoutObject and it assumes only block children.
  if (layout_parent_style.InlinifiesChildren() &&
      !builder.HasOutOfFlowPosition() &&
      !(element && IsA<HTMLFieldSetElement>(element->parentNode()))) {
    if (RuntimeEnabledFeatures::RubyLineBreakableEnabled() &&
        builder.IsFloating()) {
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
    if (!builder.IsFloating()) {
      builder.SetIsInInlinifyingDisplay();
      builder.SetDisplay(EquivalentInlineDisplay(builder.Display()));
    }
  }

  // TODO(332396355): Remove temporary blockifing of ::scroll-marker pseudo
  // elements.
  if (builder.StyleType() == kPseudoIdScrollMarker) {
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
    builder.SetWritingMode(layout_parent_style.GetWritingMode());
    builder.SetTextOrientation(layout_parent_style.GetTextOrientation());
    builder.UpdateFontOrientation();
  }

  // Blockify the child boxes of media elements. crbug.com/1379779.
  if (IsAtMediaUAShadowBoundary(element)) {
    builder.SetDisplay(EquivalentBlockDisplay(builder.Display()));
  }

  // display: -webkit-box when used with (-webkit)-line-clamp
  if (RuntimeEnabledFeatures::CSSLineClampWebkitBoxBlockificationEnabled() &&
      builder.BoxOrient() == EBoxOrient::kVertical &&
      (builder.WebkitLineClamp() != 0 || builder.StandardLineClamp() != 0 ||
       builder.HasAutoStandardLineClamp())) {
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

  bool is_replaced_canvas = IsA<HTMLCanvasElement>(element) &&
                            element->GetExecutionContext() &&
                            element->GetExecutionContext()->CanExecuteScripts(
                                kNotAboutToExecuteScript);
  bool is_non_replaced_inline_elements =
      builder.IsDisplayInlineType() &&
      !(builder.IsDisplayReplacedType() || is_svg_root ||
        IsA<HTMLImageElement>(element) || is_replaced_canvas);
  bool is_table_row_or_column = builder.IsDisplayTableRowOrColumnType();
  bool is_layout_object_needed =
      element->LayoutObjectIsNeeded(builder.GetDisplayStyle());

  TouchAction element_touch_action = TouchAction::kAuto;
  // Touch actions are only supported by elements that support both the CSS
  // width and height properties.
  // See https://www.w3.org/TR/pointerevents/#the-touch-action-css-property.
  if (!is_non_replaced_inline_elements && !is_table_row_or_column &&
      is_layout_object_needed) {
    element_touch_action = builder.GetTouchAction();
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

  // TODO(crbug.com/1346169): Full style invalidation is needed when this
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

static void AdjustStyleForInert(ComputedStyleBuilder& builder,
                                Element* element) {
  if (!element) {
    return;
  }

  Document& document = element->GetDocument();
  if (element->IsInertRoot()) {
    UseCounter::Count(document, WebFeature::kInertAttribute);
    builder.SetIsHTMLInert(true);
    builder.SetIsHTMLInertIsInherited(false);
    return;
  }

  const Element* modal_element = document.ActiveModalDialog();
  if (!modal_element) {
    modal_element = Fullscreen::FullscreenElementFrom(document);
  }
  if (modal_element == element) {
    builder.SetIsHTMLInert(false);
    builder.SetIsHTMLInertIsInherited(false);
    return;
  }
  if (modal_element && element == document.documentElement()) {
    builder.SetIsHTMLInert(true);
    builder.SetIsHTMLInertIsInherited(false);
    return;
  }

  if (StyleBaseData* base_data = builder.BaseData()) {
    if (base_data->GetBaseComputedStyle()->Display() == EDisplay::kNone) {
      // Elements which are transitioning to display:none should become inert:
      // https://github.com/w3c/csswg-drafts/issues/8389
      builder.SetIsHTMLInert(true);
      builder.SetIsHTMLInertIsInherited(false);
      return;
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
  auto is_in_web_app_scope = document.IsInWebAppScope();

  // Re-resolve some internal forced color properties whose initial
  // values are system colors. This is necessary to ensure we get
  // the correct computed value from the color provider for the
  // system color when the theme changes.
  if (builder.InternalForcedBackgroundColor().IsSystemColor()) {
    builder.SetInternalForcedBackgroundColor(
        builder.InternalForcedBackgroundColor().ResolveSystemColor(
            color_scheme, color_provider, is_in_web_app_scope));
  }
  if (builder.InternalForcedColor().IsSystemColor()) {
    builder.SetInternalForcedColor(
        builder.InternalForcedColor().ResolveSystemColor(
            color_scheme, color_provider, is_in_web_app_scope));
  }
  if (builder.InternalForcedVisitedColor().IsSystemColor()) {
    builder.SetInternalForcedVisitedColor(
        builder.InternalForcedVisitedColor().ResolveSystemColor(
            color_scheme, color_provider, is_in_web_app_scope));
  }
}

void StyleAdjuster::AdjustForSVGTextElement(ComputedStyleBuilder& builder) {
  builder.SetColumnGap(ComputedStyleInitialValues::InitialColumnGap());
  builder.SetColumnWidthInternal(
      ComputedStyleInitialValues::InitialColumnWidth());
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
  builder.ResetColumnFill();
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

  auto* svg_element = DynamicTo<SVGElement>(element);

  if (builder.Display() != EDisplay::kNone) {
    if (svg_element) {
      AdjustStyleForSvgElement(*svg_element, builder);
    }

    bool is_document_element =
        element && element->GetDocument().documentElement() == element;
    // https://drafts.csswg.org/css-position-4/#top-styling
    // Elements in the top layer must be out-of-flow positioned.
    // Root elements that are in the top layer should just be left alone
    // because the fullscreen.css doesn't apply any style to them.
    if ((builder.Overlay() == EOverlay::kAuto && !is_document_element) ||
        builder.StyleType() == kPseudoIdBackdrop) {
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
    if (builder.Display() != EDisplay::kContents &&
        (builder.HasOutOfFlowPosition() || builder.IsFloating())) {
      builder.SetDisplay(EquivalentBlockDisplay(builder.Display()));
    }

    if (is_document_element) {
      builder.SetDisplay(EquivalentBlockDisplay(builder.Display()));
    }

    // math display values on non-MathML elements compute to flow display
    // values.
    if (!IsA<MathMLElement>(element) && builder.IsDisplayMathType()) {
      builder.SetDisplay(builder.Display() == EDisplay::kBlockMath
                             ? EDisplay::kBlock
                             : EDisplay::kInline);
    }

    // We don't adjust the first letter style earlier because we may change the
    // display setting in AdjustStyleForHTMLElement() above.
    AdjustStyleForFirstLetter(builder);
    AdjustStyleForMarker(builder, parent_style, state.GetElement());

    AdjustStyleForDisplay(builder, layout_parent_style, element,
                          element ? &element->GetDocument() : nullptr);

    // If this is a child of a LayoutCustom, we need the name of the parent
    // layout function for invalidation purposes.
    if (layout_parent_style.IsDisplayLayoutCustomBox()) {
      builder.SetDisplayLayoutCustomParentName(
          layout_parent_style.DisplayLayoutCustomName());
    }

    bool is_in_main_frame = element && element->GetDocument().IsInMainFrame();
    // The root element of the main frame has no backdrop, so don't allow
    // it to have a backdrop filter either.
    if (is_document_element && is_in_main_frame &&
        builder.HasBackdropFilter()) {
      builder.SetBackdropFilter(FilterOperations());
    }
  } else {
    AdjustStyleForFirstLetter(builder);
  }

  builder.SetForcesStackingContext(false);

  // Make sure our z-index value is only applied if the object is positioned.
  if (!builder.HasAutoZIndex()) {
    if (builder.GetPosition() == EPosition::kStatic &&
        !LayoutParentStyleForcesZIndexToCreateStackingContext(
            layout_parent_style)) {
      builder.SetEffectiveZIndexZero(true);
    } else {
      builder.SetForcesStackingContext(true);
    }
  }

  if (ElementForcesStackingContext(element)) {
    builder.SetForcesStackingContext(true);
  }

  if (builder.Overlay() == EOverlay::kAuto ||
      builder.StyleType() == kPseudoIdBackdrop ||
      builder.StyleType() == kPseudoIdViewTransition) {
    builder.SetForcesStackingContext(true);
  }

  // Though will-change is not itself an inherited property, the intent
  // expressed by 'will-change: contents' includes descendants.
  // (We can't mark will-change as inherited and copy this in
  // WillChange::ApplyInherit(), as Apply() for noninherited
  // properties, like will-change, gets skipped on partial MPC hits.)
  if (state.ParentStyle()->SubtreeWillChangeContents()) {
    builder.SetSubtreeWillChangeContents(true);
  }

  if (builder.OverflowX() != EOverflow::kVisible ||
      builder.OverflowY() != EOverflow::kVisible) {
    AdjustOverflow(builder, element ? element : state.GetPseudoElement());
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
  if (state.UsesHighlightPseudoInheritance() &&
      state.OriginatingElementStyle()) {
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

  // Let the theme also have a crack at adjusting the style.
  LayoutTheme::GetTheme().AdjustStyle(element, builder);

  AdjustStyleForInert(builder, element);

  AdjustStyleForEditing(builder, element);

  bool is_svg_root = false;

  if (svg_element) {
    is_svg_root = svg_element->IsOutermostSVGSVGElement();
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
    if ((IsA<SVGForeignObjectElement>(*element) ||
         IsA<SVGTextElement>(*element)) &&
        builder.IsDisplayInlineType()) {
      builder.SetDisplay(EDisplay::kBlock);
    }

    // Columns don't apply to svg text elements.
    if (IsA<SVGTextElement>(*element)) {
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

  AdjustEffectiveTouchAction(builder, parent_style, element, is_svg_root);

  bool is_media_control =
      element && element->ShadowPseudoId().StartsWith("-webkit-media-controls");
  if (is_media_control && !builder.HasEffectiveAppearance()) {
    // For compatibility reasons if the element is a media control and the
    // -webkit-appearance is none then we should clear the background image.
    builder.MutableBackgroundInternal().ClearImage();
  }

  if (element && builder.TextOverflow() == ETextOverflow::kEllipsis) {
    const AtomicString& pseudo_id = element->ShadowPseudoId();
    if (pseudo_id == shadow_element_names::kPseudoInputPlaceholder ||
        pseudo_id == shadow_element_names::kPseudoInternalInputSuggested) {
      TextControlElement* text_control =
          ToTextControl(element->OwnerShadowHost());
      DCHECK(text_control);
      // TODO(futhark@chromium.org): We force clipping text overflow for focused
      // input elements since we don't want to render ellipsis during editing.
      // We should do this as a general solution which also includes
      // contenteditable elements being edited. The computed style should not
      // change, but LayoutBlockFlow::ShouldTruncateOverflowingText() should
      // instead return false when text is being edited inside that block.
      // https://crbug.com/814954
      builder.SetTextOverflow(text_control->ValueForTextOverflow());
    }
  }

  if (element && element->HasCustomStyleCallbacks()) {
    element->AdjustStyle(base::PassKey<StyleAdjuster>(), builder);
  }

  // We need to use styled element here to ensure coverage for pseudo-elements.
  if (state.GetStyledElement() &&
      ViewTransitionUtils::IsViewTransitionElementExcludingRootFromSupplement(
          *state.GetStyledElement())) {
    builder.SetElementIsViewTransitionParticipant();
  }

  if (RuntimeEnabledFeatures::
          CSSContentVisibilityImpliesContainIntrinsicSizeAutoEnabled() &&
      builder.ContentVisibility() == EContentVisibility::kAuto) {
    builder.SetContainIntrinsicSizeAuto();
  }
}

}  // namespace blink
