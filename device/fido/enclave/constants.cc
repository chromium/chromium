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
    0x04, 0x74, 0xcf, 0x69, 0xcb, 0xd1, 0x2b, 0x75, 0x07, 0x42, 0x85,
    0xcf, 0x69, 0x6f, 0xc2, 0x56, 0x4b, 0x90, 0xe7, 0xeb, 0xbc, 0xd0,
    0xe7, 0x20, 0x36, 0x86, 0x66, 0xbe, 0xcc, 0x94, 0x75, 0xa2, 0xa4,
    0x4c, 0x2a, 0xf8, 0xa2, 0x56, 0xb8, 0x92, 0xb7, 0x7d, 0x17, 0xba,
    0x97, 0x93, 0xbb, 0xf2, 0x9f, 0x52, 0x26, 0x7d, 0x90, 0xf9, 0x2c,
    0x37, 0x26, 0x02, 0xbb, 0x4e, 0xd1, 0x89, 0x7c, 0xad, 0x54,
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
    ret.url = enclave_url;
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

const char kResponseSuccessKey[] = "ok";
const char kResponseErrorKey[] = "err";

const char kRegisterCommandName[] = "device/register";
const char kWrapKeyCommandName[] = "keys/wrap";
const char kGenKeyPairCommandName[] = "keys/genpair";
const char kRecoveryKeyStoreWrapCommandName[] = "recovery_key_store/wrap";
const char kPasskeysWrapPinCommandName[] = "passkeys/wrap_pin";
const char kRecoveryKeyStoreWrapAsMemberCommandName[] =
    "recovery_key_store/wrap_as_member";

const char kRegisterPubKeysKey[] = "pub_keys";
const char kRegisterDeviceIdKey[] = "device_id";

const char kHardwareKey[] = "hw";
const char kUserVerificationKey[] = "uv";

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
