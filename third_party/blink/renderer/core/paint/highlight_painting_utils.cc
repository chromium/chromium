// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/highlight_painting_utils.h"

#include "components/shared_highlighting/core/common/fragment_directives_constants.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_request.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

bool NodeIsReplaced(Node* node) {
  return node && node->GetLayoutObject() &&
         node->GetLayoutObject()->IsLayoutReplaced();
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
    case kPseudoIdHighlight:
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
    case kPseudoIdHighlight:
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
                                    Color previous_layer_color,
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
          false /* active match */, style.UsedColorScheme());
    case kPseudoIdSpellingError:
    case kPseudoIdGrammarError:
    case kPseudoIdHighlight:
      if (RuntimeEnabledFeatures::HighlightOverlayPaintingEnabled()) {
        return previous_layer_color;
      } else {
        // TODO(crbug.com/1295264): unstyled custom highlights should not change
        // the foreground color, but for now the best we can do is defaulting to
        // transparent (pre-HighlightOverlayPainting with double painting). The
        // correct behaviour is to use the ‘color’ of the next topmost active
        // highlight (equivalent to 'currentColor').
        return Color::kTransparent;
      }
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
      return Color(shared_highlighting::kFragmentTextBackgroundColorARGB);
    case kPseudoIdSpellingError:
    case kPseudoIdGrammarError:
    case kPseudoIdHighlight:
      return Color::kTransparent;
    default:
      NOTREACHED();
      return Color();
  }
}

// Returns highlight styles for the given node, inheriting from the originating
// element only, like most impls did before highlights were added to css-pseudo.
scoped_refptr<const ComputedStyle>
HighlightPseudoStyleWithOriginatingInheritance(
    Node* node,
    PseudoId pseudo,
    const AtomicString& pseudo_argument = g_null_atom) {
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

  if (!element || element->IsPseudoElement())
    return nullptr;

  if (pseudo == kPseudoIdSelection &&
      element->GetDocument().GetStyleEngine().UsesWindowInactiveSelector() &&
      !element->GetDocument().GetPage()->GetFocusController().IsActive()) {
    // ::selection and ::selection:window-inactive styles may be different. Only
    // cache the styles for ::selection if there are no :window-inactive
    // selector, or if the page is active.
    return element->UncachedStyleForPseudoElement(
        StyleRequest(pseudo, element->GetComputedStyle(), pseudo_argument));
  }

  return element->CachedStyleForPseudoElement(pseudo, pseudo_argument);
}

// Paired cascade: when we encounter any highlight colors, we make all other
// highlight color properties default to initial, rather than the UA default.
// https://drafts.csswg.org/css-pseudo-4/#highlight-cascade
bool UseUaHighlightColors(PseudoId pseudo, const ComputedStyle& pseudo_style) {
  return !pseudo_style.HasAuthorHighlightColors();
}

Color HighlightColor(const Document& document,
                     const ComputedStyle& style,
                     Node* node,
                     Color previous_layer_color,
                     PseudoId pseudo,
                     const CSSProperty& color_property,
                     PaintFlags paint_flags,
                     const AtomicString& pseudo_argument = g_null_atom) {
  if (pseudo == kPseudoIdSelection) {
    // If the element is unselectable, or we are only painting the selection,
    // don't override the foreground color with the selection foreground color.
    if ((node && !style.IsSelectable()) ||
        (paint_flags & PaintFlag::kSelectionDragImageOnly)) {
      return style.VisitedDependentColor(color_property);
    }
  }

  scoped_refptr<const ComputedStyle> pseudo_style =
      HighlightPaintingUtils::HighlightPseudoStyle(node, style, pseudo,
                                                   pseudo_argument);

  mojom::blink::ColorScheme color_scheme = style.UsedColorScheme();
  if (pseudo_style && (!UsesHighlightPseudoInheritance(pseudo) ||
                       !UseUaHighlightColors(pseudo, *pseudo_style))) {
    if (!document.InForcedColorsMode() ||
        pseudo_style->ForcedColorAdjust() != EForcedColorAdjust::kAuto) {
      if (pseudo_style->ColorIsCurrentColor()) {
        if (RuntimeEnabledFeatures::HighlightOverlayPaintingEnabled())
          return previous_layer_color;
        else
          return style.VisitedDependentColor(color_property);
      }
      return pseudo_style->VisitedDependentColor(color_property);
    }
    color_scheme = pseudo_style->UsedColorScheme();
  }

  if (document.InForcedColorsMode())
    return ForcedSystemForegroundColor(pseudo, color_scheme);
  return HighlightThemeForegroundColor(document, style, color_property,
                                       previous_layer_color, pseudo);
}

}  // anonymous namespace

