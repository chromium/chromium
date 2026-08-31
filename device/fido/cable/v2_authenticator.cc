// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/v2_authenticator.h"

#include <algorithm>
#include <string_view>
#include <variant>

#include "base/containers/flat_set.h"
#include "base/containers/to_array.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/random.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/cable/websocket_adapter.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/network_context_factory.h"
#include "device/fido/public/features.h"
#include "device/fido/public/fido_constants.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "net/storage_access_api/status.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/obj.h"

namespace device::cablev2::authenticator {

using device::CtapDeviceResponseCode;
using device::CtapRequestCommand;

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("cablev2_websocket_from_authenticator",
                                        R"(semantics {
          sender: "Phone as a Security Key"
          description:
            "Chrome on a phone can communicate with other devices for the "
            "purpose of using the phone as a security key. This WebSocket "
            "connection is made to a Google service that aids in the exchange "
            "of data with the other device. The service carries only "
            "end-to-end encrypted data where the keys are shared directly "
            "between the two devices via QR code and Bluetooth broadcast."
          trigger:
            "The user scans a QR code, displayed on the other device, and "
            "confirms their desire to communicate with it."
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

// BuildGetInfoResponse returns a CBOR-encoded getInfo response.
std::vector<uint8_t> BuildGetInfoResponse() {
  std::array<uint8_t, device::kAaguidLength> aaguid{};
  std::vector<cbor::Value> versions;
  versions.emplace_back("FIDO_2_0");
  versions.emplace_back("FIDO_2_1");

  cbor::Value::MapValue options;
  // This code is only invoked if a screen-lock (i.e. user verification) is
  // configured on the device. Therefore the 'uv' option is unconditionally
  // true.
  options.emplace("uv", true);
  options.emplace("rk", true);

  std::vector<cbor::Value> transports;
  transports.emplace_back("cable");
  transports.emplace_back("hybrid");
  transports.emplace_back("internal");

  cbor::Value::ArrayValue extensions;
  extensions.emplace_back("prf");

  cbor::Value::MapValue response_map;
  response_map.emplace(1, std::move(versions));
  response_map.emplace(2, std::move(extensions));
  response_map.emplace(3, aaguid);
  response_map.emplace(4, std::move(options));
  response_map.emplace(9, std::move(transports));

  return cbor::Writer::Write(cbor::Value(std::move(response_map))).value();
}

std::array<uint8_t, device::cablev2::kNonceSize> RandomNonce() {
  std::array<uint8_t, device::cablev2::kNonceSize> ret;
  crypto::RandBytes(ret);
  return ret;
}

using GeneratePairingDataCallback =
    base::OnceCallback<std::optional<cbor::Value>(
        base::span<const uint8_t, device::kP256X962Length> peer_public_key_x962,
        device::cablev2::HandshakeHash)>;

// TunnelTransport is a transport that uses WebSockets to talk to a cloud
// service and uses BLE adverts to show proximity.
class TunnelTransport : public Transport {
 public:
  TunnelTransport(
      Platform* platform,
      NetworkContextFactory network_context_factory,
      base::span<const uint8_t> secret,
      base::span<const uint8_t, device::kP256X962Length> peer_identity,
      GeneratePairingDataCallback generate_pairing_data,
      base::flat_set<Feature> features)
      : platform_(platform),
        tunnel_id_(device::cablev2::Derive<kTunnelIdSize>(
            secret,
            base::span<uint8_t>(),
            DerivedValueType::kTunnelID)),
        eid_key_(device::cablev2::Derive<kEIDKeySize>(
            secret,
            base::span<const uint8_t>(),
            device::cablev2::DerivedValueType::kEIDKey)),
        network_context_factory_(std::move(network_context_factory)),
        peer_identity_(base::ToArray(peer_identity)),
        generate_pairing_data_(std::move(generate_pairing_data)),
        secret_(base::ToVector(secret)),
        features_(std::move(features)) {
    DCHECK_EQ(state_, State::kNone);
    state_ = State::kConnecting;

    websocket_client_ = std::make_unique<device::cablev2::WebSocketAdapter>(
        base::BindOnce(&TunnelTransport::OnTunnelReady, base::Unretained(this)),
        base::BindRepeating(&TunnelTransport::OnTunnelData,
                            base::Unretained(this)));
    target_ = device::cablev2::tunnelserver::GetNewTunnelURL(kTunnelServer,
                                                             tunnel_id_);
  }

  TunnelTransport(
      Platform* platform,
      NetworkContextFactory network_context_factory,
      base::span<const uint8_t> secret,
      base::span<const uint8_t, device::cablev2::kClientNonceSize> client_nonce,
      std::array<uint8_t, device::cablev2::kRoutingIdSize> routing_id,
      base::span<const uint8_t, 16> tunnel_id,
      bssl::UniquePtr<EC_KEY> local_identity)
      : platform_(platform),
        tunnel_id_(base::ToArray(tunnel_id)),
        eid_key_(device::cablev2::Derive<kEIDKeySize>(
            secret,
            client_nonce,
            device::cablev2::DerivedValueType::kEIDKey)),
        network_context_factory_(network_context_factory),
        secret_(base::ToVector(secret)),
        features_({Feature::kCTAP}),
        local_identity_(std::move(local_identity)) {
    DCHECK_EQ(state_, State::kNone);

    state_ = State::kConnectingPaired;

    websocket_client_ = std::make_unique<device::cablev2::WebSocketAdapter>(
        base::BindOnce(&TunnelTransport::OnTunnelReady, base::Unretained(this)),
        base::BindRepeating(&TunnelTransport::OnTunnelData,
                            base::Unretained(this)));
    target_ = device::cablev2::tunnelserver::GetConnectURL(
        kTunnelServer, routing_id, tunnel_id);
  }

  // Transport:

  void StartReading(
      base::RepeatingCallback<void(Update)> update_callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!update_callback_);

    update_callback_ = std::move(update_callback);

    // Delay the WebSocket creation by 250ms. This to measure whether DNS
    // errors are reduced in UMA stats. If so, then the network errors that we
    // see are probably due to a start-up race.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TunnelTransport::StartWebSocket,
                       weak_factory_.GetWeakPtr()),
        base::Milliseconds(250));
  }

  void Write(PayloadType payload_type, std::vector<uint8_t> data) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_EQ(state_, kReady);

    data.insert(data.begin(),
                static_cast<uint8_t>(payload_type == PayloadType::kCTAP
                                         ? MessageType::kCTAP
                                         : MessageType::kJSON));
    if (!crypter_->Encrypt(&data)) {
      FIDO_LOG(ERROR) << "Failed to encrypt response";
      return;
    }
    websocket_client_->Write(data);
  }

 private:
  enum State {
    kNone,
    kConnecting,
    kConnectingPaired,
    kConnected,
    kConnectedPaired,
    kReady,
  };

  void StartWebSocket() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    uint32_t options = network::mojom::kWebSocketOptionBlockAllCookies;
    if (base::FeatureList::IsEnabled(kWebAuthnSocketMaxPriorityMode)) {
      options |= network::mojom::kWebSocketOptionMaximumPriority;
    }
    network_context_factory_.Run()->CreateWebSocket(
        target_, {device::kCableWebSocketProtocol},
        net::StorageAccessApiStatus::kNone, net::IsolationInfo(),
        /*additional_headers=*/{}, network::OriginatingProcessId::browser(),
        url::Origin::Create(target_),
        network::mojom::ClientSecurityState::New(), options,
        net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
        websocket_client_->BindNewHandshakeClientPipe(),
        /*url_loader_network_observer=*/mojo::NullRemote(),
        /*auth_handler=*/mojo::NullRemote(),
        /*header_client=*/mojo::NullRemote(),
        /*throttling_profile_id=*/std::nullopt,
        // This is a browser-internal connection for the caBLE rendezvous
        // tunnel. It does not belong to any webpage, so we bypass connection
        // allowlists.
        /*network_restrictions_id=*/network::GetNoOpNetworkRestrictionsId(),
        /*target_address_space=*/network::mojom::IPAddressSpace::kUnknown);
    FIDO_LOG(DEBUG) << "Creating WebSocket to " << target_.spec();
  }

  void OnTunnelReady(
      WebSocketAdapter::Result result,
      std::optional<std::array<uint8_t, device::cablev2::kRoutingIdSize>>
          routing_id,
      WebSocketAdapter::ConnectSignalSupport) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(state_ == State::kConnecting || state_ == State::kConnectingPaired);
    bool ok = (result == WebSocketAdapter::Result::OK);

    if (ok && state_ == State::kConnecting && !routing_id) {
      FIDO_LOG(ERROR) << "Tunnel server did not specify routing ID";
      ok = false;
    }

    if (!ok) {
      FIDO_LOG(ERROR) << "Failed to connect to tunnel server";
      update_callback_.Run(Platform::Error::TUNNEL_SERVER_CONNECT_FAILED);
      return;
    }

    FIDO_LOG(DEBUG) << "WebSocket connection established.";

    CableEidArray plaintext_eid;
    if (state_ == State::kConnecting) {
      device::cablev2::eid::Components components;
      components.tunnel_server_domain = kTunnelServer;
      components.routing_id = *routing_id;
      components.nonce = RandomNonce();

      plaintext_eid = device::cablev2::eid::FromComponents(components);
      state_ = State::kConnected;
    } else {
      DCHECK_EQ(state_, State::kConnectingPaired);
      crypto::RandBytes(plaintext_eid);
      // The first byte is reserved to ensure that the format can be changed in
      // the future.
      plaintext_eid[0] = 0;
      state_ = State::kConnectedPaired;
    }

    ble_advert_ =
        platform_->SendBLEAdvert(eid::Encrypt(plaintext_eid, eid_key_));
    psk_ = device::cablev2::Derive<kPSKSize>(
        secret_, plaintext_eid, device::cablev2::DerivedValueType::kPSK);

    update_callback_.Run(Platform::Status::TUNNEL_SERVER_CONNECT);
  }

  void OnTunnelData(std::optional<base::span<const uint8_t>> msg) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!msg) {
      FIDO_LOG(DEBUG) << "WebSocket tunnel closed";
      update_callback_.Run(Disconnected::kDisconnected);
      return;
    }

    switch (state_) {
      case State::kConnectedPaired:
      case State::kConnected: {
        std::vector<uint8_t> response;
        HandshakeResult result = RespondToHandshake(
            psk_, std::move(local_identity_), peer_identity_, *msg, &response);
        if (!result) {
          FIDO_LOG(ERROR) << "caBLE handshake failure";
          update_callback_.Run(Platform::Error::HANDSHAKE_FAILED);
          return;
        }
        FIDO_LOG(DEBUG) << "caBLE handshake complete";
        update_callback_.Run(Platform::Status::HANDSHAKE_COMPLETE);
        websocket_client_->Write(response);
        crypter_ = std::move(result->first);

        cbor::Value::MapValue post_handshake_msg;
        post_handshake_msg.emplace(3, ToCBOR(features_));

        if (features_.contains(Feature::kCTAP)) {
          post_handshake_msg.emplace(1, BuildGetInfoResponse());
        }

        std::optional<std::vector<uint8_t>> post_handshake_msg_bytes;
        post_handshake_msg_bytes =
            cbor::Writer::Write(cbor::Value(std::move(post_handshake_msg)));
        if (!post_handshake_msg_bytes) {
          FIDO_LOG(ERROR) << "failed to encode post-handshake message";
          return;
        }

        if (!crypter_->Encrypt(&post_handshake_msg_bytes.value())) {
          FIDO_LOG(ERROR) << "failed to encrypt post-handshake message";
          return;
        }
        websocket_client_->Write(*post_handshake_msg_bytes);

        if (state_ == State::kConnected) {
          // Linking information can be sent at any time. We always send it
          // immediately after the post-handshake message.
          std::optional<cbor::Value> pairing_data(
              std::move(generate_pairing_data_)
                  .Run(*peer_identity_, result->second));

          // padding_target is the expected size of the plaintext of the update
          // message.
          constexpr size_t kPaddingTarget = 512;

          // padding_length is the length of a bytestring of zeros, included
          // just to hit `kPaddingTarget`. `Encrypt` pads to 32 bytes so we can
          // be a little sloppy here and use simpler code. Thus we aim at 16
          // bytes shy of the target so that it'll be padded up by `Encrypt`.
          static_assert(kPaddingTarget % 32 == 0);
          size_t padding_length =
              kPaddingTarget - 16 -
              /* length of CBOR map key */ 1 -
              /* length of bytestring overhead, assuming a two-byte length */ 3;

          if (pairing_data) {
            cbor::Value::MapValue update_msg_for_measurement;
            update_msg_for_measurement.emplace(1, pairing_data->Clone());
            std::optional<std::vector<uint8_t>> cbor_bytes =
                cbor::Writer::Write(
                    cbor::Value(std::move(update_msg_for_measurement)));

            if (cbor_bytes && cbor_bytes->size() < padding_length) {
              padding_length -= cbor_bytes->size();
            } else {
              DCHECK(false) << cbor_bytes.has_value();
            }
          }

          cbor::Value::MapValue update_msg;
          update_msg.emplace(0, std::vector<uint8_t>(padding_length));
          if (pairing_data) {
            update_msg.emplace(1, std::move(*pairing_data));
          }

          std::optional<std::vector<uint8_t>> update_msg_bytes =
              cbor::Writer::Write(cbor::Value(std::move(update_msg)));
          if (!update_msg_bytes) {
            FIDO_LOG(ERROR) << "failed to encode update message";
            return;
          }
          update_msg_bytes->insert(update_msg_bytes->begin(),
                                   static_cast<uint8_t>(MessageType::kUpdate));

          if (!crypter_->Encrypt(&update_msg_bytes.value())) {
            FIDO_LOG(ERROR) << "failed to encrypt update message";
            return;
          }

          DCHECK_EQ(update_msg_bytes->size(),
                    kPaddingTarget + /* AES-GCM overhead */ 16);
          websocket_client_->Write(*update_msg_bytes);
        }

        state_ = State::kReady;
        break;
      }

      case State::kReady: {
        std::vector<uint8_t> plaintext;
        if (!crypter_->Decrypt(*msg, &plaintext)) {
          FIDO_LOG(ERROR) << "failed to decrypt caBLE message";
          update_callback_.Run(Platform::Error::DECRYPT_FAILURE);
          return;
        }

        if (plaintext.empty()) {
          FIDO_LOG(ERROR) << "invalid empty message";
          update_callback_.Run(Platform::Error::DECRYPT_FAILURE);
          return;
        }

        const uint8_t message_type_byte = plaintext[0];
        plaintext.erase(plaintext.begin());
        if (message_type_byte > static_cast<uint8_t>(MessageType::kMaxValue)) {
          FIDO_LOG(ERROR) << "unknown message type "
                          << static_cast<int>(message_type_byte);
          update_callback_.Run(Disconnected::kDisconnected);
          return;
        }

        const MessageType message_type =
            static_cast<MessageType>(message_type_byte);
        switch (message_type) {
          case MessageType::kShutdown: {
            update_callback_.Run(Disconnected::kDisconnected);
            return;
          }

          case MessageType::kCTAP:
          case MessageType::kJSON:
            break;

          case MessageType::kUpdate:
            // The payload is ignored for now. Maybe there will be desktop
            // updates defined in the future. But we still check that the
            // payload is well-formed.
            if (!cbor::Reader::Read(plaintext)) {
              FIDO_LOG(ERROR) << "invalid CBOR payload in update message";
              update_callback_.Run(Disconnected::kDisconnected);
            }
            return;
        }

        if (first_message_) {
          update_callback_.Run(Platform::Status::REQUEST_RECEIVED);
          first_message_ = false;
        }
        update_callback_.Run(std::make_pair(message_type == MessageType::kJSON
                                                ? PayloadType::kJSON
                                                : PayloadType::kCTAP,
                                            std::move(plaintext)));
        break;
      }

      default:
        NOTREACHED();
    }
  }

  static cbor::Value ToCBOR(const base::flat_set<Feature>& features) {
    cbor::Value::ArrayValue ret;
    for (const auto feature : features) {
      switch (feature) {
        case Feature::kCTAP:
          ret.emplace_back("ctap");
          break;
        case Feature::kDigitialIdentities:
          ret.emplace_back("dc");
          break;
      }
    }
    std::ranges::sort(ret, [](const auto& a, const auto& b) {
      return a.GetString() < b.GetString();
    });
    return cbor::Value(std::move(ret));
  }

  const raw_ptr<Platform, DanglingUntriaged> platform_;
  State state_ = State::kNone;
  const std::array<uint8_t, kTunnelIdSize> tunnel_id_;
  const std::array<uint8_t, kEIDKeySize> eid_key_;
  std::unique_ptr<WebSocketAdapter> websocket_client_;
  std::unique_ptr<Crypter> crypter_;
  NetworkContextFactory network_context_factory_;
  const std::optional<std::array<uint8_t, kP256X962Length>> peer_identity_;
  std::array<uint8_t, kPSKSize> psk_;
  GeneratePairingDataCallback generate_pairing_data_;
  const std::vector<uint8_t> secret_;
  const base::flat_set<Feature> features_;
  bssl::UniquePtr<EC_KEY> local_identity_;
  GURL target_;
  std::unique_ptr<Platform::BLEAdvert> ble_advert_;
  base::RepeatingCallback<void(Update)> update_callback_;
  bool first_message_ = true;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<TunnelTransport> weak_factory_{this};
};

