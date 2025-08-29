// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser.h"

#include <unordered_map>

#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/gc_plugin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

auto AnyCSSParserMode() {
  return fuzztest::ElementOf<blink::CSSParserMode>(
      {blink::kHTMLStandardMode, blink::kHTMLQuirksMode,
       // SVG attributes are parsed in quirks mode but rules differ slightly.
       blink::kSVGAttributeMode,
       // @font-face rules are specially tagged in CSSPropertyValueSet so
       // CSSOM modifications don't treat them as style rules.
       blink::kCSSFontFaceRuleMode,
       // @keyframes rules are specially tagged in CSSPropertyValueSet so CSSOM
       // modifications don't allow setting animation-* in their keyframes.
       blink::kCSSKeyframeRuleMode,
       // @property rules are specially tagged so modifications through the
       // inspector don't treat them as style rules.
       blink::kCSSPropertyRuleMode,
       // @font-palette-values rules are specially tagged so modifications
       // through the inspector don't treat them as style rules.
       blink::kCSSFontPaletteValuesRuleMode,
       // @position-try rules have limitations on what they allow, also through
       // mutations in CSSOM.
       // https://drafts.csswg.org/css-anchor-position-1/#om-position-try
       blink::kCSSPositionTryRuleMode,
       // User agent stylesheets are parsed in standards mode but also allows
       // internal properties and values.
       blink::kUASheetMode,
       // This should always be the last entry.
       blink::kNumCSSParserModes});
}

auto AnyCSSDeferPropertyParsing() {
  return fuzztest::ElementOf<blink::CSSDeferPropertyParsing>(
      {blink::CSSDeferPropertyParsing::kNo,
       blink::CSSDeferPropertyParsing::kYes});
}

auto AnySecureContextMode() {
  return fuzztest::ElementOf<blink::SecureContextMode>(
      {blink::SecureContextMode::kInsecureContext,
       blink::SecureContextMode::kSecureContext});
}

void ParseSheetFuzzer(blink::CSSParserMode mode,
                      blink::SecureContextMode secure_context_mode,
                      blink::CSSDeferPropertyParsing defer_property_parsing,
                      const std::string& sheet_txt) {
  static blink::BlinkFuzzerTestSupport test_support;

  auto* context = blink::MakeGarbageCollected<blink::CSSParserContext>(
      mode, secure_context_mode);

  auto* style_sheet =
      blink::MakeGarbageCollected<blink::StyleSheetContents>(context);
  blink::String style_sheet_string(sheet_txt);
  blink::CSSParser::ParseSheet(context, style_sheet, style_sheet_string,
                               defer_property_parsing);
  blink::ThreadState::Current()->CollectAllGarbageForTesting();
}

FUZZ_TEST(CssParser, ParseSheetFuzzer)
    .WithDomains(AnyCSSParserMode(),
                 AnySecureContextMode(),
                 AnyCSSDeferPropertyParsing(),
                 fuzztest::Arbitrary<std::string>());
