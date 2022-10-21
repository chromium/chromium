// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_CODE_POINT_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_CODE_POINT_ITERATOR_H_

#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace WTF {

// An implementation of StringView::begin() and end().
// An instance must not outlive the target StringView.
class CodePointIterator {
  STACK_ALLOCATED();

 public:
  explicit CodePointIterator(StringView target, unsigned index)
      : target_(target), index_(index) {}

  UChar32 operator*() { return target_.CodepointAt(index_); }

  void operator++() { index_ = target_.NextCodePointOffset(index_); }

  bool operator==(const CodePointIterator& other) const {
    return target_.Bytes() == other.target_.Bytes() &&
           target_.length() == other.target_.length() && index_ == other.index_;
  }

  bool operator!=(const CodePointIterator& other) const {
    return !(*this == other);
  }

 private:
  const StringView target_;
  unsigned index_;
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_CODE_POINT_ITERATOR_H_
