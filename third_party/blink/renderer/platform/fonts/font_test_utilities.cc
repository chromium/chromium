// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_test_utilities.h"

namespace blink {

String To16Bit(const char* text, unsigned length) {
  return String::Make16BitFrom8BitSource(reinterpret_cast<const LChar*>(text),
                                         length);
}

}  // namespace blink
