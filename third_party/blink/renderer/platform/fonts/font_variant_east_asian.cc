// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_variant_east_asian.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

static const char* kUnknownEastAsianString = "Unknown";

String FontVariantEastAsian::ToString(EastAsianForm form) {
  switch (form) {
    case EastAsianForm::kNormalForm:
      return "Normal";
    case EastAsianForm::kJis78:
      return "Jis78";
    case EastAsianForm::kJis83:
      return "Jis83";
    case EastAsianForm::kJis90:
      return "Jis90";
    case EastAsianForm::kJis04:
      return "Jis04";
    case EastAsianForm::kSimplified:
      return "Simplified";
    case EastAsianForm::kTraditional:
      return "Traditional";
  }
  return kUnknownEastAsianString;
}

String FontVariantEastAsian::ToString(EastAsianWidth width) {
  switch (width) {
    case FontVariantEastAsian::kNormalWidth:
      return "Normal";
    case FontVariantEastAsian::kFullWidth:
      return "Full";
    case FontVariantEastAsian::kProportionalWidth:
      return "Proportional";
  }
  return kUnknownEastAsianString;
}

String FontVariantEastAsian::ToString() const {
  return String::Format(
      "form=%s, width=%s, ruby=%s", ToString(Form()).Ascii().c_str(),
      ToString(Width()).Ascii().c_str(), Ruby() ? "true" : "false");
}

}  // namespace blink
