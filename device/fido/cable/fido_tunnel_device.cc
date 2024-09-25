// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_tunnel_device.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/random.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cbor_extract.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_types.h"
#include "device/fido/network_context_factory.h"
#include "net/storage_access_api/status.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"

using device::cbor_extract::IntKey;
using device::cbor_extract::Is;
using device::cbor_extract::StepOrByte;
using device::cbor_extract::Stop;

namespace device::cablev2 {

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
    NetworkContextFactory network_context_factory,
    std::optional<base::RepeatingCallback<void(std::unique_ptr<Pairing>)>>
        pairing_callback,
    std::optional<base::RepeatingCallback<void(Event)>> event_callback,
    base::span<const uint8_t> secret,
    base::span<const uint8_t, kQRSeedSize> local_identity_seed,
    const CableEidArray& decrypted_eid,
    bool must_support_ctap)
    : info_(std::in_place_type<QRInfo>),
      id_(RandomId()),
      event_callback_(std::move(event_callback)),
      must_support_ctap_(must_support_ctap) {
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
  network_context_factory.Run()->CreateWebSocket(
      url, {kCableWebSocketProtocol}, net::SiteForCookies(),
      net::StorageAccessApiStatus::kNone, net::IsolationInfo(),
      /*additional_headers=*/{}, network::mojom::kBrowserProcessId,
      url::Origin::Create(url), network::mojom::kWebSocketOptionBlockAllCookies,
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      websocket_client_->BindNewHandshakeClientPipe(),
      /*url_loader_network_observer=*/mojo::NullRemote(),
      /*auth_handler=*/mojo::NullRemote(),
      /*header_client=*/mojo::NullRemote(),
      /*throttling_profile_id=*/std::nullopt);
}

FidoTunnelDevice::FidoTunnelDevice(
    FidoRequestType request_type,
    NetworkContextFactory network_context_factory,
    std::unique_ptr<Pairing> pairing,
    base::OnceClosure pairing_is_invalid,
    std::optional<base::RepeatingCallback<void(Event)>> event_callback)
    : info_(std::in_place_type<PairedInfo>),
      id_(RandomId()),
      event_callback_(std::move(event_callback)),
      // Paired connections always use CTAP.
      must_support_ctap_(true) {
  uint8_t client_nonce[kClientNonceSize];
  crypto::RandBytes(client_nonce);

  cbor::Value::MapValue client_payload;
  client_payload.emplace(1, pairing->id);
  client_payload.emplace(2, base::span<const uint8_t>(client_nonce));
  client_payload.emplace(3, RequestTypeToString(request_type));
  const std::optional<std::vector<uint8_t>> client_payload_bytes =
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
  headers.emplace_back(
      network::mojom::HttpHeader::New(kCableSignalConnectionHeader, "true"));
  network_context_factory.Run()->CreateWebSocket(
      url, {kCableWebSocketProtocol}, net::SiteForCookies(),
      net::StorageAccessApiStatus::kNone, net::IsolationInfo(),
      std::move(headers), network::mojom::kBrowserProcessId,
      url::Origin::Create(url), network::mojom::kWebSocketOptionBlockAllCookies,
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      websocket_client_->BindNewHandshakeClientPipe(),
      /*url_loader_network_observer=*/mojo::NullRemote(),
      /*auth_handler=*/mojo::NullRemote(),
      /*header_client=*/mojo::NullRemote(),
      /*throttling_profile_id=*/std::nullopt);
}

FidoTunnelDevice::~FidoTunnelDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kReady) {
    established_connection_->Close();
  }
}

bool FidoTunnelDevice::MatchAdvert(
    const std::array<uint8_t, kAdvertSize>& advert) {
  PairedInfo& info = absl::get<PairedInfo>(info_);

  std::optional<CableEidArray> plaintext =
      eid::Decrypt(advert, info.eid_encryption_key);
  if (!plaintext) {
    return false;
  }

  info.psk = Derive<EXTENT(*info.psk)>(info.secret, *plaintext,
                                       DerivedValueType::kPSK);

  if (state_ == State::kWaitingForEID ||
      state_ == State::kWaitingForEIDOrConnectSignal) {
    // We were waiting for this BLE advert in order to start the handshake.
    DCHECK(!handshake_);
    handshake_.emplace(*info.psk, info.peer_identity,
                       /*local_identity=*/std::nullopt);
    websocket_client_->Write(handshake_->BuildInitialMessage());
    state_ = state_ == State::kWaitingForEID ? State::kHandshakeSent
                                             : State::kWaitingForConnectSignal;
  }

  return true;
}

