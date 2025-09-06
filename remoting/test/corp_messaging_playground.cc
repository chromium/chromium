// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/corp_messaging_playground.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "net/ssl/client_cert_store.h"
#include "remoting/base/certificate_helpers.h"
#include "remoting/base/http_status.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/signaling/corp_messaging_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"

namespace remoting {

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
}

CorpMessagingPlayground::~CorpMessagingPlayground() = default;

void CorpMessagingPlayground::Start() {
  base::RunLoop run_loop;

  // `callback_subscription` is automatically unregistered after `run_loop`
  // completes and this function goes out of scope.
  auto callback_subscription = client_->RegisterMessageCallback(
      base::BindRepeating(&CorpMessagingPlayground::OnSimpleMessageReceived,
                          base::Unretained(this)));
  client_->StartReceivingMessages(
      base::BindOnce(&CorpMessagingPlayground::OnStreamOpened,
                     base::Unretained(this)),
      base::BindOnce(&CorpMessagingPlayground::OnStreamClosed,
                     base::Unretained(this), run_loop.QuitClosure()));

  run_loop.Run();
}

void CorpMessagingPlayground::OnStreamOpened() {
  LOG(INFO) << "Stream opened...";
}

void CorpMessagingPlayground::OnStreamClosed(base::OnceClosure on_closed,
                                             const HttpStatus& status) {
  LOG(INFO) << "Stream closed: " << status.ok() << ", "
            << static_cast<int>(status.error_code()) << ", "
            << status.error_message();
  std::move(on_closed).Run();
}

void CorpMessagingPlayground::OnSimpleMessageReceived(
    const internal::SimpleMessageStruct& message) {
  LOG(INFO) << "SimpleMessage received: " << message.payload;
}

}  // namespace remoting