class CTAP2Processor : public Transaction {
 public:
  CTAP2Processor(std::unique_ptr<Transport> transport,
                 std::unique_ptr<Platform> platform)
      : transport_(std::move(transport)), platform_(std::move(platform)) {
    transport_->StartReading(base::BindRepeating(
        &CTAP2Processor::OnTransportUpdate, base::Unretained(this)));
  }

 private:
  void OnTransportUpdate(Transport::Update update) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (have_completed_) {
      // If the owner of this object doesn't destroy it immediately after an
      // error then the transport could continue to send updates. These should
      // not be passed through.
      return;
    }

    if (auto* error = std::get_if<Platform::Error>(&update)) {
      have_completed_ = true;
      platform_->OnCompleted(*error);
      return;
    } else if (auto* status = std::get_if<Platform::Status>(&update)) {
      platform_->OnStatus(*status);
      return;
    } else if (std::get_if<Transport::Disconnected>(&update)) {
      std::optional<Platform::Error> maybe_error;
      if (!transaction_received_) {
        maybe_error = Platform::Error::UNEXPECTED_EOF;
      } else if (!transaction_done_) {
        maybe_error = Platform::Error::EOF_WHILE_PROCESSING;
      }
      have_completed_ = true;
      platform_->OnCompleted(maybe_error);
      return;
    }

