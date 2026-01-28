// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_CHUNKED_ENCRYPTOR_H_
#define SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_CHUNKED_ENCRYPTOR_H_

#include <array>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/types/expected.h"
#include "crypto/aead.h"
#include "crypto/process_bound_string.h"

namespace network::enterprise_encryption {

// Default key and derived key size, nonce length, and tag length in
// BoringSSL's implementation of AES-256-GCM-SIV.
inline constexpr size_t kKeySize = 32u;
inline constexpr size_t kNonceSize = 12u;
inline constexpr size_t kAuthTagSize = 16u;

// Nonce prefix and salt size.
// The nonce is constructed as:
// [ NoncePrefix (7 bytes) ] [ BlockIndex (4 bytes, Big Endian) ] [ Flag (1
// byte) ] Note: Only the `NoncePrefix` is generated randomly and saved to the
// file. `BlockIndex` and `Flag` are derived from the file offset during
// read/write and don't need to be saved.
//
// This follows the construction used in Tink's AES-GCM-HKDF-Streaming:
// https://developers.google.com/tink/streaming-aead/aes_gcm_hkdf_streaming
inline constexpr size_t kNoncePrefixSize = 7u;
inline constexpr size_t kSaltSize = kKeySize;

// Header size:
// [ Size (1 byte) ] [ Salt (32 bytes) ] [ NoncePrefix (7 bytes) ]
// Size byte ensures forward compatibility if the header format changes in the
// future, and sums up to 40 bytes for alignment.
inline constexpr size_t kHeaderSize = 1u + kSaltSize + kNoncePrefixSize;
static_assert(kHeaderSize <= 255, "Header size must fit in a single byte");

// Data size per file chunk.
// Chosen to match the typical 4KB page size on most systems.
inline constexpr size_t kChunkDataSize = 4096u;

// Total size of an encrypted chunk (Data + Tag).
inline constexpr size_t kEncryptedChunkSize = kChunkDataSize + kAuthTagSize;

struct COMPONENT_EXPORT(NETWORK_SERVICE) EncryptionContext {
  EncryptionContext(const EncryptionContext&) = delete;
  EncryptionContext& operator=(const EncryptionContext&) = delete;
  EncryptionContext(EncryptionContext&&);
  EncryptionContext& operator=(EncryptionContext&&);
  ~EncryptionContext();
  EncryptionContext(base::span<const uint8_t, kKeySize> key,
                    base::span<const uint8_t, kNoncePrefixSize> nonce_prefix);

  crypto::ProcessBoundString derived_key;
  std::array<uint8_t, kNoncePrefixSize> nonce_prefix;
};

// Error types for encryption/decryption operations. Don't remove or reorder
// existing values.
// LINT.IfChange(EnterpriseDiskCacheError)
enum class EncryptionError {
  kSuccess = 0,
  kInvalidKey = 1,        // Key is invalid/empty.
  kInvalidHeader = 2,     // Header is malformed or size mismatch.
  kDecryptionFailed = 3,  // AEAD Open failed (tag mismatch) or data too short.
  kMaxValue = kDecryptionFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:EnterpriseDiskCacheError)

// Creates a new header (containing random salt/nonce prefix) and derives the
// key. Returns the header bytes and the derived encryption context on success.
COMPONENT_EXPORT(NETWORK_SERVICE)
base::expected<std::pair<std::vector<uint8_t>, EncryptionContext>,
               EncryptionError>
CreateHeader(base::span<const uint8_t> key_value);

// Parses a header and derives the key.
COMPONENT_EXPORT(NETWORK_SERVICE)
base::expected<EncryptionContext, EncryptionError> ParseHeader(
    base::span<const uint8_t> header,
    base::span<const uint8_t> key_value);

// Handles chunk-based encryption/decryption using AES-GCM-SIV.
// This class holds the AEAD context and is designed for efficient reuse
// across multiple chunks of the same file.
class COMPONENT_EXPORT(NETWORK_SERVICE) ChunkedEncryptor {
 public:
  explicit ChunkedEncryptor(const EncryptionContext& encryption_context);
  ChunkedEncryptor(const ChunkedEncryptor&) = delete;
  ChunkedEncryptor& operator=(const ChunkedEncryptor&) = delete;
  ~ChunkedEncryptor();

  // Encrypts a single chunk.
  // |chunk_index| is used for nonce calculation.
  // |is_last_chunk| sets the flag in the nonce.
  std::vector<uint8_t> EncryptChunk(base::span<const uint8_t> plaintext,
                                    uint32_t chunk_index,
                                    bool is_last_chunk);

  // Decrypts a single chunk.
  base::expected<std::vector<uint8_t>, EncryptionError> DecryptChunk(
      base::span<const uint8_t> ciphertext,
      uint32_t chunk_index,
      bool is_last_chunk);

  // Helper to create nonce. Exposed for testing.
  std::array<uint8_t, kNonceSize> CreateNonce(uint32_t chunk_index,
                                              bool is_last_chunk) const;

 private:
  std::array<uint8_t, kNoncePrefixSize> nonce_prefix_;
  crypto::Aead aead_;
};

}  // namespace network::enterprise_encryption

#endif  // SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_CHUNKED_ENCRYPTOR_H_
