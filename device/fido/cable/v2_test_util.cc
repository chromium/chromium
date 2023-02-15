// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/v2_test_util.h"

#include <string>
#include <vector>

#include "base/base64url.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "crypto/random.h"
#include "device/fido/cable/v2_authenticator.h"
#include "device/fido/cable/v2_discovery.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/cable/websocket_adapter.h"
#include "device/fido/fido_constants.h"
#include "device/fido/virtual_ctap2_device.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_network_context.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "url/gurl.h"

namespace device::cablev2 {
namespace {

// TestNetworkContext intercepts WebSocket creation calls and simulates a
// caBLEv2 tunnel server.
class TestNetworkContext : public network::TestNetworkContext {
 public:
  explicit TestNetworkContext(absl::optional<ContactCallback> contact_callback)
      : contact_callback_(std::move(contact_callback)) {}

  void CreateWebSocket(
      const GURL& url,
      const std::vector<std::string>& requested_protocols,
      const net::SiteForCookies& site_for_cookies,
      const net::IsolationInfo& isolation_info,
      std::vector<network::mojom::HttpHeaderPtr> additional_headers,
      int32_t process_id,
      const url::Origin& origin,
      uint32_t options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
          handshake_client,
      mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
          url_loader_network_observer,
      mojo::PendingRemote<network::mojom::WebSocketAuthenticationHandler>
          auth_handler,
      mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client,
      const absl::optional<base::UnguessableToken>& throttling_profile_id)
      override {
    CHECK(url.has_path());

    base::StringPiece path = url.path_piece();
    static const char kNewPrefix[] = "/cable/new/";
    static const char kConnectPrefix[] = "/cable/connect/";
    static const char kContactPrefix[] = "/cable/contact/";
    if (path.find(kNewPrefix) == 0) {
      path.remove_prefix(sizeof(kNewPrefix) - 1);
      CHECK(!base::Contains(connections_, std::string(path)));
      connections_.emplace(std::string(path), std::make_unique<Connection>(
                                                  Connection::Type::NEW,
                                                  std::move(handshake_client)));
    } else if (path.find(kConnectPrefix) == 0) {
      path.remove_prefix(sizeof(kConnectPrefix) - 1);
      // The first part of |path| will be a hex-encoded routing ID followed by a
      // '/'. Skip it.
      constexpr size_t kRoutingIdComponentSize = 2 * kRoutingIdSize + 1;
      CHECK_GE(path.size(), kRoutingIdComponentSize);
      path.remove_prefix(kRoutingIdComponentSize);

      const auto it = connections_.find(std::string(path));
      CHECK(it != connections_.end()) << "Unknown tunnel requested";
      it->second->set_peer(std::make_unique<Connection>(
          Connection::Type::CONNECT, std::move(handshake_client)));
    } else if (path.find(kContactPrefix) == 0) {
      path.remove_prefix(sizeof(kContactPrefix) - 1);

      CHECK_EQ(additional_headers.size(), 1u);
      CHECK_EQ(additional_headers[0]->name, device::kCableClientPayloadHeader);

      if (!contact_callback_) {
        // Without a contact callback all attempts are rejected with a 410
        // status to indicate the the contact ID will never work again.
        mojo::Remote<network::mojom::WebSocketHandshakeClient>
            bound_handshake_client(std::move(handshake_client));
        bound_handshake_client->OnFailure("", net::OK, net::HTTP_GONE);
        return;
      }

      std::vector<uint8_t> client_payload_bytes;
      CHECK(base::HexStringToBytes(additional_headers[0]->value,
                                   &client_payload_bytes));

      absl::optional<cbor::Value> client_payload =
          cbor::Reader::Read(client_payload_bytes);
      const cbor::Value::MapValue& map = client_payload->GetMap();

      uint8_t tunnel_id[kTunnelIdSize];
      crypto::RandBytes(tunnel_id);

      connections_.emplace(
          base::HexEncode(tunnel_id),
          std::make_unique<Connection>(Connection::Type::CONTACT,
                                       std::move(handshake_client)));

      const std::vector<uint8_t>& pairing_id_vec =
          map.find(cbor::Value(1))->second.GetBytestring();
      base::span<const uint8_t, kPairingIDSize> pairing_id(
          pairing_id_vec.data(), pairing_id_vec.size());

      const std::vector<uint8_t>& client_nonce_vec =
          map.find(cbor::Value(2))->second.GetBytestring();
      base::span<const uint8_t, kClientNonceSize> client_nonce(
          client_nonce_vec.data(), client_nonce_vec.size());

      const std::string& request_type_hint =
          map.find(cbor::Value(3))->second.GetString();

      contact_callback_->Run(tunnel_id, pairing_id, client_nonce,
                             request_type_hint);
    } else {
      CHECK(false) << "unexpected path: " << path;
    }
  }

