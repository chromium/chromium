// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/enterprise/encryption/os_crypt_cache_encryption_delegate.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"

namespace enterprise_encryption {

OSCryptCacheEncryptionDelegate::OSCryptCacheEncryptionDelegate(
    mojo::PendingRemote<network::mojom::CacheEncryptionProvider> provider)
    : provider_(std::move(provider)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

OSCryptCacheEncryptionDelegate::~OSCryptCacheEncryptionDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool OSCryptCacheEncryptionDelegate::EncryptData(
    base::span<const uint8_t> plaintext,
    std::vector<uint8_t>* ciphertext) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kInitialized) {
    return false;
  }
  std::string plaintext_string(plaintext.begin(), plaintext.end());
  std::string ciphertext_string;
  if (!instance_->EncryptString(plaintext_string, &ciphertext_string)) {
    return false;
  }
  *ciphertext =
      std::vector<uint8_t>(ciphertext_string.begin(), ciphertext_string.end());
  return true;
}

bool OSCryptCacheEncryptionDelegate::DecryptData(
    base::span<const uint8_t> ciphertext,
    std::vector<uint8_t>* plaintext) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kInitialized) {
    return false;
  }
  std::optional<std::string> result = instance_->DecryptData(ciphertext);
  if (!result.has_value()) {
    return false;
  }
  *plaintext = std::vector<uint8_t>(result->begin(), result->end());
  return true;
}

void OSCryptCacheEncryptionDelegate::OnDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = State::kUninitialized;
  instance_.reset();
  remote_.reset();
  callbacks_.Notify(net::ERR_CONNECTION_CLOSED);
}

void OSCryptCacheEncryptionDelegate::Init(
    base::OnceCallback<void(net::Error)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kInitialized) {
    std::move(callback).Run(net::OK);
    return;
  }

  callbacks_.AddUnsafe(std::move(callback));

  if (state_ == State::kInitializing) {
    return;
  }

  state_ = State::kInitializing;

  remote_.Bind(std::move(provider_));
  remote_.set_disconnect_handler(
      base::BindOnce(&OSCryptCacheEncryptionDelegate::OnDisconnect,
                     weak_ptr_factory_.GetWeakPtr()));
  remote_->GetEncryptor(
      base::BindOnce(&OSCryptCacheEncryptionDelegate::InitCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OSCryptCacheEncryptionDelegate::InitCallback(
    os_crypt_async::Encryptor encryptor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kInitializing) {
    // OnDisconnect() was called during initialization. Callbacks have been
    // notified of failure already.
    return;
  }
  instance_.emplace(std::move(encryptor));
  if (!instance_->IsEncryptionAvailable()) {
    state_ = State::kUninitialized;
    instance_.reset();
    callbacks_.Notify(net::ERR_FAILED);
    return;
  }
  state_ = State::kInitialized;
  callbacks_.Notify(net::OK);
}

}  // namespace enterprise_encryption
