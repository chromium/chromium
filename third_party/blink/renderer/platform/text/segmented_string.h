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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_SEGMENTED_STRING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_SEGMENTED_STRING_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class SegmentedString;

class PLATFORM_EXPORT SegmentedSubstring {
  DISALLOW_NEW();

 public:
  SegmentedSubstring() { data_.string8_ptr = nullptr; }

  SegmentedSubstring(const String& str)
      : length_(str.length()),
        string_(str) {
    if (length_) {
      if (string_.Is8Bit()) {
        is_8bit_ = true;
        data_.string8_ptr = string_.Characters8();
        current_char_ = *data_.string8_ptr;
      } else {
        is_8bit_ = false;
        data_.string16_ptr = string_.Characters16();
        current_char_ = *data_.string16_ptr;
      }
    } else {
      is_8bit_ = true;
      data_.string8_ptr = nullptr;
    }
  }

  void Clear() {
    length_ = 0;
    is_8bit_ = true;
    data_.string8_ptr = nullptr;
    current_char_ = 0;
  }

  bool ExcludeLineNumbers() const { return !do_not_exclude_line_numbers_; }
  bool DoNotExcludeLineNumbers() const { return do_not_exclude_line_numbers_; }

  void SetExcludeLineNumbers() { do_not_exclude_line_numbers_ = false; }

  int NumberOfCharactersConsumed() const { return string_.length() - length_; }

  void AppendTo(StringBuilder& builder) const {
    int offset = string_.length() - length_;

    if (!offset) {
      if (length_)
        builder.Append(string_);
    } else {
      builder.Append(string_.Substring(offset, length_));
    }
  }

  bool PushIfPossible(UChar c) {
    if (!length_)
      return false;

    // This checks if either 8 or 16 bit strings are in the first character
    // (where we can't rewind). Since length_ is greater than zero, we can check
    // the Impl() directly and avoid a branch here.
    if (data_.void_ptr == string_.Impl()->Bytes())
      return false;

    if (is_8bit_) {
      if (*(data_.string8_ptr - 1) != c)
        return false;

      --data_.string8_ptr;
    } else {
      if (*(data_.string16_ptr - 1) != c)
        return false;

      --data_.string16_ptr;
    }

    current_char_ = c;
    ++length_;
    return true;
  }

  ALWAYS_INLINE UChar GetCurrentChar() const { return current_char_; }

  ALWAYS_INLINE void IncrementAndDecrementLength() {
    current_char_ = is_8bit_ ? *++data_.string8_ptr : *++data_.string16_ptr;
    --length_;
  }

  String CurrentSubString(unsigned length) {
    int offset = string_.length() - length_;
    return string_.Substring(offset, length);
  }

  ALWAYS_INLINE int length() const { return length_; }

 private:
  union {
    const LChar* string8_ptr;
    const UChar* string16_ptr;
    const void* void_ptr;
  } data_;
  int length_ = 0;
  UChar current_char_ = 0;
  bool do_not_exclude_line_numbers_ = true;
  bool is_8bit_ = true;
  String string_;
};

class PLATFORM_EXPORT SegmentedString {
  DISALLOW_NEW();

 public:
  SegmentedString()
      : number_of_characters_consumed_prior_to_current_string_(0),
        number_of_characters_consumed_prior_to_current_line_(0),
        current_line_(0),
        closed_(false),
        empty_(true) {}

  SegmentedString(const String& str)
      : current_string_(str),
        number_of_characters_consumed_prior_to_current_string_(0),
        number_of_characters_consumed_prior_to_current_line_(0),
        current_line_(0),
        closed_(false),
        empty_(!str.length()) {}

  void Clear();
  void Close();

  void Append(const SegmentedString&);
  enum class PrependType {
    kNewInput = 0,
    kUnconsume = 1,
  };
  void Prepend(const SegmentedString&, PrependType);

  bool ExcludeLineNumbers() const {
    return current_string_.ExcludeLineNumbers();
  }
  void SetExcludeLineNumbers();

  void Push(UChar);

  bool IsEmpty() const { return empty_; }
  unsigned length() const;

  bool IsClosed() const { return closed_; }

  enum LookAheadResult {
    kDidNotMatch,
    kDidMatch,
    kNotEnoughCharacters,
  };

  LookAheadResult LookAhead(const String& string) {
    return LookAheadInline(string, kTextCaseSensitive);
  }
  LookAheadResult LookAheadIgnoringCase(const String& string) {
    return LookAheadInline(string, kTextCaseASCIIInsensitive);
  }