    auto& msg = std::get<std::pair<PayloadType, std::vector<uint8_t>>>(update);
    if (msg.first != PayloadType::kCTAP) {
      have_completed_ = true;
      platform_->OnCompleted(Platform::Error::INVALID_CTAP);
      return;
    }
    const std::variant<std::vector<uint8_t>, Platform::Error> result =
        ProcessCTAPMessage(msg.second);
    if (const auto* error = std::get_if<Platform::Error>(&result)) {
      have_completed_ = true;
      platform_->OnCompleted(*error);
      return;
    }

    const std::vector<uint8_t>& response =
        std::get<std::vector<uint8_t>>(result);
    if (response.empty()) {
      // Response is pending.
      return;
    }

    transport_->Write(PayloadType::kCTAP, std::move(response));
  }

  std::variant<std::vector<uint8_t>, Platform::Error> ProcessCTAPMessage(
      base::span<const uint8_t> message_bytes) {
    if (message_bytes.empty()) {
      return Platform::Error::INVALID_CTAP;
    }
    const auto [command, cbor_bytes] = message_bytes.split_at<1>();

    std::optional<cbor::Value> payload;
    if (!cbor_bytes.empty()) {
      payload = cbor::Reader::Read(cbor_bytes);
      if (!payload) {
        FIDO_LOG(ERROR) << "CBOR decoding failed for "
                        << base::HexEncode(cbor_bytes);
        return Platform::Error::INVALID_CTAP;
      }
      FIDO_LOG(DEBUG) << "<- (" << base::HexEncode(command) << ") "
                      << cbor::DiagnosticWriter::Write(*payload);
    } else {
      FIDO_LOG(DEBUG) << "<- (" << base::HexEncode(command) << ") <no payload>";
    }

    switch (command[0]) {
      case static_cast<uint8_t>(
          device::CtapRequestCommand::kAuthenticatorGetInfo): {
        if (payload) {
          FIDO_LOG(ERROR) << "getInfo command incorrectly contained payload";
          return Platform::Error::INVALID_CTAP;
        }

        std::optional<std::vector<uint8_t>> response = BuildGetInfoResponse();
        if (!response) {
          return Platform::Error::INTERNAL_ERROR;
        }
        response->insert(
            response->begin(),
            static_cast<uint8_t>(CtapDeviceResponseCode::kSuccess));
        return *response;
      }

      case static_cast<uint8_t>(
          device::CtapRequestCommand::kAuthenticatorMakeCredential): {
        if (!payload || !payload->is_map()) {
          FIDO_LOG(ERROR) << "Invalid makeCredential payload";
          return Platform::Error::INVALID_CTAP;
        }

        std::optional<CtapMakeCredentialRequest> request =
            CtapMakeCredentialRequest::Parse(payload->GetMap());
        if (!request) {
          FIDO_LOG(ERROR) << "Failed to parse makeCredential request: "
                          << base::HexEncode(cbor_bytes);
          return Platform::Error::INVALID_CTAP;
        }

        const bool rk = request->resident_key_required;

        transaction_received_ = true;
        platform_->MakeCredential(
            std::move(*request),
            base::BindOnce(&CTAP2Processor::OnMakeCredentialResponse,
                           weak_factory_.GetWeakPtr(), rk));
        return std::vector<uint8_t>();
      }

      case static_cast<uint8_t>(
          device::CtapRequestCommand::kAuthenticatorGetAssertion): {
        if (!payload || !payload->is_map()) {
          FIDO_LOG(ERROR) << "Invalid getAssertion payload";
          return Platform::Error::INVALID_CTAP;
        }

        std::optional<CtapGetAssertionRequest> request =
            CtapGetAssertionRequest::Parse(payload->GetMap());
        if (!request) {
          FIDO_LOG(ERROR) << "Failed to parse getAssertion request";
          return Platform::Error::INVALID_CTAP;
        }

        transaction_received_ = true;
        const bool empty_allowlist = request->allow_list.empty();
        platform_->GetAssertion(
            std::move(*request),
            base::BindOnce(&CTAP2Processor::OnGetAssertionResponse,
                           weak_factory_.GetWeakPtr(), empty_allowlist));
        return std::vector<uint8_t>();
      }

      case static_cast<uint8_t>(
          device::CtapRequestCommand::kAuthenticatorSelection): {
        if (payload) {
          FIDO_LOG(ERROR) << "Invalid authenticatorSelection payload";
          return Platform::Error::INVALID_CTAP;
        }
        return Platform::Error::AUTHENTICATOR_SELECTION_RECEIVED;
      }

      default:
        FIDO_LOG(ERROR) << "Received unknown command "
                        << static_cast<unsigned>(command[0]);
        return Platform::Error::INVALID_CTAP;
    }
  }

  void OnMakeCredentialResponse(bool was_discoverable_credential_request,
                                std::vector<uint8_t> response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (response.empty()) {
      response = {static_cast<uint8_t>(CtapDeviceResponseCode::kCtap2ErrOther)};
    }

    const uint8_t ctap_status = response[0];
    if (ctap_status == static_cast<uint8_t>(CtapDeviceResponseCode::kSuccess)) {
      // Success.
    } else if (was_discoverable_credential_request &&
               ctap_status ==
                   static_cast<uint8_t>(
                       CtapDeviceResponseCode::kCtap2ErrUnsupportedOption)) {
      have_completed_ = true;
      platform_->OnCompleted(Platform::Error::DISCOVERABLE_CREDENTIALS_REQUEST);
      return;
    } else {
      platform_->OnStatus(Platform::Status::CTAP_ERROR);
    }

    if (!transaction_done_) {
      platform_->OnStatus(Platform::Status::FIRST_TRANSACTION_DONE);
      transaction_done_ = true;
    }
    transport_->Write(PayloadType::kCTAP, std::move(response));
  }

  void OnGetAssertionResponse(bool was_empty_allowlist_request,
                              std::vector<uint8_t> response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (response.empty()) {
      response = {static_cast<uint8_t>(CtapDeviceResponseCode::kCtap2ErrOther)};
    }

    const uint8_t ctap_status = response[0];
    if (ctap_status == static_cast<uint8_t>(CtapDeviceResponseCode::kSuccess)) {
      // Success.
    } else if (was_empty_allowlist_request &&
               ctap_status ==
                   static_cast<uint8_t>(
                       CtapDeviceResponseCode::kCtap2ErrNoCredentials)) {
      have_completed_ = true;
      platform_->OnCompleted(Platform::Error::DISCOVERABLE_CREDENTIALS_REQUEST);
      return;
    } else {
      platform_->OnStatus(Platform::Status::CTAP_ERROR);
    }

    if (!transaction_done_) {
      platform_->OnStatus(Platform::Status::FIRST_TRANSACTION_DONE);
      transaction_done_ = true;
    }
    transport_->Write(PayloadType::kCTAP, std::move(response));
  }

  bool have_completed_ = false;
  bool transaction_received_ = false;
  bool transaction_done_ = false;
  const std::unique_ptr<Transport> transport_;
  const std::unique_ptr<Platform> platform_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CTAP2Processor> weak_factory_{this};
};

