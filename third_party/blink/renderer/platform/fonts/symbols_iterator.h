// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SYMBOLS_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SYMBOLS_ITERATOR_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"
#include "third_party/blink/renderer/platform/fonts/utf16_ragel_iterator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class PLATFORM_EXPORT SymbolsIterator {
  USING_FAST_MALLOC(SymbolsIterator);

 public:
  SymbolsIterator(const UChar* buffer, unsigned buffer_size);

  bool Consume(unsigned* symbols_limit, FontFallbackPriority*);

 private:
  UTF16RagelIterator buffer_iterator_;
  unsigned cursor_;

  unsigned next_token_end_;
  bool next_token_emoji_;

  DISALLOW_COPY_AND_ASSIGN(SymbolsIterator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SYMBOLS_ITERATOR_H_
