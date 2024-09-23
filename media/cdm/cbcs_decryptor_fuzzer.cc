// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cbcs_decryptor.h"

#include <stdint.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "crypto/symmetric_key.h"
#include "media/base/decoder_buffer.h"
#include "media/base/encryption_pattern.h"
#include "media/base/subsample_entry.h"

const std::array<uint8_t, 16> kKey = {0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
                                      0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                                      0x10, 0x11, 0x12, 0x13};

const std::array<uint8_t, 16> kIv = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
                                     0x26, 0x27, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00};

// For disabling noisy logging.
struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOGGING_FATAL); }
};

Environment* env = new Environment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data_ptr, size_t size) {
  // SAFETY: LibFuzzer must pass a valid `data_ptr` and `size`.
  auto data = UNSAFE_BUFFERS(base::span(data_ptr, size));

  // From the data provided:
  // 1) Use the first byte to determine how much of the buffer is "clear".
  // 2) Use the second byte to determine the pattern.
  // 3) Rest of the buffer is the input data (which must be at least 1 byte).
  // So the input buffer needs at least 3 bytes.
  if (data.size() < 3)
    return 0;

  const uint8_t clear_bytes = data[0];
  const uint8_t encryption_pattern = data[1];
  data = data.subspan(2);

  static std::unique_ptr<crypto::SymmetricKey> key =
      crypto::SymmetricKey::Import(
          crypto::SymmetricKey::AES,
          std::string(std::begin(kKey), std::end(kKey)));

  // |clear_bytes| is used to determine how much of the buffer is "clear".
  // Since the code checks SubsampleEntries, use |clear_bytes| as the actual
  // number of bytes clear, and the rest as encrypted. To avoid size_t problems,
  // only set |subsamples| if |clear_bytes| <= |size|. If |subsamples| is
  // empty, the complete buffer is treated as encrypted.
  std::vector<media::SubsampleEntry> subsamples;
  if (clear_bytes <= size) {
    subsamples.push_back(
        {clear_bytes, static_cast<uint32_t>(size - clear_bytes)});
  }

  // |encryption_pattern| is used to determine the encryption pattern. Since
  // |crypt_byte_block| must be > 0, use 1 for it. |skip_byte_block| can be 0.
  // This will try patterns (1,0), (1,1), ... (1,9), which should be sufficient.
  media::EncryptionPattern pattern(1, encryption_pattern % 10);

  auto encrypted_buffer = media::DecoderBuffer::CopyFrom(data);

  // Key_ID is never used.
  encrypted_buffer->set_decrypt_config(media::DecryptConfig::CreateCbcsConfig(
      "key_id", std::string(std::begin(kIv), std::end(kIv)), subsamples,
      pattern));

  media::DecryptCbcsBuffer(*encrypted_buffer, *key);
  return 0;
}
