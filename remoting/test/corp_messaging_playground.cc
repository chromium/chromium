// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/corp_messaging_playground.h"

#include <iostream>
#include <memory>
#include <set>
#include <variant>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "net/ssl/client_cert_store.h"
#include "remoting/base/certificate_helpers.h"
#include "remoting/base/ecdh_key_exchange.h"
#include "remoting/base/http_status.h"
#include "remoting/base/internal_headers.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/signaling/corp_messaging_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace remoting {

namespace {

using internal::BurstStruct;
using internal::EncryptedStruct;
using internal::OidcStruct;
using internal::PingPongStruct;
using internal::ShareSessionTokenStruct;
using internal::SimpleStruct;

// Squirrel-related messaging constants.
constexpr char kSquirrel[] = "🐿️";
constexpr int kSquirrelCount = 1000000;
constexpr char kSquirrelMsgStart[] = "Ready for lots of squirrels? -> ";
constexpr char kSquirrelMsgEnd[] = " -> Wow! That was nuts!!!";

constexpr char kEcdhInitiatePrefix[] = "ecdh-initiate:";
constexpr char kEcdhResponsePrefix[] = "ecdh-response:";

}  // namespace

class CorpMessagingPlayground::Core {
 public:
  using OnInputCallback = base::RepeatingCallback<void(char)>;

  explicit Core(OnInputCallback on_input_callback);
  ~Core();

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  void Start();

 private:
  OnInputCallback on_input_callback_;
};

CorpMessagingPlayground::Core::Core(OnInputCallback on_input_callback)
    : on_input_callback_(on_input_callback) {}

CorpMessagingPlayground::Core::~Core() = default;

void CorpMessagingPlayground::Core::Start() {
  base::SetNonBlocking(STDIN_FILENO);
  std::cout << "Press '1' to send a small message to the client." << std::endl;
  std::cout << "Press '2' to send a burst of 10 messages to the client."
            << std::endl;
  std::cout << "Press '3' to send a burst of 100 messages to the client."
            << std::endl;
  std::cout << "Press '4' to start a ping-pong exchange." << std::endl;
  std::cout << "Press '5' to send a large message." << std::endl;
  std::cout << "Press 'x' to quit." << std::endl << std::endl;

  while (true) {
    char c;
    int n = read(STDIN_FILENO, &c, 1);
    if (n == 1) {
      on_input_callback_.Run(c);
    } else if (n == -1 && errno != EAGAIN) {
      LOG(ERROR) << "Error reading from stdin";
      break;
    }
  }
}

CorpMessagingPlayground::CorpMessagingPlayground(const std::string& username) {
  auto url_request_context_getter =
      base::MakeRefCounted<URLRequestContextGetter>(
          base::SingleThreadTaskRunner::GetCurrentDefault());
  url_loader_factory_owner_ =
      std::make_unique<network::TransitionalURLLoaderFactoryOwner>(
          url_request_context_getter, /* is_trusted= */ true);
  client_ = std::make_unique<CorpMessagingClient>(
      username, key_pair_->GetPublicKey(),
      url_loader_factory_owner_->GetURLLoaderFactory(),
      CreateClientCertStoreInstance(),
      base::BindRepeating(&CorpMessagingPlayground::OnSignalingAddressChanged,
                          weak_factory_.GetWeakPtr()));
  core_ = std::make_unique<Core>(base::BindPostTask(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::BindRepeating(&CorpMessagingPlayground::OnCharacterInput,
                          weak_factory_.GetWeakPtr())));
}

CorpMessagingPlayground::~CorpMessagingPlayground() = default;

void CorpMessagingPlayground::Start() {
  run_loop_ = std::make_unique<base::RunLoop>();

  // `callback_subscription` is automatically unregistered after `run_loop_`
  // completes and this function goes out of scope.
  auto callback_subscription = client_->RegisterMessageCallback(
      base::BindRepeating(&CorpMessagingPlayground::OnPeerMessageReceived,
                          base::Unretained(this)));
  client_->StartReceivingMessages(
      base::BindOnce(&CorpMessagingPlayground::OnStreamOpened,
                     base::Unretained(this)),
      base::BindOnce(&CorpMessagingPlayground::OnStreamClosed,
                     base::Unretained(this)));

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&Core::Start, base::Unretained(core_.get())));

  run_loop_->Run();
}

