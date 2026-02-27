// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_at_rule_id.h"

#include <optional>

#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

CSSAtRuleID CssAtRuleID(StringView name) {
  if (EqualIgnoringAsciiCase(name, "view-transition")) {
    return CSSAtRuleID::kCSSAtRuleViewTransition;
  }
  if (EqualIgnoringAsciiCase(name, "charset")) {
    return CSSAtRuleID::kCSSAtRuleCharset;
  }
  if (EqualIgnoringAsciiCase(name, "font-face")) {
    return CSSAtRuleID::kCSSAtRuleFontFace;
  }
  if (EqualIgnoringAsciiCase(name, "font-palette-values")) {
    return CSSAtRuleID::kCSSAtRuleFontPaletteValues;
  }
  if (EqualIgnoringAsciiCase(name, "font-feature-values")) {
    return CSSAtRuleID::kCSSAtRuleFontFeatureValues;
  }
  if (EqualIgnoringAsciiCase(name, "stylistic")) {
    return CSSAtRuleID::kCSSAtRuleStylistic;
  }
  if (EqualIgnoringAsciiCase(name, "styleset")) {
    return CSSAtRuleID::kCSSAtRuleStyleset;
  }
  if (EqualIgnoringAsciiCase(name, "character-variant")) {
    return CSSAtRuleID::kCSSAtRuleCharacterVariant;
  }
  if (EqualIgnoringAsciiCase(name, "swash")) {
    return CSSAtRuleID::kCSSAtRuleSwash;
  }
  if (EqualIgnoringAsciiCase(name, "ornaments")) {
    return CSSAtRuleID::kCSSAtRuleOrnaments;
  }
  if (EqualIgnoringAsciiCase(name, "annotation")) {
    return CSSAtRuleID::kCSSAtRuleAnnotation;
  }
  if (EqualIgnoringAsciiCase(name, "import")) {
    return CSSAtRuleID::kCSSAtRuleImport;
  }
  if (EqualIgnoringAsciiCase(name, "keyframes")) {
    return CSSAtRuleID::kCSSAtRuleKeyframes;
  }
  if (EqualIgnoringAsciiCase(name, "layer")) {
    return CSSAtRuleID::kCSSAtRuleLayer;
  }
  if (EqualIgnoringAsciiCase(name, "media")) {
    return CSSAtRuleID::kCSSAtRuleMedia;
  }
  if (EqualIgnoringAsciiCase(name, "namespace")) {
    return CSSAtRuleID::kCSSAtRuleNamespace;
  }
  if (EqualIgnoringAsciiCase(name, "page")) {
    return CSSAtRuleID::kCSSAtRulePage;
  }
  if (EqualIgnoringAsciiCase(name, "position-try")) {
    return CSSAtRuleID::kCSSAtRulePositionTry;
  }
  if (EqualIgnoringAsciiCase(name, "property")) {
    return CSSAtRuleID::kCSSAtRuleProperty;
  }
  if (RuntimeEnabledFeatures::RouteMatchingEnabled()) {
    if (EqualIgnoringAsciiCase(name, "route")) {
      return CSSAtRuleID::kCSSAtRuleRoute;
    }
    if (EqualIgnoringAsciiCase(name, "navigation")) {
      return CSSAtRuleID::kCSSAtRuleNavigation;
    }
  }
  if (EqualIgnoringAsciiCase(name, "container")) {
    return CSSAtRuleID::kCSSAtRuleContainer;
  }
  if (EqualIgnoringAsciiCase(name, "counter-style")) {
    return CSSAtRuleID::kCSSAtRuleCounterStyle;
  }
  if (EqualIgnoringAsciiCase(name, "scope")) {
    return CSSAtRuleID::kCSSAtRuleScope;
  }
  if (EqualIgnoringAsciiCase(name, "supports")) {
    return CSSAtRuleID::kCSSAtRuleSupports;
  }
  if (EqualIgnoringAsciiCase(name, "starting-style")) {
    return CSSAtRuleID::kCSSAtRuleStartingStyle;
  }
  if (EqualIgnoringAsciiCase(name, "-webkit-keyframes")) {
    return CSSAtRuleID::kCSSAtRuleWebkitKeyframes;
  }

  // https://www.w3.org/TR/css-page-3/#syntax-page-selector
  if (EqualIgnoringAsciiCase(name, "top-left-corner")) {
    return CSSAtRuleID::kCSSAtRuleTopLeftCorner;
  }
  if (EqualIgnoringAsciiCase(name, "top-left")) {
    return CSSAtRuleID::kCSSAtRuleTopLeft;
  }
  if (EqualIgnoringAsciiCase(name, "top-center")) {
    return CSSAtRuleID::kCSSAtRuleTopCenter;
  }
  if (EqualIgnoringAsciiCase(name, "top-right")) {
    return CSSAtRuleID::kCSSAtRuleTopRight;
  }
  if (EqualIgnoringAsciiCase(name, "top-right-corner")) {
    return CSSAtRuleID::kCSSAtRuleTopRightCorner;
  }
  if (EqualIgnoringAsciiCase(name, "bottom-left-corner")) {
    return CSSAtRuleID::kCSSAtRuleBottomLeftCorner;
  }
  if (EqualIgnoringAsciiCase(name, "bottom-left")) {
    return CSSAtRuleID::kCSSAtRuleBottomLeft;
  }
  if (EqualIgnoringAsciiCase(name, "bottom-center")) {
    return CSSAtRuleID::kCSSAtRuleBottomCenter;
  }
  if (EqualIgnoringAsciiCase(name, "bottom-right")) {
    return CSSAtRuleID::kCSSAtRuleBottomRight;
  }
  if (EqualIgnoringAsciiCase(name, "bottom-right-corner")) {
    return CSSAtRuleID::kCSSAtRuleBottomRightCorner;
  }
  if (EqualIgnoringAsciiCase(name, "left-top")) {
    return CSSAtRuleID::kCSSAtRuleLeftTop;
  }
  if (EqualIgnoringAsciiCase(name, "left-middle")) {
    return CSSAtRuleID::kCSSAtRuleLeftMiddle;
  }
  if (EqualIgnoringAsciiCase(name, "left-bottom")) {
    return CSSAtRuleID::kCSSAtRuleLeftBottom;
  }
  if (EqualIgnoringAsciiCase(name, "right-top")) {
    return CSSAtRuleID::kCSSAtRuleRightTop;
  }
  if (EqualIgnoringAsciiCase(name, "right-middle")) {
    return CSSAtRuleID::kCSSAtRuleRightMiddle;
  }
  if (EqualIgnoringAsciiCase(name, "right-bottom")) {
    return CSSAtRuleID::kCSSAtRuleRightBottom;
  }

  if (RuntimeEnabledFeatures::CSSFunctionsEnabled() &&
      EqualIgnoringAsciiCase(name, "function")) {
    return CSSAtRuleID::kCSSAtRuleFunction;
  }
  if (RuntimeEnabledFeatures::CSSMixinsEnabled()) {
    if (EqualIgnoringAsciiCase(name, "mixin")) {
      return CSSAtRuleID::kCSSAtRuleMixin;
    }
    if (EqualIgnoringAsciiCase(name, "apply")) {
      return CSSAtRuleID::kCSSAtRuleApplyMixin;
    }
    if (EqualIgnoringAsciiCase(name, "contents")) {
      return CSSAtRuleID::kCSSAtRuleContents;
    }
    if (EqualIgnoringAsciiCase(name, "result")) {
      return CSSAtRuleID::kCSSAtRuleResult;
    }
  }

  if (RuntimeEnabledFeatures::CSSCustomMediaEnabled()) {
    if (EqualIgnoringAsciiCase(name, "custom-media")) {
      return CSSAtRuleID::kCSSAtRuleCustomMedia;
    }
  }

  return CSSAtRuleID::kCSSAtRuleInvalid;
}