// Returns highlight styles for the given node, inheriting through the “tree” of
// highlight pseudo styles mirroring the originating element tree. None of the
// returned styles are influenced by originating elements or pseudo-elements.
scoped_refptr<const ComputedStyle> HighlightPaintingUtils::HighlightPseudoStyle(
    Node* node,
    const ComputedStyle& style,
    PseudoId pseudo,
    const AtomicString& pseudo_argument) {
  if (!UsesHighlightPseudoInheritance(pseudo)) {
    return HighlightPseudoStyleWithOriginatingInheritance(node, pseudo,
                                                          pseudo_argument);
  }

  if (!style.HighlightData())
    return nullptr;

  switch (pseudo) {
    case kPseudoIdSelection:
      return style.HighlightData()->Selection();
    case kPseudoIdTargetText:
      return style.HighlightData()->TargetText();
    case kPseudoIdSpellingError:
      return style.HighlightData()->SpellingError();
    case kPseudoIdGrammarError:
      return style.HighlightData()->GrammarError();
    case kPseudoIdHighlight:
      return style.HighlightData()->CustomHighlight(pseudo_argument);
    default:
      NOTREACHED();
      return nullptr;
  }
}

Color HighlightPaintingUtils::HighlightBackgroundColor(
    const Document& document,
    const ComputedStyle& style,
    Node* node,
    absl::optional<Color> previous_layer_color,
    PseudoId pseudo,
    const AtomicString& pseudo_argument) {
  if (pseudo == kPseudoIdSelection) {
    if (node && !style.IsSelectable())
      return Color::kTransparent;
  }

  scoped_refptr<const ComputedStyle> pseudo_style =
      HighlightPseudoStyle(node, style, pseudo, pseudo_argument);

  mojom::blink::ColorScheme color_scheme = style.UsedColorScheme();
  if (pseudo_style && (!UsesHighlightPseudoInheritance(pseudo) ||
                       !UseUaHighlightColors(pseudo, *pseudo_style))) {
    if (!document.InForcedColorsMode() ||
        pseudo_style->ForcedColorAdjust() != EForcedColorAdjust::kAuto) {
      Color highlight_color =
          pseudo_style->VisitedDependentColor(GetCSSPropertyBackgroundColor());
      if (pseudo_style->IsBackgroundColorCurrentColor() &&
          pseudo_style->ColorIsCurrentColor()) {
        if (RuntimeEnabledFeatures::HighlightOverlayPaintingEnabled() &&
            previous_layer_color.has_value()) {
          highlight_color = previous_layer_color.value();
        } else {
          highlight_color = style.VisitedDependentColor(GetCSSPropertyColor());
        }
      }
      if (pseudo == kPseudoIdSelection && NodeIsReplaced(node)) {
        // Avoid that ::selection full obscures selected replaced elements like
        // images.
        return highlight_color.BlendWithWhite();
      }
      return highlight_color;
    }
    color_scheme = pseudo_style->UsedColorScheme();
  }

  if (document.InForcedColorsMode())
    return ForcedSystemBackgroundColor(pseudo, color_scheme);
  return HighlightThemeBackgroundColor(document, style, pseudo);
}

absl::optional<AppliedTextDecoration>
HighlightPaintingUtils::HighlightTextDecoration(
    const ComputedStyle& style,
    const ComputedStyle& pseudo_style) {
  const Vector<AppliedTextDecoration>& style_decorations =
      style.AppliedTextDecorations();
  const Vector<AppliedTextDecoration>& pseudo_style_decorations =
      pseudo_style.AppliedTextDecorations();

  if (style_decorations.IsEmpty())
    return absl::nullopt;

  absl::optional<AppliedTextDecoration> highlight_text_decoration =
      style_decorations.back();

  if (pseudo_style_decorations.size() &&
      style_decorations.back().Lines() ==
          pseudo_style_decorations.back().Lines()) {
    highlight_text_decoration = pseudo_style_decorations.back();
  }

  highlight_text_decoration.value().SetColor(
      pseudo_style.VisitedDependentColor(GetCSSPropertyTextDecorationColor()));

  return highlight_text_decoration;
}

