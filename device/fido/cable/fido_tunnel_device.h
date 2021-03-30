// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_FIDO_TUNNEL_DEVICE_H_
#define DEVICE_FIDO_CABLE_FIDO_TUNNEL_DEVICE_H_

#include <array>
#include <vector>

#include "base/callback_forward.h"
#include "base/sequence_checker.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/websocket_adapter.h"
#include "device/fido/fido_device.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace device {
namespace cablev2 {

class Crypter;
class WebSocketAdapter;
struct Pairing;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoTunnelDevice : public FidoDevice {
 public:
  // This constructor is used for QR-initiated connections.
  FidoTunnelDevice(
      network::mojom::NetworkContext* network_context,
      base::OnceCallback<void(std::unique_ptr<Pairing>)> pairing_callback,
      base::span<const uint8_t> secret,
      base::span<const uint8_t, kQRSeedSize> local_identity_seed,
      const CableEidArray& decrypted_eid);

  // This constructor is used for pairing-initiated connections. If the given
  // |Pairing| is reported by the tunnel server to be invalid (which can happen
  // if the user opts to unlink all devices) then |pairing_is_invalid| is
  // run.
  FidoTunnelDevice(network::mojom::NetworkContext* network_context,
                   std::unique_ptr<Pairing> pairing,
                   base::OnceClosure pairing_is_invalid);

  ~FidoTunnelDevice() override;

  // MatchAdvert is only valid for a pairing-initiated connection. It returns
  // true if the given |advert| matched this pending tunnel and thus this device
  // is now ready.
  bool MatchAdvert(const std::array<uint8_t, kAdvertSize>& advert);

  // FidoDevice:
  CancelToken DeviceTransact(std::vector<uint8_t> command,
                             DeviceCallback callback) override;
  void Cancel(CancelToken token) override;
  std::string GetId() const override;
  FidoTransportProtocol DeviceTransport() const override;
  base::WeakPtr<FidoDevice> GetWeakPtr() override;

 private:
  enum class State {
    // QR (or server-link) handshakes advance through the states like this:
    //
    //  kConnecting
    //      |
    //   (Tunnel server connection completes and handshake is sent)
    //      |
    //      V
    //  kHandshakeSent
    //      |
    //   (Handshake reply is received)
    //      |
    //      V
    //  kWaitingForPostHandshakeMessage
    //      |
    //   (Post-handshake message is received)
    //      |
    //      V
    //  kReady
    //
    //
    // Paired connections are similar, but there's a race between the tunnel
    // connection completing and the BLE advert being received.
    //
    //  kConnecting -------------------------------------
    //      |                                           |
    //   (Tunnel server connection completes)           |
    //      |                              (BLE advert is received _then_
    //      V                               tunnel connection completes.)
    //  kWaitingForEID                                  |
    //      |                                           |
    //   (BLE advert is received and handshake is sent) |
    //      |                                           |
    //      V                                           |
    //   kHandshakeSent   <------------------------------
    //      |
    //   (Handshake reply is received)
    //      |
    //      V
    //  kWaitingForPostHandshakeMessage
    //      |
    //   (Post-handshake message is received)
    //      |
    //      V
    //  kReady
    kConnecting,
    kHandshakeSent,
    kWaitingForEID,
    kWaitingForPostHandshakeMessage,
    kReady,
    kError,
  };

  struct QRInfo {
    QRInfo();
    ~QRInfo();
    QRInfo(const QRInfo&) = delete;
    QRInfo& operator=(const QRInfo&) = delete;

    CableEidArray decrypted_eid;
    std::array<uint8_t, 32> psk;
    base::OnceCallback<void(std::unique_ptr<Pairing>)> pairing_callback;
    std::array<uint8_t, kQRSeedSize> local_identity_seed;
    uint32_t tunnel_server_domain;
  };

  struct PairedInfo {
    PairedInfo();
    ~PairedInfo();
    PairedInfo(const PairedInfo&) = delete;
    PairedInfo& operator=(const PairedInfo&) = delete;

    std::array<uint8_t, kEIDKeySize> eid_encryption_key;
    std::array<uint8_t, kP256X962Length> peer_identity;
    std::vector<uint8_t> secret;
    base::Optional<CableEidArray> decrypted_eid;
    base::Optional<std::array<uint8_t, 32>> psk;
    base::Optional<std::vector<uint8_t>> handshake_message;
    base::OnceClosure pairing_is_invalid;
  };

  void OnTunnelReady(
      WebSocketAdapter::Result result,
      base::Optional<std::array<uint8_t, kRoutingIdSize>> routing_id);
  void OnTunnelData(base::Optional<base::span<const uint8_t>> data);
  void OnError();
  void MaybeFlushPendingMessage();

  State state_ = State::kConnecting;
  absl::variant<QRInfo, PairedInfo> info_;
  const std::array<uint8_t, 8> id_;
  std::unique_ptr<WebSocketAdapter> websocket_client_;
  base::Optional<HandshakeInitiator> handshake_;
  base::Optional<HandshakeHash> handshake_hash_;
  std::unique_ptr<Crypter> crypter_;
  std::vector<uint8_t> getinfo_response_bytes_;
  std::vector<uint8_t> pending_message_;
  DeviceCallback callback_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<FidoTunnelDevice> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FidoTunnelDevice);
};

}  // namespace cablev2
}  // namespace device

#endif  // DEVICE_FIDO_CABLE_FIDO_TUNNEL_DEVICE_H_
