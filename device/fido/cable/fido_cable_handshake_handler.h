// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_FIDO_CABLE_HANDSHAKE_HANDLER_H_
#define DEVICE_FIDO_CABLE_FIDO_CABLE_HANDSHAKE_HANDLER_H_

#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_device.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace device {

class FidoCableDevice;

// FidoCableHandshakeHandler abstracts over the different versions of caBLE
// handshakes.
class FidoCableHandshakeHandler {
 public:
  virtual ~FidoCableHandshakeHandler() = 0;
  virtual void InitiateCableHandshake(FidoDevice::DeviceCallback callback) = 0;
  virtual bool ValidateAuthenticatorHandshakeMessage(
      base::span<const uint8_t> response) = 0;
};

// Handles exchanging handshake messages with external authenticator and
// validating the handshake messages to derive a shared session key to be used
// for message encryption.
// See: fido-client-to-authenticator-protocol.html#cable-encryption-handshake of
// the most up-to-date spec.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoCableV1HandshakeHandler
    : public FidoCableHandshakeHandler {
 public:
  FidoCableV1HandshakeHandler(FidoCableDevice* device,
                              base::span<const uint8_t, 8> nonce,
                              base::span<const uint8_t, 32> session_pre_key);
  ~FidoCableV1HandshakeHandler() override;

  // FidoCableHandshakeHandler:
  void InitiateCableHandshake(FidoDevice::DeviceCallback callback) override;
  bool ValidateAuthenticatorHandshakeMessage(
      base::span<const uint8_t> response) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(FidoCableHandshakeHandlerTest, HandShakeSuccess);
  FRIEND_TEST_ALL_PREFIXES(FidoCableHandshakeHandlerTest,
                           HandshakeFailWithIncorrectAuthenticatorResponse);

  std::vector<uint8_t> GetEncryptionKeyAfterSuccessfulHandshake(
      base::span<const uint8_t, 16> authenticator_random_nonce) const;

  FidoCableDevice* const cable_device_;
  std::array<uint8_t, 8> nonce_;
  std::array<uint8_t, 32> session_pre_key_;
  std::array<uint8_t, 16> client_session_random_;
  std::string handshake_key_;

  base::WeakPtrFactory<FidoCableV1HandshakeHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FidoCableV1HandshakeHandler);
};

// FidoCableV2HandshakeHandler implements an NNpsk0[1] handshake that provides
// forward secrecy.
//
// [1] https://noiseexplorer.com/patterns/NNpsk0/
class COMPONENT_EXPORT(DEVICE_FIDO) FidoCableV2HandshakeHandler
    : public FidoCableHandshakeHandler {
 public:
  FidoCableV2HandshakeHandler(
      FidoCableDevice* device,
      base::span<const uint8_t, 32> psk_gen_key,
      base::span<const uint8_t, 8> nonce,
      base::span<const uint8_t, kCableEphemeralIdSize> eid,
      base::Optional<base::span<const uint8_t, 65>> peer_identity,
      base::RepeatingCallback<void(std::unique_ptr<CableDiscoveryData>)>
          pairing_callback);
  ~FidoCableV2HandshakeHandler() override;

  // FidoCableHandshakeHandler:
  void InitiateCableHandshake(FidoDevice::DeviceCallback callback) override;
  bool ValidateAuthenticatorHandshakeMessage(
      base::span<const uint8_t> response) override;

 private:
  void MixHash(base::span<const uint8_t> in);
  void MixKey(base::span<const uint8_t> ikm);
  void MixKeyAndHash(base::span<const uint8_t> ikm);
  void InitializeKey(base::span<const uint8_t, 32> key);
  std::vector<uint8_t> Encrypt(base::span<const uint8_t> plaintext);
  base::Optional<std::vector<uint8_t>> Decrypt(
      base::span<const uint8_t> ciphertext);

  FidoCableDevice* const cable_device_;
  std::array<uint8_t, 16> eid_;
  std::array<uint8_t, 32> psk_;
  std::array<uint8_t, 32> chaining_key_;
  std::array<uint8_t, 32> h_;
  std::array<uint8_t, 32> symmetric_key_;
  uint32_t symmetric_nonce_;
  base::Optional<std::array<uint8_t, 65>> peer_identity_;
  bssl::UniquePtr<EC_KEY> ephemeral_key_;
  base::RepeatingCallback<void(std::unique_ptr<CableDiscoveryData>)>
      pairing_callback_;
};

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_FIDO_CABLE_HANDSHAKE_HANDLER_H_
