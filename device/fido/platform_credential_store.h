// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PLATFORM_CREDENTIAL_STORE_H_
#define DEVICE_FIDO_PLATFORM_CREDENTIAL_STORE_H_

#include "base/component_export.h"
#include "base/time/time.h"

namespace device {
namespace fido {

// The PlatformCredentialStore interface wraps methods for deleting WebAuthn
// credentials that belong to authenticators integrated into Chrome (currently
// only the TouchIdAuthenticator in //device/fido/mac).
class COMPONENT_EXPORT(DEVICE_FIDO) PlatformCredentialStore {
 public:
  virtual ~PlatformCredentialStore() = default;

  // DeleteCredentials deletes WebAuthn credentials that were created within the
  // given time interval from local storage and runs the callback when
  // finished.
  virtual void DeleteCredentials(base::Time created_not_before,
                                 base::Time created_not_after,
                                 base::OnceClosure callback) = 0;

  // CountCredentials calculates the number of credentials that would get
  // deleted by a call to |DeleteCredentials| with identical arguments, and runs
  // the callback with the number as parameter.
  virtual void CountCredentials(base::Time created_not_before,
                                base::Time created_not_after,
                                base::OnceCallback<void(size_t)> callback) = 0;
};

}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_PLATFORM_CREDENTIAL_STORE_H_
