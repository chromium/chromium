// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_tunnel_device.h"

#include "base/metrics/histogram_functions.h"
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

// CableV2TunnelEvent enumerates several steps that occur during establishing a
// caBLEv2 tunnel. Do not change the assigned values since they are used in
// histograms, only append new values. Keep synced with enums.xml.
enum class CableV2TunnelEvent {
  kStartedKeyed = 0,
  kStartedLinked = 1,
  kTunnelOk = 2,
  kTunnelGone = 3,
  kTunnelFailed410 = 4,
  kTunnelFailed = 5,
  kHandshakeFailed = 6,
  kPostHandshakeFailed = 7,
  kTunnelEstablished = 8,
  kDecryptFailed = 9,

  kMaxValue = 9,
};

void RecordEvent(CableV2TunnelEvent event) {
  base::UmaHistogramEnumeration("WebAuthentication.CableV2.TunnelEvent", event);
}

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
    const CableEidArray& decrypted_eid)
    : info_(absl::in_place_type<QRInfo>), id_(RandomId()) {
  const eid::Components components = eid::ToComponents(decrypted_eid);

  QRInfo& info = absl::get<QRInfo>(info_);
  info.pairing_callback = std::move(pairing_callback);
  info.local_identity_seed =
      fido_parsing_utils::Materialize(local_identity_seed);
  info.tunnel_server_domain = components.tunnel_server_domain;

  info.psk =
      Derive<EXTENT(info.psk)>(secret, decrypted_eid, DerivedValueType::kPSK);

  std::array<uint8_t, 16> tunnel_id;
  tunnel_id = Derive<EXTENT(tunnel_id)>(secret, base::span<uint8_t>(),
                                        DerivedValueType::kTunnelID);

  const GURL url(tunnelserver::GetConnectURL(components.tunnel_server_domain,
                                             components.routing_id, tunnel_id));
  FIDO_LOG(DEBUG) << GetId() << ": connecting caBLEv2 tunnel: " << url;
  RecordEvent(CableV2TunnelEvent::kStartedKeyed);

  websocket_client_ = std::make_unique<device::cablev2::WebSocketAdapter>(
      base::BindOnce(&FidoTunnelDevice::OnTunnelReady, base::Unretained(this)),
      base::BindRepeating(&FidoTunnelDevice::OnTunnelData,
                          base::Unretained(this)));
  network_context->CreateWebSocket(
      url, {kCableWebSocketProtocol}, net::SiteForCookies(),
      net::IsolationInfo(), /*additional_headers=*/{},
      network::mojom::kBrowserProcessId, url::Origin::Create(url),
      network::mojom::kWebSocketOptionBlockAllCookies,
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      websocket_client_->BindNewHandshakeClientPipe(),
      /*auth_cert_observer=*/mojo::NullRemote(),
      /*auth_handler=*/mojo::NullRemote(),
      /*header_client=*/mojo::NullRemote());
}

FidoTunnelDevice::FidoTunnelDevice(
    network::mojom::NetworkContext* network_context,
    std::unique_ptr<Pairing> pairing,
    base::OnceClosure pairing_is_invalid)
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
  info.pairing_is_invalid = std::move(pairing_is_invalid);

  const GURL url = tunnelserver::GetContactURL(pairing->tunnel_server_domain,
                                               pairing->contact_id);
  FIDO_LOG(DEBUG) << GetId() << ": connecting caBLEv2 tunnel: " << url;
  RecordEvent(CableV2TunnelEvent::kStartedLinked);

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
      network::mojom::kBrowserProcessId, url::Origin::Create(url),
      network::mojom::kWebSocketOptionBlockAllCookies,
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      websocket_client_->BindNewHandshakeClientPipe(),
      /*auth_cert_observer=*/mojo::NullRemote(),
      /*auth_handler=*/mojo::NullRemote(),
      /*header_client=*/mojo::NullRemote());
}

