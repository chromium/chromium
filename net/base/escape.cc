// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/escape.h"

#include "base/check_op.h"
#include "build/build_config.h"

// TODO(crbug/1100760): Move functions from net/base/escape to
// base/strings/escape.

namespace net {

std::string EscapeAllExceptUnreserved(base::StringPiece text) {
  return base::EscapeAllExceptUnreserved(text);
}

std::string EscapeQueryParamValue(base::StringPiece text, bool use_plus) {
  return base::EscapeQueryParamValue(text, use_plus);
}

std::string EscapePath(base::StringPiece path) {
  return base::EscapePath(path);
}

#if BUILDFLAG(IS_APPLE)
std::string EscapeNSURLPrecursor(base::StringPiece precursor) {
  return base::EscapeNSURLPrecursor(precursor);
}
#endif  // BUILDFLAG(IS_APPLE)

std::string EscapeUrlEncodedData(base::StringPiece path, bool use_plus) {
  return base::EscapeUrlEncodedData(path, use_plus);
}

std::string EscapeNonASCIIAndPercent(base::StringPiece input) {
  return base::EscapeNonASCIIAndPercent(input);
}

std::string EscapeNonASCII(base::StringPiece input) {
  return base::EscapeNonASCII(input);
}

std::string EscapeExternalHandlerValue(base::StringPiece text) {
  return base::EscapeExternalHandlerValue(text);
}

void AppendEscapedCharForHTML(char c, std::string* output) {
  base::AppendEscapedCharForHTML(c, output);
}

std::string EscapeForHTML(base::StringPiece input) {
  return base::EscapeForHTML(input);
}

std::u16string EscapeForHTML(base::StringPiece16 input) {
  return base::EscapeForHTML(input);
}

std::string UnescapeURLComponent(base::StringPiece escaped_text,
                                 UnescapeRule::Type rules) {
  return base::UnescapeURLComponent(escaped_text, rules);
}

std::u16string UnescapeAndDecodeUTF8URLComponentWithAdjustments(
    base::StringPiece text,
    UnescapeRule::Type rules,
    base::OffsetAdjuster::Adjustments* adjustments) {
  return base::UnescapeAndDecodeUTF8URLComponentWithAdjustments(text, rules,
                                                                adjustments);
}

std::string UnescapeBinaryURLComponent(base::StringPiece escaped_text,
                                       UnescapeRule::Type rules) {
  return base::UnescapeBinaryURLComponent(escaped_text, rules);
}

bool UnescapeBinaryURLComponentSafe(base::StringPiece escaped_text,
                                    bool fail_on_path_separators,
                                    std::string* unescaped_text) {
  return base::UnescapeBinaryURLComponentSafe(
      escaped_text, fail_on_path_separators, unescaped_text);
}

std::u16string UnescapeForHTML(base::StringPiece16 input) {
  return base::UnescapeForHTML(input);
}

}  // namespace net