void FidoTunnelDevice::DiscoverSupportedProtocolAndDeviceInfo(
    base::OnceClosure done) {
  if (features_.has_value() && features_->contains(Feature::kCTAP)) {
    FidoDevice::DiscoverSupportedProtocolAndDeviceInfo(std::move(done));
  } else if (!features_.has_value() && state_ != State::kError) {
    // The post-handshake message hasn't been processed yet.
    discover_callback_ = std::move(done);
  } else {
    // Either this is a non-CTAP2 device, in which case making a getInfo request
    // doesn't make sense, or else `state_` is `kError` and we can't.
    //
    // Here CTAP2 is set to satisfy other parts of the stack, but
    // `DoTransact` CHECKs that it is never used.
    const std::array<uint8_t, kAaguidLength> kZeroAaguid = {0};
    supported_protocol_ = ProtocolVersion::kCtap2;
    device_info_.emplace(
        base::flat_set<ProtocolVersion>{ProtocolVersion::kCtap2},
        base::flat_set<Ctap2Version>{Ctap2Version::kCtap2_0}, kZeroAaguid);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(done));
  }
}

FidoDevice::CancelToken FidoTunnelDevice::DeviceTransact(
    std::vector<uint8_t> command,
    DeviceCallback callback) {
  return DoTransact(MessageType::kCTAP, std::move(command),
                    std::move(callback));
}

FidoDevice::CancelToken FidoTunnelDevice::DeviceTransactJSON(
    std::vector<uint8_t> json,
    DeviceCallback callback) {
  return DoTransact(MessageType::kJSON, std::move(json), std::move(callback));
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
  return FidoTransportProtocol::kHybrid;
}

FidoTunnelDevice* FidoTunnelDevice::GetTunnelDevice() {
  return this;
}

base::flat_set<cablev2::Feature> FidoTunnelDevice::features() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(features_.has_value())
      << "features() was called before it was ready. This should never "
         "happen because no requests should be sent prior the post-handshake "
         "message.";
  return *features_;
}

base::WeakPtr<FidoDevice> FidoTunnelDevice::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

FidoDevice::CancelToken FidoTunnelDevice::DoTransact(MessageType msg_type,
                                                     std::vector<uint8_t> msg,
                                                     DeviceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == State::kError) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return kInvalidCancelToken + 1;
  }
  CHECK_EQ(state_, State::kReady);
  CHECK(msg_type != MessageType::kCTAP || features_->contains(Feature::kCTAP));

  if (msg_type != MessageType::kCTAP || msg.size() != 1 ||
      msg[0] !=
          static_cast<uint8_t>(CtapRequestCommand::kAuthenticatorGetInfo)) {
    established_connection_->Transact(msg_type, std::move(msg),
                                      std::move(callback));
    // TODO: cancelation would be useful, but it depends on the GMSCore action
    // being cancelable on Android, which it currently is not.
    return kInvalidCancelToken + 1;
  }

  CHECK(!getinfo_response_bytes_.empty());
  std::vector<uint8_t> reply;
  reply.reserve(1 + getinfo_response_bytes_.size());
  reply.push_back(static_cast<uint8_t>(CtapDeviceResponseCode::kSuccess));
  reply.insert(reply.end(), getinfo_response_bytes_.begin(),
               getinfo_response_bytes_.end());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(reply)));

  return kInvalidCancelToken + 1;
}