FidoTunnelDevice::~FidoTunnelDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool FidoTunnelDevice::MatchAdvert(
    const std::array<uint8_t, kAdvertSize>& advert) {
  PairedInfo& info = absl::get<PairedInfo>(info_);

  base::Optional<CableEidArray> plaintext =
      eid::Decrypt(advert, info.eid_encryption_key);
  if (!plaintext) {
    return false;
  }

  info.psk = Derive<EXTENT(*info.psk)>(info.secret, *plaintext,
                                       DerivedValueType::kPSK);

  if (state_ == State::kWaitingForEID) {
    // We were waiting for this BLE advert in order to start the handshake.
    DCHECK(!handshake_);
    handshake_.emplace(*info.psk, info.peer_identity,
                       /*local_identity=*/base::nullopt);
    websocket_client_->Write(handshake_->BuildInitialMessage());
    state_ = State::kHandshakeSent;
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
    if (state_ == State::kReady) {
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
    WebSocketAdapter::Result result,
    base::Optional<std::array<uint8_t, kRoutingIdSize>> routing_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kConnecting, state_);

  switch (result) {
    case WebSocketAdapter::Result::OK:
      DCHECK(!handshake_);
      RecordEvent(CableV2TunnelEvent::kTunnelOk);

      if (auto* info = absl::get_if<QRInfo>(&info_)) {
        // A QR handshake can start as soon as the tunnel is connected.
        handshake_.emplace(info->psk, /*peer_identity=*/base::nullopt,
                           info->local_identity_seed);
      } else {
        // A paired handshake may be able to start if we have already seen
        // the BLE advert.
        PairedInfo& paired_info = absl::get<PairedInfo>(info_);
        if (paired_info.psk) {
          handshake_.emplace(*paired_info.psk, paired_info.peer_identity,
                             /*local_identity=*/base::nullopt);
        }
      }

      if (handshake_) {
        websocket_client_->Write(handshake_->BuildInitialMessage());
        state_ = State::kHandshakeSent;
      } else {
        state_ = State::kWaitingForEID;
      }
      break;

    case WebSocketAdapter::Result::GONE:
      if (auto* info = absl::get_if<PairedInfo>(&info_)) {
        std::move(info->pairing_is_invalid).Run();
        FIDO_LOG(DEBUG) << GetId()
                        << ": tunnel server reports that contact ID is invalid";
        RecordEvent(CableV2TunnelEvent::kTunnelGone);
      } else {
        FIDO_LOG(ERROR) << GetId()
                        << ": server reported an invalid contact ID for an "
                           "unpaired connection";
        RecordEvent(CableV2TunnelEvent::kTunnelFailed410);
      }
      [[fallthrough]];

    case WebSocketAdapter::Result::FAILED:
      RecordEvent(CableV2TunnelEvent::kTunnelFailed);
      FIDO_LOG(DEBUG) << GetId() << ": tunnel failed to connect";
      OnError();
      break;
  }
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

    case State::kHandshakeSent: {
      // This is the handshake response.
      HandshakeResult result = handshake_->ProcessResponse(*data);
      handshake_.reset();

      if (!result) {
        FIDO_LOG(ERROR) << GetId() << ": caBLEv2 handshake failed";
        RecordEvent(CableV2TunnelEvent::kHandshakeFailed);
        OnError();
        return;
      }
      crypter_ = std::move(result->first);
      handshake_hash_ = result->second;
      state_ = State::kWaitingForPostHandshakeMessage;
      break;
    }

    case State::kWaitingForPostHandshakeMessage: {
      // This is the post-handshake message that contains the getInfo response
      // and, optionally, linking information.
      std::vector<uint8_t> decrypted;
      if (!crypter_->Decrypt(*data, &decrypted)) {
        FIDO_LOG(ERROR)
            << GetId()
            << ": decryption failed for caBLE post-handshake message";
        RecordEvent(CableV2TunnelEvent::kPostHandshakeFailed);
        OnError();
        return;
      }
      base::Optional<cbor::Value> payload = DecodePaddedCBORMap(decrypted);
      if (!payload || !payload->is_map()) {
        FIDO_LOG(ERROR) << GetId()
                        << ": decode failed for caBLE post-handshake message";
        RecordEvent(CableV2TunnelEvent::kPostHandshakeFailed);
        OnError();
        return;
      }
      const cbor::Value::MapValue& map = payload->GetMap();

      const cbor::Value::MapValue::const_iterator getinfo_it =
          map.find(cbor::Value(1));
      if (getinfo_it == map.end() || !getinfo_it->second.is_bytestring()) {
        FIDO_LOG(ERROR)
            << GetId()
            << ": caBLE post-handshake message missing getInfo response";
        RecordEvent(CableV2TunnelEvent::kPostHandshakeFailed);
        OnError();
        return;
      }
      getinfo_response_bytes_ = getinfo_it->second.GetBytestring();

      // Linking information is always optional. Currently it is ignored outside
      // of a QR handshake but, in future, we may need to be able to update
      // linking information.
      const cbor::Value::MapValue::const_iterator linking_it =
          map.find(cbor::Value(2));
      if (linking_it != map.end()) {
        if (!linking_it->second.is_map()) {
          FIDO_LOG(ERROR)
              << GetId()
              << ": invalid linking data in caBLE post-handshake message";
          RecordEvent(CableV2TunnelEvent::kPostHandshakeFailed);
          OnError();
          return;
        }
        if (auto* info = absl::get_if<QRInfo>(&info_)) {
          base::Optional<std::unique_ptr<Pairing>> maybe_pairing =
              Pairing::Parse(linking_it->second, info->tunnel_server_domain,
                             info->local_identity_seed, *handshake_hash_);
          if (!maybe_pairing) {
            FIDO_LOG(ERROR)
                << GetId()
                << ": invalid linking data in caBLE post-handshake message";
            RecordEvent(CableV2TunnelEvent::kPostHandshakeFailed);
            OnError();
            return;
          }

          std::move(info->pairing_callback).Run(std::move(*maybe_pairing));
        }
      }

      RecordEvent(CableV2TunnelEvent::kTunnelEstablished);
      state_ = State::kReady;
      MaybeFlushPendingMessage();
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
        RecordEvent(CableV2TunnelEvent::kDecryptFailed);
        OnError();
        return;
      }

      std::move(callback_).Run(std::move(plaintext));
      break;
    }
  }
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
