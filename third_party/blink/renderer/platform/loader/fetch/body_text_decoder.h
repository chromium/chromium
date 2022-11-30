// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_BODY_TEXT_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_BODY_TEXT_DECODER_H_

#include "third_party/blink/public/platform/web_encoding_data.h"

namespace blink {

class BodyTextDecoder {
 public:
  virtual ~BodyTextDecoder() = default;

  virtual String Decode(const char* data, size_t length) = 0;
  virtual String Flush() = 0;
  virtual WebEncodingData GetEncodingData() const = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_BODY_TEXT_DECODER_H_
