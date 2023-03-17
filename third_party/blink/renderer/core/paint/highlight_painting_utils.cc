// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/highlight_painting_utils.h"

#include "components/shared_highlighting/core/common/fragment_directives_constants.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
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

mojom::blink::ColorScheme UsedColorScheme(
    const ComputedStyle& originating_style,
    const ComputedStyle* pseudo_style) {
  return pseudo_style ? pseudo_style->UsedColorScheme()
                      : originating_style.UsedColorScheme();
}

Color PreviousLayerColor(const ComputedStyle& originating_style,
                         absl::optional<Color> previous_layer_color) {
  if (previous_layer_color &&
      RuntimeEnabledFeatures::HighlightOverlayPaintingEnabled()) {
    return *previous_layer_color;
  }
  return originating_style.VisitedDependentColor(GetCSSPropertyColor());
}

// Returns the forced foreground color for the given |pseudo|.
Color ForcedForegroundColor(PseudoId pseudo,
                            mojom::blink::ColorScheme color_scheme) {
  CSSValueID keyword = CSSValueID::kHighlighttext;
  switch (pseudo) {
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
    case kPseudoIdSpellingError:
    case kPseudoIdGrammarError:
      keyword = CSSValueID::kCanvastext;
      break;
    default:
      NOTREACHED();
      break;
  }
  return LayoutTheme::GetTheme().SystemColor(keyword, color_scheme);
}

// Returns the forced ‘background-color’ for the given |pseudo|.
Color ForcedBackgroundColor(PseudoId pseudo,
                            mojom::blink::ColorScheme color_scheme) {
  CSSValueID keyword = CSSValueID::kHighlight;
  switch (pseudo) {
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
    case kPseudoIdSpellingError:
    case kPseudoIdGrammarError:
      keyword = CSSValueID::kCanvas;
      break;
    default:
      NOTREACHED();
      break;
  }
  return LayoutTheme::GetTheme().SystemColor(keyword, color_scheme);
}

// Returns the forced background color if |property| is ‘background-color’,
// or the forced foreground color for all other properties (e.g. ‘color’,
// ‘text-decoration-color’, ‘-webkit-text-fill-color’).
Color ForcedColor(const ComputedStyle& originating_style,
                  const ComputedStyle* pseudo_style,
                  PseudoId pseudo,
                  const CSSProperty& property) {
  mojom::blink::ColorScheme color_scheme =
      UsedColorScheme(originating_style, pseudo_style);
  if (property.IDEquals(CSSPropertyID::kBackgroundColor))
    return ForcedBackgroundColor(pseudo, color_scheme);
  return ForcedForegroundColor(pseudo, color_scheme);
}

// Returns the UA default ‘color’ for the given |pseudo|.
absl::optional<Color> DefaultForegroundColor(
    const Document& document,
    PseudoId pseudo,
    mojom::blink::ColorScheme color_scheme) {
  // TODO(crbug.com/1295264): unstyled custom highlights should not change
  // the foreground color, but for now the best we can do is defaulting to
  // transparent (pre-HighlightOverlayPainting with double painting). The
  // correct behaviour is to use the ‘color’ of the next topmost active
  // highlight (equivalent to 'currentColor').
  absl::optional<Color> previous_layer_color =
      RuntimeEnabledFeatures::HighlightOverlayPaintingEnabled()
          ? absl::nullopt
          : absl::make_optional(Color::kTransparent);

  switch (pseudo) {
    case kPseudoIdSelection:
      if (!LayoutTheme::GetTheme().SupportsSelectionForegroundColors())
        return previous_layer_color;
      if (document.GetFrame()->Selection().FrameIsFocusedAndActive()) {
        return LayoutTheme::GetTheme().ActiveSelectionForegroundColor(
            color_scheme);
      }
      return LayoutTheme::GetTheme().InactiveSelectionForegroundColor(
          color_scheme);
    case kPseudoIdTargetText:
      return LayoutTheme::GetTheme().PlatformTextSearchColor(
          false /* active match */, color_scheme);
    case kPseudoIdSpellingError:
    case kPseudoIdGrammarError:
    case kPseudoIdHighlight:
      return previous_layer_color;
    default:
      NOTREACHED();
      return absl::nullopt;
  }
}

