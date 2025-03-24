// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/test_helpers.h"

#include "base/containers/span.h"

namespace blink {

AllowSharedBufferSource* StringToBuffer(std::string_view data) {
  return MakeGarbageCollected<AllowSharedBufferSource>(
      DOMArrayBuffer::Create(base::as_byte_span(data)));
}

std::string BufferToString(const media::DecoderBuffer& buffer) {
  return std::string(reinterpret_cast<const char*>(buffer.data()),
                     buffer.size());
}

std::unique_ptr<media::DecryptConfig> CreateTestDecryptConfig(
    media::EncryptionScheme scheme,
    std::optional<media::EncryptionPattern> pattern) {
  constexpr const char kKeyId[] = "123";
  using std::string_literals::operator""s;
  const std::string kIV = "\x00\x02\x02\x04\x06 abc1234567"s;
  const std::vector<media::SubsampleEntry> kSubsamples = {
      {1, 2}, {2, 3}, {4, 5}};

  switch (scheme) {
    case media::EncryptionScheme::kUnencrypted:
      return nullptr;
    case media::EncryptionScheme::kCbcs:
      return media::DecryptConfig::CreateCbcsConfig(kKeyId, kIV, kSubsamples,
                                                    pattern);
    case media::EncryptionScheme::kCenc:
      return media::DecryptConfig::CreateCencConfig(kKeyId, kIV, kSubsamples);
  };
}

}  // namespace blink
