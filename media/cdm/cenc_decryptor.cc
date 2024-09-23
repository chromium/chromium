// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/cdm/cenc_decryptor.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/subsample_entry.h"

namespace media {

namespace {

enum ClearBytesBufferSel { kSrcContainsClearBytes, kDstContainsClearBytes };

// Copy the cypher bytes as specified by |subsamples| from |src| to |dst|.
// If |sel| == kSrcContainsClearBytes, then |src| is expected to contain any
// clear bytes specified by |subsamples| and will be skipped. This is used
// when copying all the protected data out of a sample. If |sel| ==
// kDstContainsClearBytes, then any clear bytes mentioned in |subsamples|
// will be skipped in |dst|. This is used when copying the decrypted bytes
// back into the buffer, replacing the encrypted portions.
//
// TODO(crbug.com/40284755): This function is not bounds-safe. Make this
// span-based instead.
void CopySubsamples(const std::vector<SubsampleEntry>& subsamples,
                    const ClearBytesBufferSel sel,
                    const uint8_t* src,
                    uint8_t* dst) {
  for (size_t i = 0; i < subsamples.size(); i++) {
    const SubsampleEntry& subsample = subsamples[i];
    if (sel == kSrcContainsClearBytes) {
      src += subsample.clear_bytes;
    } else {
      dst += subsample.clear_bytes;
    }
    memcpy(dst, src, subsample.cypher_bytes);
    src += subsample.cypher_bytes;
    dst += subsample.cypher_bytes;
  }
}

// TODO(crbug.com/40575437): This should be done in DecoderBuffer so that
// additional fields are more easily handled.
void CopyExtraSettings(const DecoderBuffer& input, DecoderBuffer* output) {
  output->set_timestamp(input.timestamp());
  output->set_duration(input.duration());
  output->set_is_key_frame(input.is_key_frame());
  output->set_side_data(input.side_data());
}

}  // namespace

scoped_refptr<DecoderBuffer> DecryptCencBuffer(
    const DecoderBuffer& input,
    const crypto::SymmetricKey& key) {
  base::span<const uint8_t> sample = input;
  DCHECK(!sample.empty()) << "No data to decrypt.";

  const DecryptConfig* decrypt_config = input.decrypt_config();
  DCHECK(decrypt_config) << "No need to call Decrypt() on unencrypted buffer.";
  DCHECK_EQ(EncryptionScheme::kCenc, decrypt_config->encryption_scheme());

  const std::string& iv = decrypt_config->iv();
  DCHECK_EQ(iv.size(), static_cast<size_t>(DecryptConfig::kDecryptionKeySize));

  crypto::Encryptor encryptor;
  if (!encryptor.Init(&key, crypto::Encryptor::CTR, "")) {
    DVLOG(1) << "Could not initialize decryptor.";
    return nullptr;
  }

  if (!encryptor.SetCounter(iv)) {
    DVLOG(1) << "Could not set counter block.";
    return nullptr;
  }

  const std::vector<SubsampleEntry>& subsamples = decrypt_config->subsamples();
  if (subsamples.empty()) {
    std::string decrypted_text;
    std::string_view encrypted_text = base::as_string_view(sample);
    if (!encryptor.Decrypt(encrypted_text, &decrypted_text)) {
      DVLOG(1) << "Could not decrypt data.";
      return nullptr;
    }

    // TODO(xhwang): Find a way to avoid this data copy.
    auto output = DecoderBuffer::CopyFrom(base::as_byte_span(decrypted_text));
    CopyExtraSettings(input, output.get());
    return output;
  }

  if (!VerifySubsamplesMatchSize(subsamples, sample.size())) {
    DVLOG(1) << "Subsample sizes do not equal input size";
    return nullptr;
  }

  // Compute the size of the encrypted portion. Overflow, etc. checked by
  // the call to VerifySubsamplesMatchSize().
  size_t total_encrypted_size = 0;
  for (const auto& subsample : subsamples)
    total_encrypted_size += subsample.cypher_bytes;

  // No need to decrypt if there is no encrypted data.
  if (total_encrypted_size == 0) {
    auto output = DecoderBuffer::CopyFrom(sample);
    CopyExtraSettings(input, output.get());
    return output;
  }

  // The encrypted portions of all subsamples must form a contiguous block,
  // such that an encrypted subsample that ends away from a block boundary is
  // immediately followed by the start of the next encrypted subsample. We
  // copy all encrypted subsamples to a contiguous buffer, decrypt them, then
  // copy the decrypted bytes over the encrypted bytes in the output.
  // TODO(strobe): attempt to reduce number of memory copies
  auto encrypted_bytes = base::HeapArray<uint8_t>::Uninit(total_encrypted_size);
  CopySubsamples(subsamples, kSrcContainsClearBytes, sample.data(),
                 encrypted_bytes.data());

  std::string_view encrypted_text = base::as_string_view(encrypted_bytes);
  std::string decrypted_text;
  if (!encryptor.Decrypt(encrypted_text, &decrypted_text)) {
    DVLOG(1) << "Could not decrypt data.";
    return nullptr;
  }
  DCHECK_EQ(decrypted_text.size(), encrypted_text.size());

  scoped_refptr<DecoderBuffer> output = DecoderBuffer::CopyFrom(sample);
  CopySubsamples(subsamples, kDstContainsClearBytes,
                 reinterpret_cast<const uint8_t*>(decrypted_text.data()),
                 output->writable_data());
  CopyExtraSettings(input, output.get());
  return output;
}

}  // namespace media
