// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_OS_CRYPT_CACHE_ENCRYPTION_DELEGATE_H_
#define SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_OS_CRYPT_CACHE_ENCRYPTION_DELEGATE_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/cache_encryption_delegate.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/mojom/cache_encryption_provider.mojom.h"

namespace enterprise_encryption {

// Implements the net::CacheEncryptionDelegate interface using OSCrypt for
// encryption.
//
// This class can be constructed on any thread, but all methods other than the
// constructor must be called on the same sequence. The `SEQUENCE_CHECKER`
// enforces this. Initialization is performed asynchronously via a Mojo remote.
class COMPONENT_EXPORT(NETWORK_SERVICE) OSCryptCacheEncryptionDelegate
    : public net::CacheEncryptionDelegate {
 public:
  explicit OSCryptCacheEncryptionDelegate(
      mojo::PendingRemote<network::mojom::CacheEncryptionProvider> provider);

  OSCryptCacheEncryptionDelegate(const OSCryptCacheEncryptionDelegate&) =
      delete;
  OSCryptCacheEncryptionDelegate& operator=(
      const OSCryptCacheEncryptionDelegate&) = delete;

  ~OSCryptCacheEncryptionDelegate() override;

  // net::CacheEncryptionDelegate implementation:
  void Init(base::OnceCallback<void(net::Error)> callback) override;

  // Encrypts the given `plaintext` using OSCrypt. The resulting `ciphertext`
  // contains all necessary information for decryption, including the
  // initialization vector (IV), which is handled automatically by the
  // underlying cryptographic library.
  bool EncryptData(base::span<const uint8_t> plaintext,
                   std::vector<uint8_t>* ciphertext) override;

  // Decrypts the given `ciphertext` using OSCrypt. The implementation expects
  // the `ciphertext` to contain the initialization vector (IV) and uses it to
  // correctly decrypt the data.
  bool DecryptData(base::span<const uint8_t> ciphertext,
                   std::vector<uint8_t>* plaintext) override;

 private:
  void InitCallback(os_crypt_async::Encryptor encryptor);

  void OnDisconnect();

  enum class State {
    kUninitialized,
    kInitializing,
    kInitialized,
  };

  std::optional<os_crypt_async::Encryptor> instance_;
  mojo::PendingRemote<network::mojom::CacheEncryptionProvider> provider_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Remote<network::mojom::CacheEncryptionProvider> remote_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::OnceCallbackList<void(net::Error)> callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);
  State state_ GUARDED_BY_CONTEXT(sequence_checker_) = State::kUninitialized;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OSCryptCacheEncryptionDelegate> weak_ptr_factory_{this};
};
}  // namespace enterprise_encryption

#endif  // SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_OS_CRYPT_CACHE_ENCRYPTION_DELEGATE_H_
