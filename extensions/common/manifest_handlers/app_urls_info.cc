// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/app_urls_info.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

bool ParseAppURLs(Extension& extension, std::u16string* error) {
  // ParseAppURLs() should be called only by Hosted Apps.
  // TODO(crbug.com/324534603): Change this to is_hosted_app() and
  // update Extension::InitFromValue() accordingly, convert to CHECK().
  DCHECK(extension.is_app());
  const base::Value* temp_pattern_value =
      extension.manifest()->FindPath(keys::kWebURLs);
  if (temp_pattern_value == nullptr) {
    return true;
  }

  if (!temp_pattern_value->is_list()) {
    *error = errors::kInvalidWebURLs;
    return false;
  }

  const base::ListValue& pattern_list = temp_pattern_value->GetList();
  for (size_t i = 0; i < pattern_list.size(); ++i) {
    std::string pattern_string;
    if (pattern_list[i].is_string()) {
      pattern_string = pattern_list[i].GetString();
    } else {
      *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidWebURL,
                                                   base::NumberToString(i),
                                                   errors::kExpectString);
      return false;
    }

    URLPattern pattern(Extension::kValidWebExtentSchemes);
    URLPattern::ParseResult parse_result = pattern.Parse(pattern_string);
    if (parse_result == URLPattern::ParseResult::kEmptyPath) {
      pattern_string += "/";
      parse_result = pattern.Parse(pattern_string);
    }

    if (parse_result != URLPattern::ParseResult::kSuccess) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidWebURL, base::NumberToString(i),
          URLPattern::GetParseResultString(parse_result));
      return false;
    }

    // Do not allow authors to claim "<all_urls>".
    if (pattern.match_all_urls()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidWebURL, base::NumberToString(i),
          errors::kCannotClaimAllURLsInExtent);
      return false;
    }

    // Do not allow authors to claim "*" for host.
    if (pattern.host().empty()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidWebURL, base::NumberToString(i),
          errors::kCannotClaimAllHostsInExtent);
      return false;
    }

    // We do not allow authors to put wildcards in their paths. Instead, we
    // imply one at the end.
    if (pattern.path().contains('*')) {
      *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidWebURL,
                                                   base::NumberToString(i),
                                                   errors::kNoWildCardsInPaths);
      return false;
    }
    pattern.SetPath(pattern.path() + '*');

    extension.AddWebExtentPattern(pattern);
  }

  return true;
}

}  // namespace extensions
