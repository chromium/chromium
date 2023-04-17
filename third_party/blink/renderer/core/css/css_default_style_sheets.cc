/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"

#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/leak_annotations.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace {
String OverflowForReplacedElementRules() {
  return RuntimeEnabledFeatures::CSSOverflowForReplacedElementsEnabled()
             ? UncompressResourceAsASCIIString(
                   IDR_UASTYLE_OVERFLOW_REPLACED_CSS)
             : "";
}

String OverflowForSVGRules() {
  if (!RuntimeEnabledFeatures::CSSOverflowForReplacedElementsEnabled()) {
    return "";
  }

  // SVG uses an overflow value of 'hidden' for backwards compatibility with
  // flex layout. 'overflow-clip-margin' below still applies because the used
  // value of overflow at paint time is 'clip'.
  // See https://github.com/w3c/csswg-drafts/issues/7714 for context.
  return String(R"CSS(svg:not(:root) {
    overflow-clip-margin: content-box;
        })CSS");
}

}  // namespace

CSSDefaultStyleSheets& CSSDefaultStyleSheets::Instance() {
  DEFINE_STATIC_LOCAL(Persistent<CSSDefaultStyleSheets>,
                      css_default_style_sheets,
                      (MakeGarbageCollected<CSSDefaultStyleSheets>()));
  return *css_default_style_sheets;
}

static const MediaQueryEvaluator& PrintEval() {
  DEFINE_STATIC_LOCAL(const Persistent<MediaQueryEvaluator>, static_print_eval,
                      (MakeGarbageCollected<MediaQueryEvaluator>("print")));
  return *static_print_eval;
}

static const MediaQueryEvaluator& ForcedColorsEval() {
  // We use "ua-forced-colors" here instead of "forced-colors" to indicate that
  // this is a UA hack for the "forced-colors" media query.
  DEFINE_STATIC_LOCAL(
      Persistent<MediaQueryEvaluator>, forced_colors_eval,
      (MakeGarbageCollected<MediaQueryEvaluator>("ua-forced-colors")));
  return *forced_colors_eval;
}

// static
void CSSDefaultStyleSheets::Init() {
  Instance();
}

// static
StyleSheetContents* CSSDefaultStyleSheets::ParseUASheet(const String& str) {
  // UA stylesheets always parse in the insecure context mode.
  auto* sheet = MakeGarbageCollected<StyleSheetContents>(
      MakeGarbageCollected<CSSParserContext>(
          kUASheetMode, SecureContextMode::kInsecureContext));
  sheet->ParseString(str);
  // User Agent stylesheets are parsed once for the lifetime of the renderer
  // process and are intentionally leaked.
  LEAK_SANITIZER_IGNORE_OBJECT(sheet);
  return sheet;
}

// static
const MediaQueryEvaluator& CSSDefaultStyleSheets::ScreenEval() {
  DEFINE_STATIC_LOCAL(const Persistent<MediaQueryEvaluator>, static_screen_eval,
                      (MakeGarbageCollected<MediaQueryEvaluator>("screen")));
  return *static_screen_eval;
}

CSSDefaultStyleSheets::CSSDefaultStyleSheets()
    : media_controls_style_sheet_loader_(nullptr) {
  // Strict-mode rules.
  String default_rules = UncompressResourceAsASCIIString(IDR_UASTYLE_HTML_CSS) +
                         OverflowForReplacedElementRules() +
                         LayoutTheme::GetTheme().ExtraDefaultStyleSheet();

  default_style_sheet_ = ParseUASheet(default_rules);

  // Quirks-mode rules.
  String quirks_rules = UncompressResourceAsASCIIString(IDR_UASTYLE_QUIRKS_CSS);
  quirks_style_sheet_ = ParseUASheet(quirks_rules);

  InitializeDefaultStyles();

#if EXPENSIVE_DCHECKS_ARE_ON()
  default_html_style_->CompactRulesIfNeeded();
  default_mathml_style_->CompactRulesIfNeeded();
  default_svg_style_->CompactRulesIfNeeded();
  default_html_quirks_style_->CompactRulesIfNeeded();
  default_print_style_->CompactRulesIfNeeded();
  DCHECK(default_html_style_->UniversalRules().empty());
  DCHECK(default_mathml_style_->UniversalRules().empty());
  DCHECK(default_svg_style_->UniversalRules().empty());
  DCHECK(default_html_quirks_style_->UniversalRules().empty());
  DCHECK(default_print_style_->UniversalRules().empty());
#endif
}

