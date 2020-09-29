// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_FIDO_TUNNEL_DEVICE_H_
#define DEVICE_FIDO_CABLE_FIDO_TUNNEL_DEVICE_H_

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
      const CableEidArray& eid,
      const CableEidArray& decrypted_eid);

  // This constructor is used for pairing-initiated connections.
  FidoTunnelDevice(network::mojom::NetworkContext* network_context,
                   std::unique_ptr<Pairing> pairing);

  ~FidoTunnelDevice() override;

  // MatchEID is only valid for a pairing-initiated connection. It returns true
  // if the given |eid| matched this pending tunnel and thus this device is now
  // ready.
  bool MatchEID(const CableEidArray& eid);

  // FidoDevice:
  CancelToken DeviceTransact(std::vector<uint8_t> command,
                             DeviceCallback callback) override;
  void Cancel(CancelToken token) override;
  std::string GetId() const override;
  FidoTransportProtocol DeviceTransport() const override;
  base::WeakPtr<FidoDevice> GetWeakPtr() override;

 private:
  enum class State {
    kConnecting,
    kConnected,
    kWaitingForEID,
    kHandshakeProcessed,
    kReady,
    kError,
  };

  struct QRInfo {
    QRInfo();
    ~QRInfo();
    QRInfo(const QRInfo&) = delete;
    QRInfo& operator=(const QRInfo&) = delete;

    CableEidArray eid;
    std::array<uint8_t, 32> psk;
    base::OnceCallback<void(std::unique_ptr<Pairing>)> pairing_callback;
    std::array<uint8_t, kQRSeedSize> local_identity_seed;
    uint32_t tunnel_server_domain;
    base::Optional<HandshakeHash> handshake_hash;
  };

  struct PairedInfo {
    PairedInfo();
    ~PairedInfo();
    PairedInfo(const PairedInfo&) = delete;
    PairedInfo& operator=(const PairedInfo&) = delete;

    std::array<uint8_t, 32> eid_encryption_key;
    std::array<uint8_t, kP256X962Length> peer_identity;
    std::vector<uint8_t> secret;
    base::Optional<CableEidArray> eid;
    base::Optional<std::array<uint8_t, 32>> psk;
    base::Optional<std::vector<uint8_t>> handshake_message;
  };

  void OnTunnelReady(
      bool ok,
      base::Optional<std::array<uint8_t, kRoutingIdSize>> routing_id);
  void OnTunnelData(base::Optional<base::span<const uint8_t>> data);
  void ProcessHandshake(base::span<const uint8_t> data);
  void OnError();
  void MaybeFlushPendingMessage();

  State state_ = State::kConnecting;
  absl::variant<QRInfo, PairedInfo> info_;
  const std::array<uint8_t, 8> id_;
  std::unique_ptr<WebSocketAdapter> websocket_client_;
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