class DigitalIdentityProcessor : public Transaction {
 public:
  DigitalIdentityProcessor(std::unique_ptr<Transport> transport,
                           std::unique_ptr<Platform> platform,
                           PayloadType response_payload_type,
                           std::vector<uint8_t> response)
      : transport_(std::move(transport)),
        platform_(std::move(platform)),
        response_payload_type_(response_payload_type),
        response_(std::move(response)) {
    transport_->StartReading(base::BindRepeating(
        &DigitalIdentityProcessor::OnTransportUpdate, base::Unretained(this)));
  }

 private:
  void OnTransportUpdate(Transport::Update update) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(!have_completed_);

    if (auto* error = std::get_if<Platform::Error>(&update)) {
      have_completed_ = true;
      platform_->OnCompleted(*error);
      return;
    } else if (auto* status = std::get_if<Platform::Status>(&update)) {
      platform_->OnStatus(*status);
      return;
    } else if (std::get_if<Transport::Disconnected>(&update)) {
      have_completed_ = true;
      platform_->OnCompleted(std::nullopt);
      return;
    }

    auto& msg = std::get<std::pair<PayloadType, std::vector<uint8_t>>>(update);
    if (msg.first != PayloadType::kJSON) {
      have_completed_ = true;
      platform_->OnCompleted(Platform::Error::INVALID_JSON);
      return;
    }

