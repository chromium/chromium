// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_VERIFY_VERIFY_H_
#define DEVICE_FIDO_ENCLAVE_VERIFY_VERIFY_H_

#include <cstdint>
#include <vector>

namespace device::enclave {

// Verify that the enclave is running an endorsed binary.
//
// |enclave_signature| should be returned to the client in the first message
// from the server to the client after the noise handshake to eliminate an
// additional round-trip.  In this case, the server sends the first
// post-handshake message over the noise channel.  The handshake hash
// concatenated with a hash of all evidence and endorsements is signed with the
// application key from the leaf cert of the DICE chain.
//
// |evidence| is generated in trusted environments running on the server.
// |endorsements| are generated offline to aid in attesting the enclave.
// |handshake_hash| is the noise handshake hash after the handshake completes.
// |enclave_signature| is a signature by the enclave application over
//     info prefix, |handshake_hash|, and a hash of evidence and endorsements.
bool VerifyEnclave(std::vector<std::vector<uint8_t>> evidence,
                   std::vector<std::vector<uint8_t>> endorsements,
                   std::vector<uint8_t> handshake_hash,
                   std::vector<uint8_t> enclave_signature);

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_VERIFY_VERIFY_H_