 private:
  class Connection : public network::mojom::WebSocket {
   public:
    enum class Type {
      NEW,
      CONNECT,
      CONTACT,
    };

    Connection(Type type,
               mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
                   pending_handshake_client)
        : type_(type),
          in_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
          out_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
          handshake_client_(std::move(pending_handshake_client)) {
      MojoCreateDataPipeOptions options;
      memset(&options, 0, sizeof(options));
      options.struct_size = sizeof(options);
      options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
      options.element_num_bytes = sizeof(uint8_t);
      options.capacity_num_bytes = 1 << 16;

      CHECK_EQ(mojo::CreateDataPipe(&options, in_producer_, in_),
               MOJO_RESULT_OK);
      CHECK_EQ(mojo::CreateDataPipe(&options, out_, out_consumer_),
               MOJO_RESULT_OK);

      in_watcher_.Watch(in_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                        base::BindRepeating(&Connection::OnInPipeReady,
                                            base::Unretained(this)));
      out_watcher_.Watch(out_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                         MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                         base::BindRepeating(&Connection::OnOutPipeReady,
                                             base::Unretained(this)));
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&Connection::CompleteConnection,
                                    base::Unretained(this)));
    }

    void SendMessage(network::mojom::WebSocketMessageType type,
                     uint64_t length) override {
      if (!peer_ || !peer_->connected_) {
        pending_messages_.emplace_back(std::make_pair(type, length));
      } else {
        peer_->client_receiver_->OnDataFrame(/*final=*/true, type, length);
      }

      if (length > 0) {
        buffer_.resize(buffer_.size() + length);
        OnInPipeReady(MOJO_RESULT_OK, mojo::HandleSignalsState());
      }
    }

    void StartReceiving() override {}
    void StartClosingHandshake(uint16_t code,
                               const std::string& reason) override {
      CHECK(false);
    }

    void set_peer(std::unique_ptr<Connection> peer) {
      CHECK(!peer_);
      peer_ownership_ = std::move(peer);
      peer_ = peer_ownership_.get();
      peer_->set_nonowning_peer(this);

      Flush();
    }

   private:
    // name is useful when adding debugging messages. The first party to a
    // tunnel is "A" and the second is "B".
    const char* name() const {
      switch (type_) {
        case Type::NEW:
        case Type::CONTACT:
          return "A";
        case Type::CONNECT:
          return "B";
      }
    }

    void set_nonowning_peer(Connection* peer) {
      CHECK(!peer_);
      peer_ = peer;
      Flush();
    }

    void CompleteConnection() {
      CHECK(!connected_);
      auto response = network::mojom::WebSocketHandshakeResponse::New();
      response->selected_protocol = device::kCableWebSocketProtocol;

      if (type_ == Type::NEW) {
        auto header = network::mojom::HttpHeader::New();
        header->name = device::kCableRoutingIdHeader;
        std::array<uint8_t, kRoutingIdSize> routing_id = {42};
        header->value = base::HexEncode(routing_id);
        response->headers.push_back(std::move(header));
      }

      handshake_client_->OnConnectionEstablished(
          socket_.BindNewPipeAndPassRemote(),
          client_receiver_.BindNewPipeAndPassReceiver(), std::move(response),
          std::move(out_consumer_), std::move(in_producer_));

      connected_ = true;
      if (peer_) {
        peer_->Flush();
      }
    }

