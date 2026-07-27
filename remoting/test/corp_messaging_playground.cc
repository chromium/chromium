// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/corp_messaging_playground.h"

#include <poll.h>
#include <unistd.h>

#include <cstdio>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
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

std::string SanitizeForLogging(std::string_view input) {
  std::string sanitized;
  sanitized.reserve(input.size());
  for (char c : input) {
    if ((c >= ' ' && c <= '~') || c == '\n' || c == '\t') {
      sanitized += c;
    } else {
      sanitized += '.';
    }
  }
  return sanitized;
}

}  // namespace

class CorpMessagingPlayground::Core
    : public base::RefCountedThreadSafe<CorpMessagingPlayground::Core> {
 public:
  using OnInputCallback = base::RepeatingCallback<void(char)>;

  Core(int shutdown_fd, OnInputCallback on_input_callback);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  void Start();

 private:
  friend class base::RefCountedThreadSafe<CorpMessagingPlayground::Core>;
  ~Core();

  int shutdown_fd_;
  OnInputCallback on_input_callback_;
};

CorpMessagingPlayground::Core::Core(int shutdown_fd,
                                    OnInputCallback on_input_callback)
    : shutdown_fd_(shutdown_fd),
      on_input_callback_(std::move(on_input_callback)) {}

CorpMessagingPlayground::Core::~Core() {
  if (shutdown_fd_ != -1) {
    close(shutdown_fd_);
  }
}

void CorpMessagingPlayground::Core::Start() {
  printf("Press '1' to send a small message to the client.\n");
  printf("Press '2' to send a burst of 10 messages to the client.\n");
  printf("Press '3' to send a burst of 100 messages to the client.\n");
  printf("Press '4' to start a ping-pong exchange.\n");
  printf("Press '5' to send a large message.\n");
  printf("Press '6' to send a structured IqStanza.\n");
  printf("Press 'x' to quit.\n\n");

  struct pollfd fds[2];
  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;
  fds[1].fd = shutdown_fd_;
  fds[1].events = POLLIN;

  while (true) {
    int ret = poll(fds, 2, -1);
    if (ret < 0) {
      if (errno == EINTR) {
        continue;
      }
      LOG(ERROR) << "poll failed, errno=" << errno;
      break;
    }

    if (fds[1].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) {
      LOG(INFO) << "Shutdown signaled or pipe closed, exiting input loop";
      break;
    }

    if (fds[0].revents & POLLIN) {
      char c;
      ssize_t n = read(STDIN_FILENO, &c, 1);
      if (n == 1) {
        on_input_callback_.Run(c);
      } else if (n == 0) {
        LOG(INFO) << "stdin EOF, exiting input loop";
        break;
      } else if (n == -1) {
        if (errno == EINTR) {
          continue;
        }
        LOG(ERROR) << "Error reading from stdin, errno=" << errno;
        break;
      }
    } else if (fds[0].revents & (POLLHUP | POLLERR | POLLNVAL)) {
      LOG(INFO) << "stdin closed or error, exiting input loop";
      break;
    }
  }
}

CorpMessagingPlayground::CorpMessagingPlayground(std::string username)
    : username_(std::move(username)) {
  if (pipe(shutdown_pipe_) != 0) {
    PLOG(FATAL) << "Failed to create shutdown pipe";
  }
  auto url_request_context_getter =
      base::MakeRefCounted<URLRequestContextGetter>(
          base::SingleThreadTaskRunner::GetCurrentDefault());
  url_loader_factory_owner_ =
      std::make_unique<network::TransitionalURLLoaderFactoryOwner>(
          url_request_context_getter, /* is_trusted= */ true);
  core_ = base::MakeRefCounted<Core>(
      shutdown_pipe_[0],
      base::BindPostTask(base::SingleThreadTaskRunner::GetCurrentDefault(),
                         base::BindRepeating(
                             &CorpMessagingPlayground::OnCharacterInput,
                             weak_factory_.GetWeakPtr())));
}

CorpMessagingPlayground::~CorpMessagingPlayground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (shutdown_pipe_[1] != -1) {
    char c = 0;
    (void)write(shutdown_pipe_[1], &c, 1);
    close(shutdown_pipe_[1]);
  }
}

