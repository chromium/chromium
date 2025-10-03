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
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "net/ssl/client_cert_store.h"
#include "remoting/base/certificate_helpers.h"
#include "remoting/base/http_status.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/signaling/corp_messaging_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"

namespace remoting {

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
  LOG(INFO) << "SimpleMessage received from " << message.sender_id.username
            << ": " << message.payload;
  last_sender_id_ = message.sender_id;
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

}  // namespace remoting
