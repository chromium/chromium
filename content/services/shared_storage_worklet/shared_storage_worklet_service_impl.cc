// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/shared_storage_worklet/shared_storage_worklet_service_impl.h"

namespace shared_storage_worklet {

SharedStorageWorkletServiceImpl::SharedStorageWorkletServiceImpl(
    mojo::PendingReceiver<mojom::SharedStorageWorkletService> receiver,
    base::OnceClosure disconnect_handler)
    : receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(std::move(disconnect_handler));
}

SharedStorageWorkletServiceImpl::~SharedStorageWorkletServiceImpl() = default;

void SharedStorageWorkletServiceImpl::BindSharedStorageWorkletServiceClient(
    mojo::PendingAssociatedRemote<mojom::SharedStorageWorkletServiceClient>
        client) {
  DCHECK(!global_scope_);
  client_.Bind(std::move(client));
}

void SharedStorageWorkletServiceImpl::AddModule(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    const GURL& script_source_url,
    AddModuleCallback callback) {
  DCHECK(!global_scope_);
  GetGlobalScope()->AddModule(std::move(pending_url_loader_factory),
                              client_.get(), script_source_url,
                              std::move(callback));
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
  if (!global_scope_)
    global_scope_ = std::make_unique<SharedStorageWorkletGlobalScope>();

  return global_scope_.get();
}

}  // namespace shared_storage_worklet
