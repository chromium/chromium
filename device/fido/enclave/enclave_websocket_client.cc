// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/enclave_websocket_client.h"

#include <limits>

#include "components/device_event_log/device_event_log.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace device::enclave {
namespace {

constexpr size_t kMaxIncomingMessageSize = 1 << 20;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("passkey_enclave_client", R"(
        semantics {
          sender: "Cloud Enclave Passkey Authenticator Client"
          description:
            "Chrome can use a cloud-based authenticator running in a trusted "
            "execution environment to fulfill WebAuthn getAssertion requests "
            "for passkeys synced to Chrome from Google Password Manager. This "
            "is used on desktop platforms where there is not a way to safely "
            "unwrap the private keys with a lock screen knowledge factor. "
            "This traffic creates an encrypted session with the enclave "
            "service and carries the request and response over that session."
          trigger:
            "A web site initiates a WebAuthn request for passkeys on a device "
            "that has been enrolled with the cloud authenticator, and there "
            "is an available Google Password Manager passkey that can be used "
            "to provide the assertion."
          user_data {
            type: PROFILE_DATA
            type: CREDENTIALS
          }
          data: "This contains an encrypted WebAuthn assertion request as "
            "well as an encrypted passkey which can only be unwrapped by the "
            "enclave service."
          internal {
            contacts {
                email: "chrome-webauthn@google.com"
            }
          }
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2023-07-05"
        }
        policy {
          cookies_allowed: NO
          setting: "Users can disable this authenticator by opening settings "
            "and signing out of the Google account in their profile, or by "
            "disabling password sync on the profile. Password sync can be "
            "disabled from the Sync and Google Services screen."
          chrome_policy {
            SyncDisabled {
              SyncDisabled: true
            }
            SyncTypesListDisabled {
              SyncTypesListDisabled: {
                entries: "passwords"
              }
            }
          }
        })");

}  // namespace

EnclaveWebSocketClient::EnclaveWebSocketClient(
    const GURL& service_url,
    const std::string& username,
    raw_ptr<network::mojom::NetworkContext> network_context,
    OnResponseCallback on_response)
    : state_(State::kInitialized),
      service_url_(service_url),
      username_(username),
      network_context_(network_context),
      on_response_(std::move(on_response)),
      readable_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL) {}

EnclaveWebSocketClient::~EnclaveWebSocketClient() = default;

void EnclaveWebSocketClient::Write(base::span<const uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kDisconnected ||
      data.size() > std::numeric_limits<uint32_t>::max()) {
    FIDO_LOG(ERROR) << "Invalid WebSocket write.";
    ClosePipe(SocketStatus::kError);
    return;
  }

  if (state_ == State::kInitialized) {
    Connect();
  }

  if (state_ != State::kOpen) {
    pending_write_data_ = fido_parsing_utils::Materialize(data);
    return;
  }

  InternalWrite(data);
}

void EnclaveWebSocketClient::Connect() {
  // A disconnect handler is used so that the request can be completed in the
  // event of an unexpected disconnection from the network service.
  auto handshake_remote = handshake_receiver_.BindNewPipeAndPassRemote();
  handshake_receiver_.set_disconnect_handler(base::BindOnce(
      &EnclaveWebSocketClient::OnMojoPipeDisconnect, base::Unretained(this)));

  state_ = State::kConnecting;

  GURL socket_url;
  GURL::Replacements replacement;
  replacement.SetPathStr(username_);
  socket_url = service_url_.ReplaceComponents(replacement);
  network_context_->CreateWebSocket(
      socket_url, {}, net::SiteForCookies(), /*has_storage_access=*/false,
      net::IsolationInfo(),
      /*additional_headers=*/{}, network::mojom::kBrowserProcessId,
      url::Origin::Create(socket_url),
      network::mojom::kWebSocketOptionBlockAllCookies,
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      std::move(handshake_remote),
      /*url_loader_network_observer=*/mojo::NullRemote(),
      /*auth_handler=*/mojo::NullRemote(),
      /*header_client=*/mojo::NullRemote(),
      /*throttling_profile_id=*/absl::nullopt);
}

