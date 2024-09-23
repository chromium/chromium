// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <array>
#include <memory>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "components/webcrypto/algorithms/aes.h"
#include "components/webcrypto/algorithms/util.h"
#include "components/webcrypto/blink_key_handle.h"
#include "components/webcrypto/status.h"
#include "crypto/openssl_util.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/cipher.h"

namespace webcrypto {

namespace {

const EVP_CIPHER* GetAESCipherByKeyLength(size_t key_length_bytes) {
  // 192-bit AES is intentionally unsupported (http://crbug.com/533699).
  switch (key_length_bytes) {
    case 16:
      return EVP_aes_128_ctr();
    case 32:
      return EVP_aes_256_ctr();
    default:
      return nullptr;
  }
}

// Encrypts/decrypts given a 128-bit counter.
//
// |output| must have the same length as |input|.
Status AesCtrEncrypt128BitCounter(const EVP_CIPHER* cipher,
                                  base::span<const uint8_t> raw_key,
                                  base::span<const uint8_t> input,
                                  base::span<const uint8_t, 16> counter,
                                  base::span<uint8_t> output) {
  DCHECK(cipher);
  DCHECK_EQ(input.size(), output.size());

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  bssl::ScopedEVP_CIPHER_CTX context;
  if (!EVP_CipherInit_ex(context.get(), cipher, nullptr, raw_key.data(),
                         counter.data(), ENCRYPT)) {
    return Status::OperationError();
  }

  int output_len = 0;
  if (!EVP_CipherUpdate(context.get(), output.data(), &output_len, input.data(),
                        base::checked_cast<int>(input.size()))) {
    return Status::OperationError();
  }
  int final_output_chunk_len = 0;
  if (!EVP_CipherFinal_ex(context.get(), output.data() + output_len,
                          &final_output_chunk_len)) {
    return Status::OperationError();
  }

  output_len += final_output_chunk_len;
  if (static_cast<size_t>(output_len) != input.size())
    return Status::ErrorUnexpected();

  return Status::Success();
}

// Returns ceil(a/b), where a and b are integers.
template <typename T>
T CeilDiv(T a, T b) {
  return a == 0 ? 0 : 1 + (a - 1) / b;
}

// Extracts the counter as a `absl::uint128`. The counter is the rightmost
// `counter_length_bits` of the block, interpreted as a big-endian number.
absl::uint128 GetCounter(base::span<const uint8_t, 16> counter_block,
                         unsigned int counter_length_bits) {
  unsigned int counter_length_remainder_bits = counter_length_bits % 8;
  unsigned int byte_length = CeilDiv(counter_length_bits, 8u);
  DCHECK_GT(byte_length, 0u);

  base::span<const uint8_t> suffix = counter_block.last(byte_length);
  absl::uint128 ret = suffix[0];
  // The first byte may be partial.
  if (counter_length_remainder_bits != 0) {
    ret &= ~(0xFF << counter_length_remainder_bits);
  }
  for (uint8_t b : suffix.subspan(1)) {
    ret = (ret << 8) | b;
  }
  return ret;
}

// Returns a counter block with the counter bits all set all zero.
std::array<uint8_t, AES_BLOCK_SIZE> BlockWithZeroedCounter(
    base::span<const uint8_t, AES_BLOCK_SIZE> counter_block,
    unsigned int counter_length_bits) {
  unsigned int counter_length_bytes = counter_length_bits / 8;
  unsigned int counter_length_bits_remainder = counter_length_bits % 8;

  std::array<uint8_t, AES_BLOCK_SIZE> new_counter_block;
  memcpy(new_counter_block.data(), counter_block.data(), AES_BLOCK_SIZE);

  size_t index = new_counter_block.size() - counter_length_bytes;
  memset(&new_counter_block.front() + index, 0, counter_length_bytes);

  if (counter_length_bits_remainder) {
    new_counter_block[index - 1] &= 0xFF << counter_length_bits_remainder;
  }

  return new_counter_block;
}

// This function does encryption/decryption for AES-CTR (encryption and
// decryption are the same).
//
// BoringSSL's interface for AES-CTR differs from that of WebCrypto. In
// WebCrypto the caller specifies a 16-byte counter block and designates how
// many of the right-most X bits to use as a big-endian counter. Whereas in
// BoringSSL the entire counter block is interpreted as a 128-bit counter.
//
// In AES-CTR, the counter block MUST be unique across all messages that are
// encrypted/decrypted. WebCrypto expects that the counter can start at any
// value, and is therefore permitted to wrap around to zero on overflow.
//
// Some care is taken to fail if the counter wraps back to an earlier value.
// However this protection is only enforced during a *single* call to
// encrypt/decrypt.
Status AesCtrEncryptDecrypt(const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& key,
                            base::span<const uint8_t> data,
                            std::vector<uint8_t>* buffer) {
  const blink::WebCryptoAesCtrParams* params = algorithm.AesCtrParams();
  const std::vector<uint8_t>& raw_key = GetSymmetricKeyData(key);

  if (params->Counter().size() != AES_BLOCK_SIZE)
    return Status::ErrorIncorrectSizeAesCtrCounter();
  base::span<const uint8_t, AES_BLOCK_SIZE> counter_block(
      params->Counter().data(), params->Counter().size());

  unsigned int counter_length_bits = params->LengthBits();
  if (counter_length_bits < 1 || counter_length_bits > 128)
    return Status::ErrorInvalidAesCtrCounterLength();

  // The output of AES-CTR is the same size as the input. However BoringSSL
  // expects buffer sizes as an "int".
  base::CheckedNumeric<int> output_max_len = data.size();
  if (!output_max_len.IsValid())
    return Status::ErrorDataTooLarge();

  const EVP_CIPHER* const cipher = GetAESCipherByKeyLength(raw_key.size());
  if (!cipher)
    return Status::ErrorUnexpected();

  buffer->resize(base::ValueOrDieForType<size_t>(output_max_len));
  absl::uint128 current_counter =
      GetCounter(counter_block, counter_length_bits);

  if (counter_length_bits == 128) {
    return AesCtrEncrypt128BitCounter(cipher, raw_key, data, counter_block,
                                      *buffer);
  }

  // The total number of possible counter values is pow(2, counter_length_bits)
  absl::uint128 num_counter_values = absl::uint128(1) << counter_length_bits;

  // The number of AES blocks needed for encryption/decryption. The counter is
  // incremented this many times.
  size_t num_output_blocks = CeilDiv(buffer->size(), size_t{AES_BLOCK_SIZE});

  // If the counter is going to be incremented more times than there are counter
  // values, fail. (Repeating values of the counter block is bad).
  if (num_output_blocks > num_counter_values)
    return Status::ErrorAesCtrInputTooLongCounterRepeated();

  // This is the number of blocks that can be successfully encrypted without
  // overflowing the counter. Encrypting the subsequent block will need to
  // reset the counter to zero.
  absl::uint128 num_blocks_until_reset = num_counter_values - current_counter;

  // If the counter can be incremented for the entire input without
  // wrapping-around, do it as a single call into BoringSSL.
  if (num_blocks_until_reset >= num_output_blocks) {
    return AesCtrEncrypt128BitCounter(cipher, raw_key, data, counter_block,
                                      *buffer);
  }

  // Otherwise the encryption needs to be done in 2 parts. The first part using
  // the current counter_block, and the next part resetting the counter portion
  // of the block to zero.

  // This is guaranteed to fit in an `size_t` because it is bounded by the input
  // size.
  size_t input_size_part1 =
      static_cast<size_t>(num_blocks_until_reset * AES_BLOCK_SIZE);
  DCHECK_LT(input_size_part1, data.size());
  base::span<uint8_t> output_part1 =
      base::make_span(*buffer).first(input_size_part1);
  base::span<uint8_t> output_part2 =
      base::make_span(*buffer).subspan(input_size_part1);

  // Encrypt the first part (before wrap-around).
  Status status =
      AesCtrEncrypt128BitCounter(cipher, raw_key, data.first(input_size_part1),
                                 counter_block, output_part1);
  if (status.IsError())
    return status;

  // Encrypt the second part (after wrap-around).
  std::array<uint8_t, AES_BLOCK_SIZE> counter_block_part2 =
      BlockWithZeroedCounter(counter_block, counter_length_bits);

  return AesCtrEncrypt128BitCounter(cipher, raw_key,
                                    data.subspan(input_size_part1),
                                    counter_block_part2, output_part2);
}

class AesCtrImplementation : public AesAlgorithm {
 public:
  AesCtrImplementation() : AesAlgorithm("CTR") {}

  Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& key,
                 base::span<const uint8_t> data,
                 std::vector<uint8_t>* buffer) const override {
    return AesCtrEncryptDecrypt(algorithm, key, data, buffer);
  }

  Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& key,
                 base::span<const uint8_t> data,
                 std::vector<uint8_t>* buffer) const override {
    return AesCtrEncryptDecrypt(algorithm, key, data, buffer);
  }
};

}  // namespace

std::unique_ptr<AlgorithmImplementation> CreateAesCtrImplementation() {
  return std::make_unique<AesCtrImplementation>();
}

}  // namespace webcrypto
