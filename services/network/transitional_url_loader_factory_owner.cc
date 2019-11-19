// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/transitional_url_loader_factory_owner.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/no_destructor.h"
#include "base/synchronization/atomic_flag.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

namespace network {

// Portion of TransitionalURLLoaderFactoryOwner that lives on the network
// task runner associated with the URLRequestContextGetter.
class TransitionalURLLoaderFactoryOwner::Core {
 public:
  Core(scoped_refptr<net::URLRequestContextGetter> url_request_context_getter)
      : url_request_context_getter_(std::move(url_request_context_getter)),
        task_runner_(url_request_context_getter_->GetNetworkTaskRunner()) {}

  void CreateNetworkContext(
      mojo::PendingReceiver<mojom::NetworkContext> receiver) {
    if (task_runner_->RunsTasksInCurrentSequence()) {
      // This must be synchronous since in same-runner case deletes are
      // synchronous as well.
      CreateNetworkContextOnNetworkThread(std::move(receiver));
    } else {
      // Unretained is safe since cross-thread deletes will also be posted.
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&Core::CreateNetworkContextOnNetworkThread,
                         base::Unretained(this), std::move(receiver)));
    }
  }

  static void DeleteOnRightThread(std::unique_ptr<Core> instance) {
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        instance->task_runner_;
    if (!task_runner->RunsTasksInCurrentSequence())
      task_runner->DeleteSoon(FROM_HERE, std::move(instance));
    // otherwise |instance| going out of scope will do the right thing.
  }

 private:
  friend class base::DeleteHelper<Core>;
  friend struct std::default_delete<Core>;

  ~Core() {
    DCHECK(url_request_context_getter_->GetURLRequestContext());
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
  }

  void CreateNetworkContextOnNetworkThread(
      mojo::PendingReceiver<mojom::NetworkContext> receiver) {
    network_context_ = std::make_unique<network::NetworkContext>(
        nullptr /* network_service */, std::move(receiver),
        url_request_context_getter_->GetURLRequestContext(),
        /*cors_exempt_header_list=*/std::vector<std::string>());
  }

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<network::NetworkContext> network_context_;
};

TransitionalURLLoaderFactoryOwner::TransitionalURLLoaderFactoryOwner(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter)
    : core_(std::make_unique<Core>(std::move(url_request_context_getter))) {
  DCHECK(!disallowed_in_process().IsSet());
}

TransitionalURLLoaderFactoryOwner::~TransitionalURLLoaderFactoryOwner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!disallowed_in_process().IsSet());

  if (shared_url_loader_factory_)
    shared_url_loader_factory_->Detach();

  Core::DeleteOnRightThread(std::move(core_));
}

scoped_refptr<network::SharedURLLoaderFactory>
TransitionalURLLoaderFactoryOwner::GetURLLoaderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!disallowed_in_process().IsSet());

  if (!shared_url_loader_factory_) {
    core_->CreateNetworkContext(
        network_context_remote_.BindNewPipeAndPassReceiver());
    auto url_loader_factory_params =
        network::mojom::URLLoaderFactoryParams::New();
    url_loader_factory_params->process_id = mojom::kBrowserProcessId;
    url_loader_factory_params->is_corb_enabled = false;
    network_context_remote_->CreateURLLoaderFactory(
        url_loader_factory_.BindNewPipeAndPassReceiver(),
        std::move(url_loader_factory_params));
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory_.get());
  }

  return shared_url_loader_factory_;
}

network::mojom::NetworkContext*
TransitionalURLLoaderFactoryOwner::GetNetworkContext() {
  GetURLLoaderFactory();
  return network_context_remote_.get();
}

void TransitionalURLLoaderFactoryOwner::DisallowUsageInProcess() {
  disallowed_in_process().Set();
}

base::AtomicFlag& TransitionalURLLoaderFactoryOwner::disallowed_in_process() {
  static base::NoDestructor<base::AtomicFlag> instance;
  return *instance;
}

}  // namespace network
