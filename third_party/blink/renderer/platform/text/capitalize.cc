// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/capitalize.h"

#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

String Capitalize(const String& string, UChar previous_character) {
  if (string.IsNull())
    return string;

  unsigned length = string.length();
  const StringImpl& input = *string.Impl();

  CHECK_LT(length, std::numeric_limits<unsigned>::max());
  StringBuffer<UChar> string_with_previous(length + 1);
  string_with_previous[0] = previous_character == kNoBreakSpaceCharacter
                                ? kSpaceCharacter
                                : previous_character;
  for (unsigned i = 1; i < length + 1; i++) {
    // Replace &nbsp with a real space since ICU no longer treats &nbsp as a
    // word separator.
    if (input[i - 1] == kNoBreakSpaceCharacter)
      string_with_previous[i] = kSpaceCharacter;
    else
      string_with_previous[i] = input[i - 1];
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
          input[start_of_word - 1] == kNoBreakSpaceCharacter
              ? kNoBreakSpaceCharacter
              : WTF::unicode::ToTitleCase(string_with_previous[start_of_word]));
    }
    for (int i = start_of_word + 1; i < end_of_word; i++)
      result.Append(input[i - 1]);
  }

  return result.ToString();
}

}  // namespace blink