    void Flush() {
      if (!peer_->connected_) {
        return;
      }

      for (const auto& pending_message : pending_messages_) {
        peer_->client_receiver_->OnDataFrame(
            /*final=*/true, pending_message.first, pending_message.second);
      }

      if (!buffer_.empty()) {
        peer_->out_watcher_.Arm();
      }
    }

    void OnInPipeReady(MojoResult, const mojo::HandleSignalsState&) {
      const size_t todo = buffer_.size() - buffer_i_;
      CHECK_GT(todo, 0u);

      // We CHECK that the message fits into Mojo's 32-bit lengths because we
      // don't expect anything that large in unittests.
      uint32_t todo_32 = todo;
      CHECK_LE(todo, std::numeric_limits<decltype(todo_32)>::max());

      const MojoResult result = in_->ReadData(
          &buffer_.data()[buffer_i_], &todo_32, MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_OK) {
        buffer_i_ += todo_32;
        CHECK_LE(buffer_i_, buffer_.size());

        if (peer_ && buffer_i_ > 0) {
          peer_->OnOutPipeReady(MOJO_RESULT_OK, mojo::HandleSignalsState());
        }

        if (buffer_i_ < buffer_.size()) {
          in_watcher_.Arm();
        } else {
          // TODO
        }
      } else if (result == MOJO_RESULT_SHOULD_WAIT) {
        in_watcher_.Arm();
      } else {
        CHECK(false) << static_cast<int>(result);
      }
    }

    void OnOutPipeReady(MojoResult, const mojo::HandleSignalsState&) {
      const size_t todo = peer_->buffer_.size();
      if (todo == 0) {
        return;
      }

      uint32_t todo_32 = todo;
      const MojoResult result = out_->WriteData(peer_->buffer_.data(), &todo_32,
                                                MOJO_WRITE_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_OK) {
        if (todo_32 == todo) {
          peer_->buffer_.clear();
          peer_->buffer_i_ = 0;
        } else {
          const size_t new_length = todo - todo_32;
          memmove(peer_->buffer_.data(), &peer_->buffer_.data()[todo_32],
                  new_length);
          peer_->buffer_.resize(new_length);
          peer_->buffer_i_ -= todo_32;
        }

        if (!peer_->buffer_.empty()) {
          out_watcher_.Arm();
        }
      } else if (result == MOJO_RESULT_SHOULD_WAIT) {
        out_watcher_.Arm();
      } else if (result == MOJO_RESULT_FAILED_PRECONDITION) {
        // The reader has closed. Drop the message.
      } else {
        CHECK(false) << static_cast<int>(result);
      }
    }

    const Type type_;
    bool connected_ = false;
    std::unique_ptr<Connection> peer_ownership_;
    std::vector<uint8_t> buffer_;
    std::vector<std::pair<network::mojom::WebSocketMessageType, uint64_t>>
        pending_messages_;
    size_t buffer_i_ = 0;
    mojo::SimpleWatcher in_watcher_;
    mojo::SimpleWatcher out_watcher_;
    raw_ptr<Connection> peer_ = nullptr;
    mojo::Remote<network::mojom::WebSocketHandshakeClient> handshake_client_;
    mojo::Remote<network::mojom::WebSocketClient> client_receiver_;
    mojo::Receiver<network::mojom::WebSocket> socket_{this};
    mojo::ScopedDataPipeConsumerHandle in_;
    mojo::ScopedDataPipeProducerHandle in_producer_;
    mojo::ScopedDataPipeProducerHandle out_;
    mojo::ScopedDataPipeConsumerHandle out_consumer_;
  };

  std::map<std::string, std::unique_ptr<Connection>> connections_;
  const absl::optional<ContactCallback> contact_callback_;
};

class DummyBLEAdvert
    : public device::cablev2::authenticator::Platform::BLEAdvert {};

// TestPlatform implements the platform support for caBLEv2 by forwarding
// messages to the given |VirtualCtap2Device|.
class TestPlatform : public authenticator::Platform {
 public:
  TestPlatform(Discovery::AdvertEventStream::Callback ble_advert_callback,
               device::VirtualCtap2Device* ctap2_device,
               authenticator::Observer* observer)
      : ble_advert_callback_(ble_advert_callback),
        ctap2_device_(ctap2_device),
        observer_(observer) {}

