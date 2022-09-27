// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CRYPTO_CRYPTO_UTILITIES_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CRYPTO_CRYPTO_UTILITIES_H_

#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"

namespace blink {
inline WebVector<uint8_t> CopyBytes(const DOMArrayPiece& source) {
  return WebVector<uint8_t>(static_cast<const uint8_t*>(source.Data()),
                            source.ByteLength());
}
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CRYPTO_CRYPTO_UTILITIES_H_