void FidoTunnelDevice::OnTunnelReady(
    WebSocketAdapter::Result result,
    std::optional<std::array<uint8_t, kRoutingIdSize>> routing_id,
    WebSocketAdapter::ConnectSignalSupport connect_signal_support) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kConnecting, state_);

  switch (result) {
    case WebSocketAdapter::Result::OK:
      DCHECK(!handshake_);
      RecordEvent(CableV2TunnelEvent::kTunnelOk);

      if (auto* info = absl::get_if<QRInfo>(&info_)) {
        // A QR handshake can start as soon as the tunnel is connected.
        handshake_.emplace(info->psk, /*peer_identity=*/std::nullopt,
                           info->local_identity_seed);
      } else {
        // A paired handshake may be able to start if we have already seen
        // the BLE advert.
        PairedInfo& paired_info = absl::get<PairedInfo>(info_);
        if (paired_info.psk) {
          handshake_.emplace(*paired_info.psk, paired_info.peer_identity,
                             /*local_identity=*/std::nullopt);
        }
      }

      if (handshake_) {
        websocket_client_->Write(handshake_->BuildInitialMessage());
        state_ = connect_signal_support ==
                         WebSocketAdapter::ConnectSignalSupport::YES
                     ? State::kWaitingForConnectSignal
                     : State::kHandshakeSent;
      } else {
        state_ = connect_signal_support ==
                         WebSocketAdapter::ConnectSignalSupport::YES
                     ? State::kWaitingForEIDOrConnectSignal
                     : State::kWaitingForEID;
      }
      break;

    case WebSocketAdapter::Result::GONE:
      if (auto* info = absl::get_if<PairedInfo>(&info_)) {
        FIDO_LOG(DEBUG) << GetId()
                        << ": tunnel server reports that contact ID is invalid";
        RecordEvent(CableV2TunnelEvent::kTunnelGone);
        std::move(info->pairing_is_invalid).Run();
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
    std::optional<base::span<const uint8_t>> data) {
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

    case State::kWaitingForConnectSignal:
    case State::kWaitingForEIDOrConnectSignal:
      if (!ProcessConnectSignal(*data)) {
        FIDO_LOG(ERROR) << GetId() << ": bad connection signal";
        OnError();
        return;
      }
      if (state_ == State::kWaitingForConnectSignal) {
        state_ = State::kHandshakeSent;
      } else {
        state_ = State::kWaitingForEID;
      }
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

      int protocol_revision = 1;
      std::optional<cbor::Value> payload = cbor::Reader::Read(decrypted);
      if (!payload) {
        // This message was padded in revision zero, which will cause a parse
        // error from `cbor::Reader::Read`.
        protocol_revision = 0;
        payload = DecodePaddedCBORMap(decrypted);
      }
      if (!payload || !payload->is_map()) {
        FIDO_LOG(ERROR) << GetId()
                        << ": decode failed for caBLE post-handshake message";
        RecordEvent(CableV2TunnelEvent::kPostHandshakeFailed);
        OnError();
        return;
      }
      const cbor::Value::MapValue& map = payload->GetMap();

      features_.emplace();
      const cbor::Value::MapValue::const_iterator features_it =
          map.find(cbor::Value(3));
      if (features_it != map.end()) {
        if (!features_it->second.is_array()) {
          FIDO_LOG(ERROR)
              << GetId()
              << ": invalid features data in caBLE post-handshake message";
          RecordEvent(CableV2TunnelEvent::kPostHandshakeFailed);
          OnError();
          return;
        }
        const cbor::Value::ArrayValue& array = features_it->second.GetArray();
        for (const auto& capability : array) {
          if (!capability.is_string()) {
            continue;
          }
          if (capability.GetString() == "dc") {
            features_->insert(Feature::kDigitialIdentities);
          } else if (capability.GetString() == "ctap") {
            features_->insert(Feature::kCTAP);
          }
        }
      } else {
        // If the peer doesn't advertise any features then we assume that CTAP
        // is supported.
        features_->insert(Feature::kCTAP);
      }

      if (must_support_ctap_ && !features_->contains(Feature::kCTAP)) {
        FIDO_LOG(ERROR) << GetId() << ": caBLE device doesn't support CTAP";
        RecordEvent(CableV2TunnelEvent::kPostHandshakeFailed);
        OnError();
        return;
      }

      const cbor::Value::MapValue::const_iterator getinfo_it =
          map.find(cbor::Value(1));
      if (features_->contains(Feature::kCTAP)) {
        if (getinfo_it == map.end() || !getinfo_it->second.is_bytestring()) {
          FIDO_LOG(ERROR)
              << GetId()
              << ": caBLE post-handshake message missing getInfo response";
          RecordEvent(CableV2TunnelEvent::kPostHandshakeFailed);
          OnError();
          return;
        }
        getinfo_response_bytes_ = getinfo_it->second.GetBytestring();
      } else if (getinfo_it != map.end()) {
        FIDO_LOG(ERROR) << GetId()
                        << ": caBLE post-handshake message contained getInfo "
                           "response but didn't advertise CTAP support.";
        RecordEvent(CableV2TunnelEvent::kPostHandshakeFailed);
        OnError();
        return;
      }

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
          std::optional<std::unique_ptr<Pairing>> maybe_pairing =
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

          FIDO_LOG(DEBUG) << "Linking information processed from caBLE device";
          if (info->pairing_callback) {
            info->pairing_callback->Run(std::move(*maybe_pairing));
          }
        }
      } else {
        FIDO_LOG(DEBUG)
            << "Linking information was not received from caBLE device";
      }

      FIDO_LOG(DEBUG) << GetId() << ": established v2." << protocol_revision;
      RecordEvent(CableV2TunnelEvent::kTunnelEstablished);
      state_ = State::kReady;
      if (event_callback_) {
        event_callback_->Run(Event::kReady);
      }

      established_connection_ = base::MakeRefCounted<EstablishedConnection>(
          std::move(websocket_client_), GetId(), protocol_revision,
          std::move(crypter_), *handshake_hash_, absl::get_if<QRInfo>(&info_));

      if (discover_callback_) {
        CHECK(features_.has_value());
        DiscoverSupportedProtocolAndDeviceInfo(std::move(discover_callback_));
      }

      break;
    }

    case State::kReady: {
      // In |kReady| the connection is handled by |established_connection_| and
      // so this should never happen.
      NOTREACHED();
    }
  }
}