  ALWAYS_INLINE void Advance() {
    if (LIKELY(current_string_.length() > 1)) {
      current_string_.IncrementAndDecrementLength();
    } else {
      AdvanceSubstring();
    }
  }

  ALWAYS_INLINE void UpdateLineNumber() {
    if (LIKELY(current_string_.DoNotExcludeLineNumbers())) {
      ++current_line_;
      // Plus 1 because numberOfCharactersConsumed value hasn't incremented yet;
      // it does with length() decrement below.
      number_of_characters_consumed_prior_to_current_line_ =
          NumberOfCharactersConsumed() + 1;
    }
  }

  ALWAYS_INLINE void AdvanceAndUpdateLineNumber() {
    DCHECK_GE(current_string_.length(), 1);

    if (current_string_.GetCurrentChar() == '\n')
      UpdateLineNumber();

    if (LIKELY(current_string_.length() > 1)) {
      current_string_.IncrementAndDecrementLength();
    } else {
      AdvanceSubstring();
    }
  }

  ALWAYS_INLINE void AdvanceAndASSERT(UChar expected_character) {
    DCHECK_EQ(expected_character, CurrentChar());
    Advance();
  }

  ALWAYS_INLINE void AdvanceAndASSERTIgnoringCase(UChar expected_character) {
    DCHECK_EQ(WTF::unicode::FoldCase(CurrentChar()),
              WTF::unicode::FoldCase(expected_character));
    Advance();
  }

  ALWAYS_INLINE void AdvancePastNonNewline() {
    DCHECK_NE(CurrentChar(), '\n');
    Advance();
  }

  ALWAYS_INLINE void AdvancePastNewlineAndUpdateLineNumber() {
    DCHECK_EQ(CurrentChar(), '\n');
    DCHECK_GE(current_string_.length(), 1);

    UpdateLineNumber();

    if (LIKELY(current_string_.length() > 1)) {
      current_string_.IncrementAndDecrementLength();
    } else {
      AdvanceSubstring();
    }
  }

  // Writes the consumed characters into consumedCharacters, which must
  // have space for at least |count| characters.
  void Advance(unsigned count, UChar* consumed_characters);

  int NumberOfCharactersConsumed() const {
    int number_of_pushed_characters = 0;
    return number_of_characters_consumed_prior_to_current_string_ +
           current_string_.NumberOfCharactersConsumed() -
           number_of_pushed_characters;
  }

  String ToString() const;

  ALWAYS_INLINE UChar CurrentChar() const {
    return current_string_.GetCurrentChar();
  }

  // The method is moderately slow, comparing to currentLine method.
  OrdinalNumber CurrentColumn() const;
  OrdinalNumber CurrentLine() const;
  // Sets value of line/column variables. Column is specified indirectly by a
  // parameter columnAftreProlog which is a value of column that we should get
  // after a prolog (first prologLength characters) has been consumed.
  void SetCurrentPosition(OrdinalNumber line,
                          OrdinalNumber column_aftre_prolog,
                          int prolog_length);

 private:
  void Append(const SegmentedSubstring&);
  void Prepend(const SegmentedSubstring&, PrependType);

  void AdvanceSubstring();

  inline LookAheadResult LookAheadInline(const String& string,
                                         TextCaseSensitivity case_sensitivity) {
    if (string.length() <= static_cast<unsigned>(current_string_.length())) {
      String current_substring =
          current_string_.CurrentSubString(string.length());
      if (current_substring.StartsWith(string, case_sensitivity))
        return kDidMatch;
      return kDidNotMatch;
    }
    return LookAheadSlowCase(string, case_sensitivity);
  }

  LookAheadResult LookAheadSlowCase(const String& string,
                                    TextCaseSensitivity case_sensitivity) {
    unsigned count = string.length();
    if (count > length())
      return kNotEnoughCharacters;
    UChar* consumed_characters;
    String consumed_string =
        String::CreateUninitialized(count, consumed_characters);
    Advance(count, consumed_characters);
    LookAheadResult result = kDidNotMatch;
    if (consumed_string.StartsWith(string, case_sensitivity))
      result = kDidMatch;
    Prepend(SegmentedString(consumed_string), PrependType::kUnconsume);
    return result;
  }

  bool IsComposite() const { return !substrings_.IsEmpty(); }

  SegmentedSubstring current_string_;
  int number_of_characters_consumed_prior_to_current_string_;
  int number_of_characters_consumed_prior_to_current_line_;
  int current_line_;
  Deque<SegmentedSubstring> substrings_;
  bool closed_;
  bool empty_;
};

}  // namespace blink

#endif
