// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/highlight/highlight_style_utils.h"

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

// Returns the forced foreground color for the given |pseudo|.
Color ForcedForegroundColor(PseudoId pseudo,
                            mojom::blink::ColorScheme color_scheme,
                            const ui::ColorProvider* color_provider,
                            bool is_in_web_app_scope) {
  CSSValueID keyword = CSSValueID::kHighlighttext;
  switch (pseudo) {
    case kPseudoIdSearchText:
      keyword = CSSValueID::kInternalSearchTextColor;
      break;
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
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return LayoutTheme::GetTheme().SystemColor(
      keyword, color_scheme, color_provider, is_in_web_app_scope);
}

// Returns the forced ‘background-color’ for the given |pseudo|.
Color ForcedBackgroundColor(PseudoId pseudo,
                            mojom::blink::ColorScheme color_scheme,
                            const ui::ColorProvider* color_provider,
                            bool is_in_web_app_scope) {
  CSSValueID keyword = CSSValueID::kHighlight;
  switch (pseudo) {
    case kPseudoIdSearchText:
      keyword = CSSValueID::kInternalSearchColor;
      break;
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
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return LayoutTheme::GetTheme().SystemColor(
      keyword, color_scheme, color_provider, is_in_web_app_scope);
}

// Returns the forced background color if |property| is ‘background-color’,
// or the forced foreground color for all other properties (e.g. ‘color’,
// ‘text-decoration-color’, ‘-webkit-text-fill-color’).
Color ForcedColor(const ComputedStyle& originating_style,
                  const ComputedStyle* pseudo_style,
                  PseudoId pseudo,
                  const CSSProperty& property,
                  const ui::ColorProvider* color_provider,
                  bool is_in_web_app_scope) {
  mojom::blink::ColorScheme color_scheme =
      UsedColorScheme(originating_style, pseudo_style);
  if (property.IDEquals(CSSPropertyID::kBackgroundColor)) {
    return ForcedBackgroundColor(pseudo, color_scheme, color_provider,
                                 is_in_web_app_scope);
  }
  return ForcedForegroundColor(pseudo, color_scheme, color_provider,
                               is_in_web_app_scope);
}

// Returns the UA default ‘color’ for the given |pseudo|.
std::optional<Color> DefaultForegroundColor(
    const Document& document,
    PseudoId pseudo,
    mojom::blink::ColorScheme color_scheme,
    SearchTextIsCurrent search_text_is_current) {
  switch (pseudo) {
    case kPseudoIdSelection:
      if (!LayoutTheme::GetTheme().SupportsSelectionForegroundColors()) {
        return std::nullopt;
      }
      if (document.GetFrame()->Selection().FrameIsFocusedAndActive()) {
        return LayoutTheme::GetTheme().ActiveSelectionForegroundColor(
            color_scheme);
      }
      return LayoutTheme::GetTheme().InactiveSelectionForegroundColor(
          color_scheme);
    case kPseudoIdSearchText:
      return LayoutTheme::GetTheme().PlatformTextSearchColor(
          search_text_is_current == SearchTextIsCurrent::kYes,
          document.InForcedColorsMode(), color_scheme,
          document.GetColorProviderForPainting(color_scheme),
          document.IsInWebAppScope());
    case kPseudoIdTargetText:
      return LayoutTheme::GetTheme().PlatformTextSearchColor(
          false /* active match */, document.InForcedColorsMode(), color_scheme,
          document.GetColorProviderForPainting(color_scheme),
          document.IsInWebAppScope());
    case kPseudoIdSpellingError:
    case kPseudoIdGrammarError:
    case kPseudoIdHighlight:
      return std::nullopt;
    default:
      NOTREACHED_IN_MIGRATION();
      return std::nullopt;
  }
}

// Returns the UA default ‘background-color’ for the given |pseudo|.
Color DefaultBackgroundColor(const Document& document,
                             PseudoId pseudo,
                             mojom::blink::ColorScheme color_scheme,
                             SearchTextIsCurrent search_text_is_current) {
  switch (pseudo) {
    case kPseudoIdSelection:
      return document.GetFrame()->Selection().FrameIsFocusedAndActive()
                 ? LayoutTheme::GetTheme().ActiveSelectionBackgroundColor(
                       color_scheme)
                 : LayoutTheme::GetTheme().InactiveSelectionBackgroundColor(
                       color_scheme);
    case kPseudoIdSearchText:
      return LayoutTheme::GetTheme().PlatformTextSearchHighlightColor(
          search_text_is_current == SearchTextIsCurrent::kYes,
          document.InForcedColorsMode(), color_scheme,
          document.GetColorProviderForPainting(color_scheme),
          document.IsInWebAppScope());
    case kPseudoIdTargetText:
      return Color::FromRGBA32(
          shared_highlighting::kFragmentTextBackgroundColorARGB);
    case kPseudoIdSpellingError:
    case kPseudoIdGrammarError:
    case kPseudoIdHighlight:
      return Color::kTransparent;
    default:
      NOTREACHED_IN_MIGRATION();
      return Color();
  }
}

// Returns the UA default highlight color for a paired cascade |property|,
// that is, ‘color’ or ‘background-color’. Paired cascade only applies to those
// properties, not ‘-webkit-text-fill-color’ or ‘-webkit-text-stroke-color’.
std::optional<Color> DefaultHighlightColor(
    const Document& document,
    const ComputedStyle& originating_style,
    const ComputedStyle* pseudo_style,
    PseudoId pseudo,
    const CSSProperty& property,
    SearchTextIsCurrent search_text_is_current) {
  mojom::blink::ColorScheme color_scheme =
      UsedColorScheme(originating_style, pseudo_style);
  if (property.IDEquals(CSSPropertyID::kBackgroundColor)) {
    return DefaultBackgroundColor(document, pseudo, color_scheme,
                                  search_text_is_current);
  }
  DCHECK(property.IDEquals(CSSPropertyID::kColor));
  return DefaultForegroundColor(document, pseudo, color_scheme,
                                search_text_is_current);
}

// Returns highlight styles for the given node, inheriting from the originating
// element only, like most impls did before highlights were added to css-pseudo.
const ComputedStyle* HighlightPseudoStyleWithOriginatingInheritance(
    Node* node,
    PseudoId pseudo,
    const AtomicString& pseudo_argument = g_null_atom) {
  if (!node) {
    return nullptr;
  }

  Element* element = nullptr;

  // In Blink, highlight pseudo style only applies to direct children of the
  // element on which the highlight pseudo is matched. In order to be able to
  // style highlight inside elements implemented with a UA shadow tree, like
  // input::selection, we calculate highlight style on the shadow host for
  // elements inside the UA shadow.
  ShadowRoot* root = node->ContainingShadowRoot();
  if (root && root->IsUserAgent()) {
    element = node->OwnerShadowHost();
  }

  // If we request highlight style for LayoutText, query highlight style on the
  // parent element instead, as that is the node for which the highligh pseudo
  // matches. This should most likely have used FlatTreeTraversal, but since we
  // don't implement inheritance of highlight styles, it would probably break
  // cases where you style a shadow host with a highlight pseudo and expect
  // light tree text children to be affected by that style.
  if (!element) {
    element = Traversal<Element>::FirstAncestorOrSelf(*node);
  }

  if (!element || element->IsPseudoElement()) {
    return nullptr;
  }

  if (pseudo == kPseudoIdSelection &&
      element->GetDocument().GetStyleEngine().UsesWindowInactiveSelector() &&
      !element->GetDocument().GetPage()->GetFocusController().IsActive()) {
    // ::selection and ::selection:window-inactive styles may be different. Only
    // cache the styles for ::selection if there are no :window-inactive
    // selector, or if the page is active.
    // With Originating Inheritance the originating element is also the parent
    // element.
    return element->UncachedStyleForPseudoElement(
        StyleRequest(pseudo, element->GetComputedStyle(),
                     element->GetComputedStyle(), pseudo_argument));
  }

  return element->CachedStyleForPseudoElement(pseudo, pseudo_argument);
}

bool UseForcedColors(const Document& document,
                     const ComputedStyle& originating_style,
                     const ComputedStyle* pseudo_style) {
  if (!document.InForcedColorsMode()) {
    return false;
  }
  // TODO(crbug.com/1309835) simplify when valid_for_highlight_legacy is removed
  if (pseudo_style) {
    return pseudo_style->ForcedColorAdjust() == EForcedColorAdjust::kAuto;
  }
  return originating_style.ForcedColorAdjust() == EForcedColorAdjust::kAuto;
}

// Paired cascade: when we encounter any highlight colors, we make all other
// highlight color properties default to initial, rather than the UA default.
// https://drafts.csswg.org/css-pseudo-4/#paired-defaults
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

Color HighlightStyleUtils::ResolveColor(
    const Document& document,
    const ComputedStyle& originating_style,
    const ComputedStyle* pseudo_style,
    PseudoId pseudo,
    const CSSProperty& property,
    std::optional<Color> current_color,
    SearchTextIsCurrent search_text_is_current) {
  std::optional<Color> maybe_color =
      MaybeResolveColor(document, originating_style, pseudo_style, pseudo,
                        property, search_text_is_current);
  if (maybe_color) {
    return maybe_color.value();
  }
  if (!current_color) {
    return originating_style.VisitedDependentColor(GetCSSPropertyColor());
  }
  return current_color.value();
}

// Returns the used value of the given <color>-valued |property|, taking into
// account forced colors and default highlight colors. If the final result is
// ‘currentColor’, return nullopt so that the color may later be resolved
// against the previous layer.
std::optional<Color> HighlightStyleUtils::MaybeResolveColor(
    const Document& document,
    const ComputedStyle& originating_style,
    const ComputedStyle* pseudo_style,
    PseudoId pseudo,
    const CSSProperty& property,
    SearchTextIsCurrent search_text_is_current) {
  if (UseForcedColors(document, originating_style, pseudo_style)) {
    return ForcedColor(originating_style, pseudo_style, pseudo, property,
                       document.GetColorProviderForPainting(
                           UsedColorScheme(originating_style, pseudo_style)),
                       document.IsInWebAppScope());
  }
  if (UseDefaultHighlightColors(pseudo_style, pseudo, property)) {
    return DefaultHighlightColor(document, originating_style, pseudo_style,
                                 pseudo, property, search_text_is_current);
  }
  if (pseudo_style) {
    bool is_current_color;
    Color result = pseudo_style->VisitedDependentColor(To<Longhand>(property),
                                                       &is_current_color);
    if (!is_current_color) {
      return result;
    }
  }
  if (!property.IDEquals(CSSPropertyID::kColor)) {
    return MaybeResolveColor(document, originating_style, pseudo_style, pseudo,
                             GetCSSPropertyColor(), search_text_is_current);
  }
  return std::nullopt;
}

// Returns highlight styles for the given node, inheriting through the “tree” of
// highlight pseudo styles mirroring the originating element tree. None of the
// returned styles are influenced by originating elements or pseudo-elements.
const ComputedStyle* HighlightStyleUtils::HighlightPseudoStyle(
    Node* node,
    const ComputedStyle& style,
    PseudoId pseudo,
    const AtomicString& pseudo_argument) {
  if (!UsesHighlightPseudoInheritance(pseudo)) {
    return HighlightPseudoStyleWithOriginatingInheritance(node, pseudo,
                                                          pseudo_argument);
  }

  switch (pseudo) {
    case kPseudoIdSelection:
      return style.HighlightData().Selection();
    case kPseudoIdSearchText:
      // For ::search-text:current, call SearchTextCurrent() directly.
      return style.HighlightData().SearchTextNotCurrent();
    case kPseudoIdTargetText:
      return style.HighlightData().TargetText();
    case kPseudoIdSpellingError:
      return style.HighlightData().SpellingError();
    case kPseudoIdGrammarError:
      return style.HighlightData().GrammarError();
    case kPseudoIdHighlight:
      return style.HighlightData().CustomHighlight(pseudo_argument);
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

Color HighlightStyleUtils::HighlightBackgroundColor(
    const Document& document,
    const ComputedStyle& style,
    Node* node,
    std::optional<Color> current_layer_color,
    PseudoId pseudo,
    SearchTextIsCurrent search_text_is_current) {
  if (pseudo == kPseudoIdSelection) {
    if (node && !style.IsSelectable()) {
      return Color::kTransparent;
    }
  }

  const ComputedStyle* pseudo_style = HighlightPseudoStyle(node, style, pseudo);
  Color result = ResolveColor(document, style, pseudo_style, pseudo,
                              GetCSSPropertyBackgroundColor(),
                              current_layer_color, search_text_is_current);
  if (pseudo == kPseudoIdSelection) {
    if (NodeIsReplaced(node)) {
      // Avoid that ::selection full obscures selected replaced elements like
      // images.
      return result.BlendWithWhite();
    }
    if (result.IsFullyTransparent()) {
      return Color::kTransparent;
    }
    if (!RuntimeEnabledFeatures::SelectionRespectsColorsEnabled() ||
        (UseDefaultHighlightColors(pseudo_style, pseudo,
                                   GetCSSPropertyColor()) &&
         UseDefaultHighlightColors(pseudo_style, pseudo,
                                   GetCSSPropertyBackgroundColor()))) {
      // If the text color ends up being the same as the selection background
      // and we are using default colors, invert the background color. We do not
      // do this when the author has requested colors in a ::selection pseudo
      // (unless the flag is disabled).
      if (current_layer_color && *current_layer_color == result) {
        return Color(0xff - result.Red(), 0xff - result.Green(),
                     0xff - result.Blue());
      }
    }
  }
  return result;
}

std::optional<AppliedTextDecoration>
HighlightStyleUtils::SelectionTextDecoration(
    const Document& document,
    const ComputedStyle& style,
    const ComputedStyle& pseudo_style) {
  std::optional<AppliedTextDecoration> decoration =
      style.LastAppliedTextDecoration();
  if (!decoration) {
    return std::nullopt;
  }

  std::optional<AppliedTextDecoration> pseudo_decoration =
      pseudo_style.LastAppliedTextDecoration();
  if (pseudo_decoration && decoration->Lines() == pseudo_decoration->Lines()) {
    decoration = pseudo_decoration;
  }

  return decoration;
}

HighlightStyleUtils::HighlightTextPaintStyle
HighlightStyleUtils::HighlightPaintingStyle(
    const Document& document,
    const ComputedStyle& originating_style,
    const ComputedStyle* pseudo_style,
    Node* node,
    PseudoId pseudo,
    const TextPaintStyle& previous_layer_text_style,
    const PaintInfo& paint_info,
    SearchTextIsCurrent search_text_is_current) {
  TextPaintStyle highlight_style = previous_layer_text_style;
  HighlightColorPropertySet colors_from_previous_layer;
  const PaintFlags paint_flags = paint_info.GetPaintFlags();
  bool uses_text_as_clip = paint_info.phase == PaintPhase::kTextClip;
  bool ignored_selection = false;

  if (pseudo == kPseudoIdSelection) {
    if ((node && !originating_style.IsSelectable()) ||
        (paint_flags & PaintFlag::kSelectionDragImageOnly)) {
      ignored_selection = true;
    }
    highlight_style.selection_decoration_lines = TextDecorationLine::kNone;
    highlight_style.selection_decoration_color = Color::kBlack;
  }
  Color text_decoration_color = Color::kBlack;
  Color background_color = Color::kTransparent;

  // Each highlight overlay’s shadows are completely independent of any shadows
  // specified on the originating element (or the other highlight overlays).
  highlight_style.shadow = nullptr;

  if (!uses_text_as_clip && !ignored_selection) {
    std::optional<Color> maybe_color;

    maybe_color =
        MaybeResolveColor(document, originating_style, pseudo_style, pseudo,
                          GetCSSPropertyColor(), search_text_is_current);
    if (maybe_color) {
      highlight_style.current_color = maybe_color.value();
    } else {
      colors_from_previous_layer.Put(HighlightColorProperty::kCurrentColor);
    }

    maybe_color = MaybeResolveColor(document, originating_style, pseudo_style,
                                    pseudo, GetCSSPropertyWebkitTextFillColor(),
                                    search_text_is_current);
    if (maybe_color) {
      highlight_style.fill_color = maybe_color.value();
    } else {
      colors_from_previous_layer.Put(HighlightColorProperty::kFillColor);
    }

    // TODO(crbug.com/1147859) ignore highlight ‘text-emphasis-color’
    // https://github.com/w3c/csswg-drafts/issues/7101
    maybe_color = MaybeResolveColor(document, originating_style, pseudo_style,
                                    pseudo, GetCSSPropertyTextEmphasisColor(),
                                    search_text_is_current);
    if (maybe_color) {
      highlight_style.emphasis_mark_color = maybe_color.value();
    } else {
      colors_from_previous_layer.Put(HighlightColorProperty::kEmphasisColor);
    }

    maybe_color = MaybeResolveColor(
        document, originating_style, pseudo_style, pseudo,
        GetCSSPropertyWebkitTextStrokeColor(), search_text_is_current);
    if (maybe_color) {
      highlight_style.stroke_color = maybe_color.value();
    } else {
      colors_from_previous_layer.Put(HighlightColorProperty::kStrokeColor);
    }

    maybe_color = MaybeResolveColor(document, originating_style, pseudo_style,
                                    pseudo, GetCSSPropertyTextDecorationColor(),
                                    search_text_is_current);
    if (maybe_color) {
      text_decoration_color = maybe_color.value();
    } else {
      colors_from_previous_layer.Put(
          HighlightColorProperty::kTextDecorationColor);
    }

    maybe_color = MaybeResolveColor(document, originating_style, pseudo_style,
                                    pseudo, GetCSSPropertyBackgroundColor(),
                                    search_text_is_current);
    if (maybe_color) {
      background_color = maybe_color.value();
    } else {
      colors_from_previous_layer.Put(HighlightColorProperty::kBackgroundColor);
    }
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
    std::optional<AppliedTextDecoration> selection_decoration =
        SelectionTextDecoration(document, originating_style, *pseudo_style);
    if (selection_decoration) {
      highlight_style.selection_decoration_lines =
          selection_decoration->Lines();
      std::optional<Color> selection_decoration_color = MaybeResolveColor(
          document, originating_style, pseudo_style, kPseudoIdSelection,
          GetCSSPropertyTextDecorationColor(), search_text_is_current);
      if (selection_decoration_color) {
        highlight_style.selection_decoration_color =
            selection_decoration_color.value();
      } else {
        // Some code paths that do not use the highlight overlay painting system
        // may not resolve the color, so set it now.
        highlight_style.selection_decoration_color =
            previous_layer_text_style.current_color;
        colors_from_previous_layer.Put(
            HighlightColorProperty::kSelectionDecorationColor);
      }
    }
  }

  // Text shadows are disabled when printing. http://crbug.com/258321
  if (document.Printing()) {
    highlight_style.shadow = nullptr;
  }

  return {highlight_style, text_decoration_color, background_color,
          colors_from_previous_layer};
}

void HighlightStyleUtils::ResolveColorsFromPreviousLayer(
    HighlightTextPaintStyle& text_style,
    const HighlightTextPaintStyle& previous_layer_style) {
  if (text_style.properties_using_current_color.empty()) {
    return;
  }

  if (text_style.properties_using_current_color.Has(
          HighlightColorProperty::kCurrentColor)) {
    text_style.style.current_color = previous_layer_style.style.current_color;
  }
  if (text_style.properties_using_current_color.Has(
          HighlightColorProperty::kFillColor)) {
    text_style.style.fill_color = previous_layer_style.style.current_color;
  }
  if (text_style.properties_using_current_color.Has(
          HighlightColorProperty::kStrokeColor)) {
    text_style.style.stroke_color = previous_layer_style.style.current_color;
  }
  if (text_style.properties_using_current_color.Has(
          HighlightColorProperty::kEmphasisColor)) {
    text_style.style.emphasis_mark_color =
        previous_layer_style.style.current_color;
  }
  if (text_style.properties_using_current_color.Has(
          HighlightColorProperty::kSelectionDecorationColor)) {
    text_style.style.selection_decoration_color =
        previous_layer_style.style.current_color;
  }
  if (text_style.properties_using_current_color.Has(
          HighlightColorProperty::kTextDecorationColor)) {
    text_style.text_decoration_color = previous_layer_style.style.current_color;
  }
  if (text_style.properties_using_current_color.Has(
          HighlightColorProperty::kBackgroundColor)) {
    text_style.background_color = previous_layer_style.style.current_color;
  }
}

bool HighlightStyleUtils::ShouldInvalidateVisualOverflow(
    const Node& node,
    DocumentMarker::MarkerType type) {
  // Custom highlights and selection are handled separately. Here we just need
  // to handle spelling, grammar and target-text. Note that we assume
  // RuntimeEnabledFeatures::HighlightInheritanceEnabled() is true to avoid
  // needing a non-const node.
  if (type == DocumentMarker::kSpelling || type == DocumentMarker::kGrammar) {
    return true;
  }

  if (type != DocumentMarker::kTextFragment) {
    return false;
  }
  const ComputedStyle* style = node.GetComputedStyle();
  if (!style) {
    return false;
  }
  const ComputedStyle* pseudo_style = style->HighlightData().TargetText();
  if (!pseudo_style) {
    return false;
  }
  return (pseudo_style->HasAppliedTextDecorations() ||
          pseudo_style->HasVisualOverflowingEffect());
}

bool HighlightStyleUtils::CustomHighlightHasVisualOverflow(
    const Node& node,
    const AtomicString& pseudo_argument) {
  const ComputedStyle* style = node.GetComputedStyle();
  if (!style) {
    return false;
  }
  const ComputedStyle* pseudo_style =
      style->HighlightData().CustomHighlight(pseudo_argument);
  if (!pseudo_style) {
    return false;
  }
  return (pseudo_style->HasAppliedTextDecorations() ||
          pseudo_style->HasVisualOverflowingEffect());
}

}  // namespace blink