void CorpMessagingPlayground::OnStreamOpened() {
  LOG(INFO) << "Stream opened...";
}

void CorpMessagingPlayground::OnStreamClosed(const HttpStatus& status) {
  LOG(INFO) << "Stream closed: " << status.ok() << ", "
            << static_cast<int>(status.error_code()) << ", "
            << status.error_message();
  run_loop_->Quit();
}

void CorpMessagingPlayground::OnSignalingAddressChanged(
    const SignalingAddress& local_address) {
  LOG(INFO) << "Local signaling address is: " << local_address.id();
}

void CorpMessagingPlayground::OnPeerMessageReceived(
    const SignalingAddress& sender_address,
    const SignalingMessage& message) {
  const auto* peer_message = std::get_if<internal::PeerMessageStruct>(&message);
  if (!peer_message) {
    LOG(WARNING) << "Received message with unsupported payload type.";
    return;
  }
  const auto* system_test =
      std::get_if<internal::SystemTestStruct>(&peer_message->payload);
  if (!system_test) {
    LOG(WARNING) << "Received message with unsupported payload type.";
    return;
  }

  std::visit(absl::Overload(
                 [this](const PingPongStruct& message) {
                   if (message.type == PingPongStruct::Type::PONG) {
                     auto rtt = base::Time::Now() - last_ping_sent_time_;
                     ping_total_rtt_ += rtt;
                     LOG(INFO) << "Current RTT: " << rtt.InMilliseconds()
                               << "ms, Total RTT: "
                               << ping_total_rtt_.InMilliseconds() << "ms";

                     if (message.current_count >= message.exchange_count) {
                       LOG(INFO) << "Ping-pong exchange finished. Total RTT: "
                                 << ping_total_rtt_.InMilliseconds() << "ms";
                       return;
                     }

                     // Send next PING.
                     internal::PingPongStruct ping_pong;
                     ping_pong.type = PingPongStruct::Type::PING;
                     ping_pong.rally_id = message.rally_id;
                     ping_pong.current_count = message.current_count + 1;
                     ping_pong.exchange_count = message.exchange_count;

                     internal::SystemTestStruct response_message;
                     response_message.test_message = std::move(ping_pong);

                     last_ping_sent_time_ = base::Time::Now();
                     internal::PeerMessageStruct peer_message;
                     peer_message.payload = std::move(response_message);
                     client_->SendMessage(
                         SignalingAddress(messaging_authz_token_),
                         SignalingMessage{std::move(peer_message)},
                         base::DoNothing());
                   } else if (message.type == PingPongStruct::Type::PING) {
                     // Send PONG.
                     internal::PingPongStruct ping_pong;
                     ping_pong.type = PingPongStruct::Type::PONG;
                     ping_pong.rally_id = message.rally_id;
                     ping_pong.current_count = message.current_count;
                     ping_pong.exchange_count = message.exchange_count;

                     internal::SystemTestStruct response_message;
                     response_message.test_message = std::move(ping_pong);

                     internal::PeerMessageStruct peer_message;
                     peer_message.payload = std::move(response_message);
                     client_->SendMessage(
                         SignalingAddress(messaging_authz_token_),
                         SignalingMessage{std::move(peer_message)},
                         base::DoNothing());
                   } else {
                     NOTREACHED();
                   }
                 },
                 [this](const BurstStruct& message) {
                   if (expected_burst_count_ != message.burst_count) {
                     // This is the first message of a new burst, or a different
                     // burst has started.
                     if (burst_check_timer_.IsRunning()) {
                       burst_check_timer_.Stop();
                     }
                     ResetBurstState();

                     burst_start_time_ = base::TimeTicks::Now();
                     expected_burst_count_ = message.burst_count;
                     burst_check_timer_.Start(
                         FROM_HERE, base::Seconds(1), this,
                         &CorpMessagingPlayground::OnBurstCheckTimerFired);
                     LOG(INFO) << "Receiving a new burst of "
                               << message.burst_count << " messages.";
                   }

                   auto [_, inserted] =
                       received_burst_indices_.insert(message.index);

                   if (inserted) {
                     LOG(INFO)
                         << "Burst message received: index=" << message.index
                         << " (" << received_burst_indices_.size() << "/"
                         << expected_burst_count_ << ")";
                   } else {
                     LOG(WARNING) << "Duplicate burst message received: index="
                                  << message.index;
                   }

                   if (received_burst_indices_.size() ==
                       static_cast<size_t>(expected_burst_count_)) {
                     auto total_time =
                         base::TimeTicks::Now() - burst_start_time_;
                     LOG(INFO) << "All " << expected_burst_count_
                               << " burst messages received in "
                               << total_time.InMilliseconds() << "ms.";
                     burst_check_timer_.Stop();
                     ResetBurstState();
                   }
                 },
                 [](const SimpleStruct& simple_message) {
                   LOG(INFO) << "PeerMessage received: payload="
                             << simple_message.payload;
                 },
                 [](const OidcStruct& oidc_message) {
                   // TODO: joedow - Implement an OIDC code exchange helper.
                   LOG(INFO) << "OIDC message received:\n"
                             << "  redirect_uri: " << oidc_message.redirect_uri
                             << "  state: " << oidc_message.state
                             << "  code: " << oidc_message.code;
                 },
                 [this](const EncryptedStruct& encrypted_struct) {
                   if (base::StartsWith(encrypted_struct.unencrypted_payload,
                                        kEcdhInitiatePrefix)) {
                     LOG(INFO) << "Received ECDH initiate message.";
                     std::string client_public_key_base64 =
                         encrypted_struct.unencrypted_payload.substr(
                             strlen(kEcdhInitiatePrefix));
                     std::optional<std::vector<uint8_t>> client_public_key =
                         base::Base64Decode(client_public_key_base64);
                     if (!client_public_key) {
                       LOG(ERROR) << "Failed to decode client public key.";
                       return;
                     }
                     key_exchange_ = std::make_unique<EcdhKeyExchange>();
                     crypter_ =
                         key_exchange_->CreateAesGcmCrypter(*client_public_key);
                     if (!crypter_) {
                       LOG(ERROR) << "Failed to derive encryption key.";
                       key_exchange_.reset();
                       return;
                     }

                     std::vector<uint8_t> signature =
                         key_pair_->Sign(key_exchange_->public_key_bytes());
                     base::DictValue response_dict;
                     response_dict.Set("publicKey",
                                       key_exchange_->PublicKeyBase64());
                     response_dict.Set("signature",
                                       base::Base64Encode(signature));
                     std::string response_json;
                     base::JSONWriter::Write(response_dict, &response_json);

                     internal::EncryptedStruct response;
                     response.unencrypted_payload =
                         std::string(kEcdhResponsePrefix) + response_json;
                     internal::SystemTestStruct system_test_struct;
                     system_test_struct.test_message = std::move(response);
                     internal::PeerMessageStruct peer_message;
                     peer_message.payload = std::move(system_test_struct);
                     LOG(INFO) << "Sending ECDH response: " << response_json;
                     client_->SendMessage(
                         SignalingAddress(messaging_authz_token_),
                         SignalingMessage{std::move(peer_message)},
                         base::DoNothing());
                   } else if (base::StartsWith(
                                  encrypted_struct.unencrypted_payload,
                                  kEcdhResponsePrefix)) {
                     // The ECDH key exchange is initiated by the client so
                     // receiving this message is unexpected.
                     LOG(ERROR) << "Received unexpected ECDH response message.";
                   } else {
                     LOG(INFO) << "Encrypted PeerMessage received: "
                               << "unencrypted_payload="
                               << encrypted_struct.unencrypted_payload;
                     auto decrypted_payload = crypter_->Decrypt(
                         base::as_bytes(base::span(encrypted_struct.payload)));
                     if (decrypted_payload.has_value()) {
                       LOG(INFO) << "Decrypted content = "
                                 << std::string(decrypted_payload->begin(),
                                                decrypted_payload->end());
                     } else {
                       LOG(WARNING) << "Failed to decrypt content";
                     }
                   }
                 },
                 [this](const ShareSessionTokenStruct& message) {
                   LOG(INFO) << "ShareSessionToken received.";
                   messaging_authz_token_ = message.messaging_authz_token;
                 }),
             system_test->test_message);
}