// Returns the UA default ‘background-color’ for the given |pseudo|.
Color DefaultBackgroundColor(const Document& document,
                             PseudoId pseudo,
                             mojom::blink::ColorScheme color_scheme) {
  switch (pseudo) {
    case kPseudoIdSelection:
      return document.GetFrame()->Selection().FrameIsFocusedAndActive()
                 ? LayoutTheme::GetTheme().ActiveSelectionBackgroundColor(
                       color_scheme)
                 : LayoutTheme::GetTheme().InactiveSelectionBackgroundColor(
                       color_scheme);
    case kPseudoIdTargetText:
      return Color::FromRGBA32(
          shared_highlighting::kFragmentTextBackgroundColorARGB);
    case kPseudoIdSpellingError:
    case kPseudoIdGrammarError:
    case kPseudoIdHighlight:
      return Color::kTransparent;
    default:
      NOTREACHED();
      return Color();
  }
}

// Returns the UA default highlight color for a paired cascade |property|,
// that is, ‘color’ or ‘background-color’. Paired cascade only applies to those
// properties, not ‘-webkit-text-fill-color’ or ‘-webkit-text-stroke-color’.
Color DefaultHighlightColor(const Document& document,
                            const ComputedStyle& originating_style,
                            const ComputedStyle* pseudo_style,
                            PseudoId pseudo,
                            const CSSProperty& property,
                            absl::optional<Color> previous_layer_color) {
  mojom::blink::ColorScheme color_scheme =
      UsedColorScheme(originating_style, pseudo_style);
  if (property.IDEquals(CSSPropertyID::kBackgroundColor))
    return DefaultBackgroundColor(document, pseudo, color_scheme);
  DCHECK(property.IDEquals(CSSPropertyID::kColor));
  if (absl::optional<Color> result =
          DefaultForegroundColor(document, pseudo, color_scheme)) {
    return *result;
  }
  return PreviousLayerColor(originating_style, previous_layer_color);
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

bool UseForcedColors(const Document& document,
                     const ComputedStyle& originating_style,
                     const ComputedStyle* pseudo_style) {
  if (!document.InForcedColorsMode())
    return false;
  // TODO(crbug.com/1309835) simplify when valid_for_highlight_legacy is removed
  if (pseudo_style)
    return pseudo_style->ForcedColorAdjust() == EForcedColorAdjust::kAuto;
  return originating_style.ForcedColorAdjust() == EForcedColorAdjust::kAuto;
}

// Paired cascade: when we encounter any highlight colors, we make all other
// highlight color properties default to initial, rather than the UA default.
// https://drafts.csswg.org/css-pseudo-4/#highlight-cascade
bool UseDefaultHighlightColors(const ComputedStyle* pseudo_style,
                               PseudoId pseudo,
                               const CSSProperty& property) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kColor:
    case CSSPropertyID::kBackgroundColor:
      return !pseudo_style || (UsesHighlightPseudoInheritance(pseudo) &&
                               !pseudo_style->HasAuthorHighlightColors());
    default:
      return false;
  }
}

}  // anonymous namespace

// Returns the used value of the given <color>-valued |property|, taking into
// account forced colors, default highlight colors, and ‘currentColor’ fallback.
Color HighlightPaintingUtils::ResolveColor(
    const Document& document,
    const ComputedStyle& originating_style,
    const ComputedStyle* pseudo_style,
    PseudoId pseudo,
    const CSSProperty& property,
    absl::optional<Color> previous_layer_color) {
  if (UseForcedColors(document, originating_style, pseudo_style))
    return ForcedColor(originating_style, pseudo_style, pseudo, property);
  if (UseDefaultHighlightColors(pseudo_style, pseudo, property)) {
    return DefaultHighlightColor(document, originating_style, pseudo_style,
                                 pseudo, property, previous_layer_color);
  }
  if (pseudo_style) {
    bool is_current_color;
    Color result = pseudo_style->VisitedDependentColor(To<Longhand>(property),
                                                       &is_current_color);
    if (!is_current_color)
      return result;
  }
  if (!property.IDEquals(CSSPropertyID::kColor)) {
    return ResolveColor(document, originating_style, pseudo_style, pseudo,
                        GetCSSPropertyColor(), previous_layer_color);
  }
  return PreviousLayerColor(originating_style, previous_layer_color);
}

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
  Color result =
      ResolveColor(document, style, pseudo_style.get(), pseudo,
                   GetCSSPropertyBackgroundColor(), previous_layer_color);
  if (pseudo == kPseudoIdSelection && NodeIsReplaced(node)) {
    // Avoid that ::selection full obscures selected replaced elements like
    // images.
    return result.BlendWithWhite();
  }
  return result;
}