void CSSDefaultStyleSheets::PrepareForLeakDetection() {
  // Clear the optional style sheets.
  svg_style_sheet_.Clear();
  mathml_style_sheet_.Clear();
  media_controls_style_sheet_.Clear();
  text_track_style_sheet_.Clear();
  forced_colors_style_sheet_.Clear();
  fullscreen_style_sheet_.Clear();
  popover_style_sheet_.Clear();
  selectmenu_style_sheet_.Clear();
  webxr_overlay_style_sheet_.Clear();
  marker_style_sheet_.Clear();
  form_controls_not_vertical_style_sheet_.Clear();
  // Recreate the default style sheet to clean up possible SVG resources.
  String default_rules = UncompressResourceAsASCIIString(IDR_UASTYLE_HTML_CSS) +
                         OverflowForReplacedElementRules() +
                         LayoutTheme::GetTheme().ExtraDefaultStyleSheet();
  default_style_sheet_ = ParseUASheet(default_rules);

  // Initialize the styles that have the lazily loaded style sheets.
  InitializeDefaultStyles();
  default_view_source_style_.Clear();
}

void CSSDefaultStyleSheets::InitializeDefaultStyles() {
  // This must be called only from constructor / PrepareForLeakDetection.
  default_html_style_ = MakeGarbageCollected<RuleSet>();
  default_mathml_style_ = MakeGarbageCollected<RuleSet>();
  default_svg_style_ = MakeGarbageCollected<RuleSet>();
  default_html_quirks_style_ = MakeGarbageCollected<RuleSet>();
  default_print_style_ = MakeGarbageCollected<RuleSet>();
  default_media_controls_style_ = MakeGarbageCollected<RuleSet>();
  default_forced_color_style_.Clear();
  default_pseudo_element_style_.Clear();

  default_html_style_->AddRulesFromSheet(DefaultStyleSheet(), ScreenEval());
  default_html_quirks_style_->AddRulesFromSheet(QuirksStyleSheet(),
                                                ScreenEval());
  default_print_style_->AddRulesFromSheet(DefaultStyleSheet(), PrintEval());
}

RuleSet* CSSDefaultStyleSheets::DefaultViewSourceStyle() {
  if (!default_view_source_style_) {
    default_view_source_style_ = MakeGarbageCollected<RuleSet>();
    // Loaded stylesheet is leaked on purpose.
    StyleSheetContents* stylesheet = ParseUASheet(
        UncompressResourceAsASCIIString(IDR_UASTYLE_VIEW_SOURCE_CSS));
    default_view_source_style_->AddRulesFromSheet(stylesheet, ScreenEval());
  }
  return default_view_source_style_;
}

static void AddTextTrackCSSProperties(StringBuilder* builder,
                                      CSSPropertyID propertyId,
                                      String value) {
  builder->Append(CSSProperty::Get(propertyId).GetPropertyNameString());
  builder->Append(": ");
  builder->Append(value);
  builder->Append("; ");
}

