// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_V2_AUTHENTICATOR_H_
#define DEVICE_FIDO_CABLE_V2_AUTHENTICATOR_H_

#include <string>
#include <vector>

#include <stdint.h>

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/optional.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/fido_constants.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

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
    TUNNEL_SERVER_CONNECT = 1,
    HANDSHAKE_COMPLETE = 2,
    REQUEST_RECEIVED = 3,
    CTAP_ERROR = 4,
  };

  enum class Error {
    UNEXPECTED_EOF = 100,
    TUNNEL_SERVER_CONNECT_FAILED = 101,
    HANDSHAKE_FAILED = 102,
    DECRYPT_FAILURE = 103,
    INVALID_CBOR = 104,
    INVALID_CTAP = 105,
    UNKNOWN_COMMAND = 106,
    INTERNAL_ERROR = 107,
  };

  using MakeCredentialCallback =
      base::OnceCallback<void(uint32_t status,
                              base::span<const uint8_t> attestation_obj)>;

  struct MakeCredentialParams {
    MakeCredentialParams();
    ~MakeCredentialParams();
    MakeCredentialParams(const MakeCredentialParams&) = delete;
    MakeCredentialParams& operator=(const MakeCredentialParams&) = delete;
    MakeCredentialParams(MakeCredentialParams&&) = delete;

    std::vector<uint8_t> client_data_hash;
    std::string rp_id;
    std::vector<uint8_t> user_id;
    std::vector<int> algorithms;
    std::vector<std::vector<uint8_t>> excluded_cred_ids;
    bool resident_key_required = false;
    MakeCredentialCallback callback;
  };

  virtual void MakeCredential(std::unique_ptr<MakeCredentialParams> params) = 0;

  using GetAssertionCallback =
      base::OnceCallback<void(uint32_t status,
                              base::span<const uint8_t> cred_id,
                              base::span<const uint8_t> auth_data,
                              base::span<const uint8_t> sig)>;

  struct GetAssertionParams {
    GetAssertionParams();
    ~GetAssertionParams();
    GetAssertionParams(const GetAssertionParams&) = delete;
    GetAssertionParams& operator=(const GetAssertionParams&) = delete;
    GetAssertionParams(GetAssertionParams&&) = delete;

    std::vector<uint8_t> client_data_hash;
    std::string rp_id;
    std::vector<std::vector<uint8_t>> allowed_cred_ids;
    GetAssertionCallback callback;
  };

  virtual void GetAssertion(std::unique_ptr<GetAssertionParams> params) = 0;

  // OnStatus is called when a new informative status is available.
  virtual void OnStatus(Status) = 0;

  // OnCompleted is called when the transaction has completed. Note that calling
  // this may result in the |Transaction| that owns this |Platform| being
  // deleted.
  virtual void OnCompleted(base::Optional<Error>) = 0;

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
  using CompleteCallback = base::OnceCallback<void()>;

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
    std::unique_ptr<Platform> platform,
    network::mojom::NetworkContext* network_context,
    base::span<const uint8_t, kRootSecretSize> root_secret,
    const std::string& authenticator_name,
    // TODO: name this constant.
    base::span<const uint8_t, 16> qr_secret,
    base::span<const uint8_t, kP256X962Length> peer_identity,
    base::Optional<std::vector<uint8_t>> contact_id);

// TransactFromFCM starts a network-based transaction based on the decoded
// contents of a cloud message.
std::unique_ptr<Transaction> TransactFromFCM(
    std::unique_ptr<Platform> platform,
    network::mojom::NetworkContext* network_context,
    base::span<const uint8_t, kRootSecretSize> root_secret,
    std::array<uint8_t, kRoutingIdSize> routing_id,
    base::span<const uint8_t, kTunnelIdSize> tunnel_id,
    base::span<const uint8_t> pairing_id,
    base::span<const uint8_t, kClientNonceSize> client_nonce);

}  // namespace authenticator
}  // namespace cablev2
}  // namespace device

#endif  // DEVICE_FIDO_CABLE_V2_AUTHENTICATOR_H_
