// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/v2_authenticator.h"

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/random.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/cable/websocket_adapter.h"
#include "device/fido/cbor_extract.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/obj.h"

namespace device {
namespace cablev2 {
namespace authenticator {

using device::CtapDeviceResponseCode;
using device::CtapRequestCommand;
using device::cbor_extract::IntKey;
using device::cbor_extract::Is;
using device::cbor_extract::Map;
using device::cbor_extract::StepOrByte;
using device::cbor_extract::Stop;
using device::cbor_extract::StringKey;

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

// kTunnelServer is the hardcoded tunnel server that phones will use for network
// communication. This specifies a Google service and the short domain need is
// necessary to fit within a BLE advert.
constexpr uint16_t kTunnelServer = 0;

struct MakeCredRequest {
  const std::vector<uint8_t>* client_data_hash;
  const std::string* rp_id;
  const std::vector<uint8_t>* user_id;
  const cbor::Value::ArrayValue* cred_params;
  const cbor::Value::ArrayValue* excluded_credentials;
};

static constexpr StepOrByte<MakeCredRequest> kMakeCredParseSteps[] = {
    // clang-format off
    ELEMENT(Is::kRequired, MakeCredRequest, client_data_hash),
    IntKey<MakeCredRequest>(1),

    Map<MakeCredRequest>(),
    IntKey<MakeCredRequest>(2),
      ELEMENT(Is::kRequired, MakeCredRequest, rp_id),
      StringKey<MakeCredRequest>(), 'i', 'd', '\0',
    Stop<MakeCredRequest>(),

    Map<MakeCredRequest>(),
    IntKey<MakeCredRequest>(3),
      ELEMENT(Is::kRequired, MakeCredRequest, user_id),
      StringKey<MakeCredRequest>(), 'i', 'd', '\0',
    Stop<MakeCredRequest>(),

    ELEMENT(Is::kRequired, MakeCredRequest, cred_params),
    IntKey<MakeCredRequest>(4),
    ELEMENT(Is::kOptional, MakeCredRequest, excluded_credentials),
    IntKey<MakeCredRequest>(5),

    Stop<MakeCredRequest>(),
    // clang-format on
};

struct AttestationObject {
  const std::string* fmt;
  const std::vector<uint8_t>* auth_data;
  const cbor::Value* statement;
};

static constexpr StepOrByte<AttestationObject> kAttObjParseSteps[] = {
    // clang-format off
    ELEMENT(Is::kRequired, AttestationObject, fmt),
    StringKey<AttestationObject>(), 'f', 'm', 't', '\0',

    ELEMENT(Is::kRequired, AttestationObject, auth_data),
    StringKey<AttestationObject>(), 'a', 'u', 't', 'h', 'D', 'a', 't', 'a',
                                    '\0',

    ELEMENT(Is::kRequired, AttestationObject, statement),
    StringKey<AttestationObject>(), 'a', 't', 't', 'S', 't', 'm', 't', '\0',
    Stop<AttestationObject>(),
    // clang-format on
};

struct GetAssertionRequest {
  const std::string* rp_id;
  const std::vector<uint8_t>* client_data_hash;
  const cbor::Value::ArrayValue* allowed_credentials;
};

static constexpr StepOrByte<GetAssertionRequest> kGetAssertionParseSteps[] = {
    // clang-format off
    ELEMENT(Is::kRequired, GetAssertionRequest, rp_id),
    IntKey<GetAssertionRequest>(1),

    ELEMENT(Is::kRequired, GetAssertionRequest, client_data_hash),
    IntKey<GetAssertionRequest>(2),

    ELEMENT(Is::kOptional, GetAssertionRequest, allowed_credentials),
    IntKey<GetAssertionRequest>(3),

    Stop<GetAssertionRequest>(),
    // clang-format on
};

// BuildGetInfoResponse returns a CBOR-encoded getInfo response.
std::vector<uint8_t> BuildGetInfoResponse() {
  std::array<uint8_t, device::kAaguidLength> aaguid{};
  std::vector<cbor::Value> versions;
  versions.emplace_back("FIDO_2_0");
  // TODO: should be based on whether a screen-lock is enabled.
  cbor::Value::MapValue options;
  options.emplace("uv", true);

  cbor::Value::MapValue response_map;
  response_map.emplace(1, std::move(versions));
  response_map.emplace(3, aaguid);
  response_map.emplace(4, std::move(options));

  return cbor::Writer::Write(cbor::Value(std::move(response_map))).value();
}

std::array<uint8_t, device::cablev2::kNonceSize> RandomNonce() {
  std::array<uint8_t, device::cablev2::kNonceSize> ret;
  crypto::RandBytes(ret);
  return ret;
}

using GeneratePairingDataCallback =
    base::OnceCallback<base::Optional<cbor::Value>(
        base::span<const uint8_t, device::kP256X962Length> peer_public_key_x962,
        device::cablev2::HandshakeHash)>;

// TunnelTransport is a transport that uses WebSockets to talk to a cloud
// service and uses BLE adverts to show proximity.
class TunnelTransport : public Transport {
 public:
  TunnelTransport(
      Platform* platform,
      network::mojom::NetworkContext* network_context,
      base::span<const uint8_t> secret,
      base::span<const uint8_t, device::kP256X962Length> peer_identity,
      GeneratePairingDataCallback generate_pairing_data)
      : platform_(platform),
        tunnel_id_(device::cablev2::Derive<EXTENT(tunnel_id_)>(
            secret,
            base::span<uint8_t>(),
            DerivedValueType::kTunnelID)),
        eid_key_(device::cablev2::Derive<EXTENT(eid_key_)>(
            secret,
            base::span<const uint8_t>(),
            device::cablev2::DerivedValueType::kEIDKey)),
        network_context_(network_context),
        peer_identity_(device::fido_parsing_utils::Materialize(peer_identity)),
        generate_pairing_data_(std::move(generate_pairing_data)),
        secret_(fido_parsing_utils::Materialize(secret)) {
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
      network::mojom::NetworkContext* network_context,
      base::span<const uint8_t> secret,
      base::span<const uint8_t, device::cablev2::kClientNonceSize> client_nonce,
      std::array<uint8_t, device::cablev2::kRoutingIdSize> routing_id,
      base::span<const uint8_t, 16> tunnel_id,
      bssl::UniquePtr<EC_KEY> local_identity)
      : platform_(platform),
        tunnel_id_(fido_parsing_utils::Materialize(tunnel_id)),
        eid_key_(device::cablev2::Derive<EXTENT(eid_key_)>(
            secret,
            client_nonce,
            device::cablev2::DerivedValueType::kEIDKey)),
        network_context_(network_context),
        secret_(fido_parsing_utils::Materialize(secret)),
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

