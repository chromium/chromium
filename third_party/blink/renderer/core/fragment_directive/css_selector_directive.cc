// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/css_selector_directive.h"

#include "components/shared_highlighting/core/common/fragment_directives_constants.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {
namespace {
// TODO(crbug/1253707) Reject the directive string if it uses anything not
// allowed by the spec
bool ParseCssSelectorDirective(const String& directive_string, String& value) {
  if (!directive_string.StartsWith(
          shared_highlighting::kSelectorDirectiveParameterName) ||
      !directive_string.EndsWith(shared_highlighting::kSelectorDirectiveSuffix))
    return false;

  Vector<String> parts;
  // get rid of "selector(" and ")" and split the rest by ","
  directive_string
      .Substring(
          shared_highlighting::kSelectorDirectiveParameterNameLength,
          directive_string.length() -
              shared_highlighting::kSelectorDirectiveParameterNameLength -
              shared_highlighting::kSelectorDirectiveSuffixLength)
      .Split(",", /*allow_empty_entries=*/false, parts);

  bool parsed_value = false;
  bool parsed_type = false;
  String type;
  for (auto& part : parts) {
    if (part.StartsWith(shared_highlighting::kSelectorDirectiveValuePrefix)) {
      // ambiguous case, can't have two value= parts
      if (parsed_value)
        return false;
      value = DecodeURLEscapeSequences(
          part.Substring(
              shared_highlighting::kSelectorDirectiveValuePrefixLength),
          DecodeURLMode::kUTF8);
      parsed_value = true;
    } else if (part.StartsWith(
                   shared_highlighting::kSelectorDirectiveTypePrefix)) {
      // ambiguous case, can't have two type= parts
      if (parsed_type)
        return false;
      type = part.Substring(
          shared_highlighting::kSelectorDirectiveTypePrefixLength);
      parsed_type = true;
    }
  }
  return type == shared_highlighting::kTypeCssSelector && parsed_value;
}

}  // namespace

CssSelectorDirective* CssSelectorDirective::TryParse(
    const String& directive_string) {
  String value;
  if (ParseCssSelectorDirective(directive_string, value)) {
    return MakeGarbageCollected<CssSelectorDirective>(value);
  }

  return nullptr;
}

CssSelectorDirective::CssSelectorDirective(const String& value)
    : Directive(Directive::kSelector), value_(value) {}

String CssSelectorDirective::ToStringImpl() const {
  return "selector(type=CssSelector,value=" +
         EncodeWithURLEscapeSequences(value_) + ")";
}

}  // namespace blink
