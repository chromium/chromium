// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_tunnel_device.h"

#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/random.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cbor_extract.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"

using device::cbor_extract::IntKey;
using device::cbor_extract::Is;
using device::cbor_extract::StepOrByte;
using device::cbor_extract::Stop;

namespace device {
namespace cablev2 {

namespace {

std::array<uint8_t, 8> RandomId() {
  std::array<uint8_t, 8> ret;
  crypto::RandBytes(ret);
  return ret;
}

}  // namespace

FidoTunnelDevice::QRInfo::QRInfo() = default;
FidoTunnelDevice::QRInfo::~QRInfo() = default;
FidoTunnelDevice::PairedInfo::PairedInfo() = default;
FidoTunnelDevice::PairedInfo::~PairedInfo() = default;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("cablev2_websocket_from_client", R"(
        semantics {
          sender: "Phone as a Security Key"
          description:
            "Chrome can communicate with a phone for the purpose of using "
            "the phone as a security key. This WebSocket connection is made to "
            "a rendezvous service of the phone's choosing. Mostly likely that "
            "is a Google service because the phone-side is being handled by "
            "Chrome on that device. The service carries only end-to-end "
            "encrypted data where the keys are shared directly between the "
            "client and phone via QR code and Bluetooth broadcast."
          trigger:
            "A web-site initiates a WebAuthn request and the user scans a QR "
            "code with their phone."
          data: "Only encrypted data that the service does not have the keys "
                "for."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "Not controlled by a setting because the operation is "
            "triggered by significant user action."
          policy_exception_justification:
            "No policy provided because the operation is triggered by "
            " significant user action. No background activity occurs."
        })");

FidoTunnelDevice::FidoTunnelDevice(
    network::mojom::NetworkContext* network_context,
    base::OnceCallback<void(std::unique_ptr<Pairing>)> pairing_callback,
    base::span<const uint8_t> secret,
    base::span<const uint8_t, kQRSeedSize> local_identity_seed,
    const CableEidArray& eid,
    const CableEidArray& decrypted_eid)
    : info_(absl::in_place_type<QRInfo>), id_(RandomId()) {
  DCHECK(eid::IsValid(decrypted_eid));
  const eid::Components components = eid::ToComponents(decrypted_eid);

  QRInfo& info = absl::get<QRInfo>(info_);
  info.pairing_callback = std::move(pairing_callback);
  info.eid = eid;
  info.local_identity_seed =
      fido_parsing_utils::Materialize(local_identity_seed);
  info.tunnel_server_domain = components.tunnel_server_domain;

  info.psk = Derive<EXTENT(info.psk)>(secret, components.nonce,
                                      DerivedValueType::kPSK);

  std::array<uint8_t, 16> tunnel_id;
  tunnel_id = Derive<EXTENT(tunnel_id)>(secret, components.nonce,
                                        DerivedValueType::kTunnelID);

  const GURL url(tunnelserver::GetConnectURL(components.tunnel_server_domain,
                                             components.routing_id, tunnel_id));
  FIDO_LOG(DEBUG) << GetId() << ": connecting caBLEv2 tunnel: " << url;

  websocket_client_ = std::make_unique<device::cablev2::WebSocketAdapter>(
      base::BindOnce(&FidoTunnelDevice::OnTunnelReady, base::Unretained(this)),
      base::BindRepeating(&FidoTunnelDevice::OnTunnelData,
                          base::Unretained(this)));
  network_context->CreateWebSocket(
      url, {kCableWebSocketProtocol}, net::SiteForCookies(),
      net::IsolationInfo(), /*additional_headers=*/{},
      network::mojom::kBrowserProcessId,
      /*render_frame_id=*/0, url::Origin::Create(url),
      network::mojom::kWebSocketOptionBlockAllCookies,
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      websocket_client_->BindNewHandshakeClientPipe(), mojo::NullRemote(),
      mojo::NullRemote());
}

FidoTunnelDevice::FidoTunnelDevice(
    network::mojom::NetworkContext* network_context,
    std::unique_ptr<Pairing> pairing)
    : info_(absl::in_place_type<PairedInfo>), id_(RandomId()) {
  uint8_t client_nonce[kClientNonceSize];
  crypto::RandBytes(client_nonce);

  cbor::Value::MapValue client_payload;
  client_payload.emplace(1, pairing->id);
  client_payload.emplace(2, base::span<const uint8_t>(client_nonce));
  const base::Optional<std::vector<uint8_t>> client_payload_bytes =
      cbor::Writer::Write(cbor::Value(std::move(client_payload)));
  CHECK(client_payload_bytes.has_value());
  const std::string client_payload_hex = base::HexEncode(*client_payload_bytes);

  PairedInfo& info = absl::get<PairedInfo>(info_);
  info.eid_encryption_key = Derive<EXTENT(info.eid_encryption_key)>(
      pairing->secret, client_nonce, DerivedValueType::kEIDKey);
  info.peer_identity = pairing->peer_public_key_x962;
  info.secret = pairing->secret;

  const GURL url = tunnelserver::GetContactURL(pairing->tunnel_server_domain,
                                               pairing->contact_id);
  FIDO_LOG(DEBUG) << GetId() << ": connecting caBLEv2 tunnel: " << url;

  websocket_client_ = std::make_unique<device::cablev2::WebSocketAdapter>(
      base::BindOnce(&FidoTunnelDevice::OnTunnelReady, base::Unretained(this)),
      base::BindRepeating(&FidoTunnelDevice::OnTunnelData,
                          base::Unretained(this)));
  std::vector<network::mojom::HttpHeaderPtr> headers;
  headers.emplace_back(network::mojom::HttpHeader::New(
      kCableClientPayloadHeader, client_payload_hex));
  network_context->CreateWebSocket(
      url, {kCableWebSocketProtocol}, net::SiteForCookies(),
      net::IsolationInfo(), std::move(headers),
      network::mojom::kBrowserProcessId,
      /*render_frame_id=*/0, url::Origin::Create(url),
      network::mojom::kWebSocketOptionBlockAllCookies,
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      websocket_client_->BindNewHandshakeClientPipe(), mojo::NullRemote(),
      mojo::NullRemote());
}

FidoTunnelDevice::~FidoTunnelDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool FidoTunnelDevice::MatchEID(const CableEidArray& eid) {
  PairedInfo& info = absl::get<PairedInfo>(info_);

  AES_KEY key;
  CHECK(AES_set_decrypt_key(info.eid_encryption_key.data(),
                            /*bits=*/8 * info.eid_encryption_key.size(),
                            &key) == 0);
  CableEidArray plaintext;
  static_assert(EXTENT(plaintext) == AES_BLOCK_SIZE, "EIDs are not AES blocks");
  AES_decrypt(/*in=*/eid.data(), /*out=*/plaintext.data(), &key);

  if (!eid::IsValid(plaintext)) {
    return false;
  }

  const eid::Components components = eid::ToComponents(plaintext);
  static_assert(EXTENT(components.routing_id) == 3, "");
  if (components.routing_id[0] || components.routing_id[1] ||
      components.routing_id[2]) {
    return false;
  }

  info.eid = eid;
  info.psk = Derive<EXTENT(*info.psk)>(info.secret, components.nonce,
                                       DerivedValueType::kPSK);

  if (state_ == State::kWaitingForEID) {
    // The handshake message has already been received. It can now be answered.
    ProcessHandshake(*info.handshake_message);
  }

  return true;
}

FidoDevice::CancelToken FidoTunnelDevice::DeviceTransact(
    std::vector<uint8_t> command,
    DeviceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback_);

