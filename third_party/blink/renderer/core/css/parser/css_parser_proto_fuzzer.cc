// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_proto_converter.h"

#include <unordered_map>

#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

#include "third_party/blink/renderer/core/css/parser/css.pb.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

protobuf_mutator::protobuf::LogSilencer log_silencer;

using css_proto_converter::Input;

DEFINE_BINARY_PROTO_FUZZER(const Input& input) {
  static css_proto_converter::Converter converter;
  static blink::BlinkFuzzerTestSupport test_support;

  static std::unordered_map<Input::CSSParserMode, blink::CSSParserMode>
      parser_mode_map = {
          {Input::kHTMLStandardMode, blink::kHTMLStandardMode},
          {Input::kHTMLQuirksMode, blink::kHTMLQuirksMode},
          {Input::kSVGAttributeMode, blink::kSVGAttributeMode},
          {Input::kCSSViewportRuleMode, blink::kCSSViewportRuleMode},
          {Input::kCSSFontFaceRuleMode, blink::kCSSFontFaceRuleMode},
          {Input::kUASheetMode, blink::kUASheetMode}};

  static std::unordered_map<Input::SecureContextMode, blink::SecureContextMode>
      secure_context_mode_map = {
          {Input::kInsecureContext, blink::SecureContextMode::kInsecureContext},
          {Input::kSecureContext, blink::SecureContextMode::kSecureContext}};

  blink::CSSParserMode mode = parser_mode_map[input.css_parser_mode()];
  blink::SecureContextMode secure_context_mode =
      secure_context_mode_map[input.secure_context_mode()];
  const blink::CSSParserContext::SelectorProfile selector_profile =
      input.is_live_profile() ? blink::CSSParserContext::kLiveProfile
                              : blink::CSSParserContext::kSnapshotProfile;
  auto* context = blink::MakeGarbageCollected<blink::CSSParserContext>(
      mode, secure_context_mode, selector_profile);

  auto* style_sheet =
      blink::MakeGarbageCollected<blink::StyleSheetContents>(context);
  WTF::String style_sheet_string(
      converter.Convert(input.style_sheet()).c_str());
  const blink::CSSDeferPropertyParsing defer_property_parsing =
      input.defer_property_parsing() ? blink::CSSDeferPropertyParsing::kYes
                                     : blink::CSSDeferPropertyParsing::kNo;
  blink::CSSParser::ParseSheet(context, style_sheet, style_sheet_string,
                               defer_property_parsing);
}
