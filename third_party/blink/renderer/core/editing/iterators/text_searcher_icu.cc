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

#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/text/text_boundaries.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator_internal_icu.h"
#include "third_party/blink/renderer/platform/text/unicode_utilities.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/utf16.h"
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
  DCHECK(U_SUCCESS(status)) << status;
  return searcher;
}

class SearcherFactory {
  STACK_ALLOCATED();

 public:
  SearcherFactory(const SearcherFactory&) = delete;
  SearcherFactory& operator=(const SearcherFactory&) = delete;

  // Returns the global instance. If this is called again before calling
  // ReleaseSearcher(), this function crashes.
  static UStringSearch* AcquireSearcher() {
    Instance().Lock();
    return Instance().searcher_;
  }
  // Creates a normal instance. We may create instances multiple times with
  // this function.  A returned pointer should be destructed by
  // ReleaseSearcher().
  static UStringSearch* CreateLocal() { return CreateSearcher(); }

  static void ReleaseSearcher(UStringSearch* searcher) {
    if (searcher == Instance().searcher_) {
      // Leave the static object pointing to valid strings (pattern=target,
      // text=buffer). Otherwise, usearch_reset() will results in
      // 'use-after-free' error.
      UErrorCode status = U_ZERO_ERROR;
      usearch_setPattern(searcher, &kNewlineCharacter, 1, &status);
      DCHECK(U_SUCCESS(status));
      usearch_setText(searcher, &kNewlineCharacter, 1, &status);
      DCHECK(U_SUCCESS(status));
      Instance().Unlock();
    } else {
      usearch_close(searcher);
    }
  }

 private:
  static SearcherFactory& Instance() {
    static SearcherFactory factory(CreateSearcher());
    return factory;
  }

  explicit SearcherFactory(UStringSearch* searcher) : searcher_(searcher) {}

  void Lock() {
#if DCHECK_IS_ON()
    DCHECK(!locked_);
    locked_ = true;
#endif
  }

  void Unlock() {
#if DCHECK_IS_ON()
    DCHECK(locked_);
    locked_ = false;
#endif
  }

  UStringSearch* const searcher_ = nullptr;

#if DCHECK_IS_ON()
  bool locked_ = false;
#endif
};

}  // namespace

static bool IsWholeWordMatch(base::span<const UChar> text,
                             const MatchResultICU& result) {
  const wtf_size_t result_end = result.start + result.length;
  DCHECK_LE(result_end, text.size());
  UChar32 first_character = CodePointAt(text, result.start);

  // Chinese and Japanese lack word boundary marks, and there is no clear
  // agreement on what constitutes a word, so treat the position before any CJK
  // character as a word start.
  if (Character::IsCJKIdeographOrSymbol(first_character))
    return true;

  wtf_size_t word_break_search_start = result_end;
  while (word_break_search_start > result.start) {
    word_break_search_start =
        FindNextWordBackward(text, word_break_search_start);
  }
  if (word_break_search_start != result.start)
    return false;
  return result_end == static_cast<wtf_size_t>(
                           FindWordEndBoundary(text, word_break_search_start));
}

// Grab the single global searcher.
TextSearcherICU::TextSearcherICU()
    : searcher_(SearcherFactory::AcquireSearcher()) {}

TextSearcherICU::TextSearcherICU(ConstructLocalTag)
    : searcher_(SearcherFactory::CreateLocal()) {}

TextSearcherICU::~TextSearcherICU() {
  SearcherFactory::ReleaseSearcher(searcher_);
}

void TextSearcherICU::SetPattern(const StringView& pattern,
                                 FindOptions options) {
  DCHECK_GT(pattern.length(), 0u);
  options_ = options;
  SetCaseSensitivity(!options.IsCaseInsensitive());
  SetPattern(pattern.Span16());
  if (ContainsKanaLetters(pattern.ToString())) {
    normalized_search_text_ = NormalizeCharactersIntoNfc(pattern.Span16());
  }
}

void TextSearcherICU::SetText(base::span<const UChar> text) {
  UErrorCode status = U_ZERO_ERROR;
  usearch_setText(searcher_, text.data(), text.size(), &status);
  DCHECK_EQ(status, U_ZERO_ERROR);
  text_length_ = text.size();
}

void TextSearcherICU::SetOffset(wtf_size_t offset) {
  UErrorCode status = U_ZERO_ERROR;
  usearch_setOffset(searcher_, offset, &status);
  DCHECK_EQ(status, U_ZERO_ERROR);
}

std::optional<MatchResultICU> TextSearcherICU::NextMatchResult() {
  while (std::optional<MatchResultICU> result = NextMatchResultInternal()) {
    if (!ShouldSkipCurrentMatch(*result)) {
      return result;
    }
  }
  return std::nullopt;
}

std::optional<MatchResultICU> TextSearcherICU::NextMatchResultInternal() {
  UErrorCode status = U_ZERO_ERROR;
  const int match_start = usearch_next(searcher_, &status);
  DCHECK(U_SUCCESS(status));

  // TODO(iceman): It is possible to use |usearch_getText| function
  // to retrieve text length and not store it explicitly.
  if (!(match_start >= 0 &&
        static_cast<wtf_size_t>(match_start) < text_length_)) {
    DCHECK_EQ(match_start, USEARCH_DONE);
    return std::nullopt;
  }

  MatchResultICU result = {
      static_cast<wtf_size_t>(match_start),
      base::checked_cast<wtf_size_t>(usearch_getMatchedLength(searcher_))};
  // Might be possible to get zero-length result with some Unicode characters
  // that shouldn't actually match but is matched by ICU such as \u0080.
  if (result.length == 0u) {
    return std::nullopt;
  }
  return result;
}

bool TextSearcherICU::ShouldSkipCurrentMatch(
    const MatchResultICU& result) const {
  int32_t text_length_i32;
  const UChar* text = usearch_getText(searcher_, &text_length_i32);
  unsigned text_length = text_length_i32;
  DCHECK_LE(result.start + result.length, text_length);
  DCHECK_GT(result.length, 0u);
  // SAFETY: Making a span same as the SetText() argument.
  auto text_span = UNSAFE_BUFFERS(base::span<const UChar>(text, text_length));

  if (!normalized_search_text_.empty() &&
      !IsCorrectKanaMatch(text_span, result)) {
    return true;
  }

  return options_.IsWholeWord() && !IsWholeWordMatch(text_span, result);
}

bool TextSearcherICU::IsCorrectKanaMatch(base::span<const UChar> text,
                                         const MatchResultICU& result) const {
  Vector<UChar> normalized_match =
      NormalizeCharactersIntoNfc(text.subspan(result.start, result.length));
  return CheckOnlyKanaLettersInStrings(base::span(normalized_search_text_),
                                       base::span(normalized_match));
}

void TextSearcherICU::SetPattern(base::span<const UChar> pattern) {
  UErrorCode status = U_ZERO_ERROR;
  usearch_setPattern(searcher_, pattern.data(), pattern.size(), &status);
  DCHECK(U_SUCCESS(status));
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
