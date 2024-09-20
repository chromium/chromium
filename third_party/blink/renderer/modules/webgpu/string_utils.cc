// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/texture_utils.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

WTF::String StringFromASCIIAndUTF8(std::string_view message) {
  return WTF::String::FromUTF8WithLatin1Fallback(message);
}

std::string UTF8StringFromUSVStringWithNullReplacedByReplacementCodePoint(
    const String& s) {
  constexpr UChar kNullCodePoint = 0x0;
  constexpr UChar kReplacementCodePoint = 0xFFFD;

  WTF::String temp(s);
  return temp.Replace(kNullCodePoint, kReplacementCodePoint).Utf8();
}

}  // namespace blink
