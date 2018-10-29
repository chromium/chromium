// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_DEVICE_AUTHENTICATOR_H_
#define DEVICE_FIDO_FIDO_DEVICE_AUTHENTICATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "device/fido/fido_authenticator.h"

namespace device {

class CtapGetAssertionRequest;
class CtapMakeCredentialRequest;
class FidoDevice;
class FidoTask;

// Adaptor class from a |FidoDevice| to the |FidoAuthenticator| interface.
// Responsible for translating WebAuthn-level requests into serializations that
// can be passed to the device for transport.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoDeviceAuthenticator
    : public FidoAuthenticator {
 public:
  FidoDeviceAuthenticator(std::unique_ptr<FidoDevice> device);
  ~FidoDeviceAuthenticator() override;

  // FidoAuthenticator:
  void InitializeAuthenticator(base::OnceClosure callback) override;
  void MakeCredential(CtapMakeCredentialRequest request,
                      MakeCredentialCallback callback) override;
  void GetAssertion(CtapGetAssertionRequest request,
                    GetAssertionCallback callback) override;
  void Cancel() override;
  std::string GetId() const override;
  base::string16 GetDisplayName() const override;
  const AuthenticatorSupportedOptions& Options() const override;
  FidoTransportProtocol AuthenticatorTransport() const override;
  bool IsInPairingMode() const override;
  base::WeakPtr<FidoAuthenticator> GetWeakPtr() override;

  FidoDevice* device() { return device_.get(); }

 protected:
  void OnCtapMakeCredentialResponseReceived(
      MakeCredentialCallback callback,
      base::Optional<std::vector<uint8_t>> response_data);
  void OnCtapGetAssertionResponseReceived(
      GetAssertionCallback callback,
      base::Optional<std::vector<uint8_t>> response_data);

  void SetTaskForTesting(std::unique_ptr<FidoTask> task);

 private:
  const std::unique_ptr<FidoDevice> device_;
  std::unique_ptr<FidoTask> task_;
  base::WeakPtrFactory<FidoDeviceAuthenticator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FidoDeviceAuthenticator);
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_DEVICE_AUTHENTICATOR_H_
