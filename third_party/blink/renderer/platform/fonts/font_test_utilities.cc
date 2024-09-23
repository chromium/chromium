// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_test_utilities.h"

namespace blink {

String To16Bit(std::string_view text) {
  String s = String::FromUTF8(text);
  s.Ensure16Bit();
  return s;
}

}  // namespace blink
