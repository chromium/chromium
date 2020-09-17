// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_V2_AUTHENTICATOR_H_
#define DEVICE_FIDO_CABLE_V2_AUTHENTICATOR_H_

#include <string>
#include <vector>

#include <stdint.h>

#include "base/callback_forward.h"
#include "base/containers/span.h"
#include "base/optional.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/fido_constants.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace device {
namespace cablev2 {
namespace authenticator {

// Platform abstracts the actions taken by the platform, i.e. the
// credential-store operations themselves, plus an interface for BLE
// advertising.
class Platform {
 public:
  // BLEAdvert represents a currently-transmitting advert. Destroying the object
  // stops the transmission.
  class BLEAdvert {
   public:
    virtual ~BLEAdvert();
  };

  virtual ~Platform();

  using MakeCredentialCallback =
      base::OnceCallback<void(uint32_t status,
                              base::span<const uint8_t> client_data_json,
                              base::span<const uint8_t> attestation_obj)>;
  using GetAssertionCallback =
      base::OnceCallback<void(uint32_t status,
                              base::span<const uint8_t> client_data_json,
                              base::span<const uint8_t> cred_id,
                              base::span<const uint8_t> auth_data,
                              base::span<const uint8_t> sig)>;

  virtual void MakeCredential(
      const std::string& origin,
      const std::string& rp_id,
      base::span<const uint8_t> challenge,
      base::span<const uint8_t> user_id,
      base::span<const int> algorithms,
      base::span<const std::vector<uint8_t>> excluded_cred_ids,
      bool resident_key_required,
      MakeCredentialCallback callback) = 0;

  virtual void GetAssertion(
      const std::string& origin,
      const std::string& rp_id,
      base::span<const uint8_t> challenge,
      base::span<const std::vector<uint8_t>> allowed_cred_ids,
      GetAssertionCallback callback) = 0;

  virtual std::unique_ptr<BLEAdvert> SendBLEAdvert(
      base::span<uint8_t, 16> payload) = 0;
};

// Transport abstracts a way of transmitting to, and receiving from, the peer.
// The framing of messages must be preserved.
class Transport {
 public:
  virtual ~Transport();

  // StartReading requests that the given callback be called whenever a message
  // arrives from the peer.
  virtual void StartReading(
      base::RepeatingCallback<void(base::Optional<std::vector<uint8_t>>)>
          read_callback) = 0;
  virtual void Write(std::vector<uint8_t> data) = 0;
};

// A Transaction is a handle to an ongoing caBLEv2 transaction with a peer.
class Transaction {
 public:
  using CompleteCallback = base::OnceCallback<void()>;

  virtual ~Transaction();
};

// TransactWithPlaintextTransport allows an arbitrary transport to be used for a
// caBLEv2 transaction.
std::unique_ptr<Transaction> TransactWithPlaintextTransport(
    std::unique_ptr<Platform> platform,
    std::unique_ptr<Transport> transport,
    Transaction::CompleteCallback complete_callback);

// TransactFromQRCode starts a network-based transaction based on the decoded
// contents of a QR code.
std::unique_ptr<Transaction> TransactFromQRCode(
    std::unique_ptr<Platform> platform,
    network::mojom::NetworkContext* network_context,
    base::span<const uint8_t, kRootSecretSize> root_secret,
    const std::string& authenticator_name,
    // TODO: name this constant.
    base::span<const uint8_t, 16> qr_secret,
    base::span<const uint8_t, kP256X962Length> peer_identity,
    base::Optional<std::vector<uint8_t>> contact_id,
    Transaction::CompleteCallback complete_callback);

// TransactFromQRCode starts a network-based transaction based on the decoded
// contents of a cloud message.
std::unique_ptr<Transaction> TransactFromFCM(
    std::unique_ptr<Platform> platform,
    network::mojom::NetworkContext* network_context,
    base::span<const uint8_t, kRootSecretSize> root_secret,
    std::array<uint8_t, kRoutingIdSize> routing_id,
    base::span<const uint8_t, kTunnelIdSize> tunnel_id,
    base::span<const uint8_t> pairing_id,
    base::span<const uint8_t, kClientNonceSize> client_nonce,
    Transaction::CompleteCallback complete_callback);

}  // namespace authenticator
}  // namespace cablev2
}  // namespace device

#endif  // DEVICE_FIDO_CABLE_V2_AUTHENTICATOR_H_
