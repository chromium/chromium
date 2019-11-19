/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_CACHING_WORD_SHAPE_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_CACHING_WORD_SHAPE_ITERATOR_H_

#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/shaping/caching_word_shape_iterator.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_cache.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

class PLATFORM_EXPORT CachingWordShapeIterator final {
  STACK_ALLOCATED();

 public:
  CachingWordShapeIterator(ShapeCache* cache,
                           const TextRun& run,
                           const Font* font)
      : shape_cache_(cache),
        text_run_(run),
        font_(font),
        spacing_(run),
        width_so_far_(0),
        start_index_(0) {
    DCHECK(font);

    // Shaping word by word is faster as each word is cached. If we cannot
    // use the cache or if the font doesn't support word by word shaping
    // fall back on shaping the entire run.
    shape_by_word_ = font_->CanShapeWordByWord();

    // SVG sets SpacingDisabled because it handles spacing by themselves.
    if (!run.SpacingDisabled())
      spacing_.SetSpacingAndExpansion(font->GetFontDescription());
  }

  bool Next(scoped_refptr<const ShapeResult>* word_result) {
    if (UNLIKELY(text_run_.AllowTabs()))
      return NextForAllowTabs(word_result);

    if (!shape_by_word_) {
      if (start_index_)
        return false;
      *word_result = ShapeWord(text_run_, font_);
      start_index_ = 1;
      return word_result->get();
    }

    return NextWord(word_result);
  }

 private:
  scoped_refptr<const ShapeResult>
  ShapeWordWithoutSpacing(const TextRun&, const Font*);

  scoped_refptr<const ShapeResult> ShapeWord(const TextRun&,
                                             const Font*);

  bool NextWord(scoped_refptr<const ShapeResult>* word_result) {
    return ShapeToEndIndex(word_result, NextWordEndIndex());
  }

  static bool IsWordDelimiter(UChar ch) {
    return ch == kSpaceCharacter || ch == kTabulationCharacter;
  }

  unsigned NextWordEndIndex() const {
    const unsigned length = text_run_.length();
    if (start_index_ >= length)
      return 0;

    if (start_index_ + 1u == length || IsWordDelimiter(text_run_[start_index_]))
      return start_index_ + 1;

    // 8Bit words end at isWordDelimiter().
    if (text_run_.Is8Bit()) {
      for (unsigned i = start_index_ + 1;; i++) {
        if (i == length || IsWordDelimiter(text_run_[i]))
          return i;
      }
    }

    // Non-CJK/Emoji words end at isWordDelimiter() or CJK/Emoji characters.
    unsigned end = start_index_;
    UChar32 ch = text_run_.CodepointAtAndNext(end);
    if (!Character::IsCJKIdeographOrSymbol(ch)) {
      for (unsigned next_end = end; end < length; end = next_end) {
        ch = text_run_.CodepointAtAndNext(next_end);
        if (IsWordDelimiter(ch) || Character::IsCJKIdeographOrSymbolBase(ch))
          return end;
      }
      return length;
    }

    // For CJK/Emoji words, delimit every character because these scripts do
    // not delimit words by spaces, and delimiting only at isWordDelimiter()
    // worsen the cache efficiency.
    bool has_any_script = !Character::IsCommonOrInheritedScript(ch);
    for (unsigned next_end = end; end < length; end = next_end) {
      ch = text_run_.CodepointAtAndNext(next_end);
      // ZWJ and modifier check in order not to split those Emoji sequences.
      if (U_GET_GC_MASK(ch) & (U_GC_M_MASK | U_GC_LM_MASK | U_GC_SK_MASK) ||
          ch == kZeroWidthJoinerCharacter || Character::IsModifier(ch) ||
          Character::IsEmojiTagSequence(ch) || ch == kCancelTag)
        continue;
      // Avoid delimiting COMMON/INHERITED alone, which makes harder to
      // identify the script.
      if (Character::IsCJKIdeographOrSymbol(ch)) {
        if (Character::IsCommonOrInheritedScript(ch))
          continue;
        if (!has_any_script) {
          has_any_script = true;
          continue;
        }
      }
      return end;
    }
    return length;
  }

  bool ShapeToEndIndex(scoped_refptr<const ShapeResult>* result,
                       unsigned end_index) {
    if (!end_index || end_index <= start_index_)
      return false;

    const unsigned length = text_run_.length();
    if (!start_index_ && end_index == length) {
      *result = ShapeWord(text_run_, font_);
    } else {
      DCHECK_LE(end_index, length);
      TextRun sub_run =
          text_run_.SubRun(start_index_, end_index - start_index_);
      *result = ShapeWord(sub_run, font_);
    }
    start_index_ = end_index;
    return result->get();
  }

  unsigned EndIndexUntil(UChar ch) const {
    unsigned length = text_run_.length();
    DCHECK_LT(start_index_, length);
    for (unsigned i = start_index_ + 1;; i++) {
      if (i == length || text_run_[i] == ch)
        return i;
    }
  }

  bool NextForAllowTabs(scoped_refptr<const ShapeResult>* word_result) {
    unsigned length = text_run_.length();
    if (start_index_ >= length)
      return false;

    if (UNLIKELY(text_run_[start_index_] == kTabulationCharacter)) {
      for (unsigned i = start_index_ + 1;; i++) {
        if (i == length || text_run_[i] != kTabulationCharacter) {
          *word_result = ShapeResult::CreateForTabulationCharacters(
              font_, text_run_, width_so_far_, i - start_index_);
          start_index_ = i;
          break;
        }
      }
    } else if (!shape_by_word_) {
      if (!ShapeToEndIndex(word_result, EndIndexUntil(kTabulationCharacter)))
        return false;
    } else {
      if (!NextWord(word_result))
        return false;
    }
    DCHECK(*word_result);
    width_so_far_ += (*word_result)->Width();
    return true;
  }

  ShapeCache* shape_cache_;
  const TextRun& text_run_;
  const Font* font_;
  ShapeResultSpacing<TextRun> spacing_;
  float width_so_far_;  // Used only when allowTabs()
  unsigned start_index_ : 31;
  unsigned shape_by_word_ : 1;

  DISALLOW_COPY_AND_ASSIGN(CachingWordShapeIterator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_CACHING_WORD_SHAPE_ITERATOR_H_
