// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_constants.h"

#include "base/notreached.h"

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
const char kCredentialTypeMapKey[] = "type";
const char kCredentialAlgorithmMapKey[] = "alg";
const char kCredentialManagementMapKey[] = "credMgmt";
const char kCredentialManagementPreviewMapKey[] = "credentialMgmtPreview";
const char kBioEnrollmentMapKey[] = "bioEnroll";
const char kBioEnrollmentPreviewMapKey[] = "userVerificationMgmtPreview";
const char kPinUvTokenMapKey[] = "pinUvAuthToken";
const char kDefaultCredProtectKey[] = "defaultCredProtect";
const char kEnterpriseAttestationKey[] = "ep";
const char kLargeBlobsKey[] = "largeBlobs";
const char kAlwaysUvKey[] = "alwaysUv";
const char kMakeCredUvNotRqdKey[] = "makeCredUvNotRqd";

const base::TimeDelta kDeviceTimeout = base::Seconds(20);
const base::TimeDelta kU2fRetryDelay = base::Milliseconds(200);

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
const char kCtap2_1Version[] = "FIDO_2_1";

const char kExtensionHmacSecret[] = "hmac-secret";
const char kExtensionCredProtect[] = "credProtect";
const char kExtensionLargeBlob[] = "largeBlob";
const char kExtensionLargeBlobKey[] = "largeBlobKey";
const char kExtensionCredBlob[] = "credBlob";
const char kExtensionMinPINLength[] = "minPinLength";
const char kExtensionDevicePublicKey[] = "devicePubKey";
const char kExtensionPRF[] = "prf";

const char kExtensionPRFEnabled[] = "enabled";
const char kExtensionPRFEval[] = "eval";
const char kExtensionPRFEvalByCredential[] = "evalByCredential";
const char kExtensionPRFFirst[] = "first";
const char kExtensionPRFResults[] = "results";
const char kExtensionPRFSecond[] = "second";

const char kExtensionLargeBlobBlob[] = "blob";
const char kExtensionLargeBlobOriginalSize[] = "originalSize";
const char kExtensionLargeBlobRead[] = "read";
const char kExtensionLargeBlobSupport[] = "support";
const char kExtensionLargeBlobSupported[] = "supported";
const char kExtensionLargeBlobSupportPreferred[] = "preferred";
const char kExtensionLargeBlobSupportRequired[] = "required";
const char kExtensionLargeBlobWrite[] = "write";
const char kExtensionLargeBlobWritten[] = "written";

const char kDevicePublicKeyAttestationKey[] = "attestation";
const char kDevicePublicKeyAttestationFormatsKey[] = "attestationFormats";
const char kDevicePublicKeyAAGUIDKey[] = "aaguid";
const char kDevicePublicKeyDPKKey[] = "dpk";
const char kDevicePublicKeyScopeKey[] = "scope";
const char kDevicePublicKeyNonceKey[] = "nonce";
const char kDevicePublicKeyEPKey[] = "epAtt";

const base::TimeDelta kBleDevicePairingModeWaitingInterval = base::Seconds(2);

}  // namespace device
