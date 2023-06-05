// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_FIDO_TUNNEL_DEVICE_H_
#define DEVICE_FIDO_CABLE_FIDO_TUNNEL_DEVICE_H_

#include <array>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/websocket_adapter.h"
#include "device/fido/fido_constants.h"
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
      absl::optional<base::RepeatingCallback<void(std::unique_ptr<Pairing>)>>
          pairing_callback,
      absl::optional<base::RepeatingCallback<void(Event)>> event_callback,
      base::span<const uint8_t> secret,
      base::span<const uint8_t, kQRSeedSize> local_identity_seed,
      const CableEidArray& decrypted_eid);

  // This constructor is used for pairing-initiated connections. If the given
  // |Pairing| is reported by the tunnel server to be invalid (which can happen
  // if the user opts to unlink all devices) then |pairing_is_invalid| is
  // run.
  FidoTunnelDevice(
      FidoRequestType request_type,
      network::mojom::NetworkContext* network_context,
      std::unique_ptr<Pairing> pairing,
      base::OnceClosure pairing_is_invalid,
      absl::optional<base::RepeatingCallback<void(Event)>> event_callback);

  FidoTunnelDevice(const FidoTunnelDevice&) = delete;
  FidoTunnelDevice& operator=(const FidoTunnelDevice&) = delete;

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

  // GetNumEstablishedConnectionInstancesForTesting returns the current number
  // of live |EstablishedConnection| objects. This is only for testing that
  // they aren't leaking.
  static int GetNumEstablishedConnectionInstancesForTesting();

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
    //  kWaitingForEID / kWaitingForEIDOrConnectSignal  |
    //      |                                           |
    //   (BLE advert is received and handshake is sent) |
    //      |                                           |
    //      V                                           |
    //   kHandshakeSent / <------------------------------
    //   kWaitingForConnectSignal (if the tunnel server supports this)
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
    kWaitingForConnectSignal,
    kWaitingForEID,
    kWaitingForEIDOrConnectSignal,
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
    absl::optional<base::RepeatingCallback<void(std::unique_ptr<Pairing>)>>
        pairing_callback;
    std::array<uint8_t, kQRSeedSize> local_identity_seed;
    tunnelserver::KnownDomainID tunnel_server_domain;
  };

  struct PairedInfo {
    PairedInfo();
    ~PairedInfo();
    PairedInfo(const PairedInfo&) = delete;
    PairedInfo& operator=(const PairedInfo&) = delete;

    std::array<uint8_t, kEIDKeySize> eid_encryption_key;
    std::array<uint8_t, kP256X962Length> peer_identity;
    std::vector<uint8_t> secret;
    absl::optional<CableEidArray> decrypted_eid;
    absl::optional<std::array<uint8_t, 32>> psk;
    absl::optional<std::vector<uint8_t>> handshake_message;
    base::OnceClosure pairing_is_invalid;
  };

  // EstablishedConnection represents a connection where the handshake has
  // completed.
  class EstablishedConnection : public base::RefCounted<EstablishedConnection> {
   public:
    EstablishedConnection(std::unique_ptr<WebSocketAdapter> websocket_client,
                          std::string id_for_logging,
                          int protocol_revision,
                          std::unique_ptr<Crypter> crypter,
                          const HandshakeHash& handshake_hash,
                          QRInfo* maybe_qr_info);
    EstablishedConnection(const EstablishedConnection&) = delete;
    EstablishedConnection& operator=(const EstablishedConnection&) = delete;

    void Transact(std::vector<uint8_t> message, DeviceCallback callback);
    void Close();

   private:
    enum class State {
      kRunning,
      kLocallyShutdown,
      kRemoteShutdown,
      kClosed,
    };

    friend class base::RefCounted<EstablishedConnection>;
    ~EstablishedConnection();

    void OnTunnelData(absl::optional<base::span<const uint8_t>> data);
    void OnRemoteClose();
    void OnTimeout();
    bool ProcessUpdate(base::span<const uint8_t> plaintext);

    scoped_refptr<EstablishedConnection> self_reference_;
    State state_ = State::kRunning;
    std::unique_ptr<WebSocketAdapter> websocket_client_;
    const std::string id_for_logging_;
    const int protocol_revision_;
    const std::unique_ptr<Crypter> crypter_;
    const HandshakeHash handshake_hash_;

    // These three fields are either all present or all nullopt.
    absl::optional<base::RepeatingCallback<void(std::unique_ptr<Pairing>)>>
        pairing_callback_;
    absl::optional<std::array<uint8_t, kQRSeedSize>> local_identity_seed_;
    absl::optional<tunnelserver::KnownDomainID> tunnel_server_domain_;

    base::OneShotTimer timer_;
    DeviceCallback callback_;
    SEQUENCE_CHECKER(sequence_checker_);
  };

  void OnTunnelReady(
      WebSocketAdapter::Result result,
      absl::optional<std::array<uint8_t, kRoutingIdSize>> routing_id,
      WebSocketAdapter::ConnectSignalSupport connect_signal_support);
  void OnTunnelData(absl::optional<base::span<const uint8_t>> data);
  void OnError();
  void DeviceTransactReady(std::vector<uint8_t> command,
                           DeviceCallback callback);
  bool ProcessConnectSignal(base::span<const uint8_t> data);

  State state_ = State::kConnecting;
  absl::variant<QRInfo, PairedInfo> info_;
  const std::array<uint8_t, 8> id_;
  const absl::optional<base::RepeatingCallback<void(Event)>> event_callback_;
  std::vector<uint8_t> pending_message_;
  DeviceCallback pending_callback_;
  absl::optional<HandshakeInitiator> handshake_;
  absl::optional<HandshakeHash> handshake_hash_;
  std::vector<uint8_t> getinfo_response_bytes_;

  // These fields are |nullptr| when in state |kReady|.
  std::unique_ptr<WebSocketAdapter> websocket_client_;
  std::unique_ptr<Crypter> crypter_;

  // This is only valid when in state |kReady|.
  scoped_refptr<EstablishedConnection> established_connection_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<FidoTunnelDevice> weak_factory_{this};
};

}  // namespace cablev2
}  // namespace device

#endif  // DEVICE_FIDO_CABLE_FIDO_TUNNEL_DEVICE_H_