  if (state_ == State::kError) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
  } else {
    pending_message_ = std::move(command);
    callback_ = std::move(callback);
    if (state_ == State::kHandshakeProcessed || state_ == State::kReady) {
      MaybeFlushPendingMessage();
    }
  }

  // TODO: cancelation would be useful, but it depends on the GMSCore action
  // being cancelable on Android, which it currently is not.
  return kInvalidCancelToken + 1;
}

void FidoTunnelDevice::Cancel(CancelToken token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::string FidoTunnelDevice::GetId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return "tunnel-" + base::HexEncode(id_);
}

FidoTransportProtocol FidoTunnelDevice::DeviceTransport() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy;
}

base::WeakPtr<FidoDevice> FidoTunnelDevice::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void FidoTunnelDevice::OnTunnelReady(
    bool ok,
    base::Optional<std::array<uint8_t, kRoutingIdSize>> routing_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kConnecting, state_);

  if (!ok) {
    FIDO_LOG(DEBUG) << GetId() << ": tunnel failed to connect";
    OnError();
    return;
  }

  state_ = State::kConnected;
}

void FidoTunnelDevice::OnTunnelData(
    base::Optional<base::span<const uint8_t>> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!data) {
    OnError();
    return;
  }

  switch (state_) {
    case State::kError:
      break;

    case State::kConnecting:
    case State::kWaitingForEID:
      OnError();
      break;

    case State::kConnected: {
      ProcessHandshake(*data);
      break;
    }

    case State::kHandshakeProcessed: {
      // This is the post-handshake message that optionally contains pairing
      // information.
      std::vector<uint8_t> decrypted;
      if (!crypter_->Decrypt(*data, &decrypted)) {
        FIDO_LOG(ERROR) << GetId()
                        << ": decryption failed for caBLE pairing message";
        OnError();
        return;
      }
      base::Optional<cbor::Value> payload = DecodePaddedCBORMap(decrypted);
      if (!payload || !payload->is_map()) {
        FIDO_LOG(ERROR) << GetId()
                        << ": decode failed for caBLE pairing message";
        OnError();
        return;
      }

      // The map may be empty if the peer doesn't wish to send pairing
      // information.
      if (!payload->GetMap().empty()) {
        QRInfo& info = absl::get<QRInfo>(info_);
        base::Optional<std::unique_ptr<Pairing>> maybe_pairing =
            Pairing::Parse(*payload, info.tunnel_server_domain,
                           info.local_identity_seed, *info.handshake_hash);
        if (!maybe_pairing) {
          FIDO_LOG(ERROR) << GetId() << ": invalid caBLE pairing message";
          OnError();
          return;
        }

        std::move(info.pairing_callback).Run(std::move(*maybe_pairing));
      }

      state_ = State::kReady;
      break;
    }

    case State::kReady: {
      if (!callback_) {
        OnError();
        return;
      }

      std::vector<uint8_t> plaintext;
      if (!crypter_->Decrypt(*data, &plaintext)) {
        FIDO_LOG(ERROR) << GetId() << ": decryption failed for caBLE message";
        OnError();
        return;
      }

      std::move(callback_).Run(std::move(plaintext));
      break;
    }
  }
}

