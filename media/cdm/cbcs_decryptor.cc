// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/cdm/cbcs_decryptor.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/checked_math.h"
#include "crypto/symmetric_key.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/encryption_pattern.h"
#include "media/base/subsample_entry.h"
#include "media/cdm/aes_cbc_crypto.h"

namespace media {

namespace {

constexpr size_t kAesBlockSizeInBytes = 16;

// Decrypts |input_data| into |output_data|, using the pattern specified in
// |pattern|. |pattern| only applies to full blocks. Any partial block at
// the end is considered unencrypted. |output_data| must have enough room to
// hold |input_data|.size() bytes.
bool DecryptWithPattern(const crypto::SymmetricKey& key,
                        base::span<const uint8_t> iv,
                        const EncryptionPattern& pattern,
                        base::span<const uint8_t> input_data,
                        uint8_t* output_data) {
  // The AES_CBC decryption is reset for each subsample.
  AesCbcCrypto aes_cbc_crypto;
  if (!aes_cbc_crypto.Initialize(key, iv))
    return false;

  // |total_blocks| is the number of blocks in the buffer, ignoring any
  // partial block at the end. |remaining_bytes| is the number of bytes
  // in the partial block at the end of the buffer, if any.
  size_t total_blocks = input_data.size_bytes() / kAesBlockSizeInBytes;
  size_t remaining_bytes = input_data.size_bytes() % kAesBlockSizeInBytes;

  size_t crypt_byte_block =
      base::strict_cast<size_t>(pattern.crypt_byte_block());
  size_t skip_byte_block = base::strict_cast<size_t>(pattern.skip_byte_block());

  // |crypt_byte_block| and |skip_byte_block| come from 4 bit values, so fail
  // if these are too large.
  if (crypt_byte_block >= 16 || skip_byte_block >= 16)
    return false;

  if (crypt_byte_block == 0 && skip_byte_block == 0) {
    // From ISO/IEC 23001-7:2016(E), section 9.6.1:
    // "When the fields default_crypt_byte_block and default_skip_byte_block
    // in a version 1 Track Encryption Box ('tenc') are non-zero numbers,
    // pattern encryption SHALL be applied."
    // So for the pattern 0:0, assume that all blocks are encrypted.
    crypt_byte_block = total_blocks;
  }

  // Apply the pattern to |input_data|.
  // Example (using Pattern(2,3), Ex is encrypted, Ux unencrypted)
  //   input_data:  |E1|E2|U3|U4|U5|E6|E7|U8|U9|U10|E11|
  // We must decrypt 2 blocks, then simply copy the next 3 blocks, and
  // repeat until the end. Note that the input does not have to contain
  // a full pattern or even |crypt_byte_block| blocks at the end.
  size_t blocks_processed = 0;
  const uint8_t* src = input_data.data();
  uint8_t* dest = output_data;
  bool is_encrypted_blocks = false;
  while (blocks_processed < total_blocks) {
    is_encrypted_blocks = !is_encrypted_blocks;
    size_t blocks_to_process =
        std::min(is_encrypted_blocks ? crypt_byte_block : skip_byte_block,
                 total_blocks - blocks_processed);

    if (blocks_to_process == 0)
      continue;

    size_t bytes_to_process = blocks_to_process * kAesBlockSizeInBytes;

    // From ISO/IEC 23001-7:2016(E), section 10.4.2:
    // For a typical pattern length of 10 (e.g. 1:9) "the pattern is repeated
    // every 160 bytes of the protected range, until the end of the range. If
    // the protected range of the slice body is not a multiple of the pattern
    // length (e.g. 160 bytes), then the pattern sequence applies to the
    // included whole 16-byte Blocks and a partial 16-byte Block that may
    // remain where the pattern is terminated by the byte length of the range
    // BytesOfProtectedData, is left unencrypted."
    if (is_encrypted_blocks) {
      if (!aes_cbc_crypto.Decrypt(base::make_span(src, bytes_to_process),
                                  dest)) {
        return false;
      }
    } else {
      memcpy(dest, src, bytes_to_process);
    }

    blocks_processed += blocks_to_process;
    src += bytes_to_process;
    dest += bytes_to_process;
  }

  // Any partial block data remaining in this subsample is considered
  // unencrypted so simply copy it into |dest|.
  if (remaining_bytes > 0)
    memcpy(dest, src, remaining_bytes);

  return true;
}

}  // namespace

scoped_refptr<DecoderBuffer> DecryptCbcsBuffer(
    const DecoderBuffer& input,
    const crypto::SymmetricKey& key) {
  const size_t sample_size = input.size();
  DCHECK(sample_size) << "No data to decrypt.";

  const DecryptConfig* decrypt_config = input.decrypt_config();
  DCHECK(decrypt_config) << "No need to call Decrypt() on unencrypted buffer.";
  DCHECK_EQ(EncryptionScheme::kCbcs, decrypt_config->encryption_scheme());

  DCHECK(decrypt_config->HasPattern());
  const EncryptionPattern pattern =
      decrypt_config->encryption_pattern().value();

  // Decrypted data will be the same size as |input| size.
  auto buffer = base::MakeRefCounted<DecoderBuffer>(sample_size);
  base::span<uint8_t> output_data = buffer->writable_span();
  buffer->set_timestamp(input.timestamp());
  buffer->set_duration(input.duration());
  buffer->set_is_key_frame(input.is_key_frame());
  buffer->set_side_data(input.side_data());

  const std::vector<SubsampleEntry>& subsamples = decrypt_config->subsamples();
  if (subsamples.empty()) {
    // Assume the whole buffer is encrypted.
    return DecryptWithPattern(key, base::as_byte_span(decrypt_config->iv()),
                              pattern, base::span(input), output_data.data())
               ? buffer
               : nullptr;
  }

  if (!VerifySubsamplesMatchSize(subsamples, sample_size)) {
    DVLOG(1) << "Subsample sizes do not equal input size";
    return nullptr;
  }

  auto src = base::span(input);
  auto dest = output_data;
  for (const auto& subsample : subsamples) {
    if (subsample.clear_bytes) {
      DVLOG(4) << "Copying clear_bytes: " << subsample.clear_bytes;
      auto [src_copy, src_rem] = src.split_at(subsample.clear_bytes);
      auto [dest_copy, dest_rem] = dest.split_at(subsample.clear_bytes);
      src = src_rem;
      dest = dest_rem;
      dest_copy.copy_from(src_copy);
    }

    if (subsample.cypher_bytes) {
      DVLOG(4) << "Processing cypher_bytes: " << subsample.cypher_bytes
               << ", pattern(" << pattern.crypt_byte_block() << ","
               << pattern.skip_byte_block() << ")";
      auto [src_cypher, src_rem] = src.split_at(subsample.cypher_bytes);
      auto [dest_cypher, dest_rem] = dest.split_at(subsample.cypher_bytes);
      src = src_rem;
      dest = dest_rem;
      if (!DecryptWithPattern(
              key, base::as_bytes(base::make_span(decrypt_config->iv())),
              pattern, src_cypher, dest_cypher.data())) {
        return nullptr;
      }
    }
  }

  return buffer;
}

}  // namespace media