    network_context_->CreateWebSocket(
        target_, {device::kCableWebSocketProtocol}, net::SiteForCookies(),
        net::IsolationInfo(), /*additional_headers=*/{},
        network::mojom::kBrowserProcessId, url::Origin::Create(target_),
        network::mojom::kWebSocketOptionBlockAllCookies,
        net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
        websocket_client_->BindNewHandshakeClientPipe(),
        /*auth_cert_observer=*/mojo::NullRemote(),
        /*auth_handler=*/mojo::NullRemote(),
        /*header_client=*/mojo::NullRemote());
    FIDO_LOG(DEBUG) << "Creating WebSocket to " << target_.spec();
  }

  void Write(std::vector<uint8_t> data) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_EQ(state_, kReady);

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

  void OnTunnelReady(
      WebSocketAdapter::Result result,
      base::Optional<std::array<uint8_t, device::cablev2::kRoutingIdSize>>
          routing_id) {
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
      const device::cablev2::eid::Components components{
          .tunnel_server_domain = kTunnelServer,
          .routing_id = *routing_id,
          .nonce = RandomNonce(),
      };
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
    psk_ = device::cablev2::Derive<EXTENT(psk_)>(
        secret_, plaintext_eid, device::cablev2::DerivedValueType::kPSK);

    update_callback_.Run(Platform::Status::TUNNEL_SERVER_CONNECT);
  }