void FidoTunnelDevice::ProcessHandshake(base::span<const uint8_t> data) {
  DCHECK(state_ == State::kWaitingForEID || state_ == State::kConnected);

  std::vector<uint8_t> response;
  base::Optional<ResponderResult> result;

  if (auto* info = absl::get_if<QRInfo>(&info_)) {
    base::Optional<ResponderResult> inner_result(cablev2::RespondToHandshake(
        info->psk, info->eid, info->local_identity_seed, base::nullopt, data,
        &response));
    if (inner_result) {
      result.emplace(std::move(*inner_result));
    }
    state_ = State::kHandshakeProcessed;
  } else if (auto* info = absl::get_if<PairedInfo>(&info_)) {
    if (!info->eid) {
      DCHECK_EQ(state_, State::kConnected);
      state_ = State::kWaitingForEID;
      info->handshake_message = fido_parsing_utils::Materialize(data);
      return;
    }

    base::Optional<ResponderResult> inner_result(
        cablev2::RespondToHandshake(*info->psk, *info->eid,
                                    /*local_identity=*/base::nullopt,
                                    info->peer_identity, data, &response));
    if (inner_result) {
      result.emplace(std::move(*inner_result));
    }
    state_ = State::kReady;
  } else {
    CHECK(false);
  }

  if (!result || result->getinfo_bytes.empty()) {
    FIDO_LOG(ERROR) << GetId() << ": caBLEv2 handshake failed";
    OnError();
    return;
  }

  FIDO_LOG(DEBUG) << GetId() << ": caBLEv2 handshake successful";
  websocket_client_->Write(response);
  if (auto* info = absl::get_if<QRInfo>(&info_)) {
    info->handshake_hash = result->handshake_hash;
  }
  crypter_ = std::move(result->crypter);
  getinfo_response_bytes_ = std::move(result->getinfo_bytes);

  MaybeFlushPendingMessage();
}

void FidoTunnelDevice::OnError() {
  state_ = State::kError;
  websocket_client_.reset();
  if (callback_) {
    std::move(callback_).Run(base::nullopt);
  }
}

void FidoTunnelDevice::MaybeFlushPendingMessage() {
  if (pending_message_.empty()) {
    return;
  }
  std::vector<uint8_t> pending(std::move(pending_message_));

  if (pending.size() == 1 &&
      pending[0] ==
          static_cast<uint8_t>(CtapRequestCommand::kAuthenticatorGetInfo)) {
    DCHECK(!getinfo_response_bytes_.empty());
    std::vector<uint8_t> reply;
    reply.reserve(1 + getinfo_response_bytes_.size());
    reply.push_back(static_cast<uint8_t>(CtapDeviceResponseCode::kSuccess));
    reply.insert(reply.end(), getinfo_response_bytes_.begin(),
                 getinfo_response_bytes_.end());
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), std::move(reply)));
  } else if (crypter_->Encrypt(&pending)) {
    websocket_client_->Write(pending);
  }
}

}  // namespace cablev2
}  // namespace device
