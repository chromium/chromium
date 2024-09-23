// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/public/platform/web_icon_sizes_parser.h"

#include <algorithm>

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/size.h"

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
  return WTF::VisitCharacters(
      StringView(string, start, end - start), [](auto chars) {
        return CharactersToInt(chars, WTF::NumberParsingOptions(), nullptr);
      });
}

}  // namespace

WebVector<gfx::Size> WebIconSizesParser::ParseIconSizes(
    const WebString& web_sizes_string) {
  String sizes_string = web_sizes_string;
  Vector<gfx::Size> icon_sizes;
  if (sizes_string.empty())
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
      icon_sizes.push_back(gfx::Size());
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
        gfx::Size(PartialStringToInt(sizes_string, width_start, width_end),
                  PartialStringToInt(sizes_string, height_start, height_end)));
  }
  return icon_sizes;
}

}  // namespace blink