void FidoTunnelDevice::OnError() {
  const State previous_state = state_;
  state_ = State::kError;

  if (previous_state == State::kReady) {
    DCHECK(!websocket_client_);
    established_connection_->Close();
    established_connection_.reset();
  } else {
    websocket_client_.reset();
    if (discover_callback_) {
      DiscoverSupportedProtocolAndDeviceInfo(std::move(discover_callback_));
    }
  }
}

bool FidoTunnelDevice::ProcessConnectSignal(base::span<const uint8_t> data) {
  if (data.size() != 1 || data[0] != 0) {
    return false;
  }
  FIDO_LOG(DEBUG) << "caBLE authenticator has connected to the tunnel server.";
  if (event_callback_) {
    event_callback_->Run(Event::kPhoneConnected);
  }
  return true;
}

// g_num_established_connection_instances is incremented when an
// `EstablishedConnection` is created and decremented during its destructor.
// This is purely for checking that none leak in tests.
static int g_num_established_connection_instances;

int FidoTunnelDevice::GetNumEstablishedConnectionInstancesForTesting() {
  return g_num_established_connection_instances;
}

FidoTunnelDevice::EstablishedConnection::EstablishedConnection(
    std::unique_ptr<WebSocketAdapter> websocket_client,
    std::string id_for_logging,
    int protocol_revision,
    std::unique_ptr<Crypter> crypter,
    const HandshakeHash& handshake_hash,
    QRInfo* maybe_qr_info)
    : self_reference_(this),
      websocket_client_(std::move(websocket_client)),
      id_for_logging_(std::move(id_for_logging)),
      protocol_revision_(protocol_revision),
      crypter_(std::move(crypter)),
      handshake_hash_(handshake_hash) {
  g_num_established_connection_instances++;
  websocket_client_->Reparent(base::BindRepeating(
      &EstablishedConnection::OnTunnelData, base::Unretained(this)));
  if (maybe_qr_info) {
    pairing_callback_ = maybe_qr_info->pairing_callback;
    local_identity_seed_ = maybe_qr_info->local_identity_seed;
    tunnel_server_domain_ = maybe_qr_info->tunnel_server_domain;
  }
}

FidoTunnelDevice::EstablishedConnection::~EstablishedConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  g_num_established_connection_instances--;
}

void FidoTunnelDevice::EstablishedConnection::Transact(
    MessageType msg_type,
    std::vector<uint8_t> message,
    DeviceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_ == State::kRunning || state_ == State::kRemoteShutdown);

  if (protocol_revision_ >= 1) {
    message.insert(message.begin(), static_cast<uint8_t>(msg_type));
  }

  if (state_ == State::kRemoteShutdown || !crypter_->Encrypt(&message)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  DCHECK(!callback_);
  expected_reply_type_ = msg_type;
  callback_ = std::move(callback);
  websocket_client_->Write(message);
}

void FidoTunnelDevice::EstablishedConnection::Close() {
  switch (state_) {
    case State::kRunning: {
      if (protocol_revision_ < 1) {
        OnRemoteClose();
        DCHECK_EQ(state_, State::kRemoteShutdown);
        Close();
        return;
      }

      callback_.Reset();
      state_ = State::kLocallyShutdown;
      std::vector<uint8_t> shutdown_msg = {
          static_cast<uint8_t>(MessageType::kShutdown)};
      if (crypter_->Encrypt(&shutdown_msg)) {
        websocket_client_->Write(shutdown_msg);
      }
      timer_.Start(FROM_HERE, base::Minutes(3), this,
                   &EstablishedConnection::OnTimeout);
      break;
    }

    case State::kRemoteShutdown:
      state_ = State::kClosed;
      self_reference_.reset();
      // `this` may be invalid now.
      return;

    case State::kLocallyShutdown:
    case State::kClosed:
      NOTREACHED();
  }
}

