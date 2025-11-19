// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/corp_messaging_playground.h"

#include <iostream>
#include <memory>
#include <variant>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "net/ssl/client_cert_store.h"
#include "remoting/base/certificate_helpers.h"
#include "remoting/base/http_status.h"
#include "remoting/base/internal_headers.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/signaling/corp_messaging_client.h"
#include "remoting/test/ping_pong_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace remoting {

namespace {

using internal::BurstStruct;
using internal::PingPongStruct;
using internal::ShareSessionTokenStruct;
using internal::SimpleStruct;

// Squirrel-related messaging constants.
constexpr char kSquirrel[] = "🐿️";
constexpr int kSquirrelCount = 1000000;
constexpr char kSquirrelMsgStart[] = "Ready for lots of squirrels? -> ";
constexpr char kSquirrelMsgEnd[] = " -> Wow! That was nuts!!!";
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
      username, url_loader_factory_owner_->GetURLLoaderFactory(),
      CreateClientCertStoreInstance());
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

void CorpMessagingPlayground::OnPeerMessageReceived(
    const internal::PeerMessageStruct& message) {
  const auto* system_test =
      std::get_if<internal::SystemTestStruct>(&message.payload);
  if (!system_test) {
    LOG(WARNING) << "Received message with unsupported payload type.";
    return;
  }

  std::visit(absl::Overload(
                 [](const PingPongStruct& message) {
                   // TODO: joedow - Replace simple message ping-pong with proto
                   // version.
                 },
                 [](const BurstStruct& message) {
                   // TODO: joedow - Replace simple message burst with proto
                   // version.
                 },
                 [this](const SimpleStruct& simple_message) {
                   LOG(INFO) << "PeerMessage received: payload="
                             << simple_message.payload;

                   if (IsPongMessage(simple_message.payload)) {
                     auto rtt = base::Time::Now() - last_ping_sent_time_;
                     ping_total_rtt_ += rtt;
                     LOG(INFO) << "Current RTT: " << rtt.InMilliseconds()
                               << "ms, Total RTT: "
                               << ping_total_rtt_.InMilliseconds() << "ms";
                     // Now respond with a ping unless we've reached our max
                     // count.
                     std::optional<std::string> ping_payload =
                         OnPingPongMessageReceived(simple_message.payload);
                     if (ping_payload.has_value()) {
                       last_ping_sent_time_ = base::Time::Now();
                       client_->SendMessage(messaging_authz_token_,
                                            *ping_payload, base::DoNothing());
                     } else {
                       LOG(INFO) << "Ping-pong exchange finished. Total RTT: "
                                 << ping_total_rtt_.InMilliseconds() << "ms";
                     }
                   } else if (IsPingMessage(simple_message.payload)) {
                     std::optional<std::string> pong_payload =
                         OnPingPongMessageReceived(simple_message.payload);
                     if (pong_payload.has_value()) {
                       client_->SendMessage(messaging_authz_token_,
                                            *pong_payload, base::DoNothing());
                     } else {
                       LOG(ERROR) << "Failed to generate response for Ping: "
                                  << simple_message.payload;
                     }
                   }
                 },
                 [this](const ShareSessionTokenStruct& message) {
                   LOG(INFO) << "ShareSessionToken received.";
                   messaging_authz_token_ = message.messaging_authz_token;
                 }),
             system_test->test_message);
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
  for (int i = 0; i < count; i++) {
    client_->SendMessage(messaging_authz_token_, "Hello from the playground!",
                         base::DoNothing());
  }
}

void CorpMessagingPlayground::StartPingPongRally() {
  if (messaging_authz_token_.empty()) {
    LOG(WARNING) << "No authz token received yet, cannot start ping-pong.";
    return;
  }
  LOG(INFO) << "Starting a new Ping-Pong rally.";
  ping_total_rtt_ = {};
  last_ping_sent_time_ = base::Time::Now();
  client_->SendMessage(messaging_authz_token_, CreatePingMessage(1),
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

  client_->SendMessage(messaging_authz_token_, payload, base::DoNothing());
}

}  // namespace remoting
