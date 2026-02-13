// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/enterprise/encryption/os_crypt_cache_encryption_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/check_is_test.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "services/network/enterprise/encryption/chunked_encryptor.h"
#include "services/network/enterprise/encryption/encrypted_backend_file_operations_factory.h"
#include "services/network/enterprise/encryption/encrypted_cache_entry_hasher.h"

namespace network::enterprise_encryption {

OSCryptCacheEncryptionDelegate::OSCryptCacheEncryptionDelegate(
    mojo::PendingRemote<network::mojom::CacheEncryptionProvider> provider)
    : provider_(std::move(provider)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

OSCryptCacheEncryptionDelegate::~OSCryptCacheEncryptionDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

disk_cache::BackendFileOperationsFactory*
OSCryptCacheEncryptionDelegate::GetEncryptionFileOperationsFactory(
    scoped_refptr<disk_cache::BackendFileOperationsFactory>
        file_operations_factory) {
  // This method should only be called if the delegate is initialized.
  CHECK(instance_.has_value());
  // This method should only be called once.
  CHECK(!encrypted_file_operations_factory_);

  // If the caller did not provide a file operations factory, use a trivial
  // one.
  if (!file_operations_factory) {
    CHECK_IS_TEST();
    file_operations_factory =
        base::MakeRefCounted<disk_cache::TrivialFileOperationsFactory>();
  }

  std::optional<std::string> decrypted_primary_key =
      instance_->DecryptData(encrypted_primary_key_);
  if (!decrypted_primary_key.has_value()) {
    base::UmaHistogramBoolean(
        "Enterprise.EncryptedCache.FactoryCreationSuccess", false);
    LOG(ERROR) << "Failed to decrypt the primary key.";
    return nullptr;
  }
  crypto::ProcessBoundString primary_key(std::move(*decrypted_primary_key));

  base::UmaHistogramBoolean("Enterprise.EncryptedCache.FactoryCreationSuccess",
                            true);
  encrypted_file_operations_factory_ =
      base::MakeRefCounted<EncryptedBackendFileOperationsFactory>(
          file_operations_factory, std::move(primary_key));

  return encrypted_file_operations_factory_.get();
}

std::unique_ptr<disk_cache::CacheEntryHasher>
OSCryptCacheEncryptionDelegate::GetCacheEntryHasher() {
  CHECK(instance_.has_value());
  std::optional<std::string> decrypted_primary_key =
      instance_->DecryptData(encrypted_primary_key_);
  if (!decrypted_primary_key.has_value()) {
    base::UmaHistogramBoolean(
        "Enterprise.EncryptedCache.KeyHasherObtainSuccess", false);
    LOG(ERROR) << "Failed to decrypt the primary key.";
    return nullptr;
  }
  crypto::ProcessBoundString primary_key(std::move(*decrypted_primary_key));

  base::UmaHistogramBoolean("Enterprise.EncryptedCache.KeyHasherObtainSuccess",
                            true);
  return std::make_unique<EncryptedCacheEntryHasher>(std::move(primary_key));
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

  auto barrier_closure = base::BarrierClosure(
      2, base::BindOnce(&OSCryptCacheEncryptionDelegate::InitCallback,
                        weak_ptr_factory_.GetWeakPtr()));

  remote_->GetEncryptor(
      base::BindOnce(&OSCryptCacheEncryptionDelegate::OnEncryptorReceived,
                     weak_ptr_factory_.GetWeakPtr(), barrier_closure));

  remote_->GetEncryptedCacheEncryptionKey(
      base::BindOnce(&OSCryptCacheEncryptionDelegate::OnCacheKeyReceived,
                     weak_ptr_factory_.GetWeakPtr(), barrier_closure));
}

void OSCryptCacheEncryptionDelegate::OnEncryptorReceived(
    base::OnceClosure done_closure,
    os_crypt_async::Encryptor encryptor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kInitializing) {
    instance_.emplace(std::move(encryptor));
  }
  std::move(done_closure).Run();
}

void OSCryptCacheEncryptionDelegate::OnCacheKeyReceived(
    base::OnceClosure done_closure,
    const std::vector<uint8_t>& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kInitializing) {
    encrypted_primary_key_ = key;
  }
  std::move(done_closure).Run();
}

void OSCryptCacheEncryptionDelegate::InitCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kInitializing) {
    // OnDisconnect() was called during initialization. Callbacks have been
    // notified of failure already.
    return;
  }
  if (!instance_->IsEncryptionAvailable() || encrypted_primary_key_.empty()) {
    state_ = State::kUninitialized;
    instance_.reset();
    callbacks_.Notify(net::ERR_FAILED);
    return;
  }
  state_ = State::kInitialized;
  callbacks_.Notify(net::OK);
}

}  // namespace network::enterprise_encryption