    std::optional<base::Value> json = base::JSONReader::Read(
        std::string_view(reinterpret_cast<const char*>(msg.second.data()),
                         msg.second.size()),
        base::JSON_PARSE_RFC);
    if (!json) {
      have_completed_ = true;
      platform_->OnCompleted(Platform::Error::INVALID_JSON);
      return;
    }

    transport_->Write(response_payload_type_, response_);
  }

  const std::unique_ptr<Transport> transport_;
  const std::unique_ptr<Platform> platform_;
  const PayloadType response_payload_type_;
  const std::vector<uint8_t> response_;
  bool have_completed_ = false;
  SEQUENCE_CHECKER(sequence_checker_);
};

static std::array<uint8_t, 32> DerivePairedSecret(
    base::span<const uint8_t, kRootSecretSize> root_secret,
    const std::optional<base::span<const uint8_t>>& contact_id,
    base::span<const uint8_t, kPairingIDSize> pairing_id) {
  base::span<const uint8_t, kRootSecretSize> secret = root_secret;

  std::array<uint8_t, kRootSecretSize> per_contact_id_secret;
  if (contact_id) {
    // The root secret is not used directly to derive the paired secret because
    // we want the keys to change after an unlink. Unlinking invalidates and
    // replaces the contact ID therefore we derive paired secrets in two steps:
    // first using the contact ID to derive a secret from the root secret, and
    // then using the pairing ID to generate a secret from that.
    per_contact_id_secret =
        device::cablev2::Derive<per_contact_id_secret.size()>(
            root_secret, *contact_id,
            device::cablev2::DerivedValueType::kPerContactIDSecret);
    secret = per_contact_id_secret;
  }

  std::array<uint8_t, 32> paired_secret;
  paired_secret = device::cablev2::Derive<paired_secret.size()>(
      secret, pairing_id, device::cablev2::DerivedValueType::kPairedSecret);

  return paired_secret;
}