  void OnTunnelData(base::Optional<base::span<const uint8_t>> msg) {
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
        post_handshake_msg.emplace(1, BuildGetInfoResponse());

        if (state_ == State::kConnected) {
          base::Optional<cbor::Value> pairing_data(
              std::move(generate_pairing_data_)
                  .Run(*peer_identity_, result->second));
          if (pairing_data) {
            post_handshake_msg.emplace(2, std::move(*pairing_data));
          }
        }

        base::Optional<std::vector<uint8_t>> post_handshake_msg_bytes(
            EncodePaddedCBORMap(std::move(post_handshake_msg)));

        if (!post_handshake_msg_bytes ||
            !crypter_->Encrypt(&post_handshake_msg_bytes.value())) {
          FIDO_LOG(ERROR) << "failed to encode post-handshake message";
          return;
        }
        websocket_client_->Write(*post_handshake_msg_bytes);

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

        if (first_message_) {
          update_callback_.Run(Platform::Status::REQUEST_RECEIVED);
          first_message_ = false;
        }
        update_callback_.Run(std::move(plaintext));
        break;
      }

      default:
        NOTREACHED();
    }
  }

  Platform* const platform_;
  State state_ = State::kNone;
  const std::array<uint8_t, kTunnelIdSize> tunnel_id_;
  const std::array<uint8_t, kEIDKeySize> eid_key_;
  std::unique_ptr<WebSocketAdapter> websocket_client_;
  std::unique_ptr<Crypter> crypter_;
  network::mojom::NetworkContext* const network_context_;
  const base::Optional<std::array<uint8_t, kP256X962Length>> peer_identity_;
  std::array<uint8_t, kPSKSize> psk_;
  GeneratePairingDataCallback generate_pairing_data_;
  const std::vector<uint8_t> secret_;
  bssl::UniquePtr<EC_KEY> local_identity_;
  GURL target_;
  std::unique_ptr<Platform::BLEAdvert> ble_advert_;
  base::RepeatingCallback<void(Update)> update_callback_;
  bool first_message_ = true;

  SEQUENCE_CHECKER(sequence_checker_);
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

    if (auto* error = absl::get_if<Platform::Error>(&update)) {
      platform_->OnCompleted(*error);
      return;
    } else if (auto* status = absl::get_if<Platform::Status>(&update)) {
      platform_->OnStatus(*status);
      return;
    } else if (absl::get_if<Transport::Disconnected>(&update)) {
      base::Optional<Platform::Error> maybe_error;
      if (!transaction_done_) {
        maybe_error = Platform::Error::UNEXPECTED_EOF;
      }
      platform_->OnCompleted(maybe_error);
      return;
    }

    std::vector<uint8_t>& msg = absl::get<std::vector<uint8_t>>(update);
    base::Optional<std::vector<uint8_t>> response = ProcessCTAPMessage(msg);
    if (!response) {
      // TODO(agl): expose more error information from |ProcessCTAPMessage|.
      platform_->OnCompleted(Platform::Error::INVALID_CTAP);
      return;
    }

    if (response->empty()) {
      // Response is pending.
      return;
    }

