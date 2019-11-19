// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

// The prefix and suffix terms are specified to begin/end with a '-' character.
// These methods will find the prefix/suffix and return it without the '-', and
// remove the prefix/suffix from the |target_text| string. If not found, we
// return an empty string to indicate no prefix/suffix was specified or it
// was malformed and should be ignored.
String ExtractPrefix(String* target_text) {
  size_t comma_pos = target_text->find(',');
  size_t hyphen_pos = target_text->find('-');

  if (hyphen_pos != kNotFound && hyphen_pos == comma_pos - 1) {
    String prefix = target_text->Substring(0, hyphen_pos);
    target_text->Remove(0, comma_pos + 1);
    return prefix;
  }
  return "";
}

String ExtractSuffix(String* target_text) {
  size_t last_comma_pos = target_text->ReverseFind(',');
  size_t last_hyphen_pos = target_text->ReverseFind('-');

  if (last_hyphen_pos != kNotFound && last_hyphen_pos == last_comma_pos + 1) {
    String suffix = target_text->Substring(last_hyphen_pos + 1);
    target_text->Truncate(last_comma_pos);
    return suffix;
  }
  return "";
}

}  // namespace

TextFragmentSelector TextFragmentSelector::Create(String target_text) {
  SelectorType type;
  String start;
  String end;

  String prefix = ExtractPrefix(&target_text);
  String suffix = ExtractSuffix(&target_text);

  size_t comma_pos = target_text.find(',');

  if (comma_pos == kNotFound) {
    type = kExact;
    start = target_text;
    end = "";
  } else {
    type = kRange;
    start = target_text.Substring(0, comma_pos);
    end = target_text.Substring(comma_pos + 1);
  }

  return TextFragmentSelector(
      type, DecodeURLEscapeSequences(start, DecodeURLMode::kUTF8),
      DecodeURLEscapeSequences(end, DecodeURLMode::kUTF8),
      DecodeURLEscapeSequences(prefix, DecodeURLMode::kUTF8),
      DecodeURLEscapeSequences(suffix, DecodeURLMode::kUTF8));
}

TextFragmentSelector::TextFragmentSelector(SelectorType type,
                                           const String& start,
                                           const String& end,
                                           const String& prefix,
                                           const String& suffix)
    : type_(type), start_(start), end_(end), prefix_(prefix), suffix_(suffix) {}

}  // namespace blink