void CorpMessagingPlayground::OnBurstCheckTimerFired() {
  burst_timer_check_count_++;
  if (burst_timer_check_count_ >= 5) {
    LOG(WARNING) << "Burst message receipt timed out after 5 seconds.";
    burst_check_timer_.Stop();
    ResetBurstState();
    return;
  }

  if (expected_burst_count_ > 0) {
    size_t remaining = expected_burst_count_ - received_burst_indices_.size();
    LOG(INFO) << "Waiting for " << remaining << " more burst messages...";
  }
}

void CorpMessagingPlayground::ResetBurstState() {
  received_burst_indices_.clear();
  expected_burst_count_ = 0;
  burst_start_time_ = base::TimeTicks();
  burst_timer_check_count_ = 0;
}

void CorpMessagingPlayground::OnCharacterInput(char c) {
  switch (c) {
    case '1':
      SendMessage();
      break;
    case '2':
      SendMessage(10);
      break;
    case '3':
      SendMessage(100);
      break;
    case '4':
      StartPingPongRally();
      break;
    case '5':
      SendLargeMessage();
      break;
    case 'x':
      run_loop_->Quit();
      break;
  }
}

void CorpMessagingPlayground::SendMessage(int count) {
  if (messaging_authz_token_.empty()) {
    LOG(WARNING) << "No authz token received yet, cannot send message.";
    return;
  }

  if (count > 1) {
    for (int i = 0; i < count; i++) {
      internal::SystemTestStruct message;
      internal::BurstStruct burst;
      burst.index = i;
      burst.burst_count = count;
      burst.payload = "Burst message #" + base::NumberToString(i + 1) + " of " +
                      base::NumberToString(count);
      message.test_message = std::move(burst);
      internal::PeerMessageStruct peer_message;
      peer_message.payload = std::move(message);
      client_->SendMessage(SignalingAddress(messaging_authz_token_),
                           SignalingMessage{std::move(peer_message)},
                           base::DoNothing());
    }
    return;
  }
  internal::PeerMessageStruct peer_message;
  internal::SystemTestStruct system_test_struct;
  internal::SimpleStruct simple_struct;
  simple_struct.payload = "Hello from the playground!";
  system_test_struct.test_message = std::move(simple_struct);
  peer_message.payload = std::move(system_test_struct);
  client_->SendMessage(SignalingAddress(messaging_authz_token_),
                       SignalingMessage{std::move(peer_message)},
                       base::DoNothing());
}