void CSSDefaultStyleSheets::AddRulesToDefaultStyleSheets(
    StyleSheetContents* rules,
    NamespaceType type) {
  switch (type) {
    case NamespaceType::kHTML:
      default_html_style_->AddRulesFromSheet(rules, ScreenEval());
      default_html_quirks_style_->AddRulesFromSheet(rules, ScreenEval());
      break;
    case NamespaceType::kSVG:
      default_svg_style_->AddRulesFromSheet(rules, ScreenEval());
      break;
    case NamespaceType::kMathML:
      default_mathml_style_->AddRulesFromSheet(rules, ScreenEval());
      break;
    case NamespaceType::kMediaControls:
      default_media_controls_style_->AddRulesFromSheet(rules, ScreenEval());
      break;
  }
  // Add to print and forced color for all namespaces.
  default_print_style_->AddRulesFromSheet(rules, PrintEval());
  if (default_forced_color_style_) {
    default_forced_color_style_->AddRulesFromSheet(rules, ForcedColorsEval());
  }
}

bool CSSDefaultStyleSheets::EnsureDefaultStyleSheetsForElement(
    const Element& element) {
  bool changed_default_style = false;
  // FIXME: We should assert that the sheet only styles SVG elements.
  if (element.IsSVGElement() && !svg_style_sheet_) {
    svg_style_sheet_ =
        ParseUASheet(UncompressResourceAsASCIIString(IDR_UASTYLE_SVG_CSS) +
                     OverflowForSVGRules());
    AddRulesToDefaultStyleSheets(svg_style_sheet_, NamespaceType::kSVG);
    changed_default_style = true;
  }

  // FIXME: We should assert that the sheet only styles MathML elements.
  if (element.namespaceURI() == mathml_names::kNamespaceURI &&
      !mathml_style_sheet_) {
    mathml_style_sheet_ = ParseUASheet(
        RuntimeEnabledFeatures::MathMLCoreEnabled()
            ? UncompressResourceAsASCIIString(IDR_UASTYLE_MATHML_CSS)
            : UncompressResourceAsASCIIString(IDR_UASTYLE_MATHML_FALLBACK_CSS));
    AddRulesToDefaultStyleSheets(mathml_style_sheet_, NamespaceType::kMathML);
    changed_default_style = true;
  }

  if (!media_controls_style_sheet_ && HasMediaControlsStyleSheetLoader() &&
      (IsA<HTMLVideoElement>(element) || IsA<HTMLAudioElement>(element))) {
    // FIXME: We should assert that this sheet only contains rules for <video>
    // and <audio>.
    media_controls_style_sheet_ =
        ParseUASheet(media_controls_style_sheet_loader_->GetUAStyleSheet());
    AddRulesToDefaultStyleSheets(media_controls_style_sheet_,
                                 NamespaceType::kMediaControls);
    changed_default_style = true;
  }

  if (!text_track_style_sheet_ && IsA<HTMLVideoElement>(element)) {
    Settings* settings = element.GetDocument().GetSettings();
    if (settings) {
      StringBuilder builder;
      builder.Append("video::-webkit-media-text-track-display { ");
      AddTextTrackCSSProperties(&builder, CSSPropertyID::kBackgroundColor,
                                settings->GetTextTrackWindowColor());
      AddTextTrackCSSProperties(&builder, CSSPropertyID::kBorderRadius,
                                settings->GetTextTrackWindowRadius());
      builder.Append(" } video::cue { ");
      AddTextTrackCSSProperties(&builder, CSSPropertyID::kBackgroundColor,
                                settings->GetTextTrackBackgroundColor());
      AddTextTrackCSSProperties(&builder, CSSPropertyID::kFontFamily,
                                settings->GetTextTrackFontFamily());
      AddTextTrackCSSProperties(&builder, CSSPropertyID::kFontStyle,
                                settings->GetTextTrackFontStyle());
      AddTextTrackCSSProperties(&builder, CSSPropertyID::kFontVariant,
                                settings->GetTextTrackFontVariant());
      AddTextTrackCSSProperties(&builder, CSSPropertyID::kColor,
                                settings->GetTextTrackTextColor());
      AddTextTrackCSSProperties(&builder, CSSPropertyID::kTextShadow,
                                settings->GetTextTrackTextShadow());
      AddTextTrackCSSProperties(&builder, CSSPropertyID::kFontSize,
                                settings->GetTextTrackTextSize());
      builder.Append(" } ");
      text_track_style_sheet_ = ParseUASheet(builder.ReleaseString());
      AddRulesToDefaultStyleSheets(text_track_style_sheet_,
                                   NamespaceType::kMediaControls);
      changed_default_style = true;
    }
  }

  if (!popover_style_sheet_ && IsA<HTMLElement>(element) &&
      To<HTMLElement>(element).HasPopoverAttribute()) {
    // TODO: We should assert that this sheet only contains rules for popovers.
    DCHECK(RuntimeEnabledFeatures::HTMLPopoverAttributeEnabled(
        element.GetDocument().GetExecutionContext()));
    popover_style_sheet_ =
        ParseUASheet(UncompressResourceAsASCIIString(IDR_UASTYLE_POPOVER_CSS));
    AddRulesToDefaultStyleSheets(popover_style_sheet_, NamespaceType::kHTML);
    changed_default_style = true;
  }

  if (!selectmenu_style_sheet_ && IsA<HTMLSelectMenuElement>(element)) {
    // TODO: We should assert that this sheet only contains rules for
    // <selectmenu>.
    DCHECK(RuntimeEnabledFeatures::HTMLSelectMenuElementEnabled());
    selectmenu_style_sheet_ = ParseUASheet(
        UncompressResourceAsASCIIString(IDR_UASTYLE_SELECTMENU_CSS));
    AddRulesToDefaultStyleSheets(selectmenu_style_sheet_, NamespaceType::kHTML);
    changed_default_style = true;
  }

  // TODO(crbug.com/681917): The FormControlsVerticalWritingModeSupport
  // flag enables vertical writing mode to be used on form controls. When
  // it is *disabled*, we need to force horizontal writing mode.
  if (!RuntimeEnabledFeatures::
          FormControlsVerticalWritingModeSupportEnabled() &&
      !form_controls_not_vertical_style_sheet_ &&
      (IsA<HTMLProgressElement>(element) || IsA<HTMLMeterElement>(element) ||
       IsA<HTMLInputElement>(element))) {
    form_controls_not_vertical_style_sheet_ =
        ParseUASheet(UncompressResourceAsASCIIString(
            IDR_UASTYLE_FORM_CONTROLS_NOT_VERTICAL_CSS));
    AddRulesToDefaultStyleSheets(form_controls_not_vertical_style_sheet_,
                                 NamespaceType::kHTML);
    changed_default_style = true;
  }

  DCHECK(!default_html_style_->Features().HasIdsInSelectors());
  return changed_default_style;
}