StringView CssAtRuleIDToString(CSSAtRuleID id) {
  switch (id) {
    case CSSAtRuleID::kCSSAtRuleViewTransition:
      return "@view-transition";
    case CSSAtRuleID::kCSSAtRuleCharset:
      return "@charset";
    case CSSAtRuleID::kCSSAtRuleFontFace:
      return "@font-face";
    case CSSAtRuleID::kCSSAtRuleFontPaletteValues:
      return "@font-palette-values";
    case CSSAtRuleID::kCSSAtRuleImport:
      return "@import";
    case CSSAtRuleID::kCSSAtRuleKeyframes:
      return "@keyframes";
    case CSSAtRuleID::kCSSAtRuleLayer:
      return "@layer";
    case CSSAtRuleID::kCSSAtRuleMedia:
      return "@media";
    case CSSAtRuleID::kCSSAtRuleNamespace:
      return "@namespace";
    case CSSAtRuleID::kCSSAtRulePage:
      return "@page";
    case CSSAtRuleID::kCSSAtRulePositionTry:
      return "@position-try";
    case CSSAtRuleID::kCSSAtRuleProperty:
      return "@property";
    case CSSAtRuleID::kCSSAtRuleRoute:
      return "@route";
    case CSSAtRuleID::kCSSAtRuleNavigation:
      return "@navigation";
    case CSSAtRuleID::kCSSAtRuleContainer:
      return "@container";
    case CSSAtRuleID::kCSSAtRuleCounterStyle:
      return "@counter-style";
    case CSSAtRuleID::kCSSAtRuleScope:
      return "@scope";
    case CSSAtRuleID::kCSSAtRuleStartingStyle:
      return "@starting-style";
    case CSSAtRuleID::kCSSAtRuleSupports:
      return "@supports";
    case CSSAtRuleID::kCSSAtRuleWebkitKeyframes:
      return "@-webkit-keyframes";
    case CSSAtRuleID::kCSSAtRuleAnnotation:
      return "@annotation";
    case CSSAtRuleID::kCSSAtRuleCharacterVariant:
      return "@character-variant";
    case CSSAtRuleID::kCSSAtRuleFontFeatureValues:
      return "@font-feature-values";
    case CSSAtRuleID::kCSSAtRuleOrnaments:
      return "@ornaments";
    case CSSAtRuleID::kCSSAtRuleStylistic:
      return "@stylistic";
    case CSSAtRuleID::kCSSAtRuleStyleset:
      return "@styleset";
    case CSSAtRuleID::kCSSAtRuleSwash:
      return "@swash";
    case CSSAtRuleID::kCSSAtRuleTopLeftCorner:
      return "@top-left-corner";
    case CSSAtRuleID::kCSSAtRuleTopLeft:
      return "@top-left";
    case CSSAtRuleID::kCSSAtRuleTopCenter:
      return "@top-center";
    case CSSAtRuleID::kCSSAtRuleTopRight:
      return "@top-right";
    case CSSAtRuleID::kCSSAtRuleTopRightCorner:
      return "@top-right-corner";
    case CSSAtRuleID::kCSSAtRuleBottomLeftCorner:
      return "@bottom-left-corner";
    case CSSAtRuleID::kCSSAtRuleBottomLeft:
      return "@bottom-left";
    case CSSAtRuleID::kCSSAtRuleBottomCenter:
      return "@bottom-center";
    case CSSAtRuleID::kCSSAtRuleBottomRight:
      return "@bottom-right";
    case CSSAtRuleID::kCSSAtRuleBottomRightCorner:
      return "@bottom-right-corner";
    case CSSAtRuleID::kCSSAtRuleLeftTop:
      return "@left-top";
    case CSSAtRuleID::kCSSAtRuleLeftMiddle:
      return "@left-middle";
    case CSSAtRuleID::kCSSAtRuleLeftBottom:
      return "@left-bottom";
    case CSSAtRuleID::kCSSAtRuleRightTop:
      return "@right-top";
    case CSSAtRuleID::kCSSAtRuleRightMiddle:
      return "@right-middle";
    case CSSAtRuleID::kCSSAtRuleRightBottom:
      return "@right-bottom";
    case CSSAtRuleID::kCSSAtRuleFunction:
      return "@function";
    case CSSAtRuleID::kCSSAtRuleMixin:
      return "@mixin";
    case CSSAtRuleID::kCSSAtRuleApplyMixin:
      return "@apply";
    case CSSAtRuleID::kCSSAtRuleContents:
      return "@contents";
    case CSSAtRuleID::kCSSAtRuleResult:
      return "@result";
    case CSSAtRuleID::kCSSAtRuleCustomMedia:
      return "@custom-media";
    case CSSAtRuleID::kCSSAtRuleInvalid:
    case CSSAtRuleID::kCount:
      NOTREACHED();
  };
}

