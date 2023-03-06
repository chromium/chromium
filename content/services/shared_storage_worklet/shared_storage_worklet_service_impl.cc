// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/shared_storage_worklet/shared_storage_worklet_service_impl.h"

#include <utility>

#include "base/check.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"

namespace shared_storage_worklet {

SharedStorageWorkletServiceImpl::SharedStorageWorkletServiceImpl(
    mojo::PendingReceiver<blink::mojom::SharedStorageWorkletService> receiver,
    base::OnceClosure disconnect_handler)
    : receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(std::move(disconnect_handler));
}

SharedStorageWorkletServiceImpl::~SharedStorageWorkletServiceImpl() = default;

void SharedStorageWorkletServiceImpl::Initialize(
    mojo::PendingAssociatedRemote<
        blink::mojom::SharedStorageWorkletServiceClient> client,
    bool private_aggregation_permissions_policy_allowed,
    mojo::PendingRemote<blink::mojom::PrivateAggregationHost>
        private_aggregation_host,
    const absl::optional<std::u16string>& embedder_context) {
  DCHECK(!global_scope_);
  client_.Bind(std::move(client));
  private_aggregation_permissions_policy_allowed_ =
      private_aggregation_permissions_policy_allowed;
  if (private_aggregation_host)
    private_aggregation_host_.Bind(std::move(private_aggregation_host));
  embedder_context_ = embedder_context;
}

void SharedStorageWorkletServiceImpl::AddModule(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    const GURL& script_source_url,
    AddModuleCallback callback) {
  DCHECK(!global_scope_);
  GetGlobalScope()->AddModule(
      std::move(pending_url_loader_factory), client_.get(),
      private_aggregation_host_ ? private_aggregation_host_.get() : nullptr,
      script_source_url, std::move(callback));
}

void SharedStorageWorkletServiceImpl::RunURLSelectionOperation(
    const std::string& name,
    const std::vector<GURL>& urls,
    const std::vector<uint8_t>& serialized_data,
    RunURLSelectionOperationCallback callback) {
  GetGlobalScope()->RunURLSelectionOperation(name, urls, serialized_data,
                                             std::move(callback));
}

void SharedStorageWorkletServiceImpl::RunOperation(
    const std::string& name,
    const std::vector<uint8_t>& serialized_data,
    RunOperationCallback callback) {
  GetGlobalScope()->RunOperation(name, serialized_data, std::move(callback));
}

SharedStorageWorkletGlobalScope*
SharedStorageWorkletServiceImpl::GetGlobalScope() {
  if (!global_scope_) {
    global_scope_ = std::make_unique<SharedStorageWorkletGlobalScope>(
        private_aggregation_permissions_policy_allowed_, embedder_context_);
  }

  return global_scope_.get();
}

}  // namespace shared_storage_worklet