  void MakeCredential(
      blink::mojom::PublicKeyCredentialCreationOptionsPtr params,
      MakeCredentialCallback callback) override {
    device::CtapMakeCredentialRequest request(
        /*client_data_json=*/"", std::move(params->relying_party),
        std::move(params->user),
        PublicKeyCredentialParams(std::move(params->public_key_parameters)));
    CHECK_EQ(request.client_data_hash.size(), params->challenge.size());
    memcpy(request.client_data_hash.data(), params->challenge.data(),
           params->challenge.size());
    request.resident_key_required =
        !params->authenticator_selection
            ? false
            : params->authenticator_selection->resident_key ==
                  ResidentKeyRequirement::kRequired;
    if (params->device_public_key) {
      request.device_public_key.emplace();
    }
    request.prf = params->prf_enable;

    std::pair<device::CtapRequestCommand, absl::optional<cbor::Value>>
        request_cbor = AsCTAPRequestValuePair(request);

    ctap2_device_->DeviceTransact(
        ToCTAP2Command(std::move(request_cbor)),
        base::BindOnce(&TestPlatform::OnMakeCredentialResult,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetAssertion(blink::mojom::PublicKeyCredentialRequestOptionsPtr params,
                    GetAssertionCallback callback) override {
    device::CtapGetAssertionRequest request(std::move(params->relying_party_id),
                                            /* client_data_json= */ "");
    request.allow_list = std::move(params->allow_credentials);
    request.user_verification = params->user_verification;

    CHECK_EQ(request.client_data_hash.size(), params->challenge.size());
    memcpy(request.client_data_hash.data(), params->challenge.data(),
           params->challenge.size());
    if (params->device_public_key) {
      request.device_public_key.emplace();
    }
    for (const auto& prf_input_from_request : params->prf_inputs) {
      PRFInput prf_input_to_authenticator;
      prf_input_to_authenticator.credential_id =
          std::move(prf_input_from_request->id);
      CHECK(fido_parsing_utils::ExtractArray(
          prf_input_from_request->first, 0, &prf_input_to_authenticator.salt1));
      if (prf_input_from_request->second) {
        prf_input_to_authenticator.salt2.emplace();
        CHECK(fido_parsing_utils::ExtractArray(
            *prf_input_from_request->second, 0,
            &prf_input_to_authenticator.salt2.value()));
      }

      request.prf_inputs.emplace_back(std::move(prf_input_to_authenticator));
    }

    std::pair<device::CtapRequestCommand, absl::optional<cbor::Value>>
        request_cbor = AsCTAPRequestValuePair(request);

    ctap2_device_->DeviceTransact(
        ToCTAP2Command(std::move(request_cbor)),
        base::BindOnce(&TestPlatform::OnGetAssertionResult,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnStatus(Status status) override {
    if (observer_) {
      observer_->OnStatus(status);
    }
  }

  void OnCompleted(absl::optional<Error> maybe_error) override {
    if (observer_) {
      observer_->OnCompleted(maybe_error);
    }
  }

  std::unique_ptr<authenticator::Platform::BLEAdvert> SendBLEAdvert(
      base::span<const uint8_t, kAdvertSize> payload) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &TestPlatform::DoSendBLEAdvert, weak_factory_.GetWeakPtr(),
            device::fido_parsing_utils::Materialize<EXTENT(payload)>(payload)));
    return std::make_unique<DummyBLEAdvert>();
  }

 private:
  void DoSendBLEAdvert(base::span<const uint8_t, kAdvertSize> advert) {
    ble_advert_callback_.Run(advert);
  }

  std::vector<uint8_t> ToCTAP2Command(
      const std::pair<device::CtapRequestCommand, absl::optional<cbor::Value>>&
          parts) {
    std::vector<uint8_t> ret;

    if (parts.second.has_value()) {
      absl::optional<std::vector<uint8_t>> cbor_bytes =
          cbor::Writer::Write(std::move(*parts.second));
      ret.swap(*cbor_bytes);
    }

    ret.insert(ret.begin(), static_cast<uint8_t>(parts.first));
    return ret;
  }

  void OnMakeCredentialResult(MakeCredentialCallback callback,
                              absl::optional<std::vector<uint8_t>> result) {
    if (!result || result->empty()) {
      std::move(callback).Run(
          static_cast<uint32_t>(device::CtapDeviceResponseCode::kCtap2ErrOther),
          base::span<const uint8_t>(), absl::nullopt, /* prf_enabled= */ false);
      return;
    }
    const base::span<const uint8_t> payload = *result;

    if (payload.size() == 1 ||
        payload[0] !=
            static_cast<uint8_t>(device::CtapDeviceResponseCode::kSuccess)) {
      std::move(callback).Run(payload[0], base::span<const uint8_t>(),
                              absl::nullopt, /* prf_enabled= */ false);
      return;
    }

    absl::optional<cbor::Value> v = cbor::Reader::Read(payload.subspan(1));
    const cbor::Value::MapValue& in_map = v->GetMap();

    cbor::Value::MapValue out_map;
    out_map.emplace("fmt", in_map.find(cbor::Value(1))->second.GetString());
    out_map.emplace("authData",
                    in_map.find(cbor::Value(2))->second.GetBytestring());
    out_map.emplace("attStmt", in_map.find(cbor::Value(3))->second.GetMap());

    absl::optional<base::span<const uint8_t>> device_public_key_signature;
    bool prf_enabled = false;
    const auto& unsigned_extension_outputs_it = in_map.find(cbor::Value(6));
    if (unsigned_extension_outputs_it != in_map.end()) {
      const cbor::Value::MapValue& unsigned_extension_outputs =
          unsigned_extension_outputs_it->second.GetMap();
      const auto dpk_it = unsigned_extension_outputs.find(
          cbor::Value(kExtensionDevicePublicKey));
      if (dpk_it != unsigned_extension_outputs.end()) {
        device_public_key_signature = dpk_it->second.GetBytestring();
      }
      const auto prf_it =
          unsigned_extension_outputs.find(cbor::Value(kExtensionPRF));
      if (prf_it != unsigned_extension_outputs.end()) {
        prf_enabled = prf_it->second.GetMap()
                          .find(cbor::Value(kExtensionPRFEnabled))
                          ->second.GetBool();
      }
    }

    absl::optional<std::vector<uint8_t>> attestation_obj =
        cbor::Writer::Write(cbor::Value(std::move(out_map)));

    std::move(callback).Run(
        static_cast<uint32_t>(device::CtapDeviceResponseCode::kSuccess),
        *attestation_obj, device_public_key_signature, prf_enabled);
  }

  void OnGetAssertionResult(GetAssertionCallback callback,
                            absl::optional<std::vector<uint8_t>> result) {
    if (!result || result->empty()) {
      std::move(callback).Run(
          static_cast<uint32_t>(device::CtapDeviceResponseCode::kCtap2ErrOther),
          nullptr);
      return;
    }
    const base::span<const uint8_t> payload = *result;

    if (payload.size() == 1 ||
        payload[0] !=
            static_cast<uint8_t>(device::CtapDeviceResponseCode::kSuccess)) {
      std::move(callback).Run(payload[0], nullptr);
      return;
    }

    auto response = blink::mojom::GetAssertionAuthenticatorResponse::New();
    response->info = blink::mojom::CommonCredentialInfo::New();

    absl::optional<cbor::Value> v = cbor::Reader::Read(payload.subspan(1));
    const cbor::Value::MapValue& in_map = v->GetMap();

    auto cred_id_it = in_map.find(cbor::Value(1));
    response->info->raw_id = cred_id_it->second.GetMap()
                                 .find(cbor::Value("id"))
                                 ->second.GetBytestring();
    response->info->authenticator_data =
        in_map.find(cbor::Value(2))->second.GetBytestring();
    response->signature = in_map.find(cbor::Value(3))->second.GetBytestring();

    auto user_it = in_map.find(cbor::Value(4));
    if (user_it != in_map.end()) {
      response->user_handle = user_it->second.GetMap()
                                  .find(cbor::Value("id"))
                                  ->second.GetBytestring();
    }

    auto unsigned_extension_outputs_it = in_map.find(cbor::Value(8));
    if (unsigned_extension_outputs_it != in_map.end()) {
      const cbor::Value::MapValue& unsigned_extension_outputs =
          unsigned_extension_outputs_it->second.GetMap();
      const auto dpk_it = unsigned_extension_outputs.find(
          cbor::Value(kExtensionDevicePublicKey));
      if (dpk_it != unsigned_extension_outputs.end()) {
        response->device_public_key =
            blink::mojom::DevicePublicKeyResponse::New();
        response->device_public_key->signature = dpk_it->second.GetBytestring();
      }
      const auto prf_it =
          unsigned_extension_outputs.find(cbor::Value(kExtensionPRF));
      if (prf_it != unsigned_extension_outputs.end()) {
        const cbor::Value::MapValue& results_from_authenticator =
            prf_it->second.GetMap()
                .find(cbor::Value(kExtensionPRFResults))
                ->second.GetMap();
        auto results_for_response = blink::mojom::PRFValues::New();
        results_for_response->first =
            results_from_authenticator.find(cbor::Value(kExtensionPRFFirst))
                ->second.GetBytestring();
        const auto second_it =
            results_from_authenticator.find(cbor::Value(kExtensionPRFSecond));
        if (second_it != results_from_authenticator.end()) {
          results_for_response->second = second_it->second.GetBytestring();
        }
        response->prf_results = std::move(results_for_response);
      }
    }

    std::move(callback).Run(
        static_cast<uint32_t>(device::CtapDeviceResponseCode::kSuccess),
        std::move(response));
  }

  Discovery::AdvertEventStream::Callback ble_advert_callback_;
  const raw_ptr<device::VirtualCtap2Device> ctap2_device_;
  const raw_ptr<authenticator::Observer> observer_;
  base::WeakPtrFactory<TestPlatform> weak_factory_{this};
};

}  // namespace

namespace authenticator {
namespace {

class LateLinkingDevice : public authenticator::Transaction {
 public:
  LateLinkingDevice(CtapDeviceResponseCode ctap_error,
                    std::unique_ptr<Platform> platform,
                    network::mojom::NetworkContext* network_context,
                    base::span<const uint8_t> qr_secret,
                    base::span<const uint8_t, kP256X962Length> peer_identity)
      : ctap_error_(ctap_error),
        platform_(std::move(platform)),
        network_context_(network_context),
        tunnel_id_(device::cablev2::Derive<EXTENT(tunnel_id_)>(
            qr_secret,
            base::span<uint8_t>(),
            DerivedValueType::kTunnelID)),
        eid_key_(device::cablev2::Derive<EXTENT(eid_key_)>(
            qr_secret,
            base::span<const uint8_t>(),
            device::cablev2::DerivedValueType::kEIDKey)),
        peer_identity_(device::fido_parsing_utils::Materialize(peer_identity)),
        secret_(fido_parsing_utils::Materialize(qr_secret)) {
    websocket_client_ = std::make_unique<device::cablev2::WebSocketAdapter>(
        base::BindOnce(&LateLinkingDevice::OnTunnelReady,
                       base::Unretained(this)),
        base::BindRepeating(&LateLinkingDevice::OnTunnelData,
                            base::Unretained(this)));

    const GURL target = device::cablev2::tunnelserver::GetNewTunnelURL(
        kTunnelServer, tunnel_id_);

    network_context_->CreateWebSocket(
        target, {device::kCableWebSocketProtocol}, net::SiteForCookies(),
        net::IsolationInfo(), /*additional_headers=*/{},
        network::mojom::kBrowserProcessId, url::Origin::Create(target),
        network::mojom::kWebSocketOptionBlockAllCookies,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        websocket_client_->BindNewHandshakeClientPipe(),
        /*url_loader_network_observer=*/mojo::NullRemote(),
        /*auth_handler=*/mojo::NullRemote(),
        /*header_client=*/mojo::NullRemote(),
        /*throttling_profile_id=*/absl::nullopt);
  }

 private:
  void OnTunnelReady(
      WebSocketAdapter::Result result,
      absl::optional<std::array<uint8_t, device::cablev2::kRoutingIdSize>>
          routing_id) {
    CHECK_EQ(result, WebSocketAdapter::Result::OK);
    CHECK(routing_id);

    CableEidArray plaintext_eid;
    device::cablev2::eid::Components components;
    components.tunnel_server_domain = kTunnelServer;
    components.routing_id = *routing_id;
    components.nonce = RandomNonce();

    plaintext_eid = device::cablev2::eid::FromComponents(components);

    ble_advert_ =
        platform_->SendBLEAdvert(eid::Encrypt(plaintext_eid, eid_key_));
    psk_ = device::cablev2::Derive<EXTENT(psk_)>(
        secret_, plaintext_eid, device::cablev2::DerivedValueType::kPSK);
  }

  std::array<uint8_t, device::cablev2::kNonceSize> RandomNonce() {
    std::array<uint8_t, device::cablev2::kNonceSize> ret;
    crypto::RandBytes(ret);
    return ret;
  }

  // BuildGetInfoResponse returns a CBOR-encoded getInfo response.
  std::vector<uint8_t> BuildGetInfoResponse() {
    std::array<uint8_t, device::kAaguidLength> aaguid{};
    std::vector<cbor::Value> versions;
    versions.emplace_back("FIDO_2_0");
    versions.emplace_back("FIDO_2_1");

    cbor::Value::MapValue options;
    options.emplace("uv", true);
    options.emplace("rk", true);

    cbor::Value::MapValue response_map;
    response_map.emplace(1, std::move(versions));
    response_map.emplace(3, aaguid);
    response_map.emplace(4, std::move(options));

    return cbor::Writer::Write(cbor::Value(std::move(response_map))).value();
  }

  void OnTunnelData(absl::optional<base::span<const uint8_t>> msg) {
    if (!msg) {
      platform_->OnCompleted(absl::nullopt);
      return;
    }

    switch (state_) {
      case State::kWaitingForConnection: {
        std::vector<uint8_t> response;
        HandshakeResult result = RespondToHandshake(
            psk_, /*identity=*/nullptr, peer_identity_, *msg, &response);
        CHECK(result);
        handshake_hash_ = result->second;
        websocket_client_->Write(response);
        crypter_ = std::move(result->first);
        crypter_->UseNewConstruction();

        cbor::Value::MapValue post_handshake_msg;
        post_handshake_msg.emplace(1, BuildGetInfoResponse());

        absl::optional<std::vector<uint8_t>> post_handshake_msg_bytes =
            cbor::Writer::Write(cbor::Value(std::move(post_handshake_msg)));
        CHECK(post_handshake_msg_bytes);
        CHECK(crypter_->Encrypt(&post_handshake_msg_bytes.value()));
        websocket_client_->Write(*post_handshake_msg_bytes);
        state_ = State::kWaitingForShutdown;
        break;
      }

      case State::kWaitingForShutdown: {
        std::vector<uint8_t> plaintext;
        CHECK(crypter_->Decrypt(*msg, &plaintext));
        CHECK(!plaintext.empty());
        const uint8_t message_type_byte = plaintext[0];
        plaintext.erase(plaintext.begin());
        CHECK_LE(message_type_byte,
                 static_cast<uint8_t>(MessageType::kMaxValue));
        const MessageType message_type =
            static_cast<MessageType>(message_type_byte);
        switch (message_type) {
          case MessageType::kCTAP: {
            std::vector<uint8_t> response = {
                message_type_byte,
                static_cast<uint8_t>(ctap_error_),
            };
            CHECK(crypter_->Encrypt(&response));
            websocket_client_->Write(response);
            break;
          }

          case MessageType::kShutdown:
            state_ = State::kShutdownReceived;
            base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
                FROM_HERE,
                base::BindOnce(&LateLinkingDevice::SendLinkingUpdate,
                               base::Unretained(this)),
                base::Seconds(10));
            break;

          case MessageType::kUpdate:
            CHECK(false);
            break;
        }
        break;
      }

      case State::kShutdownReceived:
        CHECK(false);
        break;
    }
  }

  void SendLinkingUpdate() {
    CHECK(state_ == State::kShutdownReceived);

    std::vector<uint8_t> contact_id = {1, 2, 3, 4};
    std::array<uint8_t, kPairingIDSize> pairing_id = {5, 6, 7, 8};
    std::array<uint8_t, 32> paired_secret = {9, 10, 11, 12};
    std::array<uint8_t, 32> root_secret = {13, 14, 15, 16};

    bssl::UniquePtr<EC_KEY> identity_key(IdentityKey(root_secret));
    device::CableAuthenticatorIdentityKey public_key;
    CHECK_EQ(
        public_key.size(),
        EC_POINT_point2oct(EC_KEY_get0_group(identity_key.get()),
                           EC_KEY_get0_public_key(identity_key.get()),
                           POINT_CONVERSION_UNCOMPRESSED, public_key.data(),
                           public_key.size(), /*ctx=*/nullptr));

    cbor::Value::MapValue pairing;
    pairing.emplace(1, std::move(contact_id));
    pairing.emplace(2, pairing_id);
    pairing.emplace(3, paired_secret);
    pairing.emplace(4, public_key);
    pairing.emplace(5, "Device name");
    pairing.emplace(
        6, CalculatePairingSignature(identity_key.get(), peer_identity_,
                                     handshake_hash_));

    cbor::Value::MapValue update_msg;
    update_msg.emplace(1, cbor::Value(std::move(pairing)));

    absl::optional<std::vector<uint8_t>> update_msg_bytes =
        cbor::Writer::Write(cbor::Value(std::move(update_msg)));
    CHECK(update_msg_bytes);
    update_msg_bytes->insert(update_msg_bytes->begin(),
                             static_cast<uint8_t>(MessageType::kUpdate));
    CHECK(crypter_->Encrypt(&update_msg_bytes.value()));
    websocket_client_->Write(*update_msg_bytes);
  }

  enum class State {
    kWaitingForConnection,
    kWaitingForShutdown,
    kShutdownReceived,
  };

  const CtapDeviceResponseCode ctap_error_;
  const std::unique_ptr<Platform> platform_;
  const raw_ptr<network::mojom::NetworkContext> network_context_;
  const std::array<uint8_t, kTunnelIdSize> tunnel_id_;
  const std::array<uint8_t, kEIDKeySize> eid_key_;
  const std::array<uint8_t, kP256X962Length> peer_identity_;
  const std::vector<uint8_t> secret_;
  State state_ = State::kWaitingForConnection;
  std::unique_ptr<WebSocketAdapter> websocket_client_;
  std::unique_ptr<Crypter> crypter_;
  std::unique_ptr<Platform::BLEAdvert> ble_advert_;
  std::array<uint8_t, kPSKSize> psk_;
  GURL target_;
  HandshakeHash handshake_hash_;
};

}  // namespace
}  // namespace authenticator

std::unique_ptr<network::mojom::NetworkContext> NewMockTunnelServer(
    absl::optional<ContactCallback> contact_callback) {
  return std::make_unique<TestNetworkContext>(std::move(contact_callback));
}

namespace authenticator {

std::unique_ptr<authenticator::Platform> NewMockPlatform(
    Discovery::AdvertEventStream::Callback ble_advert_callback,
    device::VirtualCtap2Device* ctap2_device,
    authenticator::Observer* observer) {
  return std::make_unique<TestPlatform>(ble_advert_callback, ctap2_device,
                                        observer);
}

// NewLateLinkingDevice returns a caBLEv2 authenticator that sends linking
// information 10 seconds after the CTAP2 transition is complete. It fails all
// CTAP2 requests with the given error.
std::unique_ptr<Transaction> NewLateLinkingDevice(
    CtapDeviceResponseCode ctap_error,
    std::unique_ptr<Platform> platform,
    network::mojom::NetworkContext* network_context,
    base::span<const uint8_t> qr_secret,
    base::span<const uint8_t, kP256X962Length> peer_identity) {
  return std::make_unique<LateLinkingDevice>(ctap_error, std::move(platform),
                                             network_context, qr_secret,
                                             peer_identity);
}

}  // namespace authenticator
}  // namespace device::cablev2
