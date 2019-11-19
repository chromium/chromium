// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_CONSTANTS_H_
#define DEVICE_FIDO_FIDO_CONSTANTS_H_

#include <stdint.h>

#include <array>
#include <vector>

#include "base/component_export.h"
#include "base/time/time.h"
#include "device/fido/fido_types.h"

namespace device {

// Length of the U2F challenge parameter:
// https://goo.gl/y75WrX#registration-request-message---u2f_register
constexpr size_t kU2fChallengeParamLength = 32;

// Length of the U2F application parameter:
// https://goo.gl/y75WrX#registration-request-message---u2f_register
constexpr size_t kU2fApplicationParamLength = 32;

// Offset of the length of the U2F registration key handle:
// https://goo.gl/y75WrX#registration-response-message-success
constexpr size_t kU2fKeyHandleLengthOffset = 66;

// Offset of the U2F registration key handle:
// https://goo.gl/y75WrX#registration-response-message-success
constexpr size_t kU2fKeyHandleOffset = 67;

// Length of the SHA-256 hash of the JSON-serialized client data:
// https://www.w3.org/TR/webauthn/#collectedclientdata-hash-of-the-serialized-client-data
constexpr size_t kClientDataHashLength = 32;

// Length of the SHA-256 hash of the RP ID asssociated with the credential:
// https://www.w3.org/TR/webauthn/#sec-authenticator-data
constexpr size_t kRpIdHashLength = 32;

// Max length for the user handle:
// https://www.w3.org/TR/webauthn/#user-handle
constexpr size_t kUserHandleMaxLength = 64;

static_assert(kU2fApplicationParamLength == kRpIdHashLength,
              "kU2fApplicationParamLength must be equal to kRpIdHashLength.");

// Length of the flags:
// https://www.w3.org/TR/webauthn/#sec-authenticator-data
constexpr size_t kFlagsLength = 1;

// Length of the signature counter, 32-bit unsigned big-endian integer:
// https://www.w3.org/TR/webauthn/#sec-authenticator-data
constexpr size_t kSignCounterLength = 4;

// Length of the AAGUID of the authenticator:
// https://www.w3.org/TR/webauthn/#sec-attested-credential-data
constexpr size_t kAaguidLength = 16;

// Length of the byte length L of Credential ID, 16-bit unsigned big-endian
// integer: https://www.w3.org/TR/webauthn/#sec-attested-credential-data
constexpr size_t kCredentialIdLengthLength = 2;

// CTAP protocol device response code, as specified in
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#authenticator-api
enum class CtapDeviceResponseCode : uint8_t {
  kSuccess = 0x00,
  kCtap1ErrInvalidCommand = 0x01,
  kCtap1ErrInvalidParameter = 0x02,
  kCtap1ErrInvalidLength = 0x03,
  kCtap1ErrInvalidSeq = 0x04,
  kCtap1ErrTimeout = 0x05,
  kCtap1ErrChannelBusy = 0x06,
  kCtap1ErrLockRequired = 0x0A,
  kCtap1ErrInvalidChannel = 0x0B,
  kCtap2ErrCBORUnexpectedType = 0x11,
  kCtap2ErrInvalidCBOR = 0x12,
  kCtap2ErrMissingParameter = 0x14,
  kCtap2ErrLimitExceeded = 0x15,
  kCtap2ErrUnsupportedExtension = 0x16,
  kCtap2ErrTooManyElements = 0x17,
  kCtap2ErrExtensionNotSupported = 0x18,
  kCtap2ErrCredentialExcluded = 0x19,
  kCtap2ErrProcesssing = 0x21,
  kCtap2ErrInvalidCredential = 0x22,
  kCtap2ErrUserActionPending = 0x23,
  kCtap2ErrOperationPending = 0x24,
  kCtap2ErrNoOperations = 0x25,
  kCtap2ErrUnsupportedAlgorithm = 0x26,
  kCtap2ErrOperationDenied = 0x27,
  kCtap2ErrKeyStoreFull = 0x28,
  kCtap2ErrNotBusy = 0x29,
  kCtap2ErrNoOperationPending = 0x2A,
  kCtap2ErrUnsupportedOption = 0x2B,
  kCtap2ErrInvalidOption = 0x2C,
  kCtap2ErrKeepAliveCancel = 0x2D,
  kCtap2ErrNoCredentials = 0x2E,
  kCtap2ErrUserActionTimeout = 0x2F,
  kCtap2ErrNotAllowed = 0x30,
  kCtap2ErrPinInvalid = 0x31,
  kCtap2ErrPinBlocked = 0x32,
  kCtap2ErrPinAuthInvalid = 0x33,
  kCtap2ErrPinAuthBlocked = 0x34,
  kCtap2ErrPinNotSet = 0x35,
  kCtap2ErrPinRequired = 0x36,
  kCtap2ErrPinPolicyViolation = 0x37,
  kCtap2ErrPinTokenExpired = 0x38,
  kCtap2ErrRequestTooLarge = 0x39,
  kCtap2ErrOther = 0x7F,
  kCtap2ErrSpecLast = 0xDF,
  kCtap2ErrExtensionFirst = 0xE0,
  kCtap2ErrExtensionLast = 0xEF,
  kCtap2ErrVendorFirst = 0xF0,
  kCtap2ErrVendorLast = 0xFF
};

constexpr std::array<CtapDeviceResponseCode, 51> GetCtapResponseCodeList() {
  return {CtapDeviceResponseCode::kSuccess,
          CtapDeviceResponseCode::kCtap1ErrInvalidCommand,
          CtapDeviceResponseCode::kCtap1ErrInvalidParameter,
          CtapDeviceResponseCode::kCtap1ErrInvalidLength,
          CtapDeviceResponseCode::kCtap1ErrInvalidSeq,
          CtapDeviceResponseCode::kCtap1ErrTimeout,
          CtapDeviceResponseCode::kCtap1ErrChannelBusy,
          CtapDeviceResponseCode::kCtap1ErrLockRequired,
          CtapDeviceResponseCode::kCtap1ErrInvalidChannel,
          CtapDeviceResponseCode::kCtap2ErrCBORUnexpectedType,
          CtapDeviceResponseCode::kCtap2ErrInvalidCBOR,
          CtapDeviceResponseCode::kCtap2ErrMissingParameter,
          CtapDeviceResponseCode::kCtap2ErrLimitExceeded,
          CtapDeviceResponseCode::kCtap2ErrUnsupportedExtension,
          CtapDeviceResponseCode::kCtap2ErrTooManyElements,
          CtapDeviceResponseCode::kCtap2ErrExtensionNotSupported,
          CtapDeviceResponseCode::kCtap2ErrCredentialExcluded,
          CtapDeviceResponseCode::kCtap2ErrProcesssing,
          CtapDeviceResponseCode::kCtap2ErrInvalidCredential,
          CtapDeviceResponseCode::kCtap2ErrUserActionPending,
          CtapDeviceResponseCode::kCtap2ErrOperationPending,
          CtapDeviceResponseCode::kCtap2ErrNoOperations,
          CtapDeviceResponseCode::kCtap2ErrUnsupportedAlgorithm,
          CtapDeviceResponseCode::kCtap2ErrOperationDenied,
          CtapDeviceResponseCode::kCtap2ErrKeyStoreFull,
          CtapDeviceResponseCode::kCtap2ErrNotBusy,
          CtapDeviceResponseCode::kCtap2ErrNoOperationPending,
          CtapDeviceResponseCode::kCtap2ErrUnsupportedOption,
          CtapDeviceResponseCode::kCtap2ErrInvalidOption,
          CtapDeviceResponseCode::kCtap2ErrKeepAliveCancel,
          CtapDeviceResponseCode::kCtap2ErrNoCredentials,
          CtapDeviceResponseCode::kCtap2ErrUserActionTimeout,
          CtapDeviceResponseCode::kCtap2ErrNotAllowed,
          CtapDeviceResponseCode::kCtap2ErrPinInvalid,
          CtapDeviceResponseCode::kCtap2ErrPinBlocked,
          CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid,
          CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked,
          CtapDeviceResponseCode::kCtap2ErrPinNotSet,
          CtapDeviceResponseCode::kCtap2ErrPinRequired,
          CtapDeviceResponseCode::kCtap2ErrPinPolicyViolation,
          CtapDeviceResponseCode::kCtap2ErrPinTokenExpired,
          CtapDeviceResponseCode::kCtap2ErrRequestTooLarge,
          CtapDeviceResponseCode::kCtap2ErrOther,
          CtapDeviceResponseCode::kCtap2ErrSpecLast,
          CtapDeviceResponseCode::kCtap2ErrExtensionFirst,
          CtapDeviceResponseCode::kCtap2ErrExtensionLast,
          CtapDeviceResponseCode::kCtap2ErrVendorFirst,
          CtapDeviceResponseCode::kCtap2ErrVendorLast};
}

// Commands supported by CTAPHID device as specified in
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#ctaphid-commands
enum class FidoHidDeviceCommand : uint8_t {
  kMsg = 0x03,
  kCbor = 0x10,
  kInit = 0x06,
  kPing = 0x01,
  kCancel = 0x11,
  kError = 0x3F,
  kKeepAlive = 0x3B,
  kWink = 0x08,
  kLock = 0x04,
};

constexpr std::array<FidoHidDeviceCommand, 9> GetFidoHidDeviceCommandList() {
  return {FidoHidDeviceCommand::kMsg,       FidoHidDeviceCommand::kCbor,
          FidoHidDeviceCommand::kInit,      FidoHidDeviceCommand::kPing,
          FidoHidDeviceCommand::kCancel,    FidoHidDeviceCommand::kError,
          FidoHidDeviceCommand::kKeepAlive, FidoHidDeviceCommand::kWink,
          FidoHidDeviceCommand::kLock};
}

// BLE device command as specified in
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#command-status-and-error-constants
// U2F BLE device does not support cancel command.
enum class FidoBleDeviceCommand : uint8_t {
  kPing = 0x81,
  kKeepAlive = 0x82,
  kMsg = 0x83,
  kControl = 0x84,
  kCancel = 0xBE,
  kError = 0xBF,
};

// Relevant LE Discoverable Mode bits. Reference:
// Bluetooth Core Specification Supplement, Part A, section 1.3
constexpr uint8_t kLeLimitedDiscoverableModeBit = 0;
constexpr uint8_t kLeGeneralDiscoverableModeBit = 1;

// Fido Service Data Flags as specified in
// https://fidoalliance.org/specs/fido-v2.0-id-20180227/fido-client-to-authenticator-protocol-v2.0-id-20180227.html#ble-pairing-authnr-considerations
enum class FidoServiceDataFlags : uint8_t {
  kPairingMode = 0x80,
  kPasskeyEntry = 0x40,
};

// Authenticator API commands supported by CTAP devices, as specified in
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#authenticator-api
enum class CtapRequestCommand : uint8_t {
  kAuthenticatorMakeCredential = 0x01,
  kAuthenticatorGetAssertion = 0x02,
  kAuthenticatorGetNextAssertion = 0x08,
  kAuthenticatorGetInfo = 0x04,
  kAuthenticatorClientPin = 0x06,
  kAuthenticatorReset = 0x07,
  kAuthenticatorBioEnrollment = 0x09,
  kAuthenticatorBioEnrollmentPreview = 0x40,
  kAuthenticatorCredentialManagement = 0x0a,
  kAuthenticatorCredentialManagementPreview = 0x41,
};

enum class CoseAlgorithmIdentifier : int { kCoseEs256 = -7 };

// APDU instruction code for U2F request encoding.
// https://fidoalliance.org/specs/fido-u2f-v1.0-ps-20141009/fido-u2f-u2f.h-v1.0-ps-20141009.pdf
enum class U2fApduInstruction : uint8_t {
  kRegister = 0x01,
  kSign = 0x02,
  kVersion = 0x03,
  kVendorFirst = 0x40,
  kVenderLast = 0xBF,
};

// Enumerates the two types of application parameter values used: the
// "primary" value is the hash of the relying party ID[1] and is always
// provided. The "alternative" value is the hash of a U2F AppID, specified in
// an extension[2], for compatibility with keys that were registered with the
// old API.
//
// [1] https://w3c.github.io/webauthn/#rp-id
// [2] https://w3c.github.io/webauthn/#sctn-appid-extension
enum class ApplicationParameterType {
  kPrimary,
  kAlternative,
};

// Parameters for fake U2F registration used to check for user presence.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const std::array<uint8_t, 32> kBogusAppParam;
COMPONENT_EXPORT(DEVICE_FIDO)
extern const std::array<uint8_t, 32> kBogusChallenge;

// String used as Relying Party ID to check for user presence.
constexpr char kDummyRpID[] = ".dummy";

// String key values for CTAP request optional parameters and
// AuthenticatorGetInfo response.
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kResidentKeyMapKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kUserVerificationMapKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kUserPresenceMapKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kClientPinMapKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kPlatformDeviceMapKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kEntityIdMapKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kEntityNameMapKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kDisplayNameMapKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kIconUrlMapKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kCredentialTypeMapKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kCredentialAlgorithmMapKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kCredentialManagementMapKey[];
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kCredentialManagementPreviewMapKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kBioEnrollmentMapKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kBioEnrollmentPreviewMapKey[];

// HID transport specific constants.
constexpr uint32_t kHidBroadcastChannel = 0xffffffff;
constexpr size_t kHidInitPacketHeaderSize = 7;
constexpr size_t kHidContinuationPacketHeaderSize = 5;
constexpr size_t kHidMaxPacketSize = 64;

constexpr uint8_t kHidMaxLockSeconds = 10;

// Messages are limited to an initiation packet and 128 continuation packets.
constexpr size_t kHidMaxMessageSize = 7609;

// U2F APDU encoding constants, as specified in
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#bib-U2FHeader
constexpr size_t kU2fMaxResponseSize = 65536;

// P1 instructions.
constexpr uint8_t kP1TupRequired = 0x01;
constexpr uint8_t kP1TupConsumed = 0x02;
constexpr uint8_t kP1TupRequiredConsumed = kP1TupRequired | kP1TupConsumed;

// Control byte used for check-only setting. The check-only command is used to
// determine if the provided key handle was originally created by this token
// and whether it was created for the provided application parameter.
constexpr uint8_t kP1CheckOnly = 0x07;

// Indicates that an individual attestation certificate is acceptable to
// return with this registration.
constexpr uint8_t kP1IndividualAttestation = 0x80;
constexpr size_t kMaxKeyHandleLength = 255;

// Maximum wait time before client error outs on device.
COMPONENT_EXPORT(DEVICE_FIDO) extern const base::TimeDelta kDeviceTimeout;

// Wait time before polling device for U2F register/sign operation again when
// device times out waiting for user presence.
COMPONENT_EXPORT(DEVICE_FIDO) extern const base::TimeDelta kU2fRetryDelay;

// String key values for attestation object as a response to MakeCredential
// request.
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kFormatKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kAttestationStatementKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kAuthDataKey[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kNoneAttestationValue[];

// String representation of public key credential enum.
// https://w3c.github.io/webauthn/#credentialType
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kPublicKey[];

const char* CredentialTypeToString(CredentialType type);

// Values used to construct/validate handshake messages for Cable handshake
// protocol.
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kCableHandshakeKeyInfo[];
COMPONENT_EXPORT(DEVICE_FIDO)
extern const std::array<uint8_t, 24> kCableDeviceEncryptionKeyInfo;
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kCableAuthenticatorHelloMessage[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kCableClientHelloMessage[];

// TODO(hongjunchoi): Add url to the official spec once it's standardized.
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kCtap2Version[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kU2fVersion[];

COMPONENT_EXPORT(DEVICE_FIDO) extern const char kExtensionHmacSecret[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kExtensionCredProtect[];

// Maximum number of seconds the browser waits for Bluetooth authenticator to
// send packets that advertises that the device is in pairing mode before
// setting pairing mode to false. The interval time is set to 2 seconds, which
// is equivalent to the maximum Bluetooth error wait interval set by the CTAP
// spec.
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#BTCORE
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::TimeDelta kBleDevicePairingModeWaitingInterval;

// CredProtect enumerates the levels of credential protection specified by the
// `credProtect` CTAP2 extension.
enum class CredProtect : uint8_t {
  kUVOrCredIDRequired = 2,
  kUVRequired = 3,
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_CONSTANTS_H_