void CorpMessagingPlayground::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  run_loop_ = std::make_unique<base::RunLoop>();

  LOG(INFO) << "Generating key pair in background...";
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&RsaKeyPair::Generate),
      base::BindOnce(&CorpMessagingPlayground::OnKeyPairGenerated,
                     weak_factory_.GetWeakPtr()));

  run_loop_->Run();
}

void CorpMessagingPlayground::OnKeyPairGenerated(
    scoped_refptr<RsaKeyPair> key_pair) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "Key pair generated.";
  key_pair_ = std::move(key_pair);

  client_ = std::make_unique<CorpMessagingClient>(
      username_, key_pair_->GetPublicKey(),
      url_loader_factory_owner_->GetURLLoaderFactory(),
      CreateClientCertStoreInstance(),
      base::BindRepeating(&CorpMessagingPlayground::OnSignalingAddressChanged,
                          weak_factory_.GetWeakPtr()));

  message_callback_subscription_ = client_->RegisterMessageCallback(
      base::BindRepeating(&CorpMessagingPlayground::OnPeerMessageReceived,
                          weak_factory_.GetWeakPtr()));

  client_->StartReceivingMessages(
      base::BindOnce(&CorpMessagingPlayground::OnStreamOpened,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&CorpMessagingPlayground::OnStreamClosed,
                     weak_factory_.GetWeakPtr()));

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&Core::Start, core_));
}

void CorpMessagingPlayground::OnStreamOpened() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "Stream opened...";
}

void CorpMessagingPlayground::OnStreamClosed(const HttpStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "Stream closed: " << status.ok() << ", "
            << static_cast<int>(status.error_code()) << ", "
            << status.error_message();
  run_loop_->Quit();
}

void CorpMessagingPlayground::OnSignalingAddressChanged(
    const SignalingAddress& local_address) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "Local signaling address is: " << local_address.id();
}

void CorpMessagingPlayground::OnPeerMessageReceived(
    const SignalingAddress& sender_address,
    const internal::PeerMessageStruct& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::visit(absl::Overload(
                 [this](const internal::SystemTestStruct& system_test) {
                   HandleSystemTest(system_test);
                 },
                 [this](const internal::IqStanzaStruct& iq_stanza) {
                   HandleIqStanza(iq_stanza);
                 },
                 [](const auto&) {
                   LOG(WARNING)
                       << "Received message with unsupported payload type.";
                 }),
             message.payload);
}