    transport_->Write(std::move(*response));
  }

  base::Optional<std::vector<uint8_t>> ProcessCTAPMessage(
      base::span<const uint8_t> message_bytes) {
    if (message_bytes.empty()) {
      return base::nullopt;
    }
    const auto command = message_bytes[0];
    const auto cbor_bytes = message_bytes.subspan(1);

    base::Optional<cbor::Value> payload;
    if (!cbor_bytes.empty()) {
      payload = cbor::Reader::Read(cbor_bytes);
      if (!payload) {
        FIDO_LOG(ERROR) << "CBOR decoding failed for "
                        << base::HexEncode(cbor_bytes);
        return base::nullopt;
      }
      FIDO_LOG(DEBUG) << "<- (" << base::HexEncode(&command, 1) << ") "
                      << cbor::DiagnosticWriter::Write(*payload);
    } else {
      FIDO_LOG(DEBUG) << "<- (" << base::HexEncode(&command, 1)
                      << ") <no payload>";
    }

    switch (command) {
      case static_cast<uint8_t>(
          device::CtapRequestCommand::kAuthenticatorGetInfo): {
        if (payload) {
          FIDO_LOG(ERROR) << "getInfo command incorrectly contained payload";
          return base::nullopt;
        }

        base::Optional<std::vector<uint8_t>> response = BuildGetInfoResponse();
        if (!response) {
          return base::nullopt;
        }
        response->insert(
            response->begin(),
            static_cast<uint8_t>(CtapDeviceResponseCode::kSuccess));
        return response;
      }

      case static_cast<uint8_t>(
          device::CtapRequestCommand::kAuthenticatorMakeCredential): {
        if (!payload || !payload->is_map()) {
          FIDO_LOG(ERROR) << "Invalid makeCredential payload";
          return base::nullopt;
        }

        MakeCredRequest make_cred_request;
        if (!device::cbor_extract::Extract<MakeCredRequest>(
                &make_cred_request, kMakeCredParseSteps, payload->GetMap())) {
          FIDO_LOG(ERROR) << "Failed to parse makeCredential request: "
                          << base::HexEncode(cbor_bytes);
          return base::nullopt;
        }

        auto params = std::make_unique<Platform::MakeCredentialParams>();
        params->client_data_hash = *make_cred_request.client_data_hash;
        params->rp_id = *make_cred_request.rp_id;
        params->user_id = *make_cred_request.user_id;
        params->callback =
            base::BindOnce(&CTAP2Processor::OnMakeCredentialResponse,
                           weak_factory_.GetWeakPtr());

        if (!device::cbor_extract::ForEachPublicKeyEntry(
                *make_cred_request.cred_params, cbor::Value("alg"),
                base::BindRepeating(
                    [](std::vector<int>* out,
                       const cbor::Value& value) -> bool {
                      if (!value.is_integer()) {
                        return false;
                      }
                      const int64_t alg = value.GetInteger();

                      if (alg > std::numeric_limits<int>::max() ||
                          alg < std::numeric_limits<int>::min()) {
                        return false;
                      }
                      out->push_back(static_cast<int>(alg));
                      return true;
                    },
                    base::Unretained(&params->algorithms)))) {
          return base::nullopt;
        }

        if (make_cred_request.excluded_credentials &&
            !device::cbor_extract::ForEachPublicKeyEntry(
                *make_cred_request.excluded_credentials, cbor::Value("id"),
                base::BindRepeating(
                    [](std::vector<std::vector<uint8_t>>* out,
                       const cbor::Value& value) -> bool {
                      if (!value.is_bytestring()) {
                        return false;
                      }
                      out->push_back(value.GetBytestring());
                      return true;
                    },
                    base::Unretained(&params->excluded_cred_ids)))) {
          return base::nullopt;
        }

        // TODO: plumb the rk flag through once GmsCore supports resident
        // keys. This will require support for optional maps in |Extract|.
        platform_->MakeCredential(std::move(params));
        return std::vector<uint8_t>();
      }

      case static_cast<uint8_t>(
          device::CtapRequestCommand::kAuthenticatorGetAssertion): {
        if (!payload || !payload->is_map()) {
          FIDO_LOG(ERROR) << "Invalid makeCredential payload";
          return base::nullopt;
        }

        GetAssertionRequest get_assertion_request;
        if (!device::cbor_extract::Extract<GetAssertionRequest>(
                &get_assertion_request, kGetAssertionParseSteps,
                payload->GetMap())) {
          FIDO_LOG(ERROR) << "Failed to parse getAssertion request";
          return base::nullopt;
        }

        auto params = std::make_unique<Platform::GetAssertionParams>();
        params->client_data_hash = *get_assertion_request.client_data_hash;
        params->rp_id = *get_assertion_request.rp_id;
        params->callback =
            base::BindOnce(&CTAP2Processor::OnGetAssertionResponse,
                           weak_factory_.GetWeakPtr());

        if (get_assertion_request.allowed_credentials &&
            !device::cbor_extract::ForEachPublicKeyEntry(
                *get_assertion_request.allowed_credentials, cbor::Value("id"),
                base::BindRepeating(
                    [](std::vector<std::vector<uint8_t>>* out,
                       const cbor::Value& value) -> bool {
                      if (!value.is_bytestring()) {
                        return false;
                      }
                      out->push_back(value.GetBytestring());
                      return true;
                    },
                    base::Unretained(&params->allowed_cred_ids)))) {
          return base::nullopt;
        }

        platform_->GetAssertion(std::move(params));
        return std::vector<uint8_t>();
      }

      default:
        FIDO_LOG(ERROR) << "Received unknown command "
                        << static_cast<unsigned>(command);
        return base::nullopt;
    }
  }

  void OnMakeCredentialResponse(uint32_t ctap_status,
                                base::span<const uint8_t> attestation_object) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_LE(ctap_status, 0xFFu);

    std::vector<uint8_t> response = {base::checked_cast<uint8_t>(ctap_status)};
    if (ctap_status == static_cast<uint8_t>(CtapDeviceResponseCode::kSuccess)) {
      // TODO: pass response parameters from the Java side.
      base::Optional<cbor::Value> cbor_attestation_object =
          cbor::Reader::Read(attestation_object);
      if (!cbor_attestation_object || !cbor_attestation_object->is_map()) {
        FIDO_LOG(ERROR) << "invalid CBOR attestation object";
        return;
      }

      AttestationObject attestation_object;
      if (!device::cbor_extract::Extract<AttestationObject>(
              &attestation_object, kAttObjParseSteps,
              cbor_attestation_object->GetMap())) {
        FIDO_LOG(ERROR) << "attestation object parse failed";
        return;
      }

      cbor::Value::MapValue response_map;
      response_map.emplace(1, base::StringPiece(*attestation_object.fmt));
      response_map.emplace(
          2, base::span<const uint8_t>(*attestation_object.auth_data));
      response_map.emplace(3, attestation_object.statement->Clone());

      base::Optional<std::vector<uint8_t>> response_payload =
          cbor::Writer::Write(cbor::Value(std::move(response_map)));
      if (!response_payload) {
        return;
      }
      response.insert(response.end(), response_payload->begin(),
                      response_payload->end());
    } else {
      platform_->OnStatus(Platform::Status::CTAP_ERROR);
    }

    transaction_done_ = true;
    transport_->Write(std::move(response));
  }

  void OnGetAssertionResponse(uint32_t ctap_status,
                              base::span<const uint8_t> credential_id,
                              base::span<const uint8_t> authenticator_data,
                              base::span<const uint8_t> signature) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_LE(ctap_status, 0xFFu);
    std::vector<uint8_t> response = {base::checked_cast<uint8_t>(ctap_status)};

    if (ctap_status == static_cast<uint8_t>(CtapDeviceResponseCode::kSuccess)) {
      cbor::Value::MapValue credential_descriptor;
      credential_descriptor.emplace("type", device::kPublicKey);
      credential_descriptor.emplace("id", credential_id);
      cbor::Value::ArrayValue transports;
      transports.emplace_back("internal");
      transports.emplace_back("cable");
      credential_descriptor.emplace("transports", std::move(transports));
      cbor::Value::MapValue response_map;
      response_map.emplace(1, std::move(credential_descriptor));
      response_map.emplace(2, authenticator_data);
      response_map.emplace(3, signature);
      // TODO: add user entity to support resident keys.

      base::Optional<std::vector<uint8_t>> response_payload =
          cbor::Writer::Write(cbor::Value(std::move(response_map)));
      if (!response_payload) {
        return;
      }
      response.insert(response.end(), response_payload->begin(),
                      response_payload->end());
    } else {
      platform_->OnStatus(Platform::Status::CTAP_ERROR);
    }

    transaction_done_ = true;
    transport_->Write(std::move(response));
  }

  bool transaction_done_ = false;
  const std::unique_ptr<Transport> transport_;
  const std::unique_ptr<Platform> platform_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CTAP2Processor> weak_factory_{this};
};

