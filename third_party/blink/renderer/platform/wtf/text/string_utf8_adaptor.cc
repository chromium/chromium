// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace WTF {

StringUTF8Adaptor::StringUTF8Adaptor(StringView string,
                                     UTF8ConversionMode mode) {
  if (string.empty())
    return;
  // Unfortunately, 8 bit WTFStrings are encoded in Latin-1 and GURL uses
  // UTF-8 when processing 8 bit strings. If |relative| is entirely ASCII, we
  // luck out and can avoid mallocing a new buffer to hold the UTF-8 data
  // because UTF-8 and Latin-1 use the same code units for ASCII code points.
  if (string.Is8Bit() && string.ContainsOnlyASCIIOrEmpty()) {
    data_ = reinterpret_cast<const char*>(string.Characters8());
    size_ = string.length();
  } else {
    utf8_buffer_ = string.Utf8(mode);
    data_ = utf8_buffer_.c_str();
    size_ = utf8_buffer_.length();
  }
}

StringUTF8Adaptor::~StringUTF8Adaptor() = default;

}  // namespace WTF
