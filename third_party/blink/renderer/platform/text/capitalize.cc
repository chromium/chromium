// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/capitalize.h"

#include <unicode/utf16.h>

#include <limits>

#include "base/compiler_specific.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

namespace {

String CapitalizeWithPreviousCodeUnit(const String& string,
                                      UChar previous_character) {
  if (string.IsNull())
    return string;

  wtf_size_t length = string.length();
  const StringImpl& input = *string.Impl();

  CHECK_LT(length, std::numeric_limits<wtf_size_t>::max());
  StringBuffer<UChar> string_with_previous(length + 1);
  string_with_previous[0] = previous_character == uchar::kNoBreakSpace
                                ? uchar::kSpace
                                : previous_character;
  for (wtf_size_t i = 1; i < length + 1; ++i) {
    // Replace &nbsp with a real space since ICU no longer treats &nbsp as a
    // word separator.
    if (UNSAFE_TODO(input[i - 1]) == uchar::kNoBreakSpace) {
      string_with_previous[i] = uchar::kSpace;
    } else {
      string_with_previous[i] = UNSAFE_TODO(input[i - 1]);
    }
  }

  TextBreakIterator* boundary = WordBreakIterator(string_with_previous.Span());
  if (!boundary)
    return string;

  StringBuilder result;
  result.ReserveCapacity(length);

  int32_t end_of_word;
  int32_t start_of_word = boundary->first();
  for (end_of_word = boundary->next(); end_of_word != kTextBreakDone;
       start_of_word = end_of_word, end_of_word = boundary->next()) {
    if (start_of_word) {  // Ignore first char of previous string
      result.Append(
          UNSAFE_TODO(input[start_of_word - 1]) == uchar::kNoBreakSpace
              ? uchar::kNoBreakSpace
              : unicode::ToTitleCase(string_with_previous[start_of_word]));
    }
    for (int i = start_of_word + 1; i < end_of_word; i++)
      result.Append(UNSAFE_TODO(input[i - 1]));
  }

  return result.ToString();
}

String CapitalizeWithPreviousCodePoint(const String& string,
                                       UChar32 previous_character) {
  if (string.IsNull()) {
    return string;
  }

  wtf_size_t length = string.length();
  if (previous_character == uchar::kNoBreakSpace) {
    previous_character = uchar::kSpace;
  }
  const int32_t previous_character_length = U16_LENGTH(previous_character);
  CHECK_LE(length,
           std::numeric_limits<wtf_size_t>::max() - previous_character_length);
  StringBuilder string_with_previous;
  string_with_previous.Reserve16BitCapacity(length + previous_character_length);
  string_with_previous.Append(previous_character);
  for (wtf_size_t i = 0; i < length; ++i) {
    // Replace &nbsp with a real space since ICU no longer treats &nbsp as a
    // word separator.
    const UChar character = string[i];
    string_with_previous.Append(
        character == uchar::kNoBreakSpace ? uchar::kSpace : character);
  }

  TextBreakIterator* boundary = WordBreakIterator(string_with_previous);
  if (!boundary) {
    return string;
  }

  StringBuilder result;
  result.ReserveCapacity(length);

  int32_t end_of_word;
  int32_t start_of_word = boundary->first();
  for (end_of_word = boundary->next(); end_of_word != kTextBreakDone;
       start_of_word = end_of_word, end_of_word = boundary->next()) {
    if (end_of_word <= previous_character_length) {
      continue;
    }

    wtf_size_t input_cursor = 0;
    if (start_of_word >= previous_character_length) {
      input_cursor = start_of_word - previous_character_length;
      const UChar first_character = string[input_cursor++];
      result.Append(first_character == uchar::kNoBreakSpace
                        ? uchar::kNoBreakSpace
                        : unicode::ToTitleCase(first_character));
    }
    result.Append(string, input_cursor,
                  end_of_word - previous_character_length - input_cursor);
  }

  return result.ToString();
}

}  // namespace

String Capitalize(const String& string, UChar32 previous_character) {
  if (!RuntimeEnabledFeatures::
          CapitalizeAfterSupplementaryCharacterFixEnabled()) {
    return CapitalizeWithPreviousCodeUnit(
        string, static_cast<UChar>(previous_character));
  }
  return CapitalizeWithPreviousCodePoint(string, previous_character);
}

}  // namespace blink