absl::optional<AppliedTextDecoration>
HighlightPaintingUtils::SelectionTextDecoration(
    const Document& document,
    const ComputedStyle& style,
    const ComputedStyle& pseudo_style,
    absl::optional<Color> previous_layer_color) {
  absl::optional<AppliedTextDecoration> decoration =
      style.LastAppliedTextDecoration();
  if (!decoration) {
    return absl::nullopt;
  }

  absl::optional<AppliedTextDecoration> pseudo_decoration =
      pseudo_style.LastAppliedTextDecoration();
  if (pseudo_decoration && decoration->Lines() == pseudo_decoration->Lines()) {
    decoration = pseudo_decoration;
  }

  decoration->SetColor(
      ResolveColor(document, style, &pseudo_style, kPseudoIdSelection,
                   GetCSSPropertyTextDecorationColor(), previous_layer_color));
  return decoration;
}

TextPaintStyle HighlightPaintingUtils::HighlightPaintingStyle(
    const Document& document,
    const ComputedStyle& style,
    Node* node,
    PseudoId pseudo,
    const TextPaintStyle& previous_layer_text_style,
    const PaintInfo& paint_info,
    const AtomicString& pseudo_argument) {
  TextPaintStyle highlight_style = previous_layer_text_style;
  const PaintFlags paint_flags = paint_info.GetPaintFlags();
  bool uses_text_as_clip = paint_info.phase == PaintPhase::kTextClip;
  bool ignored_selection = pseudo == kPseudoIdSelection &&
                           ((node && !style.IsSelectable()) ||
                            (paint_flags & PaintFlag::kSelectionDragImageOnly));

  // Each highlight overlay’s shadows are completely independent of any shadows
  // specified on the originating element (or the other highlight overlays).
  highlight_style.shadow = nullptr;

  scoped_refptr<const ComputedStyle> pseudo_style =
      HighlightPseudoStyle(node, style, pseudo, pseudo_argument);
  Color previous_layer_current_color = previous_layer_text_style.current_color;

  if (!uses_text_as_clip && !ignored_selection) {
    highlight_style.current_color =
        ResolveColor(document, style, pseudo_style.get(), pseudo,
                     GetCSSPropertyColor(), previous_layer_current_color);
    highlight_style.fill_color = ResolveColor(
        document, style, pseudo_style.get(), pseudo,
        GetCSSPropertyWebkitTextFillColor(), previous_layer_current_color);
    // TODO(crbug.com/1147859) ignore highlight ‘text-emphasis-color’
    // https://github.com/w3c/csswg-drafts/issues/7101
    highlight_style.emphasis_mark_color = ResolveColor(
        document, style, pseudo_style.get(), pseudo,
        GetCSSPropertyTextEmphasisColor(), previous_layer_current_color);
    highlight_style.stroke_color = ResolveColor(
        document, style, pseudo_style.get(), pseudo,
        GetCSSPropertyWebkitTextStrokeColor(), previous_layer_current_color);
  }

  if (pseudo_style) {
    highlight_style.stroke_width = pseudo_style->TextStrokeWidth();
    // TODO(crbug.com/1164461) For now, don't paint text shadows for ::highlight
    // because some details of how this will be standardized aren't yet
    // settled. Once the final standardization and implementation of highlight
    // text-shadow behavior is complete, remove the following check.
    if (pseudo != kPseudoIdHighlight) {
      highlight_style.shadow =
          uses_text_as_clip ? nullptr : pseudo_style->TextShadow();
    }
    highlight_style.selection_text_decoration = SelectionTextDecoration(
        document, style, *pseudo_style, previous_layer_current_color);
  }

  // Text shadows are disabled when printing. http://crbug.com/258321
  if (document.Printing())
    highlight_style.shadow = nullptr;

  return highlight_style;
}

absl::optional<Color> HighlightPaintingUtils::HighlightTextDecorationColor(
    const Document& document,
    const ComputedStyle& style,
    Node* node,
    absl::optional<Color> previous_layer_color,
    PseudoId pseudo) {
  DCHECK(pseudo == kPseudoIdSpellingError || pseudo == kPseudoIdGrammarError);

  if (!RuntimeEnabledFeatures::CSSSpellingGrammarErrorsEnabled())
    return absl::nullopt;

  if (scoped_refptr<const ComputedStyle> pseudo_style =
          HighlightPseudoStyle(node, style, pseudo)) {
    return ResolveColor(document, style, pseudo_style.get(), pseudo,
                        GetCSSPropertyTextDecorationColor(),
                        previous_layer_color);
  }

  return absl::nullopt;
}

}  // namespace blink
