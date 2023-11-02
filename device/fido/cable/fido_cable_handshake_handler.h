// Copyright 2018 The Chromium Authors
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
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/noise.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/fido_device.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace device {

class FidoCableDevice;

// FidoCableHandshakeHandler abstracts FidoCableV1HandshakeHandler to allow
// tests to inject fake handshake handlers.
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

  FidoCableV1HandshakeHandler(const FidoCableV1HandshakeHandler&) = delete;
  FidoCableV1HandshakeHandler& operator=(const FidoCableV1HandshakeHandler&) =
      delete;

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

  const raw_ptr<FidoCableDevice> cable_device_;
  std::array<uint8_t, 8> nonce_;
  std::array<uint8_t, 32> session_pre_key_;
  std::array<uint8_t, 16> client_session_random_;
  std::string handshake_key_;

  base::WeakPtrFactory<FidoCableV1HandshakeHandler> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_FIDO_CABLE_HANDSHAKE_HANDLER_H_