static bssl::UniquePtr<EC_KEY> IdentityKey(
    base::span<const uint8_t, 32> root_secret) {
  std::array<uint8_t, 32> seed;
  seed = device::cablev2::Derive<EXTENT(seed)>(
      root_secret, /*nonce=*/base::span<uint8_t>(),
      device::cablev2::DerivedValueType::kIdentityKeySeed);
  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  return bssl::UniquePtr<EC_KEY>(
      EC_KEY_derive_from_secret(p256.get(), seed.data(), seed.size()));
}

class PairingDataGenerator {
 public:
  static GeneratePairingDataCallback GetClosure(
      base::span<const uint8_t, kRootSecretSize> root_secret,
      const std::string& name,
      base::Optional<std::vector<uint8_t>> contact_id) {
    auto* generator =
        new PairingDataGenerator(root_secret, name, std::move(contact_id));
    return base::BindOnce(&PairingDataGenerator::Generate,
                          base::Owned(generator));
  }

 private:
  PairingDataGenerator(base::span<const uint8_t, kRootSecretSize> root_secret,
                       const std::string& name,
                       base::Optional<std::vector<uint8_t>> contact_id)
      : root_secret_(fido_parsing_utils::Materialize(root_secret)),
        name_(name),
        contact_id_(std::move(contact_id)) {}

