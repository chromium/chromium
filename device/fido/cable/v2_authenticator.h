// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_V2_AUTHENTICATOR_H_
#define DEVICE_FIDO_CABLE_V2_AUTHENTICATOR_H_

#include <string>
#include <vector>

#include <stdint.h>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/fido_constants.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-forward.h"

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

  enum class Status {
    // These values must match up with CableAuthenticatorUI.java.
    TUNNEL_SERVER_CONNECT = 1,
    HANDSHAKE_COMPLETE = 2,
    REQUEST_RECEIVED = 3,
    CTAP_ERROR = 4,
    FIRST_TRANSACTION_DONE = 5,
  };

  enum class Error {
    // These values must match up with CableAuthenticatorUI.java and zero
    // is considered to be not an error by the Java code.

    // NONE = 0,
    UNEXPECTED_EOF = 100,
    TUNNEL_SERVER_CONNECT_FAILED = 101,
    HANDSHAKE_FAILED = 102,
    DECRYPT_FAILURE = 103,
    INVALID_CBOR = 104,
    INVALID_CTAP = 105,
    UNKNOWN_COMMAND = 106,
    INTERNAL_ERROR = 107,
    SERVER_LINK_WRONG_LENGTH = 108,
    SERVER_LINK_NOT_ON_CURVE = 109,
    NO_SCREENLOCK = 110,
    NO_BLUETOOTH_PERMISSION = 111,
    QR_URI_ERROR = 112,
    EOF_WHILE_PROCESSING = 113,
    AUTHENTICATOR_SELECTION_RECEIVED = 114,
    DISCOVERABLE_CREDENTIALS_REQUEST = 115,
  };

  using MakeCredentialCallback = base::OnceCallback<void(
      uint32_t status,
      base::span<const uint8_t> attestation_obj,
      absl::optional<base::span<const uint8_t>> device_public_key_signature)>;

  virtual void MakeCredential(
      blink::mojom::PublicKeyCredentialCreationOptionsPtr params,
      MakeCredentialCallback callback) = 0;

  using GetAssertionCallback = base::OnceCallback<void(
      uint32_t status,
      blink::mojom::GetAssertionAuthenticatorResponsePtr response)>;

  virtual void GetAssertion(
      blink::mojom::PublicKeyCredentialRequestOptionsPtr params,
      GetAssertionCallback callback) = 0;

  // OnStatus is called when a new informative status is available.
  virtual void OnStatus(Status) = 0;

  // OnCompleted is called when the transaction has completed. Note that calling
  // this may result in the |Transaction| that owns this |Platform| being
  // deleted.
  virtual void OnCompleted(absl::optional<Error>) = 0;

  virtual std::unique_ptr<BLEAdvert> SendBLEAdvert(
      base::span<const uint8_t, kAdvertSize> payload) = 0;
};

// Transport abstracts a way of transmitting to, and receiving from, the peer.
// The framing of messages must be preserved.
class Transport {
 public:
  // Disconnected is a fresh type in order to be distinguishable in |Update|.
  enum class Disconnected { kDisconnected = 200 };

  // Update is a sum type of all the possible signals that a transport can
  // report. The first element is a message from the peer. |Disconnected| is
  // handled separately because it's context dependent whether that is an error
  // or not.
  using Update = absl::variant<std::vector<uint8_t>,
                               Platform::Error,
                               Platform::Status,
                               Disconnected>;
  virtual ~Transport();

  // StartReading requests that the given callback be called whenever a message
  // arrives from the peer, an error occurs, or the status of the link changes.
  virtual void StartReading(
      base::RepeatingCallback<void(Update)> update_callback) = 0;
  virtual void Write(std::vector<uint8_t> data) = 0;
};

// A Transaction is a handle to an ongoing caBLEv2 transaction with a peer.
class Transaction {
 public:
  virtual ~Transaction();
};

// TransactWithPlaintextTransport allows an arbitrary transport to be used for a
// caBLEv2 transaction.
std::unique_ptr<Transaction> TransactWithPlaintextTransport(
    std::unique_ptr<Platform> platform,
    std::unique_ptr<Transport> transport);

// TransactFromQRCode starts a network-based transaction based on the decoded
// contents of a QR code.
std::unique_ptr<Transaction> TransactFromQRCode(
    unsigned protocol_revision,
    std::unique_ptr<Platform> platform,
    network::mojom::NetworkContext* network_context,
    base::span<const uint8_t, kRootSecretSize> root_secret,
    const std::string& authenticator_name,
    // TODO: name this constant.
    base::span<const uint8_t, 16> qr_secret,
    base::span<const uint8_t, kP256X962Length> peer_identity,
    absl::optional<std::vector<uint8_t>> contact_id);

// TransactFromFCM starts a network-based transaction based on the decoded
// contents of a cloud message.
std::unique_ptr<Transaction> TransactFromFCM(
    unsigned protocol_revision,
    std::unique_ptr<Platform> platform,
    network::mojom::NetworkContext* network_context,
    base::span<const uint8_t, kRootSecretSize> root_secret,
    std::array<uint8_t, kRoutingIdSize> routing_id,
    base::span<const uint8_t, kTunnelIdSize> tunnel_id,
    base::span<const uint8_t, kPairingIDSize> pairing_id,
    base::span<const uint8_t, kClientNonceSize> client_nonce,
    absl::optional<base::span<const uint8_t>> contact_id);

}  // namespace authenticator
}  // namespace cablev2
}  // namespace device

#endif  // DEVICE_FIDO_CABLE_V2_AUTHENTICATOR_H_
