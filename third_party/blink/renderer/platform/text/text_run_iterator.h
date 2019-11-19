// Copyright (C) 2013 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_TEXT_RUN_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_TEXT_RUN_ITERATOR_H_

#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class TextRunIterator {
  DISALLOW_NEW();

 public:
  TextRunIterator() : text_run_(nullptr), offset_(0), length_(0) {}

  TextRunIterator(const TextRun* text_run, unsigned offset)
      : text_run_(text_run), offset_(offset), length_(text_run_->length()) {}

  TextRunIterator(const TextRunIterator& other)
      : text_run_(other.text_run_),
        offset_(other.offset_),
        length_(text_run_->length()) {}

  unsigned Offset() const { return offset_; }
  void Increment() { offset_++; }
  bool AtEnd() const { return offset_ >= length_; }
  UChar Current() const { return (*text_run_)[offset_]; }
  WTF::unicode::CharDirection Direction() const {
    return AtEnd() ? WTF::unicode::kOtherNeutral
                   : WTF::unicode::Direction(Current());
  }
  bool AtParagraphSeparator() const { return Current() == '\n'; }

  bool operator==(const TextRunIterator& other) {
    return offset_ == other.offset_ && text_run_ == other.text_run_;
  }

  bool operator!=(const TextRunIterator& other) { return !operator==(other); }

 private:
  const TextRun* text_run_;
  unsigned offset_;
  unsigned length_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_TEXT_RUN_ITERATOR_H_
