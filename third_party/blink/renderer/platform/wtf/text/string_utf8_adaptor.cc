// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

StringUtf8Adaptor::StringUtf8Adaptor(StringView string,
                                     Utf8ConversionMode mode) {
  if (string.empty())
    return;
  // Unfortunately, 8 bit WTFStrings are encoded in Latin-1 and GURL uses
  // UTF-8 when processing 8 bit strings. If |relative| is entirely ASCII, we
  // luck out and can avoid mallocing a new buffer to hold the UTF-8 data
  // because UTF-8 and Latin-1 use the same code units for ASCII code points.
  if (string.Is8Bit() && string.ContainsOnlyASCIIOrEmpty()) {
    span_ = base::as_chars(string.Span8());
  } else {
    utf8_buffer_ = string.Utf8(mode);
    span_ = base::span(utf8_buffer_);
  }
}

StringUtf8Adaptor::~StringUtf8Adaptor() = default;

}  // namespace blink
