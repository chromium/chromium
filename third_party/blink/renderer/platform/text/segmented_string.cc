/*
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/text/segmented_string.h"

namespace blink {

unsigned SegmentedString::length() const {
  unsigned length = current_string_.length();
  if (IsComposite()) {
    for (auto& substring : substrings_)
      length += substring.length();
  }
  return length;
}

void SegmentedString::SetExcludeLineNumbers() {
  current_string_.SetExcludeLineNumbers();
  if (IsComposite()) {
    for (auto& substring : substrings_)
      substring.SetExcludeLineNumbers();
  }
}

void SegmentedString::Clear() {
  current_string_.Clear();
  number_of_characters_consumed_prior_to_current_string_ = 0;
  number_of_characters_consumed_prior_to_current_line_ = 0;
  current_line_ = 0;
  substrings_.clear();
  closed_ = false;
  empty_ = true;
}

void SegmentedString::Append(const SegmentedSubstring& s) {
  DCHECK(!closed_);
  if (!s.length())
    return;

  if (!current_string_.length()) {
    number_of_characters_consumed_prior_to_current_string_ +=
        current_string_.NumberOfCharactersConsumed();
    current_string_ = s;
    current_char_ = current_string_.GetCurrentChar();
  } else {
    substrings_.push_back(s);
  }
  empty_ = false;
}

void SegmentedString::Push(UChar c) {
  DCHECK(c);

  // pushIfPossible attempts to rewind the pointer in the SegmentedSubstring,
  // however it will fail if the SegmentedSubstring is empty, or
  // when we prepended some text while consuming a SegmentedSubstring by
  // document.write().
  if (current_string_.PushIfPossible(c)) {
    current_char_ = current_string_.GetCurrentChar();
    return;
  }

  Prepend(SegmentedString(String(&c, 1u)), PrependType::kUnconsume);
}

void SegmentedString::Prepend(const SegmentedSubstring& s, PrependType type) {
  DCHECK(!s.NumberOfCharactersConsumed());
  if (!s.length())
    return;

  // FIXME: We're also ASSERTing that s is a fresh SegmentedSubstring.
  //        The assumption is sufficient for our current use, but we might
  //        need to handle the more elaborate cases in the future.
  number_of_characters_consumed_prior_to_current_string_ +=
      current_string_.NumberOfCharactersConsumed();
  if (type == PrependType::kUnconsume)
    number_of_characters_consumed_prior_to_current_string_ -= s.length();
  if (!current_string_.length()) {
    current_string_ = s;
  } else {
    // Shift our m_currentString into our list.
    substrings_.push_front(current_string_);
    current_string_ = s;
  }
  current_char_ = current_string_.GetCurrentChar();
  empty_ = false;
}

void SegmentedString::Close() {
  // Closing a stream twice is likely a coding mistake.
  DCHECK(!closed_);
  closed_ = true;
}

void SegmentedString::Append(const SegmentedString& s) {
  DCHECK(!closed_);

  Append(s.current_string_);
  if (s.IsComposite()) {
    for (auto& substring : s.substrings_)
      Append(substring);
  }
}

void SegmentedString::Prepend(const SegmentedString& s, PrependType type) {
  if (s.IsComposite()) {
    auto it = s.substrings_.rbegin();
    auto e = s.substrings_.rend();
    for (; it != e; ++it)
      Prepend(*it, type);
  }
  Prepend(s.current_string_, type);
}

void SegmentedString::Advance(unsigned num_chars,
                              unsigned num_lines,
                              int current_column) {
  SECURITY_DCHECK(num_chars <= length());
  current_line_ += num_lines;
  while (num_chars) {
    num_chars -= current_string_.Advance(num_chars);
    if (num_chars) {
      // AdvanceSubstring() assumes one char is remaining.
      DCHECK_EQ(current_string_.length(), 1);
      AdvanceSubstring();
      --num_chars;
    }
  }
  number_of_characters_consumed_prior_to_current_line_ =
      NumberOfCharactersConsumed() - current_column;
  current_char_ = empty_ ? '\0' : current_string_.GetCurrentChar();
}

UChar SegmentedString::AdvanceSubstring() {
  number_of_characters_consumed_prior_to_current_string_ +=
      current_string_.NumberOfCharactersConsumed() + 1;
  if (IsComposite()) {
    current_string_ = substrings_.TakeFirst();
    // If we've previously consumed some characters of the non-current
    // string, we now account for those characters as part of the current
    // string, not as part of "prior to current string."
    number_of_characters_consumed_prior_to_current_string_ -=
        current_string_.NumberOfCharactersConsumed();
    current_char_ = current_string_.GetCurrentChar();
    return CurrentChar();
  } else {
    current_string_.Clear();
    empty_ = true;
    current_char_ = '\0';
    return 0;
  }
}

String SegmentedString::ToString() const {
  StringBuilder result;
  current_string_.AppendTo(result);
  if (IsComposite()) {
    for (auto& substring : substrings_)
      substring.AppendTo(result);
  }
  return result.ToString();
}

void SegmentedString::AdvanceAndCollect(base::span<UChar> characters) {
  CHECK_LE(characters.size(), length());
  for (size_t i = 0; i < characters.size(); ++i) {
    characters[i] = CurrentChar();
    Advance();
  }
}

OrdinalNumber SegmentedString::CurrentLine() const {
  return OrdinalNumber::FromZeroBasedInt(current_line_);
}

OrdinalNumber SegmentedString::CurrentColumn() const {
  int zero_based_column = NumberOfCharactersConsumed() -
                          number_of_characters_consumed_prior_to_current_line_;
  return OrdinalNumber::FromZeroBasedInt(zero_based_column);
}

void SegmentedString::SetCurrentPosition(OrdinalNumber line,
                                         OrdinalNumber column_aftre_prolog,
                                         int prolog_length) {
  current_line_ = line.ZeroBasedInt();
  number_of_characters_consumed_prior_to_current_line_ =
      NumberOfCharactersConsumed() + prolog_length -
      column_aftre_prolog.ZeroBasedInt();
}

}  // namespace blink
