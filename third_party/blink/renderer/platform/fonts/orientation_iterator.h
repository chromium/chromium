// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_ORIENTATION_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_ORIENTATION_ITERATOR_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/platform/fonts/font_orientation.h"
#include "third_party/blink/renderer/platform/fonts/script_run_iterator.h"
#include "third_party/blink/renderer/platform/fonts/utf16_text_iterator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class PLATFORM_EXPORT OrientationIterator {
  USING_FAST_MALLOC(OrientationIterator);

 public:
  enum RenderOrientation {
    kOrientationKeep,
    kOrientationRotateSideways,
    kOrientationInvalid
  };

  OrientationIterator(const UChar* buffer,
                      unsigned buffer_size,
                      FontOrientation run_orientation);

  bool Consume(unsigned* orientation_limit, RenderOrientation*);

 private:
  std::unique_ptr<UTF16TextIterator> utf16_iterator_;
  unsigned buffer_size_;
  bool at_end_;

  DISALLOW_COPY_AND_ASSIGN(OrientationIterator);
};

}  // namespace blink

#endif
