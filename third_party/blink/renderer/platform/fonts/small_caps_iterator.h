// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SMALL_CAPS_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SMALL_CAPS_ITERATOR_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/platform/fonts/font_orientation.h"
#include "third_party/blink/renderer/platform/fonts/script_run_iterator.h"
#include "third_party/blink/renderer/platform/fonts/utf16_text_iterator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class PLATFORM_EXPORT SmallCapsIterator {
  USING_FAST_MALLOC(SmallCapsIterator);

 public:
  enum SmallCapsBehavior {
    kSmallCapsSameCase,
    kSmallCapsUppercaseNeeded,
    kSmallCapsInvalid
  };

  SmallCapsIterator(const UChar* buffer, unsigned buffer_size);

  bool Consume(unsigned* caps_limit, SmallCapsBehavior*);

 private:
  std::unique_ptr<UTF16TextIterator> utf16_iterator_;
  unsigned buffer_size_;
  UChar32 next_u_char32_;
  bool at_end_;

  SmallCapsBehavior current_small_caps_behavior_;
  SmallCapsBehavior previous_small_caps_behavior_;

  DISALLOW_COPY_AND_ASSIGN(SmallCapsIterator);
};

}  // namespace blink

#endif
