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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TRACK_VTT_VTT_SCANNER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TRACK_VTT_VTT_SCANNER_H_

#include "base/check_op.h"
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
  VTTScanner(const VTTScanner&) = delete;
  VTTScanner& operator=(const VTTScanner&) = delete;

  // Return the number of remaining characters.
  size_t Remaining() const {
    return is_8bit_
               ? static_cast<size_t>(end_.characters8 - data_.characters8)
               : static_cast<size_t>(end_.characters16 - data_.characters16);
  }
  // Check if the input pointer points at the end of the input.
  bool IsAtEnd() const { return GetPosition() == end(); }
  // Match the character |c| against the character at the input pointer
  // (~lookahead).
  bool Match(char c) const { return !IsAtEnd() && CurrentChar() == c; }
  // Scan the character |c|.
  bool Scan(char);
  // Scan the string |str|.
  bool Scan(StringView str);

  // Return the count of characters for which the specified
  // |characterPredicate| returns true. The start of the run will be the
  // current input pointer.
  template <bool predicate(UChar)>
  size_t CountWhile();

  // Like CountWhile, but using a negated predicate.
  template <bool predicate(UChar)>
  size_t CountUntil();

  // Skip (advance the input pointer) as long as the specified
  // |characterPredicate| returns true, and the input pointer is not passed
  // the end of the input.
  template <bool predicate(UChar)>
  void SkipWhile();

  // Like SkipWhile, but using a negated predicate.
  template <bool predicate(UChar)>
  void SkipUntil();

  // Return a scanner with a buffer containing the run of characters for which
  // the specified `predicate` returns true. The characters will be consumed in
  // this scanner.
  template <bool predicate(UChar)>
  VTTScanner SubrangeWhile();

  // Like SubrangeWhile, but using a negated predicate.
  template <bool predicate(UChar)>
  VTTScanner SubrangeUntil();

  // Return the String made up of the first `length` characters, and advance the
  // input pointer accordingly.
  String ExtractString(size_t length);

  // Return a String constructed from the rest of the input (between input
  // pointer and end of input), and advance the input pointer accordingly.
  String RestOfInputAsString();

  // Scan a set of ASCII digits from the input. Return the number of digits
  // scanned, and set |number| to the computed value. If the digits make up a
  // number that does not fit the 'unsigned' type, |number| is set to UINT_MAX.
  // Note: Does not handle sign.
  size_t ScanDigits(unsigned& number);

  // Scan a floating point value on one of the forms: \d+\.? \d+\.\d+ \.\d+
  bool ScanDouble(double& number);

  // Scan a floating point value (per ScanDouble) followed by a '%'.
  bool ScanPercentage(double& percentage);

 protected:
  typedef const LChar* Position;

  VTTScanner(Position start, Position end, bool is_8bit) : is_8bit_(is_8bit) {
    data_.characters8 = start;
    end_.characters8 = end;
  }

  Position GetPosition() const { return data_.characters8; }
  Position end() const { return end_.characters8; }
  UChar CurrentChar() const;
  void Advance(unsigned amount = 1);
  void AdvanceIfNonZero(unsigned amount);
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
};

template <bool predicate(UChar)>
inline size_t VTTScanner::CountWhile() {
  if (is_8bit_) {
    const LChar* current = data_.characters8;
    WTF::SkipWhile<LChar, LCharPredicateAdapter<predicate>>(current,
                                                            end_.characters8);
    return current - data_.characters8;
  }
  const UChar* current = data_.characters16;
  WTF::SkipWhile<UChar, predicate>(current, end_.characters16);
  return current - data_.characters16;
}

template <bool predicate(UChar)>
inline size_t VTTScanner::CountUntil() {
  if (is_8bit_) {
    const LChar* current = data_.characters8;
    WTF::SkipUntil<LChar, LCharPredicateAdapter<predicate>>(current,
                                                            end_.characters8);
    return current - data_.characters8;
  }
  const UChar* current = data_.characters16;
  WTF::SkipUntil<UChar, predicate>(current, end_.characters16);
  return current - data_.characters16;
}

template <bool predicate(UChar)>
inline void VTTScanner::SkipWhile() {
  AdvanceIfNonZero(CountWhile<predicate>());
}

template <bool predicate(UChar)>
inline void VTTScanner::SkipUntil() {
  AdvanceIfNonZero(CountUntil<predicate>());
}

template <bool predicate(UChar)>
inline VTTScanner VTTScanner::SubrangeWhile() {
  const size_t count = CountWhile<predicate>();
  const Position start = GetPosition();
  AdvanceIfNonZero(count);
  return VTTScanner(start, GetPosition(), is_8bit_);
}

template <bool predicate(UChar)>
inline VTTScanner VTTScanner::SubrangeUntil() {
  const size_t count = CountUntil<predicate>();
  const Position start = GetPosition();
  AdvanceIfNonZero(count);
  return VTTScanner(start, GetPosition(), is_8bit_);
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

inline void VTTScanner::AdvanceIfNonZero(unsigned amount) {
  if (amount) {
    Advance(amount);
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TRACK_VTT_VTT_SCANNER_H_
