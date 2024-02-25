// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_CONSTANTS_H_
#define DEVICE_FIDO_ENCLAVE_CONSTANTS_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"

namespace device::enclave {

// This file contains various constants used to communicate with the enclave.

struct EnclaveIdentity;

// GetEnclaveIdentity returns the default URL & public-key of the enclave. In
// tests, its return value can be set using `ScopedEnclaveOverride`.
COMPONENT_EXPORT(DEVICE_FIDO)
EnclaveIdentity GetEnclaveIdentity();

// Creating a `ScopedEnclaveOverride` allows the URL and public key of the
// enclave to be overridden for testing. These objects can be nested.
class COMPONENT_EXPORT(DEVICE_FIDO) ScopedEnclaveOverride {
 public:
  ScopedEnclaveOverride(EnclaveIdentity identity);
  ~ScopedEnclaveOverride();

 private:
  const raw_ptr<const EnclaveIdentity> prev_;
  const std::unique_ptr<EnclaveIdentity> enclave_identity_;
};

// Keys in the top-level request message.
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kCommandEncodedRequestsKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kCommandDeviceIdKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kCommandSigKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kCommandAuthLevelKey[];

// Generic keys for all request types.
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kRequestCommandKey[];

// Keys in the top-level of each response.
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kResponseSuccessKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kResponseErrorKey[];

// Command names
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kRegisterCommandName[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kWrapKeyCommandName[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kGenKeyPairCommandName[];
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kRecoveryKeyStoreWrapCommandName[];

// Register request keys
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kRegisterPubKeysKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kRegisterDeviceIdKey[];

// Device key types
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kHardwareKey[];

// Wrapping request keys
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kWrappingPurpose[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kWrappingKeyToWrap[];

// Wrapping response keys
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kWrappingResponsePublicKey[];
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kWrappingResponseWrappedPrivateKey[];

// Key purpose strings.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kKeyPurposeSecurityDomainMemberKey[];
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kKeyPurposeSecurityDomainSecret[];

// Recovery key store commands.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kRecoveryKeyStorePinHash[];
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kRecoveryKeyStoreCertXml[];
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kRecoveryKeyStoreSigXml[];

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_CONSTANTS_H_