class PairingDataGenerator {
 public:
  static GeneratePairingDataCallback GetClosure(
      base::span<const uint8_t, kRootSecretSize> root_secret,
      const std::string& name,
      std::optional<std::vector<uint8_t>> contact_id) {
    auto* generator =
        new PairingDataGenerator(root_secret, name, std::move(contact_id));
    return base::BindOnce(&PairingDataGenerator::Generate,
                          base::Owned(generator));
  }

 private:
  PairingDataGenerator(base::span<const uint8_t, kRootSecretSize> root_secret,
                       const std::string& name,
                       std::optional<std::vector<uint8_t>> contact_id)
      : root_secret_(base::ToArray(root_secret)),
        name_(name),
        contact_id_(std::move(contact_id)) {}

  std::optional<cbor::Value> Generate(
      base::span<const uint8_t, device::kP256X962Length> peer_public_key_x962,
      device::cablev2::HandshakeHash handshake_hash) {
    if (!contact_id_) {
      return std::nullopt;
    }

    std::array<uint8_t, kPairingIDSize> pairing_id;
    crypto::RandBytes(pairing_id);
    const std::array<uint8_t, 32> paired_secret =
        DerivePairedSecret(root_secret_, *contact_id_, pairing_id);

    cbor::Value::MapValue map;
    map.emplace(1, std::move(*contact_id_));
    map.emplace(2, pairing_id);
    map.emplace(3, paired_secret);

    bssl::UniquePtr<EC_KEY> identity_key(IdentityKey(root_secret_));
    device::CableAuthenticatorIdentityKey public_key;
    CHECK_EQ(
        public_key.size(),
        EC_POINT_point2oct(EC_KEY_get0_group(identity_key.get()),
                           EC_KEY_get0_public_key(identity_key.get()),
                           POINT_CONVERSION_UNCOMPRESSED, public_key.data(),
                           public_key.size(), /*ctx=*/nullptr));

    map.emplace(4, public_key);
    map.emplace(5, name_);

    map.emplace(6,
                device::cablev2::CalculatePairingSignature(
                    identity_key.get(), peer_public_key_x962, handshake_hash));

    return cbor::Value(std::move(map));
  }

