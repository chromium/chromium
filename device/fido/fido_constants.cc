// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_constants.h"

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"

namespace device {

namespace {

std::string CtapRequestCommandName(CtapRequestCommand command) {
  switch (command) {
    case CtapRequestCommand::kAuthenticatorMakeCredential:
      return "kAuthenticatorMakeCredential";
    case CtapRequestCommand::kAuthenticatorGetAssertion:
      return "kAuthenticatorGetAssertion";
    case CtapRequestCommand::kAuthenticatorGetNextAssertion:
      return "kAuthenticatorGetNextAssertion";
    case CtapRequestCommand::kAuthenticatorGetInfo:
      return "kAuthenticatorGetInfo";
    case CtapRequestCommand::kAuthenticatorClientPin:
      return "kAuthenticatorClientPin";
    case CtapRequestCommand::kAuthenticatorReset:
      return "kAuthenticatorReset";
    case CtapRequestCommand::kAuthenticatorBioEnrollment:
      return "kAuthenticatorBioEnrollment";
    case CtapRequestCommand::kAuthenticatorSelection:
      return "kAuthenticatorSelection";
    case CtapRequestCommand::kAuthenticatorLargeBlobs:
      return "kAuthenticatorLargeBlobs";
    case CtapRequestCommand::kAuthenticatorBioEnrollmentPreview:
      return "kAuthenticatorBioEnrollmentPreview";
    case CtapRequestCommand::kAuthenticatorCredentialManagement:
      return "kAuthenticatorCredentialManagement";
    case CtapRequestCommand::kAuthenticatorCredentialManagementPreview:
      return "kAuthenticatorCredentialManagementPreview";
  }
}

std::string CtapDeviceResponseCodeName(CtapDeviceResponseCode code) {
  switch (code) {
    case CtapDeviceResponseCode::kSuccess:
      return "kSuccess";
    case CtapDeviceResponseCode::kCtap1ErrInvalidCommand:
      return "kCtap1ErrInvalidCommand";
    case CtapDeviceResponseCode::kCtap1ErrInvalidParameter:
      return "kCtap1ErrInvalidParameter";
    case CtapDeviceResponseCode::kCtap1ErrInvalidLength:
      return "kCtap1ErrInvalidLength";
    case CtapDeviceResponseCode::kCtap1ErrInvalidSeq:
      return "kCtap1ErrInvalidSeq";
    case CtapDeviceResponseCode::kCtap1ErrTimeout:
      return "kCtap1ErrTimeout";
    case CtapDeviceResponseCode::kCtap1ErrChannelBusy:
      return "kCtap1ErrChannelBusy";
    case CtapDeviceResponseCode::kCtap1ErrLockRequired:
      return "kCtap1ErrLockRequired";
    case CtapDeviceResponseCode::kCtap1ErrInvalidChannel:
      return "kCtap1ErrInvalidChannel";
    case CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType:
      return "kCtap2ErrCBORUnexpectedType";
    case CtapDeviceResponseCode::kCtap2ErrInvalidCBOR:
      return "kCtap2ErrInvalidCBOR";
    case CtapDeviceResponseCode::kCtap2ErrMissingParameter:
      return "kCtap2ErrMissingParameter";
    case CtapDeviceResponseCode::kCtap2ErrLimitExceeded:
      return "kCtap2ErrLimitExceeded";
    case CtapDeviceResponseCode::kCtap2ErrUnsupportedExtension:
      return "kCtap2ErrUnsupportedExtension";
    case CtapDeviceResponseCode::kCtap2ErrFpDatabaseFull:
      return "kCtap2ErrFpDatabaseFull";
    case CtapDeviceResponseCode::kCtap2ErrLargeBlobStorageFull:
      return "kCtap2ErrLargeBlobStorageFull";
    case CtapDeviceResponseCode::kCtap2ErrCredentialExcluded:
      return "kCtap2ErrCredentialExcluded";
    case CtapDeviceResponseCode::kCtap2ErrProcesssing:
      return "kCtap2ErrProcesssing";
    case CtapDeviceResponseCode::kCtap2ErrInvalidCredential:
      return "kCtap2ErrInvalidCredential";
    case CtapDeviceResponseCode::kCtap2ErrUserActionPending:
      return "kCtap2ErrUserActionPending";
    case CtapDeviceResponseCode::kCtap2ErrOperationPending:
      return "kCtap2ErrOperationPending";
    case CtapDeviceResponseCode::kCtap2ErrNoOperations:
      return "kCtap2ErrNoOperations";
    case CtapDeviceResponseCode::kCtap2ErrUnsupportedAlgorithm:
      return "kCtap2ErrUnsupportedAlgorithm";
    case CtapDeviceResponseCode::kCtap2ErrOperationDenied:
      return "kCtap2ErrOperationDenied";
    case CtapDeviceResponseCode::kCtap2ErrKeyStoreFull:
      return "kCtap2ErrKeyStoreFull";
    case CtapDeviceResponseCode::kCtap2ErrNotBusy:
      return "kCtap2ErrNotBusy";
    case CtapDeviceResponseCode::kCtap2ErrNoOperationPending:
      return "kCtap2ErrNoOperationPending";
    case CtapDeviceResponseCode::kCtap2ErrUnsupportedOption:
      return "kCtap2ErrUnsupportedOption";
    case CtapDeviceResponseCode::kCtap2ErrInvalidOption:
      return "kCtap2ErrInvalidOption";
    case CtapDeviceResponseCode::kCtap2ErrKeepAliveCancel:
      return "kCtap2ErrKeepAliveCancel";
    case CtapDeviceResponseCode::kCtap2ErrNoCredentials:
      return "kCtap2ErrNoCredentials";
    case CtapDeviceResponseCode::kCtap2ErrUserActionTimeout:
      return "kCtap2ErrUserActionTimeout";
    case CtapDeviceResponseCode::kCtap2ErrNotAllowed:
      return "kCtap2ErrNotAllowed";
    case CtapDeviceResponseCode::kCtap2ErrPinInvalid:
      return "kCtap2ErrPinInvalid";
    case CtapDeviceResponseCode::kCtap2ErrPinBlocked:
      return "kCtap2ErrPinBlocked";
    case CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid:
      return "kCtap2ErrPinAuthInvalid";
    case CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked:
      return "kCtap2ErrPinAuthBlocked";
    case CtapDeviceResponseCode::kCtap2ErrPinNotSet:
      return "kCtap2ErrPinNotSet";
    case CtapDeviceResponseCode::kCtap2ErrPinRequired:
      return "kCtap2ErrPinRequired";
    case CtapDeviceResponseCode::kCtap2ErrPinPolicyViolation:
      return "kCtap2ErrPinPolicyViolation";
    case CtapDeviceResponseCode::kCtap2ErrPinTokenExpired:
      return "kCtap2ErrPinTokenExpired";
    case CtapDeviceResponseCode::kCtap2ErrRequestTooLarge:
      return "kCtap2ErrRequestTooLarge";
    case CtapDeviceResponseCode::kCtap2ErrActionTimeout:
      return "kCtap2ErrActionTimeout";
    case CtapDeviceResponseCode::kCtap2ErrUpRequired:
      return "kCtap2ErrUpRequired";
    case CtapDeviceResponseCode::kCtap2ErrUvBlocked:
      return "kCtap2ErrUvBlocked";
    case CtapDeviceResponseCode::kCtap2ErrIntegrityFailure:
      return "kCtap2ErrIntegrityFailure";
    case CtapDeviceResponseCode::kCtap2ErrInvalidSubcommand:
      return "kCtap2ErrInvalidSubcommand";
    case CtapDeviceResponseCode::kCtap2ErrUvInvalid:
      return "kCtap2ErrUvInvalid";
    case CtapDeviceResponseCode::kCtap2ErrUnauthorizedPermission:
      return "kCtap2ErrUnauthorizedPermission";
    case CtapDeviceResponseCode::kCtap2ErrOther:
      return "kCtap2ErrOther";
    case CtapDeviceResponseCode::kCtap2ErrSpecLast:
      return "kCtap2ErrSpecLast";
    case CtapDeviceResponseCode::kCtap2ErrExtensionFirst:
      return "kCtap2ErrExtensionFirst";
    case CtapDeviceResponseCode::kCtap2ErrExtensionLast:
      return "kCtap2ErrExtensionLast";
    case CtapDeviceResponseCode::kCtap2ErrVendorFirst:
      return "kCtap2ErrVendorFirst";
    case CtapDeviceResponseCode::kCtap2ErrVendorLast:
      return "kCtap2ErrVendorLast";
  }
}

}  // namespace

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

const base::TimeDelta kBleDevicePairingModeWaitingInterval = base::Seconds(2);

std::ostream& operator<<(std::ostream& os, CtapRequestCommand command) {
  return os << "0x" << std::hex << static_cast<int>(command) << " ("
            << CtapRequestCommandName(command) << ")";
}

std::ostream& operator<<(std::ostream& os, CtapDeviceResponseCode code) {
  return os << "0x" << std::hex << static_cast<int>(code) << " ("
            << CtapDeviceResponseCodeName(code) << ")";
}

}  // namespace device
