// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/text_fragments_utils.h"

#include <cstring.h>

#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ios/web/common/features.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kDirectivePrefix[] = ":~:";
const char kTextFragmentPrefix[] = "text=";

base::Value DecodeStringToValue(const std::string& str) {
  return base::Value(base::UnescapeBinaryURLComponent(str));
}

}  // namespace

namespace web {

base::Value ParseTextFragments(const GURL& url) {
  if (!url.has_ref())
    return {};
  std::vector<std::string> fragments = ExtractTextFragments(url.ref());
  if (fragments.empty())
    return {};

  base::Value parsed(base::Value::Type::LIST);
  for (const std::string& fragment : fragments) {
    base::Value parsed_fragment = TextFragmentToValue(fragment);
    if (parsed_fragment.type() == base::Value::Type::NONE)
      continue;
    parsed.Append(std::move(parsed_fragment));
  }

  return parsed;
}

std::vector<std::string> ExtractTextFragments(std::string ref_string) {
  size_t start_pos = ref_string.find(kDirectivePrefix);
  if (start_pos == std::string::npos)
    return {};
  ref_string.erase(0, start_pos + strlen(kDirectivePrefix));

  std::vector<std::string> fragment_strings;
  while (ref_string.size()) {
    // Consume everything up to and including the text= prefix
    size_t prefix_pos = ref_string.find(kTextFragmentPrefix);
    if (prefix_pos == std::string::npos)
      break;
    ref_string.erase(0, prefix_pos + strlen(kTextFragmentPrefix));

    // A & indicates the end of the fragment (and the start of the next).
    // Save everything up to this point, and then consume it (including the &).
    size_t ampersand_pos = ref_string.find("&");
    if (ampersand_pos != 0)
      fragment_strings.push_back(ref_string.substr(0, ampersand_pos));
    if (ampersand_pos == std::string::npos)
      break;
    ref_string.erase(0, ampersand_pos + 1);
  }
  return fragment_strings;
}

base::Value TextFragmentToValue(std::string fragment) {
  // Text fragments have the format: [prefix-,]textStart[,textEnd][,-suffix]
  // That is, textStart is the only required param, all params are separated by
  // commas, and prefix/suffix have a trailing/leading hyphen.
  // Any commas, ampersands, or hypens inside of these values must be
  // URL-encoded.

  base::Value dict(base::Value::Type::DICTIONARY);

  // First, try to extract the optional prefix and suffix params. These have a
  // '-' as their last or first character, respectively, which should not be
  // carried over to the final dict.
  std::string prefix = "";
  size_t prefix_delimiter_pos = fragment.find("-,");
  if (prefix_delimiter_pos != std::string::npos) {
    prefix = fragment.substr(0, prefix_delimiter_pos);
    fragment.erase(0, prefix_delimiter_pos + 2);
  }

  std::string suffix = "";
  size_t suffix_delimiter_pos = fragment.rfind(",-");
  if (suffix_delimiter_pos != std::string::npos) {
    suffix = fragment.substr(suffix_delimiter_pos + 2);
    fragment.erase(suffix_delimiter_pos);
  }

  std::vector<std::string> pieces = base::SplitString(
      fragment, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (pieces.size() > 2 || pieces.empty() || pieces[0].empty()) {
    // Malformed if no piece is left for the textStart
    return base::Value(base::Value::Type::NONE);
  }

  std::string text_start = pieces[0];
  std::string text_end = pieces.size() == 2 ? pieces[1] : "";

  if (prefix.find_first_of("&-,") != std::string::npos ||
      text_start.find_first_of("&-,") != std::string::npos ||
      text_end.find_first_of("&-,") != std::string::npos ||
      suffix.find_first_of("&-,") != std::string::npos) {
    // Malformed if any of the pieces contain characters that are supposed to be
    // URL-encoded.
    return base::Value(base::Value::Type::NONE);
  }

  if (prefix.size())
    dict.SetKey("prefix", DecodeStringToValue(prefix));

  // Guaranteed non-empty after checking for malformed input above.
  dict.SetKey("textStart", DecodeStringToValue(text_start));

  if (text_end.size())
    dict.SetKey("textEnd", DecodeStringToValue(text_end));

  if (suffix.size())
    dict.SetKey("suffix", DecodeStringToValue(suffix));

  return dict;
}

}  // namespace web