  const std::array<uint8_t, kRootSecretSize> root_secret_;
  const std::string name_;
  std::optional<std::vector<uint8_t>> contact_id_;
};

}  // namespace

Platform::BLEAdvert::~BLEAdvert() = default;
Platform::~Platform() = default;
Transport::~Transport() = default;
Transaction::~Transaction() = default;

std::unique_ptr<Transaction> TransactWithPlaintextTransport(
    std::unique_ptr<Platform> platform,
    std::unique_ptr<Transport> transport) {
  return std::make_unique<CTAP2Processor>(std::move(transport),
                                          std::move(platform));
}

std::unique_ptr<Transaction> TransactFromQRCode(
    std::unique_ptr<Platform> platform,
    NetworkContextFactory network_context_factory,
    base::span<const uint8_t, kRootSecretSize> root_secret,
    const std::string& authenticator_name,
    base::span<const uint8_t, 16> qr_secret,
    base::span<const uint8_t, kP256X962Length> peer_identity,
    std::optional<std::vector<uint8_t>> contact_id) {
  auto generate_pairing_data = PairingDataGenerator::GetClosure(
      root_secret, authenticator_name, std::move(contact_id));

  Platform* const platform_ptr = platform.get();
  return std::make_unique<CTAP2Processor>(
      std::make_unique<TunnelTransport>(
          platform_ptr, std::move(network_context_factory), qr_secret,
          peer_identity, std::move(generate_pairing_data),
          base::flat_set<Feature>{Feature::kCTAP}),
      std::move(platform));
}

