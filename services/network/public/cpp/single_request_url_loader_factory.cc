// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/single_request_url_loader_factory.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace network {

class SingleRequestURLLoaderFactory::HandlerState
    : public base::RefCountedThreadSafe<
          SingleRequestURLLoaderFactory::HandlerState> {
 public:
  explicit HandlerState(RequestHandler handler)
      : handler_(std::move(handler)),
        handler_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

  HandlerState(const HandlerState&) = delete;
  HandlerState& operator=(const HandlerState&) = delete;

  void HandleRequest(const ResourceRequest& resource_request,
                     mojo::PendingReceiver<mojom::URLLoader> loader,
                     mojo::PendingRemote<mojom::URLLoaderClient> client) {
    if (!handler_task_runner_->RunsTasksInCurrentSequence()) {
      handler_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&HandlerState::HandleRequest, this, resource_request,
                         std::move(loader), std::move(client)));
      return;
    }

    DCHECK(handler_);
    std::move(handler_).Run(resource_request, std::move(loader),
                            std::move(client));
  }

 private:
  friend class base::RefCountedThreadSafe<HandlerState>;

  ~HandlerState() {
    if (handler_) {
      handler_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&DropHandlerOnHandlerSequence, std::move(handler_)));
    }
  }

  static void DropHandlerOnHandlerSequence(RequestHandler handler) {}

  RequestHandler handler_;
  const scoped_refptr<base::SequencedTaskRunner> handler_task_runner_;
};

class SingleRequestURLLoaderFactory::PendingFactory
    : public PendingSharedURLLoaderFactory {
 public:
  explicit PendingFactory(scoped_refptr<HandlerState> state)
      : state_(std::move(state)) {}

  PendingFactory(const PendingFactory&) = delete;
  PendingFactory& operator=(const PendingFactory&) = delete;

  ~PendingFactory() override = default;

  // PendingSharedURLLoaderFactory:
  scoped_refptr<SharedURLLoaderFactory> CreateFactory() override {
    return new SingleRequestURLLoaderFactory(std::move(state_));
  }

 private:
  scoped_refptr<HandlerState> state_;
};

SingleRequestURLLoaderFactory::SingleRequestURLLoaderFactory(
    RequestHandler handler)
    : state_(base::MakeRefCounted<HandlerState>(std::move(handler))) {}

void SingleRequestURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  state_->HandleRequest(request, std::move(loader), std::move(client));
}

void SingleRequestURLLoaderFactory::Clone(
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) {
  // Pass |this| as the recevier context to make sure this object stays alive
  // while it still has receivers.
  receivers_.Add(this, std::move(receiver), this);
}

std::unique_ptr<PendingSharedURLLoaderFactory>
SingleRequestURLLoaderFactory::Clone() {
  return std::make_unique<PendingFactory>(state_);
}

SingleRequestURLLoaderFactory::SingleRequestURLLoaderFactory(
    scoped_refptr<HandlerState> state)
    : state_(std::move(state)) {}

SingleRequestURLLoaderFactory::~SingleRequestURLLoaderFactory() = default;

}  // namespace network