void FidoTunnelDevice::EstablishedConnection::OnTunnelData(
    std::optional<base::span<const uint8_t>> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_ == State::kRunning || state_ == State::kLocallyShutdown);

  if (!data) {
    OnRemoteClose();
    // `this` may be invalid now.
    return;
  }

  std::vector<uint8_t> plaintext;
  if (!crypter_->Decrypt(*data, &plaintext)) {
    FIDO_LOG(ERROR) << id_for_logging_
                    << ": decryption failed for caBLE message";
    RecordEvent(CableV2TunnelEvent::kDecryptFailed);
    OnRemoteClose();
    // `this` may be invalid now.
    return;
  }

  if (protocol_revision_ >= 1) {
    if (plaintext.empty()) {
      FIDO_LOG(ERROR) << id_for_logging_ << ": invalid empty message";
      OnRemoteClose();
      // `this` may be invalid now.
      return;
    }

    const uint8_t message_type_byte = plaintext[0];
    plaintext.erase(plaintext.begin());

    if (message_type_byte > static_cast<uint8_t>(MessageType::kMaxValue)) {
      FIDO_LOG(ERROR) << id_for_logging_ << ": invalid message type "
                      << static_cast<int>(message_type_byte);
      OnRemoteClose();
      // `this` may be invalid now.
      return;
    }

    const MessageType message_type =
        static_cast<MessageType>(message_type_byte);
    switch (message_type) {
      case MessageType::kShutdown:
        // Authenticators don't send shutdown alerts, only clients.
        FIDO_LOG(ERROR) << id_for_logging_ << ": invalid shutdown frame";
        OnRemoteClose();
        // `this` may be invalid now.
        return;

      case MessageType::kCTAP:
      case MessageType::kJSON:
        if (message_type != expected_reply_type_.value()) {
          FIDO_LOG(ERROR) << "incorrect message type in reply";
          OnRemoteClose();
          // `this` may be invalid now.
          return;
        }
        break;

      case MessageType::kUpdate: {
        if (!ProcessUpdate(plaintext)) {
          FIDO_LOG(ERROR) << id_for_logging_ << ": invalid update frame";
          OnRemoteClose();
          // `this` may be invalid now.
          return;
        }
        return;
      }
    }
  }

  if (!callback_) {
    if (state_ == State::kLocallyShutdown) {
      // If locally shutdown then `callback_` will have been erased this so
      // might have been a valid reply.
      return;
    }

    FIDO_LOG(ERROR) << id_for_logging_
                    << ": unsolicited CTAP message from caBLE device";
    OnRemoteClose();
    // `this` may be invalid now.
    return;
  }

  std::move(callback_).Run(std::move(plaintext));
}

bool FidoTunnelDevice::EstablishedConnection::ProcessUpdate(
    base::span<const uint8_t> plaintext) {
  std::optional<cbor::Value> payload = cbor::Reader::Read(plaintext);
  if (!payload || !payload->is_map()) {
    return false;
  }
  const cbor::Value::MapValue& map = payload->GetMap();
  const cbor::Value::MapValue::const_iterator linking_it =
      map.find(cbor::Value(1));
  if (linking_it != map.end()) {
    if (!linking_it->second.is_map()) {
      return false;
    }
    if (!pairing_callback_) {
      FIDO_LOG(DEBUG) << id_for_logging_
                      << ": unexpected linking information was discarded";
      return true;
    }

    std::optional<std::unique_ptr<Pairing>> maybe_pairing =
        Pairing::Parse(linking_it->second, *tunnel_server_domain_,
                       *local_identity_seed_, handshake_hash_);
    if (!maybe_pairing) {
      return false;
    }

    FIDO_LOG(DEBUG) << id_for_logging_ << ": received linking information";
    pairing_callback_->Run(std::move(*maybe_pairing));
  }

  return true;
}

void FidoTunnelDevice::EstablishedConnection::OnRemoteClose() {
  websocket_client_.reset();

  switch (state_) {
    case State::kRunning:
      state_ = State::kRemoteShutdown;
      if (callback_) {
        std::move(callback_).Run(std::nullopt);
      }
      break;

    case State::kLocallyShutdown:
      state_ = State::kClosed;
      self_reference_.reset();
      // `this` may be invalid now.
      return;

    case State::kRemoteShutdown:
    case State::kClosed:
      NOTREACHED();
  }
}

void FidoTunnelDevice::EstablishedConnection::OnTimeout() {
  DCHECK(state_ == State::kLocallyShutdown);
  FIDO_LOG(DEBUG) << id_for_logging_ << ": closing connection due to timeout";
  OnRemoteClose();
}

}  // namespace device::cablev2
