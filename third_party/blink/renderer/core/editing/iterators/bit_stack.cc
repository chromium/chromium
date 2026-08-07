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

#include "third_party/blink/renderer/core/editing/iterators/bit_stack.h"

namespace blink {
static const wtf_size_t kBitsInWord = sizeof(uint32_t) * 8;
static const wtf_size_t kBitInWordMask = kBitsInWord - 1;

BitStack::BitStack() = default;

BitStack::~BitStack() = default;

void BitStack::Push(bool bit) {
  wtf_size_t index = size_ / kBitsInWord;
  wtf_size_t shift = size_ & kBitInWordMask;
  if (!shift && index == words_.size()) {
    words_.Grow(index + 1);
    words_[index] = 0;
  }
  uint32_t& word = words_[index];
  uint32_t mask = 1U << shift;
  if (bit)
    word |= mask;
  else
    word &= ~mask;
  ++size_;
}

void BitStack::Pop() {
  if (size_)
    --size_;
}

bool BitStack::Top() const {
  if (!size_)
    return false;
  wtf_size_t shift = (size_ - 1) & kBitInWordMask;
  wtf_size_t index = (size_ - 1) / kBitsInWord;
  return words_[index] & (1U << shift);
}

wtf_size_t BitStack::size() const {
  return size_;
}

}  // namespace blink
