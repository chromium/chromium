// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_PROCESS_BOUND_STRING_H_
#define CRYPTO_PROCESS_BOUND_STRING_H_

#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "crypto/crypto_export.h"
#include "crypto/features.h"

namespace crypto {

namespace internal {

// Maybe round the size of the data to a size needed for the encrypt or decrypt
// operation. Returns the new size, or `size` if no rounding up is needed.
CRYPTO_EXPORT size_t MaybeRoundUp(size_t size);

// Maybe encrypt a buffer, in place. Returns true if the buffer was successfully
// encrypted or false if unsupported by the platform or failed to encrypt.
CRYPTO_EXPORT bool MaybeEncryptBuffer(base::span<uint8_t> buffer);

// Maybe decrypt a buffer, in place. Returns true if the buffer was successfully
// decrypted or false if unsupported by the platform or failed to decrypt.
CRYPTO_EXPORT bool MaybeDecryptBuffer(base::span<uint8_t> buffer);

// Securely zero a buffer using a platform specific method.
CRYPTO_EXPORT void SecureZeroBuffer(base::span<uint8_t> buffer);

}  // namespace internal

// SecureAllocator is used by the SecureString variants below to clear the
// memory when the string moves out of scope.
template <typename T>
struct CRYPTO_EXPORT SecureAllocator {
  using value_type = T;

  SecureAllocator() noexcept = default;

  T* allocate(std::size_t n) { return std::allocator<T>().allocate(n); }

  void deallocate(T* p, std::size_t n) noexcept {
    if (p) {
      // SAFETY: deallocate() has a fixed prototype from the std library, and
      // passes an unsafe buffer, so convert it to a base::span here.
      internal::SecureZeroBuffer(UNSAFE_BUFFERS(
          base::span<uint8_t>(reinterpret_cast<uint8_t*>(p), n * sizeof(T))));
      std::allocator<T>().deallocate(p, n);
    }
  }
};

// On supported platforms, a process bound string cannot have its content read
// by other processes on the system. On unsupported platforms it provides no
// difference over a native string except it does more copies.
template <typename StringType>
class CRYPTO_EXPORT ProcessBound {
 public:
  using CharType = typename StringType::value_type;

  ProcessBound(const ProcessBound& other) = default;
  ProcessBound(ProcessBound&& other) = default;
  ProcessBound& operator=(const ProcessBound& other) = default;
  ProcessBound& operator=(ProcessBound&& other) = default;

  // Create a process bound string. Takes a copy of the string passed in.
  explicit ProcessBound(const StringType& value)
      : original_size_(value.size()) {
    std::vector<CharType> data(value.begin(), value.end());
    if (base::FeatureList::IsEnabled(
            crypto::features::kProcessBoundStringEncryption)) {
      data.resize(internal::MaybeRoundUp(data.size()));
      encrypted_ =
          internal::MaybeEncryptBuffer(base::as_writable_byte_span(data));
    }
    maybe_encrypted_data_ = std::move(data);
  }

  ~ProcessBound() = default;

  // Return the decrypted string.
  StringType value() const { return StringType(secure_value()); }

  // Return the decrypted string as a string that attempts to wipe itself after
  // use. Prefer over calling `value()` if caller can support it.
  std::basic_string<CharType,
                    std::char_traits<CharType>,
                    SecureAllocator<CharType>>
  secure_value() const {
    if (!encrypted_) {
      return std::basic_string<CharType, std::char_traits<CharType>,
                               SecureAllocator<CharType>>(
          maybe_encrypted_data_.data(), original_size_);
    }

    // Copy to decrypt in-place.
    std::basic_string<CharType, std::char_traits<CharType>,
                      SecureAllocator<CharType>>
        decrypted(maybe_encrypted_data_.begin(), maybe_encrypted_data_.end());
    // Attempt to avoid Small String Optimization (SSO) by reserving a larger
    // allocation than the SSO default, forcing a dynamic allocation to occur,
    // before any decrypted data is written to the string. This value was
    // determined empirically.
    constexpr size_t kSSOMaxSize = 64u;
    if (decrypted.size() < kSSOMaxSize) {
      decrypted.reserve(kSSOMaxSize);
    }
    CHECK(internal::MaybeDecryptBuffer(base::as_writable_byte_span(decrypted)));
    decrypted.resize(original_size_);
    return decrypted;
  }

  size_t size() const { return original_size_; }
  bool empty() const { return size() == 0; }

 private:
  FRIEND_TEST_ALL_PREFIXES(ProcessBoundFeatureTest, Encryption);
  std::vector<CharType> maybe_encrypted_data_;
  size_t original_size_;
  bool encrypted_ = false;
};

using ProcessBoundString = ProcessBound<std::string>;
using ProcessBoundWString = ProcessBound<std::wstring>;
using ProcessBoundU16String = ProcessBound<std::u16string>;

// SecureString variants here attempt to clean memory for the string data when
// the string goes out of scope. However, while in memory it can be read, and if
// copied somewhere else, the memory can also be read. This is a defense in
// depth hardening and not meant to provide strong security guarantees.
using SecureString =
    std::basic_string<std::string::value_type,
                      std::char_traits<std::string::value_type>,
                      SecureAllocator<std::string::value_type>>;
using SecureWString =
    std::basic_string<std::wstring::value_type,
                      std::char_traits<std::wstring::value_type>,
                      SecureAllocator<std::wstring::value_type>>;
using SecureU16String =
    std::basic_string<std::u16string::value_type,
                      std::char_traits<std::u16string::value_type>,
                      SecureAllocator<std::u16string::value_type>>;

}  // namespace crypto

#endif  // CRYPTO_PROCESS_BOUND_STRING_H_
