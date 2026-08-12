// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_key_manager_ios.h"

#import <utility>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/task/sequenced_task_runner.h"

namespace {

using KeyRotationResult =
    enterprise_connectors::DeviceTrustKeyManager::KeyRotationResult;
using KeyMetadata = enterprise_connectors::DeviceTrustKeyManager::KeyMetadata;

}  // namespace

DeviceTrustKeyManagerIOS::DeviceTrustKeyManagerIOS() = default;

DeviceTrustKeyManagerIOS::~DeviceTrustKeyManagerIOS() = default;

void DeviceTrustKeyManagerIOS::StartInitialization() {
  // No-op on iOS for the primary unsigned key implementation.
}

// Key rotation is not supported on iOS. Post a task to fulfill the interface
// contract and ensure the callback is never invoked synchronously.
void DeviceTrustKeyManagerIOS::RotateKey(
    const std::string&,
    base::OnceCallback<void(KeyRotationResult)> callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), KeyRotationResult::FAILURE));
}

// Post a task to fulfill the interface contract and guarantee the callback is
// never called synchronously, preventing re-entrancy.
void DeviceTrustKeyManagerIOS::ExportPublicKeyAsync(
    base::OnceCallback<void(std::optional<std::string>)> callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
}

// Post a task with `nullopt` to produce an unsigned response
// (`kSuccessNoSignature`) while fulfilling the asynchronous interface contract
// and ensuring the callback is never invoked synchronously.
void DeviceTrustKeyManagerIOS::SignStringAsync(
    const std::string&,
    base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
}

std::optional<KeyMetadata> DeviceTrustKeyManagerIOS::GetLoadedKeyMetadata()
    const {
  return std::nullopt;
}

bool DeviceTrustKeyManagerIOS::HasPermanentFailure() const {
  return false;
}
