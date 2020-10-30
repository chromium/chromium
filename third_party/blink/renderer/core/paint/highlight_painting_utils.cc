// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/highlight_painting_utils.h"

#include "third_party/blink/renderer/core/css/pseudo_style_request.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/v0_insertion_point.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

namespace {

bool NodeIsSelectable(const ComputedStyle& style, Node* node) {
  return !node->IsInert() && !(style.UserSelect() == EUserSelect::kNone &&
                               style.UserModify() == EUserModify::kReadOnly);
}

Color ForcedSystemForegroundColor(PseudoId pseudo_id,
                                  mojom::blink::ColorScheme color_scheme) {
  CSSValueID keyword = CSSValueID::kHighlighttext;
  switch (pseudo_id) {
    case kPseudoIdTargetText:
      // TODO(futhark): According to the spec, the UA style should use Marktext.
      keyword = CSSValueID::kHighlighttext;
      break;
    case kPseudoIdSelection:
      keyword = CSSValueID::kHighlighttext;
      break;
    default:
      NOTREACHED();
      break;
  }
  return LayoutTheme::GetTheme().SystemColor(keyword, color_scheme);
}

Color ForcedSystemBackgroundColor(PseudoId pseudo_id,
                                  mojom::blink::ColorScheme color_scheme) {
  CSSValueID keyword = CSSValueID::kHighlight;
  switch (pseudo_id) {
    case kPseudoIdTargetText:
      // TODO(futhark): According to the spec, the UA style should use Mark.
      keyword = CSSValueID::kHighlight;
      break;
    case kPseudoIdSelection:
      keyword = CSSValueID::kHighlight;
      break;
    default:
      NOTREACHED();
      break;
  }
  return LayoutTheme::GetTheme().SystemColor(keyword, color_scheme);
}

Color HighlightThemeForegroundColor(const Document& document,
                                    const ComputedStyle& style,
                                    const CSSProperty& color_property,
                                    PseudoId pseudo_id) {
  switch (pseudo_id) {
    case kPseudoIdSelection:
      if (!LayoutTheme::GetTheme().SupportsSelectionForegroundColors())
        return style.VisitedDependentColor(color_property);
      if (document.GetFrame()->Selection().FrameIsFocusedAndActive()) {
        return LayoutTheme::GetTheme().ActiveSelectionForegroundColor(
            style.UsedColorScheme());
      }
      return LayoutTheme::GetTheme().InactiveSelectionForegroundColor(
          style.UsedColorScheme());
    case kPseudoIdTargetText:
      return LayoutTheme::GetTheme().PlatformTextSearchColor(
          false /* active match */, document.InForcedColorsMode(),
          style.UsedColorScheme());
    default:
      NOTREACHED();
      return Color();
  }
}

Color HighlightThemeBackgroundColor(const Document& document,
                                    const ComputedStyle& style,
                                    PseudoId pseudo_id) {
  switch (pseudo_id) {
    case kPseudoIdSelection:
      return document.GetFrame()->Selection().FrameIsFocusedAndActive()
                 ? LayoutTheme::GetTheme().ActiveSelectionBackgroundColor(
                       style.UsedColorScheme())
                 : LayoutTheme::GetTheme().InactiveSelectionBackgroundColor(
                       style.UsedColorScheme());
    case kPseudoIdTargetText:
      return LayoutTheme::GetTheme().PlatformTextSearchHighlightColor(
          false /* active match */, document.InForcedColorsMode(),
          style.UsedColorScheme());
    default:
      NOTREACHED();
      return Color();
  }
}

scoped_refptr<const ComputedStyle> HighlightPseudoStyle(Node* node,
                                                        PseudoId pseudo) {
  if (!node)
    return nullptr;

  Element* element = nullptr;

  // In Blink, highlight pseudo style only applies to direct children of the
  // element on which the highligh pseudo is matched. In order to be able to
  // style highlight inside elements implemented with a UA shadow tree, like
  // input::selection, we calculate highlight style on the shadow host for
  // elements inside the UA shadow.
  ShadowRoot* root = node->ContainingShadowRoot();
  if (root && root->IsUserAgent())
    element = node->OwnerShadowHost();

  // If we request highlight style for LayoutText, query highlight style on the
  // parent element instead, as that is the node for which the highligh pseudo
  // matches. This should most likely have used FlatTreeTraversal, but since we
  // don't implement inheritance of highlight styles, it would probably break
  // cases where you style a shadow host with a highlight pseudo and expect
  // light tree text children to be affected by that style.
  if (!element)
    element = Traversal<Element>::FirstAncestorOrSelf(*node);

  // <content> and <shadow> elements do not have ComputedStyle, hence they will
  // return null for StyleForPseudoElement(). Return early to avoid DCHECK
  // failure for GetComputedStyle() inside StyleForPseudoElement() below.
  if (!element || element->IsPseudoElement() ||
      IsActiveV0InsertionPoint(*element)) {
    return nullptr;
  }

  PseudoElementStyleRequest request(pseudo);

  if (pseudo == kPseudoIdSelection &&
      element->GetDocument().GetStyleEngine().UsesWindowInactiveSelector() &&
      !element->GetDocument().GetPage()->GetFocusController().IsActive()) {
    // ::selection and ::selection:window-inactive styles may be different. Only
    // cache the styles for ::selection if there are no :window-inactive
    // selector, or if the page is active.
    return element->StyleForPseudoElement(request, element->GetComputedStyle());
  }

  return element->CachedStyleForPseudoElement(request);
}

Color HighlightColor(const Document& document,
                     const ComputedStyle& style,
                     Node* node,
                     PseudoId pseudo,
                     const CSSProperty& color_property,
                     const GlobalPaintFlags global_paint_flags) {
  if (pseudo == kPseudoIdSelection) {
    // If the element is unselectable, or we are only painting the selection,
    // don't override the foreground color with the selection foreground color.
    if ((node && !NodeIsSelectable(style, node)) ||
        (global_paint_flags & kGlobalPaintSelectionDragImageOnly)) {
      return style.VisitedDependentColor(color_property);
    }
  }

  scoped_refptr<const ComputedStyle> pseudo_style =
      HighlightPseudoStyle(node, pseudo);

  mojom::blink::ColorScheme color_scheme = style.UsedColorScheme();
  if (pseudo_style) {
    if (!document.InForcedColorsMode() ||
        pseudo_style->ForcedColorAdjust() == EForcedColorAdjust::kNone) {
      return pseudo_style->VisitedDependentColor(color_property);
    }
    color_scheme = pseudo_style->UsedColorScheme();
  }

  if (document.InForcedColorsMode())
    return ForcedSystemForegroundColor(pseudo, color_scheme);
  return HighlightThemeForegroundColor(document, style, color_property, pseudo);
}

}  // anonymous namespace

