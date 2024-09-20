// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_BODY_TEXT_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_BODY_TEXT_DECODER_H_

#include "base/containers/span.h"
#include "third_party/blink/public/platform/web_encoding_data.h"

namespace WTF {
class String;
}  // namespace WTF

namespace blink {

class BodyTextDecoder {
 public:
  virtual ~BodyTextDecoder() = default;

  virtual WTF::String Decode(base::span<const char> data) = 0;
  virtual WTF::String Flush() = 0;
  virtual WebEncodingData GetEncodingData() const = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_BODY_TEXT_DECODER_H_