void EnclaveWebSocketClient::InternalWrite(base::span<const uint8_t> data) {
  CHECK(state_ == State::kOpen);

  websocket_->SendMessage(network::mojom::WebSocketMessageType::BINARY,
                          data.size());
  uint32_t num_bytes = static_cast<uint32_t>(data.size());
  MojoResult result = writable_->WriteData(data.data(), &num_bytes,
                                           MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
  CHECK(result != MOJO_RESULT_OK ||
        data.size() == static_cast<size_t>(num_bytes));
  if (result != MOJO_RESULT_OK) {
    FIDO_LOG(ERROR) << "Failed to write to WebSocket.";
    ClosePipe(SocketStatus::kError);
  }
}

void EnclaveWebSocketClient::OnOpeningHandshakeStarted(
    network::mojom::WebSocketHandshakeRequestPtr request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void EnclaveWebSocketClient::OnFailure(const std::string& message,
                                       int net_error,
                                       int response_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FIDO_LOG(ERROR) << "Enclave service connection failed " << message << ", "
                  << net_error << ", " << response_code;

  ClosePipe(SocketStatus::kError);
}

void EnclaveWebSocketClient::OnConnectionEstablished(
    mojo::PendingRemote<network::mojom::WebSocket> socket,
    mojo::PendingReceiver<network::mojom::WebSocketClient> client_receiver,
    network::mojom::WebSocketHandshakeResponsePtr response,
    mojo::ScopedDataPipeConsumerHandle readable,
    mojo::ScopedDataPipeProducerHandle writable) {
  CHECK(!websocket_.is_bound());
  CHECK(state_ == State::kConnecting);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  websocket_.Bind(std::move(socket));
  readable_ = std::move(readable);
  CHECK_EQ(readable_watcher_.Watch(
               readable_.get(), MOJO_HANDLE_SIGNAL_READABLE,
               MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
               base::BindRepeating(&EnclaveWebSocketClient::ReadFromDataPipe,
                                   base::Unretained(this))),
           MOJO_RESULT_OK);
  writable_ = std::move(writable);
  client_receiver_.Bind(std::move(client_receiver));

  // |handshake_receiver_| will disconnect soon. In order to catch network
  // process crashes, we switch to watching |client_receiver_|.
  handshake_receiver_.set_disconnect_handler(base::DoNothing());
  client_receiver_.set_disconnect_handler(base::BindOnce(
      &EnclaveWebSocketClient::OnMojoPipeDisconnect, base::Unretained(this)));

  websocket_->StartReceiving();

  state_ = State::kOpen;

  if (pending_write_data_) {
    InternalWrite(*pending_write_data_);
    pending_write_data_ = absl::nullopt;
  }
}

void EnclaveWebSocketClient::OnDataFrame(
    bool finish,
    network::mojom::WebSocketMessageType type,
    uint64_t data_len) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kOpen);
  CHECK_EQ(pending_read_data_index_, pending_read_data_.size());
  CHECK(!pending_read_finished_);

  if (data_len == 0) {
    if (finish) {
      ProcessCompletedResponse();
    }
    return;
  }

  const size_t old_size = pending_read_data_index_;
  const size_t new_size = old_size + data_len;
  if ((type != network::mojom::WebSocketMessageType::BINARY &&
       type != network::mojom::WebSocketMessageType::CONTINUATION) ||
      data_len > std::numeric_limits<uint32_t>::max() || new_size < old_size ||
      new_size > kMaxIncomingMessageSize) {
    FIDO_LOG(ERROR) << "Invalid WebSocket frame (type: "
                    << static_cast<int>(type) << ", len: " << data_len << ")";
    ClosePipe(SocketStatus::kError);
    return;
  }

  pending_read_data_.resize(new_size);
  pending_read_finished_ = finish;
  client_receiver_.Pause();
  ReadFromDataPipe(MOJO_RESULT_OK, mojo::HandleSignalsState());
}

void EnclaveWebSocketClient::OnDropChannel(bool was_clean,
                                           uint16_t code,
                                           const std::string& reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(state_ == State::kOpen || state_ == State::kConnecting);

  ClosePipe(SocketStatus::kSocketClosed);
}

void EnclaveWebSocketClient::OnClosingHandshake() {}

void EnclaveWebSocketClient::ReadFromDataPipe(MojoResult,
                                              const mojo::HandleSignalsState&) {
  const size_t todo = pending_read_data_.size() - pending_read_data_index_;
  CHECK_GT(todo, 0u);

  // Truncation to 32-bits cannot overflow because |pending_read_data_.size()|
  // is bound by |kMaxIncomingMessageSize| when it is resized in |OnDataFrame|.
  uint32_t todo_32 = static_cast<uint32_t>(todo);
  static_assert(
      kMaxIncomingMessageSize <= std::numeric_limits<decltype(todo_32)>::max(),
      "");
  const MojoResult result =
      readable_->ReadData(&pending_read_data_.data()[pending_read_data_index_],
                          &todo_32, MOJO_READ_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_OK) {
    pending_read_data_index_ += todo_32;
    DCHECK_LE(pending_read_data_index_, pending_read_data_.size());

    if (pending_read_data_index_ < pending_read_data_.size()) {
      readable_watcher_.Arm();
    } else {
      client_receiver_.Resume();
      if (pending_read_finished_) {
        ProcessCompletedResponse();
      }
    }
  } else if (result == MOJO_RESULT_SHOULD_WAIT) {
    readable_watcher_.Arm();
  } else {
    FIDO_LOG(ERROR) << "Reading WebSocket frame failed: "
                    << static_cast<int>(result);
    ClosePipe(SocketStatus::kError);
  }
}

void EnclaveWebSocketClient::ProcessCompletedResponse() {
  on_response_.Run(SocketStatus::kOk, pending_read_data_);
  pending_read_data_index_ = 0;
  pending_read_finished_ = false;
  pending_read_data_.clear();
}

void EnclaveWebSocketClient::ClosePipe(SocketStatus status) {
  state_ = State::kDisconnected;
  client_receiver_.reset();
  pending_write_data_ = absl::nullopt;
  pending_read_data_index_ = 0;
  pending_read_finished_ = false;
  pending_read_data_.clear();
  on_response_.Run(status, std::vector<uint8_t>());
}

void EnclaveWebSocketClient::OnMojoPipeDisconnect() {
  ClosePipe(SocketStatus::kSocketClosed);
}

}  // namespace device::enclave