Color HighlightPaintingUtils::HighlightBackgroundColor(
    const Document& document,
    const ComputedStyle& style,
    Node* node,
    PseudoId pseudo) {
  if (pseudo == kPseudoIdSelection) {
    if (node && !NodeIsSelectable(style, node))
      return Color::kTransparent;
  }

  mojom::blink::ColorScheme color_scheme = style.UsedColorScheme();
  if (scoped_refptr<const ComputedStyle> pseudo_style =
          HighlightPseudoStyle(node, pseudo)) {
    if (!document.InForcedColorsMode() ||
        pseudo_style->ForcedColorAdjust() == EForcedColorAdjust::kNone) {
      Color highlight_color =
          pseudo_style->VisitedDependentColor(GetCSSPropertyBackgroundColor());
      if (pseudo == kPseudoIdSelection)
        return highlight_color.BlendWithWhite();
      return highlight_color;
    }
    color_scheme = pseudo_style->UsedColorScheme();
  }

  if (document.InForcedColorsMode())
    return ForcedSystemBackgroundColor(pseudo, color_scheme);
  return HighlightThemeBackgroundColor(document, style, pseudo);
}

base::Optional<AppliedTextDecoration>
HighlightPaintingUtils::HighlightTextDecoration(
    const ComputedStyle& style,
    const ComputedStyle& pseudo_style) {
  const Vector<AppliedTextDecoration>& style_decorations =
      style.AppliedTextDecorations();
  const Vector<AppliedTextDecoration>& pseudo_style_decorations =
      pseudo_style.AppliedTextDecorations();

  if (style_decorations.IsEmpty())
    return base::nullopt;

  base::Optional<AppliedTextDecoration> highlight_text_decoration =
      base::nullopt;

  if (style_decorations.back().Lines() ==
      pseudo_style_decorations.back().Lines()) {
    highlight_text_decoration = pseudo_style_decorations.back();

    if (style_decorations.size() == pseudo_style_decorations.size()) {
      highlight_text_decoration.value().SetColor(
          pseudo_style.VisitedDependentColor(
              GetCSSPropertyTextDecorationColor()));
    }
  }

  return highlight_text_decoration;
}

Color HighlightPaintingUtils::HighlightForegroundColor(
    const Document& document,
    const ComputedStyle& style,
    Node* node,
    PseudoId pseudo,
    const GlobalPaintFlags global_paint_flags) {
  return HighlightColor(document, style, node, pseudo,
                        GetCSSPropertyWebkitTextFillColor(),
                        global_paint_flags);
}

Color HighlightPaintingUtils::HighlightEmphasisMarkColor(
    const Document& document,
    const ComputedStyle& style,
    Node* node,
    PseudoId pseudo,
    const GlobalPaintFlags global_paint_flags) {
  return HighlightColor(document, style, node, pseudo,
                        GetCSSPropertyWebkitTextEmphasisColor(),
                        global_paint_flags);
}

TextPaintStyle HighlightPaintingUtils::HighlightPaintingStyle(
    const Document& document,
    const ComputedStyle& style,
    Node* node,
    PseudoId pseudo,
    const TextPaintStyle& text_style,
    const PaintInfo& paint_info) {
  TextPaintStyle highlight_style = text_style;
  bool uses_text_as_clip = paint_info.phase == PaintPhase::kTextClip;
  const GlobalPaintFlags global_paint_flags = paint_info.GetGlobalPaintFlags();

  if (!uses_text_as_clip) {
    highlight_style.fill_color = HighlightForegroundColor(
        document, style, node, pseudo, global_paint_flags);
    highlight_style.emphasis_mark_color = HighlightEmphasisMarkColor(
        document, style, node, pseudo, global_paint_flags);
  }

  if (scoped_refptr<const ComputedStyle> pseudo_style =
          HighlightPseudoStyle(node, pseudo)) {
    highlight_style.stroke_color =
        uses_text_as_clip ? Color::kBlack
                          : pseudo_style->VisitedDependentColor(
                                GetCSSPropertyWebkitTextStrokeColor());
    highlight_style.stroke_width = pseudo_style->TextStrokeWidth();
    highlight_style.shadow =
        uses_text_as_clip ? nullptr : pseudo_style->TextShadow();
    highlight_style.selection_text_decoration =
        HighlightTextDecoration(style, *pseudo_style);
  }

  // Text shadows are disabled when printing. http://crbug.com/258321
  if (paint_info.IsPrinting())
    highlight_style.shadow = nullptr;

  return highlight_style;
}

}  // namespace blink