  base::Optional<cbor::Value> Generate(
      base::span<const uint8_t, device::kP256X962Length> peer_public_key_x962,
      device::cablev2::HandshakeHash handshake_hash) {
    if (!contact_id_) {
      return base::nullopt;
    }

    cbor::Value::MapValue map;
    map.emplace(1, std::move(*contact_id_));

    std::array<uint8_t, device::cablev2::kNonceSize> pairing_id;
    crypto::RandBytes(pairing_id);

    map.emplace(2, pairing_id);

    std::array<uint8_t, 32> paired_secret;
    paired_secret = device::cablev2::Derive<EXTENT(paired_secret)>(
        root_secret_, pairing_id,
        device::cablev2::DerivedValueType::kPairedSecret);

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
  base::Optional<std::vector<uint8_t>> contact_id_;
};

}  // namespace

Platform::BLEAdvert::~BLEAdvert() = default;
Platform::MakeCredentialParams::MakeCredentialParams() = default;
Platform::MakeCredentialParams::~MakeCredentialParams() = default;
Platform::GetAssertionParams::GetAssertionParams() = default;
Platform::GetAssertionParams::~GetAssertionParams() = default;
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
    network::mojom::NetworkContext* network_context,
    base::span<const uint8_t, kRootSecretSize> root_secret,
    const std::string& authenticator_name,
    base::span<const uint8_t, 16> qr_secret,
    base::span<const uint8_t, kP256X962Length> peer_identity,
    base::Optional<std::vector<uint8_t>> contact_id) {
  auto generate_pairing_data = PairingDataGenerator::GetClosure(
      root_secret, authenticator_name, contact_id);

  Platform* const platform_ptr = platform.get();
  return std::make_unique<CTAP2Processor>(
      std::make_unique<TunnelTransport>(platform_ptr, network_context,
                                        qr_secret, peer_identity,
                                        std::move(generate_pairing_data)),
      std::move(platform));
}

std::unique_ptr<Transaction> TransactFromFCM(
    std::unique_ptr<Platform> platform,
    network::mojom::NetworkContext* network_context,
    base::span<const uint8_t, kRootSecretSize> root_secret,
    std::array<uint8_t, kRoutingIdSize> routing_id,
    base::span<const uint8_t, kTunnelIdSize> tunnel_id,
    base::span<const uint8_t> pairing_id,
    base::span<const uint8_t, kClientNonceSize> client_nonce) {
  std::array<uint8_t, 32> paired_secret;
  paired_secret = Derive<EXTENT(paired_secret)>(
      root_secret, pairing_id, DerivedValueType::kPairedSecret);

  Platform* const platform_ptr = platform.get();
  return std::make_unique<CTAP2Processor>(
      std::make_unique<TunnelTransport>(platform_ptr, network_context,
                                        paired_secret, client_nonce, routing_id,
                                        tunnel_id, IdentityKey(root_secret)),
      std::move(platform));
}

}  // namespace authenticator
}  // namespace cablev2
}  // namespace device
