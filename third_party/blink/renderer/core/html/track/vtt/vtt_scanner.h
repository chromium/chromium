/*
 * Copyright (c) 2013, Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Opera Software ASA nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TRACK_VTT_VTT_SCANNER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TRACK_VTT_VTT_SCANNER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Helper class for "scanning" an input string and performing parsing of
// "micro-syntax"-like constructs.
//
// There's two primary operations: match and scan.
//
// The 'match' operation matches an explicitly or implicitly specified sequence
// against the characters ahead of the current input pointer, and returns true
// if the sequence can be matched.
//
// The 'scan' operation performs a 'match', and if the match is successful it
// advance the input pointer past the matched sequence.
class CORE_EXPORT VTTScanner {
  STACK_ALLOCATED();

 public:
  explicit VTTScanner(const String& line);

  typedef const LChar* Position;

  class Run {
    STACK_ALLOCATED();

   public:
    Run(Position start, Position end, bool is_8bit)
        : start_(start), end_(end), is_8bit_(is_8bit) {}

    Position Start() const { return start_; }
    Position end() const { return end_; }

    bool IsEmpty() const { return start_ == end_; }
    wtf_size_t length() const;

   private:
    Position start_;
    Position end_;
    bool is_8bit_;
  };

  // Check if the input pointer points at the specified position.
  bool IsAt(Position check_position) const {
    return GetPosition() == check_position;
  }
  // Check if the input pointer points at the end of the input.
  bool IsAtEnd() const { return GetPosition() == end(); }
  // Match the character |c| against the character at the input pointer
  // (~lookahead).
  bool Match(char c) const { return !IsAtEnd() && CurrentChar() == c; }
  // Scan the character |c|.
  bool Scan(char);
  // Scan the first |charactersCount| characters of the string |characters|.
  bool Scan(const LChar* characters, wtf_size_t characters_count);

  // Scan the literal |characters|.
  template <unsigned charactersCount>
  bool Scan(const char (&characters)[charactersCount]);

  // Skip (advance the input pointer) as long as the specified
  // |characterPredicate| returns true, and the input pointer is not passed
  // the end of the input.
  template <bool characterPredicate(UChar)>
  void SkipWhile();

  // Like skipWhile, but using a negated predicate.
  template <bool characterPredicate(UChar)>
  void SkipUntil();

  // Return the run of characters for which the specified
  // |characterPredicate| returns true. The start of the run will be the
  // current input pointer.
  template <bool characterPredicate(UChar)>
  Run CollectWhile();

  // Like collectWhile, but using a negated predicate.
  template <bool characterPredicate(UChar)>
  Run CollectUntil();

  // Scan the string |toMatch|, using the specified |run| as the sequence to
  // match against.
  bool ScanRun(const Run&, const String& to_match);

  // Skip to the end of the specified |run|.
  void SkipRun(const Run&);

  // Return the String made up of the characters in |run|, and advance the
  // input pointer to the end of the run.
  String ExtractString(const Run&);

  // Return a String constructed from the rest of the input (between input
  // pointer and end of input), and advance the input pointer accordingly.
  String RestOfInputAsString();

  // Scan a set of ASCII digits from the input. Return the number of digits
  // scanned, and set |number| to the computed value. If the digits make up a
  // number that does not fit the 'unsigned' type, |number| is set to UINT_MAX.
  // Note: Does not handle sign.
  unsigned ScanDigits(unsigned& number);

  // Scan a floating point value on one of the forms: \d+\.? \d+\.\d+ \.\d+
  bool ScanDouble(double& number);

  // Scan a floating point value (per ScanDouble) followed by a '%'.
  bool ScanPercentage(double& percentage);

 protected:
  Position GetPosition() const { return data_.characters8; }
  Position end() const { return end_.characters8; }
  void SeekTo(Position);
  UChar CurrentChar() const;
  void Advance(unsigned amount = 1);
  // Adapt a UChar-predicate to an LChar-predicate.
  // (For use with SkipWhile/Until from parsing_utilities.h).
  template <bool characterPredicate(UChar)>
  static inline bool LCharPredicateAdapter(LChar c) {
    return characterPredicate(c);
  }
  union {
    const LChar* characters8;
    const UChar* characters16;
  } data_;
  union {
    const LChar* characters8;
    const UChar* characters16;
  } end_;
  bool is_8bit_;

  DISALLOW_COPY_AND_ASSIGN(VTTScanner);
};

inline wtf_size_t VTTScanner::Run::length() const {
  if (is_8bit_)
    return static_cast<wtf_size_t>(end_ - start_);
  return static_cast<wtf_size_t>(reinterpret_cast<const UChar*>(end_) -
                                 reinterpret_cast<const UChar*>(start_));
}

template <unsigned charactersCount>
inline bool VTTScanner::Scan(const char (&characters)[charactersCount]) {
  return Scan(reinterpret_cast<const LChar*>(characters), charactersCount - 1);
}

template <bool characterPredicate(UChar)>
inline void VTTScanner::SkipWhile() {
  if (is_8bit_)
    WTF::SkipWhile<LChar, LCharPredicateAdapter<characterPredicate>>(
        data_.characters8, end_.characters8);
  else
    WTF::SkipWhile<UChar, characterPredicate>(data_.characters16,
                                              end_.characters16);
}

template <bool characterPredicate(UChar)>
inline void VTTScanner::SkipUntil() {
  if (is_8bit_)
    WTF::SkipUntil<LChar, LCharPredicateAdapter<characterPredicate>>(
        data_.characters8, end_.characters8);
  else
    WTF::SkipUntil<UChar, characterPredicate>(data_.characters16,
                                              end_.characters16);
}

template <bool characterPredicate(UChar)>
inline VTTScanner::Run VTTScanner::CollectWhile() {
  if (is_8bit_) {
    const LChar* current = data_.characters8;
    WTF::SkipWhile<LChar, LCharPredicateAdapter<characterPredicate>>(
        current, end_.characters8);
    return Run(GetPosition(), current, is_8bit_);
  }
  const UChar* current = data_.characters16;
  WTF::SkipWhile<UChar, characterPredicate>(current, end_.characters16);
  return Run(GetPosition(), reinterpret_cast<Position>(current), is_8bit_);
}

template <bool characterPredicate(UChar)>
inline VTTScanner::Run VTTScanner::CollectUntil() {
  if (is_8bit_) {
    const LChar* current = data_.characters8;
    WTF::SkipUntil<LChar, LCharPredicateAdapter<characterPredicate>>(
        current, end_.characters8);
    return Run(GetPosition(), current, is_8bit_);
  }
  const UChar* current = data_.characters16;
  WTF::SkipUntil<UChar, characterPredicate>(current, end_.characters16);
  return Run(GetPosition(), reinterpret_cast<Position>(current), is_8bit_);
}

inline void VTTScanner::SeekTo(Position position) {
  DCHECK_LE(position, end());
  data_.characters8 = position;
}

inline UChar VTTScanner::CurrentChar() const {
  DCHECK_LT(GetPosition(), end());
  return is_8bit_ ? *data_.characters8 : *data_.characters16;
}

inline void VTTScanner::Advance(unsigned amount) {
  DCHECK_LT(GetPosition(), end());
  if (is_8bit_)
    data_.characters8 += amount;
  else
    data_.characters16 += amount;
}

}  // namespace blink

#endif
