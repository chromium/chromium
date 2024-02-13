// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/utf16_ragel_iterator.h"

#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/text/emoji_segmentation_category_inline_header.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

UTF16RagelIterator& UTF16RagelIterator::SetCursor(unsigned new_cursor) {
  CHECK_GE(new_cursor, 0u);
  CHECK_LT(new_cursor, buffer_size_);
  cursor_ = new_cursor;
  UpdateCachedCategory();
  return *this;
}

void UTF16RagelIterator::UpdateCachedCategory() {
  if (cursor_ >= buffer_size_)
    return;
  cached_category_ = GetEmojiSegmentationCategory(Codepoint());
}

}  // namespace blink
