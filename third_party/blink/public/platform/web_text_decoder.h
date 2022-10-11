// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_TEXT_DECODER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_TEXT_DECODER_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"

namespace blink {

class BLINK_PLATFORM_EXPORT WebTextDecoder {
 public:
  struct EncodingData {
    WebString encoding;
    bool was_detected_heuristically = false;
    bool saw_decoding_error = false;
  };

  virtual ~WebTextDecoder() = default;

  virtual WebString Decode(const char* data, size_t length) = 0;
  virtual WebString Flush() = 0;
  virtual EncodingData GetEncodingData() const = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_TEXT_DECODER_H_
