// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_KEY_MANAGER_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_KEY_MANAGER_IOS_H_

#import <optional>
#import <string>
#import <vector>

#import "base/functional/callback_forward.h"
#import "components/enterprise/device_trust/core/device_trust_key_manager.h"

// iOS `DeviceTrustKeyManager` implementation for unsigned attestation.
class DeviceTrustKeyManagerIOS
    : public enterprise_connectors::DeviceTrustKeyManager {
 public:
  DeviceTrustKeyManagerIOS();
  ~DeviceTrustKeyManagerIOS() override;

  DeviceTrustKeyManagerIOS(const DeviceTrustKeyManagerIOS&) = delete;
  DeviceTrustKeyManagerIOS& operator=(const DeviceTrustKeyManagerIOS&) = delete;

  // `enterprise_connectors::DeviceTrustKeyManager` implementation:
  void StartInitialization() override;
  void RotateKey(
      const std::string& nonce,
      base::OnceCallback<
          void(enterprise_connectors::DeviceTrustKeyManager::KeyRotationResult)>
          callback) override;
  void ExportPublicKeyAsync(
      base::OnceCallback<void(std::optional<std::string>)> callback) override;
  void SignStringAsync(
      const std::string& str,
      base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> callback)
      override;
  std::optional<enterprise_connectors::DeviceTrustKeyManager::KeyMetadata>
  GetLoadedKeyMetadata() const override;
  bool HasPermanentFailure() const override;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_KEY_MANAGER_IOS_H_
