// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/constants.h"

#include <array>

#include "base/command_line.h"
#include "base/logging.h"
#include "device/fido/enclave/types.h"
#include "device/fido/fido_constants.h"

namespace device::enclave {

namespace {

const EnclaveIdentity* g_enclave_override = nullptr;

constexpr char kEnclaveUrl[] = "wss://enclave.ua5v.com/enclave";

// The name of the commandline flag that allows to specify the enclave URL.
constexpr char kEnclaveUrlSwitch[] = "enclave-url";

// This is the public key of the `cloud_authenticator_test_service` that
// can be built in the Chromium source tree.
constexpr std::array<uint8_t, device::kP256X962Length> kLocalPublicKey = {
    0x04, 0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc,
    0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2, 0x77, 0x03, 0x7d, 0x81, 0x2d,
    0xeb, 0x33, 0xa0, 0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96,
    0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b, 0x8e, 0xe7, 0xeb,
    0x4a, 0x7c, 0x0f, 0x9e, 0x16, 0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31,
    0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5,
};

// This is the public key of the production enclave service.
constexpr std::array<uint8_t, device::kP256X962Length> kProdPublicKey = {
    0x04, 0xfe, 0x2c, 0x88, 0x76, 0xec, 0x1a, 0x5a, 0x64, 0x44, 0x40,
    0xfe, 0x11, 0xc1, 0x2a, 0xd8, 0x48, 0x6e, 0xf8, 0xed, 0x51, 0x35,
    0x30, 0x4c, 0xcd, 0xaf, 0xa1, 0x4c, 0xed, 0xb0, 0x16, 0xfd, 0x59,
    0x59, 0x3c, 0x16, 0x06, 0x86, 0xde, 0xd2, 0x9f, 0xba, 0x11, 0x77,
    0x13, 0x66, 0x1e, 0x7e, 0x23, 0x78, 0xd3, 0x41, 0xa3, 0x66, 0x8b,
    0x6b, 0x46, 0xfa, 0xff, 0xe9, 0x61, 0xd0, 0xfb, 0x4a, 0x12,
};

}  // namespace

EnclaveIdentity GetEnclaveIdentity() {
  if (g_enclave_override) {
    return *g_enclave_override;
  }

  auto* command_line = base::CommandLine::ForCurrentProcess();

  EnclaveIdentity ret;
  if (command_line->HasSwitch(kEnclaveUrlSwitch)) {
    GURL enclave_url(command_line->GetSwitchValueASCII(kEnclaveUrlSwitch));
    CHECK(enclave_url.is_valid());
    ret.url = std::move(enclave_url);
    ret.public_key = kLocalPublicKey;
  } else {
    ret.url = GURL(kEnclaveUrl);
    ret.public_key = kProdPublicKey;
  }
  return ret;
}

ScopedEnclaveOverride::ScopedEnclaveOverride(EnclaveIdentity identity)
    : prev_(g_enclave_override),
      enclave_identity_(
          std::make_unique<EnclaveIdentity>(std::move(identity))) {
  g_enclave_override = enclave_identity_.get();
}

ScopedEnclaveOverride::~ScopedEnclaveOverride() {
  CHECK(g_enclave_override == enclave_identity_.get());
  g_enclave_override = prev_;
}

const char kCommandEncodedRequestsKey[] = "encoded_requests";
const char kCommandDeviceIdKey[] = "device_id";
const char kCommandSigKey[] = "sig";
const char kCommandAuthLevelKey[] = "auth_level";

const char kRequestCommandKey[] = "cmd";
const char kRequestWrappedSecretKey[] = "wrapped_secret";
const char kRequestSecretKey[] = "secret";
const char kRequestCounterIDKey[] = "counter_id";
const char kRequestVaultHandleWithoutTypeKey[] = "vault_handle_without_type";
const char kRequestWrappedPINDataKey[] = "wrapped_pin_data";

const char kResponseSuccessKey[] = "ok";
const char kResponseErrorKey[] = "err";

const char kRegisterCommandName[] = "device/register";
const char kForgetCommandName[] = "device/forget";
const char kWrapKeyCommandName[] = "keys/wrap";
const char kGenKeyPairCommandName[] = "keys/genpair";
const char kRecoveryKeyStoreWrapCommandName[] = "recovery_key_store/wrap";
const char kPasskeysWrapPinCommandName[] = "passkeys/wrap_pin";
const char kRecoveryKeyStoreWrapAsMemberCommandName[] =
    "recovery_key_store/wrap_as_member";
const char kRecoveryKeyStoreRewrapCommandName[] = "recovery_key_store/rewrap";

const char kRegisterPubKeysKey[] = "pub_keys";
const char kRegisterDeviceIdKey[] = "device_id";
const char kRegisterUVKeyPending[] = "uv_key_pending";

const char kHardwareKey[] = "hw";
const char kSoftwareKey[] = "sw";
const char kUserVerificationKey[] = "uv";
const char kSoftwareUserVerificationKey[] = "swuv";

const char kWrappingPurpose[] = "purpose";
const char kWrappingKeyToWrap[] = "key";

const char kPinHash[] = "pin_hash";
const char kGeneration[] = "pin_generation";
const char kClaimKey[] = "pin_claim_key";

const char kWrappingResponsePublicKey[] = "pub_key";
const char kWrappingResponseWrappedPrivateKey[] = "priv_key";

const char kKeyPurposeSecurityDomainMemberKey[] = "security domain member key";
const char kKeyPurposeSecurityDomainSecret[] = "security domain secret";

const char kRecoveryKeyStorePinHash[] = "pin_hash";
const char kRecoveryKeyStoreCertXml[] = "cert_xml";
const char kRecoveryKeyStoreSigXml[] = "sig_xml";

const char kRecoveryKeyStoreURL[] =
    "https://cryptauthvault.googleapis.com/v1/vaults/0";
const char kRecoveryKeyStoreCertFileURL[] =
    "https://www.gstatic.com/cryptauthvault/v0/cert.xml";
const char kRecoveryKeyStoreSigFileURL[] =
    "https://www.gstatic.com/cryptauthvault/v0/cert.sig.xml";

}  // namespace device::enclave
