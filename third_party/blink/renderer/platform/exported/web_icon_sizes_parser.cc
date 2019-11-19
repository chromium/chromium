// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_icon_sizes_parser.h"

#include <algorithm>
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

static inline bool IsIntegerStart(UChar c) {
  return c > '0' && c <= '9';
}

static bool IsWhitespace(UChar c) {
  // Sizes space characters are U+0020 SPACE, U+0009 CHARACTER TABULATION (tab),
  // U+000A LINE FEED (LF), U+000C FORM FEED (FF),
  // and U+000D CARRIAGE RETURN (CR).
  return c == ' ' || c == '\t' || c == '\n' || c == '\f' || c == '\r';
}

static bool IsNotWhitespace(UChar c) {
  return !IsWhitespace(c);
}

static bool IsNonDigit(UChar c) {
  return !IsASCIIDigit(c);
}

static inline wtf_size_t FindEndOfWord(const String& string, wtf_size_t start) {
  return std::min(string.Find(IsWhitespace, start), string.length());
}

static inline int PartialStringToInt(const String& string,
                                     wtf_size_t start,
                                     wtf_size_t end) {
  if (string.Is8Bit()) {
    return CharactersToInt(string.Characters8() + start, end - start,
                           WTF::NumberParsingOptions::kNone, nullptr);
  }
  return CharactersToInt(string.Characters16() + start, end - start,
                         WTF::NumberParsingOptions::kNone, nullptr);
}

}  // namespace

WebVector<WebSize> WebIconSizesParser::ParseIconSizes(
    const WebString& web_sizes_string) {
  String sizes_string = web_sizes_string;
  Vector<WebSize> icon_sizes;
  if (sizes_string.IsEmpty())
    return icon_sizes;

  wtf_size_t length = sizes_string.length();
  for (wtf_size_t i = 0; i < length; ++i) {
    // Skip whitespaces.
    i = std::min(sizes_string.Find(IsNotWhitespace, i), length);
    if (i >= length)
      break;

    // See if the current size is "any".
    if (sizes_string.Substring(i, 3).StartsWithIgnoringCase("any") &&
        (i + 3 == length || IsWhitespace(sizes_string[i + 3]))) {
      icon_sizes.push_back(WebSize(0, 0));
      i = i + 3;
      continue;
    }

    // Parse the width.
    if (!IsIntegerStart(sizes_string[i])) {
      i = FindEndOfWord(sizes_string, i);
      continue;
    }
    wtf_size_t width_start = i;
    i = std::min(sizes_string.Find(IsNonDigit, i), length);
    if (i >= length || (sizes_string[i] != 'x' && sizes_string[i] != 'X')) {
      i = FindEndOfWord(sizes_string, i);
      continue;
    }
    wtf_size_t width_end = i++;

    // Parse the height.
    if (i >= length || !IsIntegerStart(sizes_string[i])) {
      i = FindEndOfWord(sizes_string, i);
      continue;
    }
    wtf_size_t height_start = i;
    i = std::min(sizes_string.Find(IsNonDigit, i), length);
    if (i < length && !IsWhitespace(sizes_string[i])) {
      i = FindEndOfWord(sizes_string, i);
      continue;
    }
    wtf_size_t height_end = i;

    // Append the parsed size to iconSizes.
    icon_sizes.push_back(
        WebSize(PartialStringToInt(sizes_string, width_start, width_end),
                PartialStringToInt(sizes_string, height_start, height_end)));
  }
  return icon_sizes;
}

}  // namespace blink
