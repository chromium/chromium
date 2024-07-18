// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "base/containers/span.h"
#include "device/fido/cable/v2_handshake.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/obj.h"

namespace device {

namespace {

constexpr std::array<uint8_t, 32> kTestPSK = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};
constexpr std::array<uint8_t, 65> kTestPeerIdentity = {
    0x04, 0x67, 0x80, 0xc5, 0xfc, 0x70, 0x27, 0x5e, 0x2c, 0x70, 0x61,
    0xa0, 0xe7, 0x87, 0x7b, 0xb1, 0x74, 0xde, 0xad, 0xeb, 0x98, 0x87,
    0x02, 0x7f, 0x3f, 0xa8, 0x36, 0x54, 0x15, 0x8b, 0xa7, 0xf5, 0x0c,
    0x3c, 0xba, 0x8c, 0x34, 0xbc, 0x35, 0xd2, 0x0e, 0x81, 0xf7, 0x30,
    0xac, 0x1c, 0x7b, 0xd6, 0xd6, 0x61, 0xa9, 0x42, 0xf9, 0x0c, 0x6a,
    0x9c, 0xa5, 0x5c, 0x51, 0x2f, 0x9e, 0x4a, 0x00, 0x12, 0x66,
};
constexpr std::array<uint8_t, 32> kTestLocalSeed = {
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* raw_data, size_t size) {
  auto input = base::make_span(raw_data, size);
  if (input.empty()) {
    return 0;
  }
  const bool initiate = input[0] & 1;
  const bool have_local_key = input[0] & 2;
  input = input.subspan(1);

  std::optional<base::span<const uint8_t, 65>> peer_identity;
  std::optional<base::span<const uint8_t, 32>> local_seed;
  bssl::UniquePtr<EC_KEY> local_key;
  if (have_local_key) {
    local_seed = kTestLocalSeed;
    bssl::UniquePtr<EC_GROUP> p256(
        EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
    local_key.reset(EC_KEY_derive_from_secret(p256.get(), local_seed->data(),
                                              local_seed->size()));
  } else {
    peer_identity = kTestPeerIdentity;
  }

  if (initiate) {
    cablev2::HandshakeInitiator handshaker(kTestPSK, peer_identity, local_seed);
    handshaker.BuildInitialMessage();
    handshaker.ProcessResponse(input);
  } else {
    std::vector<uint8_t> response;
    cablev2::RespondToHandshake(kTestPSK, std::move(local_key), peer_identity,
                                input, &response);
  }

  return 0;
}

}  // namespace device