namespace {

std::optional<WebFeature> AtRuleFeature(CSSAtRuleID rule_id) {
  switch (rule_id) {
    case CSSAtRuleID::kCSSAtRuleAnnotation:
      return WebFeature::kCSSAtRuleAnnotation;
    case CSSAtRuleID::kCSSAtRuleViewTransition:
      return WebFeature::kCSSAtRuleViewTransition;
    case CSSAtRuleID::kCSSAtRuleCharset:
      return WebFeature::kCSSAtRuleCharset;
    case CSSAtRuleID::kCSSAtRuleCharacterVariant:
      return WebFeature::kCSSAtRuleCharacterVariant;
    case CSSAtRuleID::kCSSAtRuleFontFace:
      return WebFeature::kCSSAtRuleFontFace;
    case CSSAtRuleID::kCSSAtRuleFontPaletteValues:
      return WebFeature::kCSSAtRuleFontPaletteValues;
    case CSSAtRuleID::kCSSAtRuleFontFeatureValues:
      return WebFeature::kCSSAtRuleFontFeatureValues;
    case CSSAtRuleID::kCSSAtRuleImport:
      return WebFeature::kCSSAtRuleImport;
    case CSSAtRuleID::kCSSAtRuleKeyframes:
      return WebFeature::kCSSAtRuleKeyframes;
    case CSSAtRuleID::kCSSAtRuleLayer:
      return WebFeature::kCSSCascadeLayers;
    case CSSAtRuleID::kCSSAtRuleMedia:
      return WebFeature::kCSSAtRuleMedia;
    case CSSAtRuleID::kCSSAtRuleNamespace:
      return WebFeature::kCSSAtRuleNamespace;
    case CSSAtRuleID::kCSSAtRulePage:
      return WebFeature::kCSSAtRulePage;
    case CSSAtRuleID::kCSSAtRuleTopLeftCorner:
    case CSSAtRuleID::kCSSAtRuleTopLeft:
    case CSSAtRuleID::kCSSAtRuleTopCenter:
    case CSSAtRuleID::kCSSAtRuleTopRight:
    case CSSAtRuleID::kCSSAtRuleTopRightCorner:
    case CSSAtRuleID::kCSSAtRuleBottomLeftCorner:
    case CSSAtRuleID::kCSSAtRuleBottomLeft:
    case CSSAtRuleID::kCSSAtRuleBottomCenter:
    case CSSAtRuleID::kCSSAtRuleBottomRight:
    case CSSAtRuleID::kCSSAtRuleBottomRightCorner:
    case CSSAtRuleID::kCSSAtRuleLeftTop:
    case CSSAtRuleID::kCSSAtRuleLeftMiddle:
    case CSSAtRuleID::kCSSAtRuleLeftBottom:
    case CSSAtRuleID::kCSSAtRuleRightTop:
    case CSSAtRuleID::kCSSAtRuleRightMiddle:
    case CSSAtRuleID::kCSSAtRuleRightBottom:
      return WebFeature::kCSSAtRulePageMargin;
    case CSSAtRuleID::kCSSAtRuleProperty:
      return WebFeature::kCSSAtRuleProperty;
    case CSSAtRuleID::kCSSAtRuleRoute:
    case CSSAtRuleID::kCSSAtRuleNavigation:
      return WebFeature::kCSSAtRuleRoute;
    case CSSAtRuleID::kCSSAtRuleContainer:
      return WebFeature::kCSSAtRuleContainer;
    case CSSAtRuleID::kCSSAtRuleCounterStyle:
      return WebFeature::kCSSAtRuleCounterStyle;
    case CSSAtRuleID::kCSSAtRuleOrnaments:
      return WebFeature::kCSSAtRuleOrnaments;
    case CSSAtRuleID::kCSSAtRuleScope:
      return WebFeature::kCSSAtRuleScope;
    case CSSAtRuleID::kCSSAtRuleStartingStyle:
      return WebFeature::kCSSAtRuleStartingStyle;
    case CSSAtRuleID::kCSSAtRuleStyleset:
      return WebFeature::kCSSAtRuleStylistic;
    case CSSAtRuleID::kCSSAtRuleStylistic:
      return WebFeature::kCSSAtRuleStylistic;
    case CSSAtRuleID::kCSSAtRuleSwash:
      return WebFeature::kCSSAtRuleSwash;
    case CSSAtRuleID::kCSSAtRuleSupports:
      return WebFeature::kCSSAtRuleSupports;
    case CSSAtRuleID::kCSSAtRulePositionTry:
      return WebFeature::kCSSAnchorPositioning;
    case CSSAtRuleID::kCSSAtRuleWebkitKeyframes:
      return WebFeature::kCSSAtRuleWebkitKeyframes;
    case CSSAtRuleID::kCSSAtRuleFunction:
      return WebFeature::kCSSFunctions;
    case CSSAtRuleID::kCSSAtRuleMixin:
    case CSSAtRuleID::kCSSAtRuleApplyMixin:
    case CSSAtRuleID::kCSSAtRuleContents:
    case CSSAtRuleID::kCSSAtRuleResult:
      return WebFeature::kCSSMixins;
    case CSSAtRuleID::kCSSAtRuleCustomMedia:
      return WebFeature::kCSSCustomMedia;
    case CSSAtRuleID::kCSSAtRuleInvalid:
    case CSSAtRuleID::kCount:
      NOTREACHED();
  }
}

}  // namespace

void CountAtRule(const CSSParserContext* context, CSSAtRuleID rule_id) {
  if (std::optional<WebFeature> feature = AtRuleFeature(rule_id)) {
    context->Count(*feature);
  }
}

}  // namespace blink
