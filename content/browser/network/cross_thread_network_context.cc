// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/cross_thread_network_context.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/reconnect_notifier.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/connection_change_observer_client.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace content {

// static
scoped_refptr<CrossThreadNetworkContext> CrossThreadNetworkContext::Create(
    base::WeakPtr<StoragePartitionImpl> storage_partition) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return CreateForTesting(
      base::BindRepeating(&StoragePartitionImpl::CloneNetworkContext,
                          std::move(storage_partition)),
      GetUIThreadTaskRunner({}));
}

// static
scoped_refptr<CrossThreadNetworkContext>
CrossThreadNetworkContext::CreateForTesting(
    NetworkContextBinder network_context_binder,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner) {
  return base::WrapRefCounted(new CrossThreadNetworkContext(
      std::move(network_context_binder), std::move(ui_task_runner)));
}

CrossThreadNetworkContext::CrossThreadNetworkContext(
    NetworkContextBinder network_context_binder,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : network_context_binder_(std::move(network_context_binder)),
      ui_task_runner_(std::move(ui_task_runner)) {
  CHECK(network_context_binder_);
  CHECK(ui_task_runner_);
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  // Create a PendingRemote on the UI thread upfront so the initial call on the
  // background sequence can bind it directly without posting back to the UI
  // thread.
  network_context_binder_.Run(
      pending_network_context_.InitWithNewPipeAndPassReceiver());
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CrossThreadNetworkContext::~CrossThreadNetworkContext() {
  if (network_context_ && bound_task_runner_ &&
      !bound_task_runner_->RunsTasksInCurrentSequence()) {
    bound_task_runner_->DeleteSoon(FROM_HERE, std::move(network_context_));
  }
}

network::mojom::NetworkContext*
CrossThreadNetworkContext::EnsureNetworkContext() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!bound_task_runner_) {
    bound_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  }

  if (!network_context_) {
    network_context_ =
        std::make_unique<mojo::Remote<network::mojom::NetworkContext>>();
  }
  if (network_context_->is_bound()) {
    return network_context_->get();
  }

  // Initial binding: consume the PendingRemote created on the UI thread during
  // construction so that messages can be sent directly to the NetworkService
  // without waiting for a UI thread task hop.
  if (pending_network_context_) {
    network_context_->Bind(std::move(pending_network_context_),
                           bound_task_runner_);
    network_context_->reset_on_disconnect();
    return network_context_->get();
  }

  // Reconnect after a NetworkService crash.
  auto receiver =
      network_context_->BindNewPipeAndPassReceiver(bound_task_runner_);
  network_context_->reset_on_disconnect();

  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(network_context_binder_, std::move(receiver)));

  return network_context_->get();
}

void CrossThreadNetworkContext::PreconnectSockets(
    uint32_t num_streams,
    const GURL& url,
    network::mojom::CredentialsMode credentials_mode,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    const base::UnguessableToken& network_restrictions_id,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    const std::optional<net::ConnectionKeepAliveConfig>& keepalive_config,
    mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>
        connection_change_observer_client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureNetworkContext()->PreconnectSockets(
      num_streams, url, credentials_mode, network_anonymization_key,
      network_restrictions_id, traffic_annotation, keepalive_config,
      std::move(connection_change_observer_client));
}

void CrossThreadNetworkContext::AddQuicHints(
    const std::vector<url::SchemeHostPort>& origins,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureNetworkContext()->AddQuicHints(origins, network_anonymization_key);
}

void CrossThreadNetworkContext::FlushAsyncForTesting(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (network_context_ && network_context_->is_bound()) {
    network_context_->FlushAsyncForTesting(std::move(callback));  // IN-TEST
  } else {
    std::move(callback).Run();
  }
}

}  // namespace content
