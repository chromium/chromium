// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/parsed_specifier.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

// <specdef href="https://html.spec.whatwg.org/#resolve-a-module-specifier">
// <specdef label="import-specifier"
// href="https://wicg.github.io/import-maps/#parse-a-url-like-import-specifier">
// This can return a kBare ParsedSpecifier for cases where the spec concepts
// listed above should return failure/null. The users of ParsedSpecifier should
// handle kBare cases properly, depending on contexts and whether import maps
// are enabled.
ParsedSpecifier ParsedSpecifier::Create(const String& specifier,
                                        const KURL& base_url) {
  // <spec step="1">Apply the URL parser to specifier. If the result is not
  // failure, return the result.</spec>
  //
  // <spec label="import-specifier" step="2">Let url be the result of parsing
  // specifier (with no base URL).</spec>
  KURL url(NullURL(), specifier);
  if (url.IsValid()) {
    // <spec label="import-specifier" step="4">If url’s scheme is either a fetch
    // scheme or "std", then return url.</spec>
    //
    // TODO(hiroshige): This check is done in the callers of ParsedSpecifier.
    return ParsedSpecifier(url);
  }

  // <spec step="2">If specifier does not start with the character U+002F
  // SOLIDUS (/), the two-character sequence U+002E FULL STOP, U+002F SOLIDUS
  // (./), or the three-character sequence U+002E FULL STOP, U+002E FULL STOP,
  // U+002F SOLIDUS (../), return failure.</spec>
  //
  // <spec label="import-specifier" step="1">If specifier starts with "/", "./",
  // or "../", then:</spec>
  if (!specifier.StartsWith("/") && !specifier.StartsWith("./") &&
      !specifier.StartsWith("../")) {
    // Do not consider an empty specifier as a valid bare specifier.
    //
    // <spec
    // href="https://wicg.github.io/import-maps/#normalize-a-specifier-key"
    // step="1">If specifierKey is the empty string, then:</spec>
    if (specifier.IsEmpty())
      return ParsedSpecifier();

    // <spec label="import-specifier" step="3">If url is failure, then return
    // null.</spec>
    return ParsedSpecifier(specifier);
  }

  // <spec step="3">Return the result of applying the URL parser to specifier
  // with base URL as the base URL.</spec>
  //
  // <spec label="import-specifier" step="1.1">Let url be the result of parsing
  // specifier with baseURL as the base URL.</spec>
  DCHECK(base_url.IsValid());
  KURL absolute_url(base_url, specifier);
  // <spec label="import-specifier" step="1.3">Return url.</spec>
  if (absolute_url.IsValid())
    return ParsedSpecifier(absolute_url);

  // <spec label="import-specifier" step="1.2">If url is failure, then return
  // null.</spec>
  return ParsedSpecifier();
}

String ParsedSpecifier::GetImportMapKeyString() const {
  switch (GetType()) {
    case Type::kInvalid:
      return String();
    case Type::kBare:
      return bare_specifier_;
    case Type::kURL:
      return url_.GetString();
  }
}

KURL ParsedSpecifier::GetUrl() const {
  switch (GetType()) {
    case Type::kInvalid:
    case Type::kBare:
      return NullURL();
    case Type::kURL:
      return url_;
  }
}

}  // namespace blink