std::unique_ptr<Transaction> TransactDigitalIdentityFromQRCodeForTesting(
    std::unique_ptr<Platform> platform,
    NetworkContextFactory network_context_factory,
    base::span<const uint8_t, 16> qr_secret,
    base::span<const uint8_t, kP256X962Length> peer_identity,
    PayloadType response_payload_type,
    std::vector<uint8_t> response) {
  auto no_pairing_data = base::BindOnce(
      [](base::span<const uint8_t, device::kP256X962Length>
             peer_public_key_x962,
         device::cablev2::HandshakeHash) -> std::optional<cbor::Value> {
        return std::nullopt;
      });

  Platform* const platform_ptr = platform.get();
  return std::make_unique<DigitalIdentityProcessor>(
      std::make_unique<TunnelTransport>(
          platform_ptr, std::move(network_context_factory), qr_secret,
          peer_identity, std::move(no_pairing_data),
          base::flat_set<Feature>{Feature::kDigitialIdentities}),
      std::move(platform), response_payload_type, std::move(response));
}

std::unique_ptr<Transaction> TransactFromQRCodeDeprecated(
    std::unique_ptr<Platform> platform,
    network::mojom::NetworkContext* network_context,
    base::span<const uint8_t, kRootSecretSize> root_secret,
    const std::string& authenticator_name,
    base::span<const uint8_t, 16> qr_secret,
    base::span<const uint8_t, kP256X962Length> peer_identity,
    std::optional<std::vector<uint8_t>> contact_id) {
  NetworkContextFactory factory = base::BindRepeating(
      [](network::mojom::NetworkContext* network_context) {
        return network_context;
      },
      network_context);
  return TransactFromQRCode(std::move(platform), std::move(factory),
                            root_secret, authenticator_name, qr_secret,
                            peer_identity, std::move(contact_id));
}

std::unique_ptr<Transaction> TransactFromFCM(
    std::unique_ptr<Platform> platform,
    NetworkContextFactory network_context_factory,
    base::span<const uint8_t, kRootSecretSize> root_secret,
    std::array<uint8_t, kRoutingIdSize> routing_id,
    base::span<const uint8_t, kTunnelIdSize> tunnel_id,
    base::span<const uint8_t, kPairingIDSize> pairing_id,
    base::span<const uint8_t, kClientNonceSize> client_nonce,
    std::optional<base::span<const uint8_t>> contact_id) {
  const std::array<uint8_t, 32> paired_secret =
      DerivePairedSecret(root_secret, contact_id, pairing_id);

  Platform* const platform_ptr = platform.get();
  return std::make_unique<CTAP2Processor>(
      std::make_unique<TunnelTransport>(
          platform_ptr, std::move(network_context_factory), paired_secret,
          client_nonce, routing_id, tunnel_id, IdentityKey(root_secret)),
      std::move(platform));
}

std::unique_ptr<Transaction> TransactFromFCMDeprecated(
    std::unique_ptr<Platform> platform,
    network::mojom::NetworkContext* network_context,
    base::span<const uint8_t, kRootSecretSize> root_secret,
    std::array<uint8_t, kRoutingIdSize> routing_id,
    base::span<const uint8_t, kTunnelIdSize> tunnel_id,
    base::span<const uint8_t, kPairingIDSize> pairing_id,
    base::span<const uint8_t, kClientNonceSize> client_nonce,
    std::optional<base::span<const uint8_t>> contact_id) {
  NetworkContextFactory factory = base::BindRepeating(
      [](network::mojom::NetworkContext* network_context) {
        return network_context;
      },
      network_context);
  return TransactFromFCM(std::move(platform), std::move(factory), root_secret,
                         std::move(routing_id), tunnel_id, pairing_id,
                         client_nonce, std::move(contact_id));
}

}  // namespace device::cablev2::authenticator