void CorpMessagingPlayground::HandleSystemTest(
    const internal::SystemTestStruct& system_test) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::visit(
      absl::Overload(
          [this](const PingPongStruct& message) {
            if (messaging_authz_token_.empty()) {
              LOG(ERROR) << "Received PingPong message but messaging_authz_token is empty.";
              return;
            }
            if (message.exchange_count <= 0 || message.exchange_count > 100) {
              LOG(ERROR) << "Invalid exchange_count: "
                         << message.exchange_count;
              return;
            }
            if (message.current_count <= 0 ||
                message.current_count > message.exchange_count) {
              LOG(ERROR) << "Invalid current_count: " << message.current_count;
              return;
            }
            if (message.rally_id.size() > 256) {
              LOG(ERROR) << "Invalid rally_id size: "
                         << message.rally_id.size();
              return;
            }

            if (message.type == PingPongStruct::Type::PONG) {
              int count = message.current_count;
              auto it = ping_sent_times_.find(count);
              if (it != ping_sent_times_.end()) {
                auto rtt = base::TimeTicks::Now() - it->second;
                ping_rtts_[count] = rtt;
                LOG(INFO) << "Pong received for " << count << ": RTT "
                          << rtt.InMilliseconds() << "ms";
              } else {
                LOG(ERROR) << "Ping-Pong error. No start time for iteration "
                           << count;
                return;
              }

              if (count < message.exchange_count) {
                int next_count = count + 1;
                ping_sent_times_[next_count] = base::TimeTicks::Now();
                internal::PingPongStruct ping_pong;
                ping_pong.type = PingPongStruct::Type::PING;
                ping_pong.rally_id = message.rally_id;
                ping_pong.current_count = next_count;
                ping_pong.exchange_count = message.exchange_count;

                internal::SystemTestStruct response_message;
                response_message.test_message = std::move(ping_pong);

                internal::PeerMessageStruct peer_message;
                peer_message.payload = std::move(response_message);
                client_->SendMessage(SignalingAddress(messaging_authz_token_),
                                     std::move(peer_message),
                                     base::DoNothing());
              } else {
                auto start_time_it = ping_sent_times_.find(1);
                if (start_time_it != ping_sent_times_.end()) {
                  auto total_time =
                      base::TimeTicks::Now() - start_time_it->second;
                  std::string rtt_report = "Ping-Pong finished.\n";
                  for (const auto& [c, rtt] : ping_rtts_) {
                    rtt_report += "count " + base::NumberToString(c) + ": " +
                                  base::NumberToString(rtt.InMilliseconds()) +
                                  "ms\n";
                  }
                  rtt_report +=
                      "Total time: " +
                      base::NumberToString(total_time.InMilliseconds()) + "ms";
                  LOG(INFO) << rtt_report;
                }
              }
            } else if (message.type == PingPongStruct::Type::PING) {
              int count = message.current_count;
              LOG(INFO) << "Ping:" << count << " received, sending Pong.";
              if (count > 0) {
                internal::PingPongStruct ping_pong;
                ping_pong.type = PingPongStruct::Type::PONG;
                ping_pong.rally_id = message.rally_id;
                ping_pong.current_count = count;
                ping_pong.exchange_count = message.exchange_count;

                internal::SystemTestStruct response_message;
                response_message.test_message = std::move(ping_pong);

                internal::PeerMessageStruct peer_message;
                peer_message.payload = std::move(response_message);
                client_->SendMessage(SignalingAddress(messaging_authz_token_),
                                     std::move(peer_message),
                                     base::DoNothing());
              }
            } else {
              NOTREACHED();
            }
          },
          [this](const BurstStruct& message) {
            if (message.burst_count <= 0 || message.burst_count > 1000) {
              LOG(ERROR) << "Invalid burst_count: " << message.burst_count;
              return;
            }
            if (message.index < 0 || message.index >= message.burst_count) {
              LOG(ERROR) << "Invalid index " << message.index
                         << " for burst_count " << message.burst_count;
              return;
            }

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
              LOG(INFO) << "Receiving a new burst of " << message.burst_count
                        << " messages.";
            }

            auto [_, inserted] = received_burst_indices_.insert(message.index);

            if (inserted) {
              LOG(INFO) << "Burst message received: index=" << message.index
                        << " (" << received_burst_indices_.size() << "/"
                        << expected_burst_count_ << ")";
            } else {
              LOG(WARNING) << "Duplicate burst message received: index="
                           << message.index;
            }

            if (received_burst_indices_.size() ==
                static_cast<size_t>(expected_burst_count_)) {
              auto total_time = base::TimeTicks::Now() - burst_start_time_;
              LOG(INFO) << "All " << expected_burst_count_
                        << " burst messages received in "
                        << total_time.InMilliseconds() << "ms.";
              burst_check_timer_.Stop();
              ResetBurstState();
            }
          },
          [](const SimpleStruct& simple_message) {
            if (simple_message.payload.size() > 5 * 1024 * 1024) {
              LOG(ERROR) << "Payload too large: "
                         << simple_message.payload.size();
              return;
            }
            if (simple_message.payload.size() > 1000) {
              LOG(INFO) << "PeerMessage received (large): size="
                        << simple_message.payload.size() << ", prefix="
                        << SanitizeForLogging(
                               std::string_view(simple_message.payload)
                                   .substr(0, 100))
                        << "...";
            } else {
              LOG(INFO) << "PeerMessage received: payload="
                        << SanitizeForLogging(simple_message.payload);
            }
          },
          [](const OidcStruct& oidc_message) {
            if (oidc_message.redirect_uri.size() > 1024 ||
                oidc_message.state.size() > 1024 ||
                oidc_message.code.size() > 1024) {
              LOG(ERROR) << "OIDC message fields too large";
              return;
            }
            // TODO: joedow - Implement an OIDC code exchange helper.
            LOG(INFO) << "OIDC message received:\n"
                      << "  redirect_uri: "
                      << SanitizeForLogging(oidc_message.redirect_uri)
                      << "  state: " << SanitizeForLogging(oidc_message.state)
                      << "  code: " << SanitizeForLogging(oidc_message.code);
          },
          [this](const EncryptedStruct& encrypted_struct) {
            if (messaging_authz_token_.empty()) {
              LOG(ERROR) << "Received EncryptedStruct but messaging_authz_token is empty.";
              return;
            }
            if (encrypted_struct.payload.size() > 5 * 1024 * 1024 ||
                encrypted_struct.unencrypted_payload.size() > 4096) {
              LOG(ERROR) << "Encrypted message too large";
              return;
            }
            if (base::StartsWith(encrypted_struct.unencrypted_payload,
                                 kEcdhInitiatePrefix)) {
              LOG(INFO) << "Received ECDH initiate message.";
              std::string_view client_public_key_base64 =
                  std::string_view(encrypted_struct.unencrypted_payload)
                      .substr(strlen(kEcdhInitiatePrefix));
              std::optional<std::vector<uint8_t>> client_public_key =
                  base::Base64Decode(client_public_key_base64);
              if (!client_public_key) {
                LOG(ERROR) << "Failed to decode client public key.";
                return;
              }
              key_exchange_ = std::make_unique<EcdhKeyExchange>();
              crypter_ = key_exchange_->CreateAesGcmCrypter(*client_public_key);
              if (!crypter_) {
                LOG(ERROR) << "Failed to derive encryption key.";
                key_exchange_.reset();
                return;
              }

              std::vector<uint8_t> signature =
                  key_pair_->Sign(key_exchange_->public_key_bytes());
              base::DictValue response_dict;
              response_dict.Set("publicKey", key_exchange_->PublicKeyBase64());
              response_dict.Set("signature", base::Base64Encode(signature));
              std::optional<std::string> response_json =
                  base::WriteJson(response_dict);
              if (!response_json) {
                LOG(ERROR) << "Failed to serialize JSON";
                return;
              }

              internal::EncryptedStruct response;
              response.unencrypted_payload =
                  std::string(kEcdhResponsePrefix) + *response_json;
              internal::SystemTestStruct system_test_struct;
              system_test_struct.test_message = std::move(response);
              internal::PeerMessageStruct peer_message;
              peer_message.payload = std::move(system_test_struct);
              LOG(INFO) << "Sending ECDH response: " << *response_json;
              client_->SendMessage(SignalingAddress(messaging_authz_token_),
                                   std::move(peer_message), base::DoNothing());
            } else if (base::StartsWith(encrypted_struct.unencrypted_payload,
                                        kEcdhResponsePrefix)) {
              // The ECDH key exchange is initiated by the client so
              // receiving this message is unexpected.
              LOG(ERROR) << "Received unexpected ECDH response message.";
            } else {
              LOG(INFO) << "Encrypted PeerMessage received: "
                        << "unencrypted_payload="
                        << SanitizeForLogging(
                               encrypted_struct.unencrypted_payload);
              if (!crypter_) {
                LOG(ERROR) << "Received encrypted message but key exchange is "
                              "not complete.";
                return;
              }
              auto decrypted_payload = crypter_->Decrypt(
                  base::as_bytes(base::span(encrypted_struct.payload)));
              if (decrypted_payload.has_value()) {
                std::string_view decrypted_view(
                    reinterpret_cast<const char*>(decrypted_payload->data()),
                    decrypted_payload->size());
                if (decrypted_view.size() > 1000) {
                  LOG(INFO) << "Decrypted content (large): size="
                            << decrypted_view.size() << ", prefix="
                            << SanitizeForLogging(decrypted_view.substr(0, 100))
                            << "...";
                } else {
                  LOG(INFO) << "Decrypted content = "
                            << SanitizeForLogging(decrypted_view);
                }
              } else {
                LOG(WARNING) << "Failed to decrypt content";
              }
            }
          },
          [this](const ShareSessionTokenStruct& message) {
            if (message.messaging_authz_token.size() > 4096) {
              LOG(ERROR) << "messaging_authz_token too large: "
                         << message.messaging_authz_token.size();
              return;
            }
            LOG(INFO) << "ShareSessionToken received.";
            messaging_authz_token_ = message.messaging_authz_token;
          }),
      system_test.test_message);
}

