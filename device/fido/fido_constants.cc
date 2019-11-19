// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_constants.h"

namespace device {

const std::array<uint8_t, 32> kBogusAppParam = {
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41};

const std::array<uint8_t, 32> kBogusChallenge = {
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42};

const char kResidentKeyMapKey[] = "rk";
const char kUserVerificationMapKey[] = "uv";
const char kUserPresenceMapKey[] = "up";
const char kClientPinMapKey[] = "clientPin";
const char kPlatformDeviceMapKey[] = "plat";
const char kEntityIdMapKey[] = "id";
const char kEntityNameMapKey[] = "name";
const char kDisplayNameMapKey[] = "displayName";
const char kIconUrlMapKey[] = "icon";
const char kCredentialTypeMapKey[] = "type";
const char kCredentialAlgorithmMapKey[] = "alg";
const char kCredentialManagementMapKey[] = "credMgmt";
const char kCredentialManagementPreviewMapKey[] = "credentialMgmtPreview";
const char kBioEnrollmentMapKey[] = "bioEnroll";
const char kBioEnrollmentPreviewMapKey[] = "userVerificationMgmtPreview";

const base::TimeDelta kDeviceTimeout = base::TimeDelta::FromSeconds(20);
const base::TimeDelta kU2fRetryDelay = base::TimeDelta::FromMilliseconds(200);

const char kFormatKey[] = "fmt";
const char kAttestationStatementKey[] = "attStmt";
const char kAuthDataKey[] = "authData";
const char kNoneAttestationValue[] = "none";

const char kPublicKey[] = "public-key";

const char* CredentialTypeToString(CredentialType type) {
  switch (type) {
    case CredentialType::kPublicKey:
      return kPublicKey;
  }
  NOTREACHED();
  return kPublicKey;
}

const char kCableHandshakeKeyInfo[] = "FIDO caBLE v1 handshakeKey";
const std::array<uint8_t, 24> kCableDeviceEncryptionKeyInfo = {
    'F', 'I', 'D', 'O', ' ', 'c', 'a', 'B', 'L', 'E', ' ', 'v',
    '1', ' ', 's', 'e', 's', 's', 'i', 'o', 'n', 'K', 'e', 'y',
};
const char kCableAuthenticatorHelloMessage[] = "caBLE v1 authenticator hello";
const char kCableClientHelloMessage[] = "caBLE v1 client hello";

const char kCtap2Version[] = "FIDO_2_0";
const char kU2fVersion[] = "U2F_V2";

const char kExtensionHmacSecret[] = "hmac-secret";
const char kExtensionCredProtect[] = "credProtect";

const base::TimeDelta kBleDevicePairingModeWaitingInterval =
    base::TimeDelta::FromSeconds(2);

}  // namespace device
