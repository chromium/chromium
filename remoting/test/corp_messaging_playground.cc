// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/corp_messaging_playground.h"

#include <iostream>
#include <memory>

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

namespace remoting {

namespace {
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

CorpMessagingPlayground::CorpMessagingPlayground() {
  auto url_request_context_getter =
      base::MakeRefCounted<URLRequestContextGetter>(
          base::SingleThreadTaskRunner::GetCurrentDefault());
  url_loader_factory_owner_ =
      std::make_unique<network::TransitionalURLLoaderFactoryOwner>(
          url_request_context_getter, /* is_trusted= */ true);
  client_ = std::make_unique<CorpMessagingClient>(
      url_loader_factory_owner_->GetURLLoaderFactory(),
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
      base::BindRepeating(&CorpMessagingPlayground::OnSimpleMessageReceived,
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

void CorpMessagingPlayground::OnSimpleMessageReceived(
    const internal::SimpleMessageStruct& message) {
  // `create_time` is not used because it is set on the client and may be out of
  // sync with the time values set by the server.
  auto routing_latency = message.deliver_time - message.receive_time;
  LOG(INFO) << "SimpleMessage received: sender=" << message.sender_id.username
            << ", routing_latency=" << routing_latency.InMilliseconds()
            << "ms, payload=" << message.payload;
  last_sender_id_ = message.sender_id;

  if (IsPongMessage(message.payload)) {
    auto rtt = base::Time::Now() - last_ping_sent_time_;
    ping_total_rtt_ += rtt;
    LOG(INFO) << "Current RTT: " << rtt.InMilliseconds()
              << "ms, Total RTT: " << ping_total_rtt_.InMilliseconds() << "ms";
    // Now respond with a ping unless we've reached our max count.
    std::optional<std::string> ping_payload =
        OnPingPongMessageReceived(message.payload);
    if (ping_payload.has_value()) {
      last_ping_sent_time_ = base::Time::Now();
      client_->SendMessage(last_sender_id_, *ping_payload, base::DoNothing());
    } else {
      LOG(INFO) << "Ping-pong exchange finished. Total RTT: "
                << ping_total_rtt_.InMilliseconds() << "ms";
    }
  } else if (IsPingMessage(message.payload)) {
    std::optional<std::string> pong_payload =
        OnPingPongMessageReceived(message.payload);
    if (pong_payload.has_value()) {
      client_->SendMessage(last_sender_id_, *pong_payload, base::DoNothing());
    } else {
      LOG(ERROR) << "Failed to generate response for Ping: " << message.payload;
    }
  }
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
      StartPingPongMatch();
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
  if (last_sender_id_.username.empty()) {
    LOG(WARNING) << "No message received yet, destination ID is unknown.";
    return;
  }
  for (int i = 0; i < count; i++) {
    client_->SendMessage(last_sender_id_, "Hello from the playground!",
                         base::DoNothing());
  }
}

void CorpMessagingPlayground::StartPingPongMatch() {
  if (last_sender_id_.username.empty()) {
    LOG(WARNING) << "No message received yet, destination ID is unknown.";
    return;
  }
  LOG(INFO) << "Starting a new Ping-Pong match.";
  ping_total_rtt_ = {};
  last_ping_sent_time_ = base::Time::Now();
  client_->SendMessage(last_sender_id_, CreatePingMessage(1),
                       base::DoNothing());
}

void CorpMessagingPlayground::SendLargeMessage() {
  if (last_sender_id_.username.empty()) {
    LOG(WARNING) << "No message received yet, destination ID is unknown.";
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

  client_->SendMessage(last_sender_id_, payload, base::DoNothing());
}

}  // namespace remoting