void CorpMessagingPlayground::HandleIqStanza(
    const internal::IqStanzaStruct& iq_stanza) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (iq_stanza.id.size() > 256) {
    LOG(ERROR) << "IqStanza ID too large: " << iq_stanza.id.size();
    return;
  }
  if (iq_stanza.xml.size() > 65536) {
    LOG(ERROR) << "IqStanza XML too large: " << iq_stanza.xml.size();
    return;
  }

  LOG(INFO) << "Received IqStanza:";
  LOG(INFO) << "  ID: " << SanitizeForLogging(iq_stanza.id);
  if (!iq_stanza.xml.empty()) {
    LOG(INFO) << "  XML: " << SanitizeForLogging(iq_stanza.xml);
  }
  std::visit(
      absl::Overload([](std::monostate) { LOG(INFO) << "  Payload: Empty"; },
                     [](const internal::JingleMessageStruct& jingle) {
                       if (jingle.session_id.size() > 256) {
                         LOG(ERROR) << "Jingle session_id too large: "
                                    << jingle.session_id.size();
                         return;
                       }
                       LOG(INFO) << "  Payload: Jingle, SessionID="
                                 << SanitizeForLogging(jingle.session_id);
                     },
                     [](const internal::JingleReplyStruct&) {
                       LOG(INFO) << "  Payload: JingleReply";
                     },
                     [](const internal::ErrorStanzaStruct& error) {
                       if (error.text.size() > 4096) {
                         LOG(ERROR)
                             << "Error text too large: " << error.text.size();
                         return;
                       }
                       LOG(INFO) << "  Payload: Error, Condition="
                                 << static_cast<int>(error.condition)
                                 << ", Text=" << SanitizeForLogging(error.text);
                     }),
      iq_stanza.payload);
}

