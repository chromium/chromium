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

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/list_marker.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/transforms/transform_operations.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "ui/base/ui_base_features.h"

namespace blink {

namespace {

bool IsOverflowClipOrVisible(EOverflow overflow) {
  return overflow == EOverflow::kClip || overflow == EOverflow::kVisible;
}

bool IsEditableElement(Element* element, const ComputedStyle& style) {
  if (style.UserModify() != EUserModify::kReadOnly)
    return true;

  if (!element)
    return false;

  if (auto* textarea = DynamicTo<HTMLTextAreaElement>(*element))
    return !textarea->IsDisabledOrReadOnly();

  if (auto* input = DynamicTo<HTMLInputElement>(*element))
    return !input->IsDisabledOrReadOnly() && input->IsTextField();

  return false;
}

TouchAction AdjustTouchActionForElement(TouchAction touch_action,
                                        const ComputedStyle& style,
                                        Element* element) {
  // if body is the viewport defining element then ScrollsOverflow should
  // return false as body should have overflow-x/overflow-y set to visible
  Element* body = element ? element->GetDocument().body() : nullptr;
  bool is_body_and_viewport =
      element && element == body &&
      body == element->GetDocument().ViewportDefiningElement();
  bool is_child_document =
      element && element == element->GetDocument().documentElement() &&
      element->GetDocument().LocalOwner();
  if ((!is_body_and_viewport && style.ScrollsOverflow()) || is_child_document)
    return touch_action | TouchAction::kPan;
  return touch_action;
}

bool HostIsInputFile(const Element* element) {
  if (!element || !element->IsInUserAgentShadowRoot())
    return false;
  if (const Element* shadow_host = element->OwnerShadowHost()) {
    if (const auto* input = DynamicTo<HTMLInputElement>(shadow_host))
      return input->type() == input_type_names::kFile;
  }
  return false;
}

}  // namespace

static EDisplay EquivalentBlockDisplay(EDisplay display) {
  switch (display) {
    case EDisplay::kBlock:
    case EDisplay::kTable:
    case EDisplay::kWebkitBox:
    case EDisplay::kFlex:
    case EDisplay::kGrid:
    case EDisplay::kBlockMath:
    case EDisplay::kListItem:
    case EDisplay::kFlowRoot:
    case EDisplay::kLayoutCustom:
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
    case EDisplay::kInlineLayoutCustom:
      return EDisplay::kLayoutCustom;

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
      return EDisplay::kBlock;
    case EDisplay::kNone:
      NOTREACHED();
      return display;
  }
  NOTREACHED();
  return EDisplay::kBlock;
}

static bool IsOutermostSVGElement(const Element* element) {
  auto* svg_element = DynamicTo<SVGElement>(element);
  return svg_element && svg_element->IsOutermostSVGSVGElement();
}

static bool IsAtMediaUAShadowBoundary(const Element* element) {
  if (!element)
    return false;
  if (ContainerNode* parent = element->parentNode()) {
    if (auto* shadow_root = DynamicTo<ShadowRoot>(parent))
      return shadow_root->host().IsMediaElement();
  }
  return false;
}

// CSS requires text-decoration to be reset at each DOM element for inline
// blocks, inline tables, floating elements, and absolute or relatively
// positioned elements. Outermost <svg> roots are considered to be atomic
// inline-level. Media elements have a special rendering where the media
// controls do not use a proper containing block model which means we need
// to manually stop text-decorations to apply to text inside media controls.
static bool StopPropagateTextDecorations(const ComputedStyle& style,
                                         const Element* element) {
  return style.Display() == EDisplay::kInlineTable ||
         style.Display() == EDisplay::kInlineBlock ||
         style.Display() == EDisplay::kWebkitInlineBox ||
         IsAtMediaUAShadowBoundary(element) || style.IsFloating() ||
         style.HasOutOfFlowPosition() || IsOutermostSVGElement(element) ||
         IsA<HTMLRTElement>(element);
}

// Certain elements (<a>, <font>) override text decoration colors.  "The font
// element is expected to override the color of any text decoration that spans
// the text of the element to the used value of the element's 'color' property."
// (https://html.spec.whatwg.org/C/#phrasing-content-3)
// The <a> behavior is non-standard.
static bool OverridesTextDecorationColors(const Element* element) {
  return element &&
         (IsA<HTMLFontElement>(element) || IsA<HTMLAnchorElement>(element));
}

// FIXME: This helper is only needed because pseudoStyleForElement passes a null
// element to adjustComputedStyle, so we can't just use element->isInTopLayer().
static bool IsInTopLayer(const Element* element, const ComputedStyle& style) {
  return (element && element->IsInTopLayer()) ||
         style.StyleType() == kPseudoIdBackdrop;
}

static bool LayoutParentStyleForcesZIndexToCreateStackingContext(
    const ComputedStyle& layout_parent_style) {
  return layout_parent_style.IsDisplayFlexibleOrGridBox();
}

void StyleAdjuster::AdjustStyleForEditing(ComputedStyle& style) {
  if (style.UserModify() != EUserModify::kReadWritePlaintextOnly)
    return;
  // Collapsing whitespace is harmful in plain-text editing.
  if (style.WhiteSpace() == EWhiteSpace::kNormal)
    style.SetWhiteSpace(EWhiteSpace::kPreWrap);
  else if (style.WhiteSpace() == EWhiteSpace::kNowrap)
    style.SetWhiteSpace(EWhiteSpace::kPre);
  else if (style.WhiteSpace() == EWhiteSpace::kPreLine)
    style.SetWhiteSpace(EWhiteSpace::kPreWrap);
}

static void AdjustStyleForFirstLetter(ComputedStyle& style) {
  if (style.StyleType() != kPseudoIdFirstLetter)
    return;

  // Force inline display (except for floating first-letters).
  style.SetDisplay(style.IsFloating() ? EDisplay::kBlock : EDisplay::kInline);

  // CSS2 says first-letter can't be positioned.
  style.SetPosition(EPosition::kStatic);
}

static void AdjustStyleForFirstLine(ComputedStyle& style) {
  if (style.StyleType() != kPseudoIdFirstLine)
    return;

  // Force inline display.
  style.SetDisplay(EDisplay::kInline);
}

static void AdjustStyleForMarker(ComputedStyle& style,
                                 const ComputedStyle& parent_style,
                                 const Element& parent_element) {
  if (style.StyleType() != kPseudoIdMarker)
    return;

  bool is_inside =
      parent_style.ListStylePosition() == EListStylePosition::kInside ||
      (IsA<HTMLLIElement>(parent_element) &&
       !parent_style.IsInsideListElement());

  if (is_inside) {
    auto margins = ListMarker::InlineMarginsForInside(style, parent_style);
    style.SetMarginStart(Length::Fixed(margins.first));
    style.SetMarginEnd(Length::Fixed(margins.second));
  } else {
    // Outside list markers should generate a block container.
    style.SetDisplay(EDisplay::kInlineBlock);

    // Do not break inside the marker, and honor the trailing spaces.
    style.SetWhiteSpace(EWhiteSpace::kPre);

    // Compute margins for 'outside' during layout, because it requires the
    // layout size of the marker.
    // TODO(kojii): absolute position looks more reasonable, and maybe required
    // in some cases, but this is currently blocked by crbug.com/734554
    // style.SetPosition(EPosition::kAbsolute);
  }
}

static void AdjustStyleForHTMLElement(ComputedStyle& style,
                                      HTMLElement& element) {
  // <div> and <span> are the most common elements on the web, we skip all the
  // work for them.
  if (IsA<HTMLDivElement>(element) || IsA<HTMLSpanElement>(element))
    return;

  if (IsA<HTMLTableCellElement>(element)) {
    if (style.WhiteSpace() == EWhiteSpace::kWebkitNowrap) {
      // Figure out if we are really nowrapping or if we should just
      // use normal instead. If the width of the cell is fixed, then
      // we don't actually use NOWRAP.
      if (style.Width().IsFixed())
        style.SetWhiteSpace(EWhiteSpace::kNormal);
      else
        style.SetWhiteSpace(EWhiteSpace::kNowrap);
    }
    return;
  }

  if (auto* image = DynamicTo<HTMLImageElement>(element)) {
    if (image->IsCollapsed() || style.Display() == EDisplay::kContents)
      style.SetDisplay(EDisplay::kNone);
    return;
  }

  if (IsA<HTMLTableElement>(element)) {
    // Tables never support the -webkit-* values for text-align and will reset
    // back to the default.
    if (style.GetTextAlign() == ETextAlign::kWebkitLeft ||
        style.GetTextAlign() == ETextAlign::kWebkitCenter ||
        style.GetTextAlign() == ETextAlign::kWebkitRight)
      style.SetTextAlign(ETextAlign::kStart);
    return;
  }

  if (IsA<HTMLFrameElement>(element) || IsA<HTMLFrameSetElement>(element)) {
    // Frames and framesets never honor position:relative or position:absolute.
    // This is necessary to fix a crash where a site tries to position these
    // objects. They also never honor display nor floating.
    style.SetPosition(EPosition::kStatic);
    style.SetDisplay(EDisplay::kBlock);
    style.SetFloating(EFloat::kNone);
    return;
  }

  if (IsA<HTMLFrameElementBase>(element)) {
    if (style.Display() == EDisplay::kContents) {
      style.SetDisplay(EDisplay::kNone);
      return;
    }
    // Frames cannot overflow (they are always the size we ask them to be).
    // Some compositing code paths may try to draw scrollbars anyhow.
    style.SetOverflowX(EOverflow::kVisible);
    style.SetOverflowY(EOverflow::kVisible);
    return;
  }

  if (IsA<HTMLRTElement>(element)) {
    // Ruby text does not support float or position. This might change with
    // evolution of the specification.
    style.SetPosition(EPosition::kStatic);
    style.SetFloating(EFloat::kNone);
    return;
  }

  if (IsA<HTMLLegendElement>(element) &&
      style.Display() != EDisplay::kContents) {
    // Allow any blockified display value for legends. Note that according to
    // the spec, this shouldn't affect computed style (like we do here).
    // Instead, the display override should be determined during box creation,
    // and even then only be applied to the rendered legend inside a
    // fieldset. However, Blink determines the rendered legend during layout
    // instead of during layout object creation, and also generally makes
    // assumptions that the computed display value is the one to use.
    style.SetDisplay(EquivalentBlockDisplay(style.Display()));
    return;
  }

  if (IsA<HTMLMarqueeElement>(element)) {
    // For now, <marquee> requires an overflow clip to work properly.
    style.SetOverflowX(EOverflow::kHidden);
    style.SetOverflowY(EOverflow::kHidden);
    return;
  }

  if (IsA<HTMLTextAreaElement>(element)) {
    // Textarea considers overflow visible as auto.
    style.SetOverflowX(style.OverflowX() == EOverflow::kVisible
                           ? EOverflow::kAuto
                           : style.OverflowX());
    style.SetOverflowY(style.OverflowY() == EOverflow::kVisible
                           ? EOverflow::kAuto
                           : style.OverflowY());
    if (style.Display() == EDisplay::kContents)
      style.SetDisplay(EDisplay::kNone);
    return;
  }

  if (auto* html_plugin_element = DynamicTo<HTMLPlugInElement>(element)) {
    style.SetRequiresAcceleratedCompositingForExternalReasons(
        html_plugin_element->ShouldAccelerate());
    if (style.Display() == EDisplay::kContents)
      style.SetDisplay(EDisplay::kNone);
    return;
  }

  if (IsA<HTMLUListElement>(element) || IsA<HTMLOListElement>(element)) {
    style.SetIsInsideListElement();
    return;
  }

  if (IsA<HTMLSummaryElement>(element)) {
    // <summary> should be a list item by default, but currently it's a block
    // and the disclosure symbol is not a ::marker (bug 590014). If an author
    // specifies 'display: list-item', the <summary> would seem to have two
    // markers (the real one and the disclosure symbol). To avoid this, compute
    // to 'display: block'. This adjustment should go away with bug 590014.
    if (style.Display() == EDisplay::kListItem)
      style.SetDisplay(EDisplay::kBlock);
    return;
  }

  if (style.Display() == EDisplay::kContents) {
    // See https://drafts.csswg.org/css-display/#unbox-html
    // Some of these elements are handled with other adjustments above.
    if (IsA<HTMLBRElement>(element) || IsA<HTMLWBRElement>(element) ||
        IsA<HTMLMeterElement>(element) || IsA<HTMLProgressElement>(element) ||
        IsA<HTMLCanvasElement>(element) || IsA<HTMLMediaElement>(element) ||
        IsA<HTMLInputElement>(element) || IsA<HTMLTextAreaElement>(element) ||
        IsA<HTMLSelectElement>(element)) {
      style.SetDisplay(EDisplay::kNone);
    }
  }
}

void StyleAdjuster::AdjustOverflow(ComputedStyle& style) {
  DCHECK(style.OverflowX() != EOverflow::kVisible ||
         style.OverflowY() != EOverflow::kVisible);

  if (style.IsDisplayTableBox()) {
    // Tables only support overflow:hidden and overflow:visible and ignore
    // anything else, see https://drafts.csswg.org/css2/visufx.html#overflow. As
    // a table is not a block container box the rules for resolving conflicting
    // x and y values in CSS Overflow Module Level 3 do not apply. Arguably
    // overflow-x and overflow-y aren't allowed on tables but all UAs allow it.
    if (style.OverflowX() != EOverflow::kHidden)
      style.SetOverflowX(EOverflow::kVisible);
    if (style.OverflowY() != EOverflow::kHidden)
      style.SetOverflowY(EOverflow::kVisible);
    // If we are left with conflicting overflow values for the x and y axes on a
    // table then resolve both to OverflowVisible. This is interoperable
    // behaviour but is not specced anywhere.
    // TODO(https://crbug.com/966283): figure out how 'clip' should be handled.
    if (style.OverflowX() == EOverflow::kVisible)
      style.SetOverflowY(EOverflow::kVisible);
    else if (style.OverflowY() == EOverflow::kVisible)
      style.SetOverflowX(EOverflow::kVisible);
  } else if (!IsOverflowClipOrVisible(style.OverflowY())) {
    // Values of 'clip' and 'visible' can only be used with 'clip' and
    // 'visible.' If they aren't, 'clip' and 'visible' is reset.
    if (style.OverflowX() == EOverflow::kVisible)
      style.SetOverflowX(EOverflow::kAuto);
    else if (style.OverflowX() == EOverflow::kClip)
      style.SetOverflowX(EOverflow::kHidden);
  } else if (!IsOverflowClipOrVisible(style.OverflowX())) {
    // Values of 'clip' and 'visible' can only be used with 'clip' and
    // 'visible.' If they aren't, 'clip' and 'visible' is reset.
    if (style.OverflowY() == EOverflow::kVisible)
      style.SetOverflowY(EOverflow::kAuto);
    else if (style.OverflowY() == EOverflow::kClip)
      style.SetOverflowY(EOverflow::kHidden);
  }
}

static void AdjustStyleForDisplay(ComputedStyle& style,
                                  const ComputedStyle& layout_parent_style,
                                  const Element* element,
                                  Document* document) {
  // Blockify the children of flex, grid or LayoutCustom containers.
  if (layout_parent_style.BlockifiesChildren() && !HostIsInputFile(element)) {
    style.SetIsInBlockifyingDisplay();
    if (style.Display() != EDisplay::kContents) {
      style.SetDisplay(EquivalentBlockDisplay(style.Display()));
      if (!style.HasOutOfFlowPosition())
        style.SetIsFlexOrGridOrCustomItem();
    }
    if (layout_parent_style.IsDisplayFlexibleOrGridBox())
      style.SetIsFlexOrGridItem();
  }

  if (style.Display() == EDisplay::kBlock && !style.IsFloating())
    return;

  if (style.Display() == EDisplay::kContents)
    return;

  // FIXME: Don't support this mutation for pseudo styles like first-letter or
  // first-line, since it's not completely clear how that should work.
  if (style.Display() == EDisplay::kInline &&
      style.StyleType() == kPseudoIdNone &&
      style.GetWritingMode() != layout_parent_style.GetWritingMode())
    style.SetDisplay(EDisplay::kInlineBlock);

  // Cannot support position: sticky for table columns and column groups because
  // current code is only doing background painting through columns / column
  // groups.
  if ((style.Display() == EDisplay::kTableColumnGroup ||
       style.Display() == EDisplay::kTableColumn) &&
      style.GetPosition() == EPosition::kSticky)
    style.SetPosition(EPosition::kStatic);

  // writing-mode does not apply to table row groups, table column groups, table
  // rows, and table columns.
  if (style.Display() == EDisplay::kTableColumn ||
      style.Display() == EDisplay::kTableColumnGroup ||
      style.Display() == EDisplay::kTableFooterGroup ||
      style.Display() == EDisplay::kTableHeaderGroup ||
      style.Display() == EDisplay::kTableRow ||
      style.Display() == EDisplay::kTableRowGroup) {
    style.SetWritingMode(layout_parent_style.GetWritingMode());
    style.UpdateFontOrientation();
  }

  // FIXME: Since we don't support block-flow on flexible boxes yet, disallow
  // setting of block-flow to anything other than TopToBottomWritingMode.
  // https://bugs.webkit.org/show_bug.cgi?id=46418 - Flexible box support.
  if (style.GetWritingMode() != WritingMode::kHorizontalTb &&
      style.IsDeprecatedWebkitBox()) {
    style.SetWritingMode(WritingMode::kHorizontalTb);
    style.UpdateFontOrientation();
  }

  // Disable editing custom layout elements, until EditingNG is ready.
  if (!RuntimeEnabledFeatures::EditingNGEnabled() &&
      (style.Display() == EDisplay::kLayoutCustom ||
       style.Display() == EDisplay::kInlineLayoutCustom))
    style.SetUserModify(EUserModify::kReadOnly);

  if (layout_parent_style.IsDisplayFlexibleOrGridBox()) {
    // We want to count vertical percentage paddings/margins on flex items
    // because our current behavior is different from the spec and we want to
    // gather compatibility data.
    if (style.PaddingBefore().IsPercentOrCalc() ||
        style.PaddingAfter().IsPercentOrCalc()) {
      UseCounter::Count(document,
                        WebFeature::kFlexboxPercentagePaddingVertical);
    }
    if (style.MarginBefore().IsPercentOrCalc() ||
        style.MarginAfter().IsPercentOrCalc()) {
      UseCounter::Count(document, WebFeature::kFlexboxPercentageMarginVertical);
    }
  }
}

static void AdjustEffectiveTouchAction(ComputedStyle& style,
                                       const ComputedStyle& parent_style,
                                       Element* element,
                                       bool is_svg_root) {
  TouchAction inherited_action = parent_style.GetEffectiveTouchAction();

  bool is_replaced_canvas = element && IsA<HTMLCanvasElement>(element) &&
                            element->GetExecutionContext() &&
                            element->GetExecutionContext()->CanExecuteScripts(
                                kNotAboutToExecuteScript);
  bool is_non_replaced_inline_elements =
      style.IsDisplayInlineType() &&
      !(style.IsDisplayReplacedType() || is_svg_root ||
        IsA<HTMLImageElement>(element) || is_replaced_canvas);
  bool is_table_row_or_column = style.IsDisplayTableRowOrColumnType();
  bool is_layout_object_needed =
      element && element->LayoutObjectIsNeeded(style);

  TouchAction element_touch_action = TouchAction::kAuto;
  // Touch actions are only supported by elements that support both the CSS
  // width and height properties.
  // See https://www.w3.org/TR/pointerevents/#the-touch-action-css-property.
  if (!is_non_replaced_inline_elements && !is_table_row_or_column &&
      is_layout_object_needed) {
    element_touch_action = style.GetTouchAction();
  }

  if (!element) {
    style.SetEffectiveTouchAction(element_touch_action & inherited_action);
    return;
  }

  bool is_child_document = element == element->GetDocument().documentElement();

  // Apply touch action inherited from parent frame.
  if (is_child_document && element->GetDocument().GetFrame()) {
    inherited_action &=
        TouchAction::kPan |
        element->GetDocument().GetFrame()->InheritedEffectiveTouchAction();
  }

  // The effective touch action is the intersection of the touch-action values
  // of the current element and all of its ancestors up to the one that
  // implements the gesture. Since panning is implemented by the scroller it is
  // re-enabled for scrolling elements.
  // The panning-restricted cancellation should also apply to iframes, so we
  // allow (panning & local touch action) on the first descendant element of a
  // iframe element.
  inherited_action =
      AdjustTouchActionForElement(inherited_action, style, element);

  TouchAction enforced_by_policy = TouchAction::kNone;
  if (element->GetDocument().IsVerticalScrollEnforced())
    enforced_by_policy = TouchAction::kPanY;
  if (base::FeatureList::IsEnabled(::features::kSwipeToMoveCursor) &&
      IsEditableElement(element, style)) {
    EventHandlerRegistry& registry =
        element->GetDocument().GetFrame()->GetEventHandlerRegistry();
    registry.DidAddEventHandler(
        *element, EventHandlerRegistry::kTouchStartOrMoveEventBlocking);
  }

  // Apply the adjusted parent effective touch actions.
  style.SetEffectiveTouchAction((element_touch_action & inherited_action) |
                                enforced_by_policy);

  // Propagate touch action to child frames.
  if (auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(element)) {
    Frame* content_frame = frame_owner->ContentFrame();
    if (content_frame) {
      content_frame->SetInheritedEffectiveTouchAction(
          style.GetEffectiveTouchAction());
    }
  }
}

static void AdjustStateForContentVisibility(ComputedStyle& style,
                                            Element* element) {
  if (!element)
    return;
  auto* context = element->GetDisplayLockContext();
  // The common case for most elements is that we don't have a context and have
  // the default (visible) content-visibility value.
  if (LIKELY(!context &&
             style.ContentVisibility() == EContentVisibility::kVisible)) {
    return;
  }

  if (!context)
    context = &element->EnsureDisplayLockContext();
  context->SetRequestedState(style.ContentVisibility());
  context->AdjustElementStyle(&style);
}

void StyleAdjuster::AdjustComputedStyle(StyleResolverState& state,
                                        Element* element) {
  DCHECK(state.LayoutParentStyle());
  DCHECK(state.ParentStyle());
  ComputedStyle& style = state.StyleRef();
  const ComputedStyle& parent_style = *state.ParentStyle();
  const ComputedStyle& layout_parent_style = *state.LayoutParentStyle();

  auto* html_element = DynamicTo<HTMLElement>(element);
  if (html_element && (style.Display() != EDisplay::kNone ||
                       element->LayoutObjectIsNeeded(style))) {
    AdjustStyleForHTMLElement(style, *html_element);
  }
  if (style.Display() != EDisplay::kNone) {
    bool is_document_element =
        element && element->GetDocument().documentElement() == element;
    // Per the spec, position 'static' and 'relative' in the top layer compute
    // to 'absolute'. Root elements that are in the top layer should just
    // be left alone because the fullscreen.css doesn't apply any style to
    // them.
    if (IsInTopLayer(element, style) && !is_document_element &&
        (style.GetPosition() == EPosition::kStatic ||
         style.GetPosition() == EPosition::kRelative))
      style.SetPosition(EPosition::kAbsolute);

    // Absolute/fixed positioned elements, floating elements and the document
    // element need block-like outside display.
    if (style.Display() != EDisplay::kContents &&
        (style.HasOutOfFlowPosition() || style.IsFloating()))
      style.SetDisplay(EquivalentBlockDisplay(style.Display()));

    if (is_document_element)
      style.SetDisplay(EquivalentBlockDisplay(style.Display()));

    // math display values on non-MathML elements compute to flow display
    // values.
    if ((!element || !element->IsMathMLElement()) &&
        style.IsDisplayMathBox(style.Display())) {
      style.SetDisplay(style.Display() == EDisplay::kBlockMath
                           ? EDisplay::kBlock
                           : EDisplay::kInline);
    }

    // We don't adjust the first letter style earlier because we may change the
    // display setting in adjustStyeForTagName() above.
    AdjustStyleForFirstLetter(style);
    AdjustStyleForFirstLine(style);
    AdjustStyleForMarker(style, parent_style, state.GetElement());

    AdjustStyleForDisplay(style, layout_parent_style, element,
                          element ? &element->GetDocument() : nullptr);

    // If this is a child of a LayoutNGCustom, we need the name of the parent
    // layout function for invalidation purposes.
    if (layout_parent_style.IsDisplayLayoutCustomBox()) {
      style.SetDisplayLayoutCustomParentName(
          layout_parent_style.DisplayLayoutCustomName());
    }

    bool is_in_main_frame = element && element->GetDocument().IsInMainFrame();
    // The root element of the main frame has no backdrop, so don't allow
    // it to have a backdrop filter either.
    if (is_document_element && is_in_main_frame && style.HasBackdropFilter())
      style.MutableBackdropFilter().clear();
  } else {
    AdjustStyleForFirstLetter(style);
  }

  if (RuntimeEnabledFeatures::CSSContentVisibilityEnabled())
    AdjustStateForContentVisibility(style, element);

  // Make sure our z-index value is only applied if the object is positioned.
  if (style.GetPosition() == EPosition::kStatic &&
      !LayoutParentStyleForcesZIndexToCreateStackingContext(
          layout_parent_style)) {
    style.SetIsStackingContextWithoutContainment(false);
    if (!style.HasAutoZIndex())
      style.SetEffectiveZIndexZero(true);
  } else if (!style.HasAutoZIndex()) {
    style.SetIsStackingContextWithoutContainment(true);
  }

  if (style.OverflowX() != EOverflow::kVisible ||
      style.OverflowY() != EOverflow::kVisible)
    AdjustOverflow(style);

  // overflow-clip-margin only applies if 'overflow: clip' is set along both
  // axis.
  if (style.OverflowX() != EOverflow::kClip ||
      style.OverflowY() != EOverflow::kClip) {
    style.SetOverflowClipMargin(LayoutUnit());
  }

  if (StopPropagateTextDecorations(style, element))
    style.ClearAppliedTextDecorations();
  else
    style.RestoreParentTextDecorations(parent_style);
  style.ApplyTextDecorations(
      parent_style.VisitedDependentColor(GetCSSPropertyTextDecorationColor()),
      OverridesTextDecorationColors(element));

  // Cull out any useless layers and also repeat patterns into additional
  // layers.
  style.AdjustBackgroundLayers();
  style.AdjustMaskLayers();

  // Let the theme also have a crack at adjusting the style.
  LayoutTheme::GetTheme().AdjustStyle(element, style);

  AdjustStyleForEditing(style);

  bool is_svg_root = false;
  auto* svg_element = DynamicTo<SVGElement>(element);

  if (svg_element) {
    is_svg_root = svg_element->IsOutermostSVGSVGElement();
    if (!is_svg_root) {
      // Only the root <svg> element in an SVG document fragment tree honors css
      // position.
      style.SetPosition(ComputedStyleInitialValues::InitialPosition());
    }

    if (style.Display() == EDisplay::kContents &&
        (is_svg_root ||
         (!IsA<SVGSVGElement>(element) && !IsA<SVGGElement>(element) &&
          !IsA<SVGUseElement>(element) && !IsA<SVGTSpanElement>(element)))) {
      // According to the CSS Display spec[1], nested <svg> elements, <g>,
      // <use>, and <tspan> elements are not rendered and their children are
      // "hoisted". For other elements display:contents behaves as display:none.
      //
      // [1] https://drafts.csswg.org/css-display/#unbox-svg
      style.SetDisplay(EDisplay::kNone);
    }

    // SVG text layout code expects us to be a block-level style element.
    if ((IsA<SVGForeignObjectElement>(*element) ||
         IsA<SVGTextElement>(*element)) &&
        style.IsDisplayInlineType())
      style.SetDisplay(EDisplay::kBlock);

    // Columns don't apply to svg text elements.
    if (IsA<SVGTextElement>(*element))
      style.ClearMultiCol();
  } else if (element && element->IsMathMLElement()) {
    if (style.Display() == EDisplay::kContents) {
      // https://drafts.csswg.org/css-display/#unbox-mathml
      style.SetDisplay(EDisplay::kNone);
    }

    if (style.GetWritingMode() != WritingMode::kHorizontalTb) {
      // TODO(rbuis): this will not work with logical CSS properties.
      // Disable vertical writing-mode for now.
      style.SetWritingMode(WritingMode::kHorizontalTb);
      style.UpdateFontOrientation();
    }
  }

  // If this node is sticky it marks the creation of a sticky subtree, which we
  // must track to properly handle document lifecycle in some cases.
  //
  // It is possible that this node is already in a sticky subtree (i.e. we have
  // nested sticky nodes) - in that case the bit will already be set via
  // inheritance from the ancestor and there is no harm to setting it again.
  if (style.GetPosition() == EPosition::kSticky)
    style.SetSubtreeIsSticky(true);

  // If the inherited value of justify-items includes the 'legacy'
  // keyword (plus 'left', 'right' or 'center'), 'legacy' computes to
  // the the inherited value.  Otherwise, 'auto' computes to 'normal'.
  if (parent_style.JustifyItemsPositionType() == ItemPositionType::kLegacy &&
      style.JustifyItemsPosition() == ItemPosition::kLegacy) {
    style.SetJustifyItems(parent_style.JustifyItems());
  }

  AdjustEffectiveTouchAction(style, parent_style, element, is_svg_root);

  bool is_media_control =
      element && element->ShadowPseudoId().StartsWith("-webkit-media-controls");
  if (is_media_control && !style.HasEffectiveAppearance()) {
    // For compatibility reasons if the element is a media control and the
    // -webkit-appearance is none then we should clear the background image.
    style.MutableBackgroundInternal().ClearImage();
  }

  if (element && style.TextOverflow() == ETextOverflow::kEllipsis) {
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
      style.SetTextOverflow(text_control->ValueForTextOverflow());
    }
  }

  if (RuntimeEnabledFeatures::LayoutNGBlockFragmentationEnabled()) {
    // When establishing a block fragmentation context for LayoutNG, we require
    // that everything fragmentable inside can be laid out by NG natively, since
    // NG and legacy layout cannot cooperate within the same fragmentation
    // context. Set a flag, so that we can quickly determine whether we need to
    // check that an element is compatible with the NG block fragmentation
    // machinery.
    if (style.SpecifiesColumns() ||
        (element && element->GetDocument().Printing()))
      style.SetInsideNGFragmentationContext(true);
  }
}
}  // namespace blink
