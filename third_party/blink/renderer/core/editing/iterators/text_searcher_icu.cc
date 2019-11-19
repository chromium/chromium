/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/iterators/text_searcher_icu.h"

#include <unicode/usearch.h>
#include "base/macros.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/text/text_boundaries.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator_internal_icu.h"
#include "third_party/blink/renderer/platform/text/unicode_utilities.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

UStringSearch* CreateSearcher() {
  // Provide a non-empty pattern and non-empty text so usearch_open will not
  // fail, but it doesn't matter exactly what it is, since we don't perform any
  // searches without setting both the pattern and the text.
  UErrorCode status = U_ZERO_ERROR;
  String search_collator_name =
      CurrentSearchLocaleID() + String("@collation=search");
  UStringSearch* searcher =
      usearch_open(&kNewlineCharacter, 1, &kNewlineCharacter, 1,
                   search_collator_name.Utf8().c_str(), nullptr, &status);
  DCHECK(status == U_ZERO_ERROR || status == U_USING_FALLBACK_WARNING ||
         status == U_USING_DEFAULT_WARNING)
      << status;
  return searcher;
}

class ICULockableSearcher {
  STACK_ALLOCATED();

 public:
  static UStringSearch* AcquireSearcher() {
    Instance().lock();
    return Instance().searcher_;
  }

  static void ReleaseSearcher() { Instance().unlock(); }

 private:
  static ICULockableSearcher& Instance() {
    static ICULockableSearcher searcher(CreateSearcher());
    return searcher;
  }

  explicit ICULockableSearcher(UStringSearch* searcher) : searcher_(searcher) {}

  void lock() {
#if DCHECK_IS_ON()
    DCHECK(!locked_);
    locked_ = true;
#endif
  }

  void unlock() {
#if DCHECK_IS_ON()
    DCHECK(locked_);
    locked_ = false;
#endif
  }

  UStringSearch* const searcher_ = nullptr;

#if DCHECK_IS_ON()
  bool locked_ = false;
#endif

  DISALLOW_COPY_AND_ASSIGN(ICULockableSearcher);
};

}  // namespace

static bool IsWholeWordMatch(const UChar* text,
                             int text_length,
                             MatchResultICU& result) {
  DCHECK_LE((int)(result.start + result.length), text_length);
  UChar32 first_character;
  U16_GET(text, 0, result.start, result.length, first_character);

  // Chinese and Japanese lack word boundary marks, and there is no clear
  // agreement on what constitutes a word, so treat the position before any CJK
  // character as a word start.
  if (Character::IsCJKIdeographOrSymbol(first_character))
    return true;

  wtf_size_t word_break_search_start = result.start + result.length;
  while (word_break_search_start > result.start) {
    word_break_search_start =
        FindNextWordBackward(text, text_length, word_break_search_start);
  }
  if (word_break_search_start != result.start)
    return false;
  return static_cast<int>(result.start + result.length) ==
         FindWordEndBoundary(text, text_length, word_break_search_start);
}

// Grab the single global searcher.
// If we ever have a reason to do more than once search buffer at once, we'll
// have to move to multiple searchers.
TextSearcherICU::TextSearcherICU()
    : searcher_(ICULockableSearcher::AcquireSearcher()) {}

TextSearcherICU::~TextSearcherICU() {
  // Leave the static object pointing to valid strings (pattern=target,
  // text=buffer). Otheriwse, usearch_reset() will results in 'use-after-free'
  // error.
  SetPattern(&kNewlineCharacter, 1);
  SetText(&kNewlineCharacter, 1);
  ICULockableSearcher::ReleaseSearcher();
}

void TextSearcherICU::SetPattern(const StringView& pattern,
                                 FindOptions options) {
  DCHECK_GT(pattern.length(), 0u);
  options_ = options;
  SetCaseSensitivity(!(options & kCaseInsensitive));
  SetPattern(pattern.Characters16(), pattern.length());
  if (ContainsKanaLetters(pattern.ToString())) {
    NormalizeCharactersIntoNFCForm(pattern.Characters16(), pattern.length(),
                                   normalized_search_text_);
  }
}

void TextSearcherICU::SetText(const UChar* text, wtf_size_t length) {
  UErrorCode status = U_ZERO_ERROR;
  usearch_setText(searcher_, text, length, &status);
  DCHECK_EQ(status, U_ZERO_ERROR);
  text_length_ = length;
}

void TextSearcherICU::SetOffset(wtf_size_t offset) {
  UErrorCode status = U_ZERO_ERROR;
  usearch_setOffset(searcher_, offset, &status);
  DCHECK_EQ(status, U_ZERO_ERROR);
}

bool TextSearcherICU::NextMatchResult(MatchResultICU& result) {
  while (NextMatchResultInternal(result)) {
    if (!ShouldSkipCurrentMatch(result))
      return true;
  }
  return false;
}

bool TextSearcherICU::NextMatchResultInternal(MatchResultICU& result) {
  UErrorCode status = U_ZERO_ERROR;
  const int match_start = usearch_next(searcher_, &status);
  DCHECK_EQ(status, U_ZERO_ERROR);

  // TODO(iceman): It is possible to use |usearch_getText| function
  // to retrieve text length and not store it explicitly.
  if (!(match_start >= 0 &&
        static_cast<wtf_size_t>(match_start) < text_length_)) {
    DCHECK_EQ(match_start, USEARCH_DONE);
    result.start = 0;
    result.length = 0;
    return false;
  }

  result.start = static_cast<wtf_size_t>(match_start);
  result.length = usearch_getMatchedLength(searcher_);
  // Might be possible to get zero-length result with some Unicode characters
  // that shouldn't actually match but is matched by ICU such as \u0080.
  if (result.length == 0u) {
    result.start = 0;
    return false;
  }
  return true;
}

bool TextSearcherICU::ShouldSkipCurrentMatch(MatchResultICU& result) const {
  int32_t text_length;
  const UChar* text = usearch_getText(searcher_, &text_length);
  DCHECK_LE((int32_t)(result.start + result.length), text_length);
  DCHECK_GT(result.length, 0u);

  if (!normalized_search_text_.IsEmpty() && !IsCorrectKanaMatch(text, result))
    return true;

  if ((options_ & kWholeWord) && !IsWholeWordMatch(text, text_length, result))
    return true;
  return false;
}

bool TextSearcherICU::IsCorrectKanaMatch(const UChar* text,
                                         MatchResultICU& result) const {
  Vector<UChar> normalized_match;
  NormalizeCharactersIntoNFCForm(text + result.start, result.length,
                                 normalized_match);
  return CheckOnlyKanaLettersInStrings(
      normalized_search_text_.data(), normalized_search_text_.size(),
      normalized_match.begin(), normalized_match.size());
}

void TextSearcherICU::SetPattern(const UChar* pattern, wtf_size_t length) {
  UErrorCode status = U_ZERO_ERROR;
  usearch_setPattern(searcher_, pattern, length, &status);
  DCHECK_EQ(status, U_ZERO_ERROR);
}

void TextSearcherICU::SetCaseSensitivity(bool case_sensitive) {
  const UCollationStrength strength =
      case_sensitive ? UCOL_TERTIARY : UCOL_PRIMARY;

  UCollator* const collator = usearch_getCollator(searcher_);
  if (ucol_getStrength(collator) == strength)
    return;

  ucol_setStrength(collator, strength);
  usearch_reset(searcher_);
}

}  // namespace blink
