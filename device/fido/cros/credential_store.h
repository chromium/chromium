// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CROS_CREDENTIAL_STORE_H_
#define DEVICE_FIDO_CROS_CREDENTIAL_STORE_H_

#include <optional>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/platform_credential_store.h"

namespace u2f {
class DeleteCredentialsInTimeRangeResponse;
class CountCredentialsInTimeRangeResponse;
}  // namespace u2f

namespace device {
namespace fido {
namespace cros {

// PlatformAuthenticatorCredentialStore allows operations on the  credentials
// stored in the Chrome OS platform authenticator.
class COMPONENT_EXPORT(DEVICE_FIDO) PlatformAuthenticatorCredentialStore
    : public ::device::fido::PlatformCredentialStore {
 public:
  PlatformAuthenticatorCredentialStore();
  PlatformAuthenticatorCredentialStore(
      const PlatformAuthenticatorCredentialStore&) = delete;
  PlatformAuthenticatorCredentialStore& operator=(
      const PlatformAuthenticatorCredentialStore&) = delete;

  ~PlatformAuthenticatorCredentialStore() override;

  // PlatformCredentialStore:

  void DeleteCredentials(base::Time created_not_before,
                         base::Time created_not_after,
                         base::OnceClosure callback) override;

  void CountCredentials(base::Time created_not_before,
                        base::Time created_not_after,
                        base::OnceCallback<void(size_t)> callback) override;

 private:
  void DoDeleteCredentials(base::Time created_not_before,
                           base::Time created_not_after,
                           base::OnceClosure callback,
                           bool is_u2f_service_available);

  void DoCountCredentials(base::Time created_not_before,
                          base::Time created_not_after,
                          base::OnceCallback<void(size_t)> callback,
                          bool is_u2f_service_available);

  void OnDeleteCredentialsFinished(
      base::OnceClosure callback,
      std::optional<u2f::DeleteCredentialsInTimeRangeResponse> response);
  void OnCountCredentialsFinished(
      base::OnceCallback<void(size_t)> callback,
      std::optional<u2f::CountCredentialsInTimeRangeResponse> response);

  base::WeakPtrFactory<PlatformAuthenticatorCredentialStore> weak_factory_{
      this};
};

}  // namespace cros
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_CROS_CREDENTIAL_STORE_H_