Color HighlightPaintingUtils::HighlightForegroundColor(
    const Document& document,
    const ComputedStyle& style,
    Node* node,
    Color previous_layer_color,
    PseudoId pseudo,
    PaintFlags paint_flags,
    const AtomicString& pseudo_argument) {
  return HighlightColor(document, style, node, previous_layer_color, pseudo,
                        GetCSSPropertyWebkitTextFillColor(), paint_flags,
                        pseudo_argument);
}

Color HighlightPaintingUtils::HighlightEmphasisMarkColor(
    const Document& document,
    const ComputedStyle& style,
    Node* node,
    Color previous_layer_color,
    PseudoId pseudo,
    PaintFlags paint_flags,
    const AtomicString& pseudo_argument) {
  return HighlightColor(document, style, node, previous_layer_color, pseudo,
                        GetCSSPropertyTextEmphasisColor(), paint_flags,
                        pseudo_argument);
}

TextPaintStyle HighlightPaintingUtils::HighlightPaintingStyle(
    const Document& document,
    const ComputedStyle& style,
    Node* node,
    PseudoId pseudo,
    const TextPaintStyle& text_style,
    const PaintInfo& paint_info,
    const AtomicString& pseudo_argument) {
  TextPaintStyle highlight_style = text_style;
  bool uses_text_as_clip = paint_info.phase == PaintPhase::kTextClip;
  const PaintFlags paint_flags = paint_info.GetPaintFlags();

  // Each highlight overlay’s shadows are completely independent of any shadows
  // specified on the originating element (or the other highlight overlays).
  highlight_style.shadow = nullptr;

  if (!uses_text_as_clip) {
    highlight_style.fill_color =
        HighlightForegroundColor(document, style, node, text_style.fill_color,
                                 pseudo, paint_flags, pseudo_argument);
    highlight_style.emphasis_mark_color = HighlightEmphasisMarkColor(
        document, style, node, text_style.emphasis_mark_color, pseudo,
        paint_flags, pseudo_argument);
  }

  if (scoped_refptr<const ComputedStyle> pseudo_style =
          HighlightPseudoStyle(node, style, pseudo, pseudo_argument)) {
    highlight_style.stroke_color =
        uses_text_as_clip ? Color::kBlack
                          : pseudo_style->VisitedDependentColor(
                                GetCSSPropertyWebkitTextStrokeColor());
    highlight_style.stroke_width = pseudo_style->TextStrokeWidth();
    // TODO(crbug.com/1164461) For now, don't paint text shadows for ::highlight
    // because some details of how this will be standardized aren't yet
    // settled. Once the final standardization and implementation of highlight
    // text-shadow behavior is complete, remove the following check.
    if (pseudo != kPseudoIdHighlight) {
      highlight_style.shadow =
          uses_text_as_clip ? nullptr : pseudo_style->TextShadow();
    }
    highlight_style.selection_text_decoration =
        HighlightTextDecoration(style, *pseudo_style);
  }

  // Text shadows are disabled when printing. http://crbug.com/258321
  if (document.Printing())
    highlight_style.shadow = nullptr;

  return highlight_style;
}

absl::optional<Color> HighlightPaintingUtils::HighlightTextDecorationColor(
    const ComputedStyle& style,
    Node* node,
    PseudoId pseudo) {
  DCHECK(pseudo == kPseudoIdSpellingError || pseudo == kPseudoIdGrammarError);

  if (!RuntimeEnabledFeatures::CSSSpellingGrammarErrorsEnabled())
    return absl::nullopt;

  if (scoped_refptr<const ComputedStyle> pseudo_style =
          HighlightPseudoStyle(node, style, pseudo)) {
    return pseudo_style->VisitedDependentColor(
        GetCSSPropertyTextDecorationColor());
  }

  return absl::nullopt;
}

}  // namespace blink