void CorpMessagingPlayground::StartPingPongRally() {
  if (messaging_authz_token_.empty()) {
    LOG(WARNING) << "No authz token received yet, cannot start ping-pong.";
    return;
  }
  LOG(INFO) << "Starting a new Ping-Pong rally.";
  // TODO: joedow - Use a map to track RTTs for concurrent rallies.
  ping_total_rtt_ = {};
  last_ping_sent_time_ = base::Time::Now();
  internal::SystemTestStruct message;
  internal::PingPongStruct ping_pong;
  ping_pong.type = PingPongStruct::Type::PING;
  ping_pong.rally_id = "chromium-playground-rally-" +
                       base::Uuid::GenerateRandomV4().AsLowercaseString();
  ping_pong.current_count = 1;
  ping_pong.exchange_count = 10;
  message.test_message = std::move(ping_pong);
  internal::PeerMessageStruct peer_message;
  peer_message.payload = std::move(message);
  client_->SendMessage(SignalingAddress(messaging_authz_token_),
                       SignalingMessage{std::move(peer_message)},
                       base::DoNothing());
}

void CorpMessagingPlayground::SendLargeMessage() {
  if (messaging_authz_token_.empty()) {
    LOG(WARNING) << "No authz token received yet, cannot send large message.";
    return;
  }

  std::string payload(kSquirrelMsgStart);
  payload.reserve((sizeof(kSquirrelMsgStart) - 1) +
                  (sizeof(kSquirrelMsgEnd) - 1) +
                  (sizeof(kSquirrel) - 1) * kSquirrelCount);
  for (int i = 0; i < kSquirrelCount; i++) {
    payload += kSquirrel;
  }
  payload += kSquirrelMsgEnd;

  internal::PeerMessageStruct peer_message;
  internal::SystemTestStruct system_test_struct;
  internal::SimpleStruct simple_struct;
  simple_struct.payload = payload;
  system_test_struct.test_message = std::move(simple_struct);
  peer_message.payload = std::move(system_test_struct);
  client_->SendMessage(SignalingAddress(messaging_authz_token_),
                       SignalingMessage{std::move(peer_message)},
                       base::DoNothing());
}

}  // namespace remoting