void CorpMessagingPlayground::OnBurstCheckTimerFired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  received_burst_indices_.clear();
  expected_burst_count_ = 0;
  burst_start_time_ = base::TimeTicks();
  burst_timer_check_count_ = 0;
}

void CorpMessagingPlayground::OnCharacterInput(char c) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
    case '6':
      SendIqStanza();
      break;
    case 'x':
      run_loop_->Quit();
      break;
  }
}

void CorpMessagingPlayground::SendMessage(int count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (messaging_authz_token_.empty()) {
    LOG(WARNING) << "No authz token received yet, cannot send message.";
    return;
  }

  SignalingAddress destination(messaging_authz_token_);

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
      client_->SendMessage(destination, std::move(peer_message),
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
  client_->SendMessage(destination, std::move(peer_message), base::DoNothing());
}

void CorpMessagingPlayground::StartPingPongRally() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (messaging_authz_token_.empty()) {
    LOG(WARNING) << "No authz token received yet, cannot start ping-pong.";
    return;
  }
  LOG(INFO) << "Starting a new Ping-Pong rally.";
  ping_rtts_.clear();
  ping_sent_times_.clear();
  ping_sent_times_[1] = base::TimeTicks::Now();
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
                       std::move(peer_message), base::DoNothing());
}

void CorpMessagingPlayground::SendLargeMessage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  simple_struct.payload = std::move(payload);
  system_test_struct.test_message = std::move(simple_struct);
  peer_message.payload = std::move(system_test_struct);
  client_->SendMessage(SignalingAddress(messaging_authz_token_),
                       std::move(peer_message), base::DoNothing());
}

void CorpMessagingPlayground::SendIqStanza() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (messaging_authz_token_.empty()) {
    LOG(WARNING) << "No authz token received yet, cannot send IqStanza.";
    return;
  }

  internal::IqStanzaStruct iq_stanza;
  iq_stanza.id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  iq_stanza.sender.local_part = username_;
  iq_stanza.sender.domain_part = "google.com";
  iq_stanza.receiver.local_part = "fake-host";
  iq_stanza.receiver.domain_part = "archboard.corp.google.com";

  internal::JingleMessageStruct jingle;
  jingle.session_id = "playground-sid-123";

  iq_stanza.payload = std::move(jingle);
  iq_stanza.messaging_authz_token = messaging_authz_token_;

  LOG(INFO) << "Sending structured IqStanza, ID: " << iq_stanza.id;

  internal::PeerMessageStruct peer_message;
  peer_message.payload = std::move(iq_stanza);
  client_->SendMessage(SignalingAddress(messaging_authz_token_),
                       std::move(peer_message), base::DoNothing());
}

}  // namespace remoting
