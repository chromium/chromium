// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <functional>
#include <memory>
#include <utility>

#import <LocalAuthentication/LocalAuthentication.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_thread_priority.h"
#include "crypto/apple_keychain_v2.h"
#include "crypto/scoped_lacontext.h"
#include "crypto/unexportable_key.h"
#include "crypto/unexportable_key_mac.h"
#include "crypto/user_verifying_key.h"

namespace crypto {

namespace {

// Refcounted wrapper for UnexportableSigningKey.
class RefCountedUnexportableSigningKey
    : public base::RefCountedThreadSafe<RefCountedUnexportableSigningKey> {
 public:
  explicit RefCountedUnexportableSigningKey(
      std::unique_ptr<UnexportableSigningKey> key)
      : key_(std::move(key)) {}

  UnexportableSigningKey* key() { return key_.get(); }

 private:
  friend class base::RefCountedThreadSafe<RefCountedUnexportableSigningKey>;
  ~RefCountedUnexportableSigningKey() = default;

  std::unique_ptr<UnexportableSigningKey> key_;
};

// Wraps signing |data| with |key|.
std::optional<std::vector<uint8_t>> DoSign(
    std::vector<uint8_t> data,
    scoped_refptr<RefCountedUnexportableSigningKey> key) {
  return key->key()->SignSlowly(data);
}

std::string ToString(const std::vector<uint8_t>& vec) {
  return std::string(vec.begin(), vec.end());
}

// User verifying key implementation that delegates the heavy lifting to
// UnexportableKeyMac.
class UserVerifyingSigningKeyMac : public UserVerifyingSigningKey {
 public:
  explicit UserVerifyingSigningKeyMac(
      std::unique_ptr<UnexportableSigningKey> key)
      : key_name_(ToString(key->GetWrappedKey())),
        key_(base::MakeRefCounted<RefCountedUnexportableSigningKey>(
            std::move(key))) {}
  ~UserVerifyingSigningKeyMac() override = default;

  void Sign(base::span<const uint8_t> data,
            base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>
                callback) override {
    // Signing will result in the system TouchID prompt being shown if the
    // caller does not pass an authenticated LAContext, so we run the signing
    // code in a separate thread.
    scoped_refptr<base::SequencedTaskRunner> worker_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_BLOCKING});

    // Copy |data| to avoid needing to guarantee its backing storage to outlive
    // the thread.
    std::vector<uint8_t> data_copy(data.begin(), data.end());

    worker_task_runner->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&DoSign, std::move(data_copy), key_),
        std::move(callback));
  }

  std::vector<uint8_t> GetPublicKey() const override {
    return key_->key()->GetSubjectPublicKeyInfo();
  }

  const UserVerifyingKeyLabel& GetKeyLabel() const override {
    return key_name_;
  }

 private:
  // The key's wrapped key as a binary string.
  const std::string key_name_;
  const scoped_refptr<RefCountedUnexportableSigningKey> key_;
};

class UserVerifyingKeyProviderMac : public UserVerifyingKeyProvider {
 public:
  explicit UserVerifyingKeyProviderMac(UserVerifyingKeyProvider::Config config)
      : lacontext_(config.lacontext ? config.lacontext->release() : nil),
        config_(std::move(config)) {}
  ~UserVerifyingKeyProviderMac() override = default;

  void GenerateUserVerifyingSigningKey(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      base::OnceCallback<void(std::unique_ptr<UserVerifyingSigningKey>)>
          callback) override {
    std::unique_ptr<UnexportableKeyProviderMac> key_provider =
        GetUnexportableKeyProviderMac(MakeUnexportableKeyConfig());
    if (!key_provider) {
      std::move(callback).Run(nullptr);
      return;
    }
    std::unique_ptr<UnexportableSigningKey> key =
        key_provider->GenerateSigningKeySlowly(acceptable_algorithms,
                                               lacontext_);
    if (!key) {
      std::move(callback).Run(nullptr);
      return;
    }
    std::move(callback).Run(
        std::make_unique<UserVerifyingSigningKeyMac>(std::move(key)));
  }

  void GetUserVerifyingSigningKey(
      UserVerifyingKeyLabel key_label,
      base::OnceCallback<void(std::unique_ptr<UserVerifyingSigningKey>)>
          callback) override {
    std::vector<uint8_t> wrapped_key(key_label.begin(), key_label.end());
    std::unique_ptr<UnexportableKeyProviderMac> key_provider =
        GetUnexportableKeyProviderMac(MakeUnexportableKeyConfig());
    if (!key_provider) {
      std::move(callback).Run(nullptr);
      return;
    }
    std::unique_ptr<UnexportableSigningKey> key =
        key_provider->FromWrappedSigningKeySlowly(wrapped_key, lacontext_);
    if (!key) {
      std::move(callback).Run(nullptr);
      return;
    }
    std::move(callback).Run(
        std::make_unique<UserVerifyingSigningKeyMac>(std::move(key)));
  }

  void DeleteUserVerifyingKey(
      UserVerifyingKeyLabel key_label,
      base::OnceCallback<void(bool)> callback) override {
    std::unique_ptr<UnexportableKeyProvider> key_provider =
        GetUnexportableKeyProvider(MakeUnexportableKeyConfig());
    if (!key_provider) {
      std::move(callback).Run(false);
      return;
    }
    std::vector<uint8_t> wrapped_key(key_label.begin(), key_label.end());
    std::move(callback).Run(key_provider->DeleteSigningKey(wrapped_key));
  }

 private:
  UnexportableKeyProvider::Config MakeUnexportableKeyConfig() {
    return {
        .keychain_access_group = config_.keychain_access_group,
        .access_control =
            UnexportableKeyProvider::Config::AccessControl::kUserPresence,
    };
  }
  LAContext* __strong lacontext_;
  const UserVerifyingKeyProvider::Config config_;
};

}  // namespace

std::unique_ptr<UserVerifyingKeyProvider> GetUserVerifyingKeyProviderMac(
    UserVerifyingKeyProvider::Config config) {
  return std::make_unique<UserVerifyingKeyProviderMac>(std::move(config));
}

void AreMacUnexportableKeysAvailable(UserVerifyingKeyProvider::Config config,
                                     base::OnceCallback<void(bool)> callback) {
  if (!GetUnexportableKeyProvider(
          {.keychain_access_group = std::move(config.keychain_access_group)})) {
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(
      AppleKeychainV2::GetInstance().LAContextCanEvaluatePolicy(
          LAPolicyDeviceOwnerAuthentication, /*error=*/nil));
}

}  // namespace crypto