bool CSSDefaultStyleSheets::EnsureDefaultStyleSheetsForPseudoElement(
    PseudoId pseudo_id) {
  switch (pseudo_id) {
    case kPseudoIdMarker: {
      if (marker_style_sheet_) {
        return false;
      }
      marker_style_sheet_ =
          ParseUASheet(UncompressResourceAsASCIIString(IDR_UASTYLE_MARKER_CSS));
      if (!default_pseudo_element_style_) {
        default_pseudo_element_style_ = MakeGarbageCollected<RuleSet>();
      }
      default_pseudo_element_style_->AddRulesFromSheet(MarkerStyleSheet(),
                                                       ScreenEval());
      return true;
    }
    default:
      return false;
  }
}

void CSSDefaultStyleSheets::SetMediaControlsStyleSheetLoader(
    std::unique_ptr<UAStyleSheetLoader> loader) {
  media_controls_style_sheet_loader_.swap(loader);
}

bool CSSDefaultStyleSheets::EnsureDefaultStyleSheetForXrOverlay() {
  if (webxr_overlay_style_sheet_) {
    return false;
  }

  webxr_overlay_style_sheet_ = ParseUASheet(
      UncompressResourceAsASCIIString(IDR_UASTYLE_WEBXR_OVERLAY_CSS));
  AddRulesToDefaultStyleSheets(webxr_overlay_style_sheet_,
                               NamespaceType::kHTML);
  return true;
}

