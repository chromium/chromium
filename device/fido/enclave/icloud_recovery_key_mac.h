// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_ICLOUD_RECOVERY_KEY_MAC_H_
#define DEVICE_FIDO_ENCLAVE_ICLOUD_RECOVERY_KEY_MAC_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"

namespace trusted_vault {
class SecureBoxKeyPair;
}  // namespace trusted_vault

namespace device::enclave {

// A key pair backed and synced by iCloud keychain that can be used as a
// (Google) keychain recovery factor. These keys are used as an additional
// recovery mechanism for the cloud enclave authenticator, and they may also be
// created and retrieved by other Google products.
class COMPONENT_EXPORT(DEVICE_FIDO) ICloudRecoveryKey {
 public:
  using CreateCallback =
      base::OnceCallback<void(std::unique_ptr<ICloudRecoveryKey>)>;
  using RetrieveCallback =
      base::OnceCallback<void(std::vector<std::unique_ptr<ICloudRecoveryKey>>)>;
  ICloudRecoveryKey(ICloudRecoveryKey&) = delete;
  ICloudRecoveryKey operator=(ICloudRecoveryKey&) = delete;
  ~ICloudRecoveryKey();

  // Creates, stores in iCloud Keychain, and returns an ICloudRecoveryKey.
  static void Create(CreateCallback callback,
                     std::string_view keychain_access_group);

  // Retrieves from iCloud Keychain all ICloudRecoveryKeys.
  static void Retrieve(RetrieveCallback callback,
                       std::string_view keychain_access_group);

  // Randomly generates an ICloudRecoveryKey that is not persisted to the
  // keychain for unit tests.
  static std::unique_ptr<ICloudRecoveryKey> CreateForTest();

  const trusted_vault::SecureBoxKeyPair* key() const { return key_.get(); }
  const std::vector<uint8_t>& id() const { return id_; }

 private:
  // Like |Create| but synchronous.
  static std::unique_ptr<ICloudRecoveryKey> CreateAndStoreKeySlowly(
      std::string_view keychain_access_group);

  // Like |Retrieve| but synchronous.
  static std::vector<std::unique_ptr<ICloudRecoveryKey>> RetrieveKeysSlowly(
      std::string_view keychain_access_group);

  explicit ICloudRecoveryKey(
      std::unique_ptr<trusted_vault::SecureBoxKeyPair> key);

  const std::unique_ptr<trusted_vault::SecureBoxKeyPair> key_;

  // The ID of the key is the result of exporting the public key.
  const std::vector<uint8_t> id_;
};

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_ICLOUD_RECOVERY_KEY_MAC_H_
