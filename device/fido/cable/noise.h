// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_NOISE_H_
#define DEVICE_FIDO_CABLE_NOISE_H_

#include <stdint.h>

#include <array>
#include <optional>
#include <tuple>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace device {

// Noise implements a small subset of the Noise Protocol Framework [1].
//
// http://www.noiseprotocol.org/noise.html#the-handshakestate-object
class COMPONENT_EXPORT(DEVICE_FIDO) Noise {
 public:
  // HandshakeType enumerates the supported handshake patterns.
  enum class HandshakeType {
    kKNpsk0,  // https://noiseexplorer.com/patterns/KNpsk0/
    kNKpsk0,  // https://noiseexplorer.com/patterns/NKpsk0/
    kNK,      // https://noiseexplorer.com/patterns/NK/
  };

  Noise();
  ~Noise();

  // Init must be called immediately after construction to initialise values.
  void Init(HandshakeType type);

  // The following functions reflect the functions of the same name from
  // http://www.noiseprotocol.org/noise.html#the-symmetricstate-object
  void MixHash(base::span<const uint8_t> in);
  void MixKey(base::span<const uint8_t> ikm);
  void MixKeyAndHash(base::span<const uint8_t> ikm);
  std::vector<uint8_t> EncryptAndHash(base::span<const uint8_t> plaintext);
  std::optional<std::vector<uint8_t>> DecryptAndHash(
      base::span<const uint8_t> ciphertext);
  std::array<uint8_t, 32> handshake_hash() const;

  // MaxHashPoint calls |MixHash| with the uncompressed, X9.62 serialization of
  // |point|.
  void MixHashPoint(const EC_POINT* point);

  // traffic_keys() calls Split from the protocol spec but, rather than
  // returning CipherState objects, returns the raw keys.
  std::tuple<std::array<uint8_t, 32>, std::array<uint8_t, 32>> traffic_keys()
      const;

 private:
  void InitializeKey(base::span<const uint8_t, 32> key);

  std::array<uint8_t, 32> chaining_key_;
  std::array<uint8_t, 32> h_;
  std::array<uint8_t, 32> symmetric_key_;
  uint32_t symmetric_nonce_;
};

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_NOISE_H_