void CSSDefaultStyleSheets::EnsureDefaultStyleSheetForFullscreen() {
  if (fullscreen_style_sheet_) {
    return;
  }

  String fullscreen_rules =
      UncompressResourceAsASCIIString(IDR_UASTYLE_FULLSCREEN_CSS) +
      LayoutTheme::GetTheme().ExtraFullscreenStyleSheet();
  fullscreen_style_sheet_ = ParseUASheet(fullscreen_rules);
  AddRulesToDefaultStyleSheets(fullscreen_style_sheet_, NamespaceType::kHTML);
}

bool CSSDefaultStyleSheets::EnsureDefaultStyleSheetForForcedColors() {
  if (forced_colors_style_sheet_) {
    return false;
  }

  String forced_colors_rules =
      RuntimeEnabledFeatures::ForcedColorsEnabled()
          ? UncompressResourceAsASCIIString(IDR_UASTYLE_THEME_FORCED_COLORS_CSS)
          : String();
  forced_colors_style_sheet_ = ParseUASheet(forced_colors_rules);

  if (!default_forced_color_style_) {
    default_forced_color_style_ = MakeGarbageCollected<RuleSet>();
  }
  default_forced_color_style_->AddRulesFromSheet(DefaultStyleSheet(),
                                                 ForcedColorsEval());
  default_forced_color_style_->AddRulesFromSheet(ForcedColorsStyleSheet(),
                                                 ForcedColorsEval());
  if (svg_style_sheet_) {
    default_forced_color_style_->AddRulesFromSheet(SvgStyleSheet(),
                                                   ForcedColorsEval());
  }
  if (media_controls_style_sheet_) {
    default_forced_color_style_->AddRulesFromSheet(MediaControlsStyleSheet(),
                                                   ForcedColorsEval());
  }
  if (webxr_overlay_style_sheet_) {
    default_forced_color_style_->AddRulesFromSheet(webxr_overlay_style_sheet_,
                                                   ForcedColorsEval());
  }

  return true;
}

void CSSDefaultStyleSheets::CollectFeaturesTo(const Document& document,
                                              RuleFeatureSet& features) {
  if (DefaultHtmlStyle()) {
    features.Merge(DefaultHtmlStyle()->Features());
  }
  if (DefaultMediaControlsStyle()) {
    features.Merge(DefaultMediaControlsStyle()->Features());
  }
  if (DefaultMathMLStyle()) {
    features.Merge(DefaultMathMLStyle()->Features());
  }
  if (document.IsViewSource() && DefaultViewSourceStyle()) {
    features.Merge(DefaultViewSourceStyle()->Features());
  }
}

void CSSDefaultStyleSheets::Trace(Visitor* visitor) const {
  visitor->Trace(default_html_style_);
  visitor->Trace(default_mathml_style_);
  visitor->Trace(default_svg_style_);
  visitor->Trace(default_html_quirks_style_);
  visitor->Trace(default_print_style_);
  visitor->Trace(default_view_source_style_);
  visitor->Trace(default_forced_color_style_);
  visitor->Trace(default_media_controls_style_);
  visitor->Trace(default_style_sheet_);
  visitor->Trace(default_pseudo_element_style_);
  visitor->Trace(quirks_style_sheet_);
  visitor->Trace(svg_style_sheet_);
  visitor->Trace(mathml_style_sheet_);
  visitor->Trace(media_controls_style_sheet_);
  visitor->Trace(text_track_style_sheet_);
  visitor->Trace(forced_colors_style_sheet_);
  visitor->Trace(fullscreen_style_sheet_);
  visitor->Trace(popover_style_sheet_);
  visitor->Trace(selectmenu_style_sheet_);
  visitor->Trace(webxr_overlay_style_sheet_);
  visitor->Trace(marker_style_sheet_);
  visitor->Trace(form_controls_not_vertical_style_sheet_);
}

}  // namespace blink
