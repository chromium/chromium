// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cenc_decryptor.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "crypto/aes_ctr.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/subsample_entry.h"

namespace media {

namespace {

constexpr size_t kRequiredKeyBytes = 16;

enum ClearBytesBufferSel { kSrcContainsClearBytes, kDstContainsClearBytes };

// Copy the cypher bytes as specified by |subsamples| from |src| to |dst|.
// If |sel| == kSrcContainsClearBytes, then |src| is expected to contain any
// clear bytes specified by |subsamples| and will be skipped. This is used
// when copying all the protected data out of a sample. If |sel| ==
// kDstContainsClearBytes, then any clear bytes mentioned in |subsamples|
// will be skipped in |dst|. This is used when copying the decrypted bytes
// back into the buffer, replacing the encrypted portions.
void CopySubsamples(const std::vector<SubsampleEntry>& subsamples,
                    const ClearBytesBufferSel sel,
                    base::span<const uint8_t> src,
                    base::span<uint8_t> dst) {
  size_t src_i = 0;
  size_t dst_i = 0;
  for (const auto& subsample : subsamples) {
    if (sel == kSrcContainsClearBytes) {
      src_i += subsample.clear_bytes;
    } else {
      dst_i += subsample.clear_bytes;
    }

    auto src_view = src.subspan(src_i, subsample.cypher_bytes);
    auto dst_view = dst.subspan(dst_i, subsample.cypher_bytes);
    dst_view.copy_from(src_view);
    src_i += subsample.cypher_bytes;
    dst_i += subsample.cypher_bytes;
  }
}

// TODO(crbug.com/40575437): This should be done in DecoderBuffer so that
// additional fields are more easily handled.
void CopyExtraSettings(const DecoderBuffer& input, DecoderBuffer* output) {
  output->set_timestamp(input.timestamp());
  output->set_duration(input.duration());
  output->set_is_key_frame(input.is_key_frame());
  if (input.side_data()) {
    output->set_side_data(input.side_data()->Clone());
  }
}

}  // namespace

scoped_refptr<DecoderBuffer> DecryptCencBuffer(const DecoderBuffer& input,
                                               base::span<const uint8_t> key) {
  base::span<const uint8_t> sample = input;
  CHECK(!sample.empty(), base::NotFatalUntil::M140) << "No data to decrypt.";

  const DecryptConfig* decrypt_config = input.decrypt_config();
  CHECK(decrypt_config, base::NotFatalUntil::M140)
      << "No need to call Decrypt() on unencrypted buffer.";
  DCHECK_EQ(EncryptionScheme::kCenc, decrypt_config->encryption_scheme());

  if (key.size() != kRequiredKeyBytes) {
    DVLOG(1) << "Supplied key is the wrong size for CENC";
    return nullptr;
  }

  auto iv = base::as_byte_span(decrypt_config->iv())
                .to_fixed_extent<crypto::aes_ctr::kCounterSize>();
  if (!iv) {
    DVLOG(1) << "Supplied IV is the wrong size for CENC";
    return nullptr;
  }

  const std::vector<SubsampleEntry>& subsamples = decrypt_config->subsamples();
  if (subsamples.empty()) {
    auto decrypted = base::HeapArray<uint8_t>::Uninit(sample.size());
    crypto::aes_ctr::Decrypt(key, *iv, sample, decrypted);
    auto output = DecoderBuffer::FromArray(std::move(decrypted));
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
  auto encrypted = base::HeapArray<uint8_t>::Uninit(total_encrypted_size);
  auto decrypted = base::HeapArray<uint8_t>::Uninit(total_encrypted_size);
  CopySubsamples(subsamples, kSrcContainsClearBytes, sample, encrypted);

  crypto::aes_ctr::Decrypt(key, *iv, encrypted, decrypted);

  scoped_refptr<DecoderBuffer> output = DecoderBuffer::CopyFrom(sample);
  CopySubsamples(subsamples, kDstContainsClearBytes, decrypted,
                 output->writable_span());
  CopyExtraSettings(input, output.get());
  return output;
}

}  // namespace media
