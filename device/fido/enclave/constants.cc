// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/constants.h"

#include <array>

#include "device/fido/enclave/types.h"
#include "device/fido/fido_constants.h"

namespace device::enclave {

namespace {

const EnclaveIdentity* g_enclave_override = nullptr;

// Once running, this should be "wss://enclave.ua5v.com/enclave" and the public
// key should be updated.
constexpr char kEnclaveUrl[] = "wss://127.0.0.1:8080/";
constexpr std::array<uint8_t, device::kP256X962Length> kEnclavePublicKey = {
    0x04, 0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc,
    0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2, 0x77, 0x03, 0x7d, 0x81, 0x2d,
    0xeb, 0x33, 0xa0, 0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96,
    0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b, 0x8e, 0xe7, 0xeb,
    0x4a, 0x7c, 0x0f, 0x9e, 0x16, 0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31,
    0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5,
};

}  // namespace

EnclaveIdentity GetEnclaveIdentity() {
  if (g_enclave_override) {
    return *g_enclave_override;
  }

  EnclaveIdentity ret;
  ret.url = GURL(kEnclaveUrl);
  ret.public_key = kEnclavePublicKey;
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

const char kResponseSuccessKey[] = "ok";
const char kResponseErrorKey[] = "err";

const char kRegisterCommandName[] = "device/register";
const char kWrapKeyCommandName[] = "keys/wrap";
const char kGenKeyPairCommandName[] = "keys/genpair";

const char kRegisterPubKeysKey[] = "pub_keys";
const char kRegisterDeviceIdKey[] = "device_id";

const char kHardwareKey[] = "hw";

const char kWrappingPurpose[] = "purpose";
const char kWrappingKeyToWrap[] = "key";

const char kWrappingResponsePublicKey[] = "pub_key";
const char kWrappingResponseWrappedPrivateKey[] = "priv_key";

const char kKeyPurposeSecurityDomainMemberKey[] = "security domain member key";
const char kKeyPurposeSecurityDomainSecret[] = "security domain secret";

}  // namespace device::enclave
