// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_UTF16_RAGEL_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_UTF16_RAGEL_ITERATOR_H_

#include <unicode/uchar.h>

#include "base/check_op.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// UTF16RagelIterator is set up on top of a UTF-16 UChar* buffer iterating over
// a Blink internal text string and as such is used as an adapter between Blink
// strings and the Ragel-based emoji scanner. It supports forwarding and
// reversing using arithmetic operators. Dereferencing the iterator means
// retrieving a character class as defined in the Ragel grammar of
// third-party/emoji-segmenter. The dereferenced character category is cached
// since Ragel dereferences multiple times without moving the iterator's cursor.
class PLATFORM_EXPORT UTF16RagelIterator {
  DISALLOW_NEW();

 public:
  UTF16RagelIterator() : buffer_(nullptr), buffer_size_(0), cursor_(0) {}

  UTF16RagelIterator(const UChar* buffer,
                     unsigned buffer_size,
                     unsigned cursor = 0)
      : buffer_(buffer),
        buffer_size_(buffer_size),
        cursor_(cursor),
        cached_category_(kMaxEmojiScannerCategory) {
    UpdateCachedCategory();
  }

  UTF16RagelIterator end() {
    UTF16RagelIterator ret = *this;
    ret.cursor_ = buffer_size_;
    return ret;
  }

  UTF16RagelIterator& SetCursor(unsigned new_cursor);

  unsigned Cursor() { return cursor_; }

  UTF16RagelIterator& operator+=(int v) {
    if (v > 0) {
      U16_FWD_N(buffer_, cursor_, buffer_size_, v);
    } else if (v < 0) {
      U16_BACK_N(buffer_, 0, cursor_, -v);
    }
    UpdateCachedCategory();
    return *this;
  }

  UTF16RagelIterator& operator-=(int v) { return *this += -v; }

  UTF16RagelIterator operator+(int v) {
    UTF16RagelIterator ret = *this;
    return ret += v;
  }

  UTF16RagelIterator operator-(int v) { return *this + -v; }

  int operator-(const UTF16RagelIterator& other) {
    DCHECK_EQ(buffer_, other.buffer_);
    return cursor_ - other.cursor_;
  }

  UTF16RagelIterator& operator++() {
    DCHECK_LT(cursor_, buffer_size_);
    U16_FWD_1(buffer_, cursor_, buffer_size_);
    UpdateCachedCategory();
    return *this;
  }

  UTF16RagelIterator& operator--() {
    DCHECK_GT(cursor_, 0u);
    U16_BACK_1(buffer_, 0, cursor_);
    UpdateCachedCategory();
    return *this;
  }

  UTF16RagelIterator operator++(int) {
    UTF16RagelIterator ret = *this;
    ++(*this);
    return ret;
  }

  UTF16RagelIterator operator--(int) {
    UTF16RagelIterator ret = *this;
    --(*this);
    return ret;
  }

  UTF16RagelIterator operator=(int v) {
    // We need this integer assignment operator because Ragel has initialization
    // code for assigning 0 to ts, te.
    DCHECK_EQ(v, 0);
    UTF16RagelIterator ret = *this;
    ret.cursor_ = v;
    return ret;
  }

  UChar32 operator*() {
    CHECK(buffer_size_);
    return cached_category_;
  }

  bool operator==(const UTF16RagelIterator& other) const {
    return buffer_ == other.buffer_ && buffer_size_ == other.buffer_size_ &&
           cursor_ == other.cursor_;
  }

  bool operator!=(const UTF16RagelIterator& other) const {
    return !(*this == other);
  }

  // Must match the categories defined in third-party/emoji-segmenter/.
  // TODO(drott): Add static asserts once emoji-segmenter is imported to
  // third-party.
  enum EmojiScannerCharacterClass {
    EMOJI = 0,
    EMOJI_TEXT_PRESENTATION = 1,
    EMOJI_EMOJI_PRESENTATION = 2,
    EMOJI_MODIFIER_BASE = 3,
    EMOJI_MODIFIER = 4,
    EMOJI_VS_BASE = 5,
    REGIONAL_INDICATOR = 6,
    KEYCAP_BASE = 7,
    COMBINING_ENCLOSING_KEYCAP = 8,
    COMBINING_ENCLOSING_CIRCLE_BACKSLASH = 9,
    ZWJ = 10,
    VS15 = 11,
    VS16 = 12,
    TAG_BASE = 13,
    TAG_SEQUENCE = 14,
    TAG_TERM = 15,
    kMaxEmojiScannerCategory = 16
  };

 private:
  UChar32 Codepoint() const {
    DCHECK_GT(buffer_size_, 0u);
    UChar32 output;
    U16_GET(buffer_, 0, cursor_, buffer_size_, output);
    return output;
  }

  void UpdateCachedCategory();

  const UChar* buffer_;
  unsigned buffer_size_;
  unsigned cursor_;
  unsigned char cached_category_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_UTF16_RAGEL_ITERATOR_H_
