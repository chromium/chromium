// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_STRING_MESSAGE_CODEC_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_STRING_MESSAGE_CODEC_H_

#include <string>
#include <vector>
#include "base/containers/span.h"
#include "third_party/blink/public/common/common_export.h"

namespace blink {

// To support exposing HTML message ports to Java, it is necessary to be able
// to encode and decode message data using the same serialization format as V8.
// That format is an implementation detail of V8, but we cannot invoke V8 in
// the browser process. Rather than IPC over to the renderer process to execute
// the V8 serialization code, we duplicate some of the serialization logic
// (just for simple string messages) here. This is a trade-off between overall
// complexity / performance and code duplication. Fortunately, we only need to
// handle string messages and this serialization format is static, as it is a
// format we currently persist to disk via IndexedDB.

BLINK_COMMON_EXPORT std::vector<uint8_t> EncodeStringMessage(
    const std::u16string& data);

BLINK_COMMON_EXPORT bool DecodeStringMessage(
    base::span<const uint8_t> encoded_data,
    std::u16string* result);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_STRING_MESSAGE_CODEC_H_
