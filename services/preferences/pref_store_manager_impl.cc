// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/pref_store_manager_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_value_store.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/preferences/persistent_pref_store_impl.h"
#include "services/preferences/pref_store_impl.h"
#include "services/preferences/scoped_pref_connection_builder.h"
#include "services/preferences/shared_pref_registry.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace prefs {

class PrefStoreManagerImpl::ConnectorConnection
    : public mojom::PrefStoreConnector {
 public:
  ConnectorConnection(PrefStoreManagerImpl* owner,
                      const service_manager::BindSourceInfo& source_info)
      : owner_(owner), source_info_(source_info) {}

  void Connect(mojom::PrefRegistryPtr pref_registry,
               const base::Optional<base::Token>& client_token,
               ConnectCallback callback) override {
    service_manager::Identity modified_identity = source_info_.identity;
    if (client_token) {
      modified_identity = service_manager::Identity(
          modified_identity.name(), modified_identity.instance_group(),
          modified_identity.instance_id(), *client_token);
    }
    auto connection = owner_->shared_pref_registry_->CreateConnectionBuilder(
        std::move(pref_registry), modified_identity, std::move(callback));
    if (owner_->persistent_pref_store_ &&
        owner_->persistent_pref_store_->initialized()) {
      connection->ProvidePersistentPrefStore(
          owner_->persistent_pref_store_.get());
    } else {
      owner_->pending_persistent_connections_.push_back(connection);
    }
    if (owner_->incognito_persistent_pref_store_underlay_) {
      if (owner_->incognito_persistent_pref_store_underlay_->initialized()) {
        connection->ProvideIncognitoPersistentPrefStoreUnderlay(
            owner_->incognito_persistent_pref_store_underlay_.get(),
            owner_->persistent_perf_names_);
      } else {
        owner_->pending_persistent_incognito_connections_.push_back(connection);
      }
    }
    connection->ProvidePrefStoreConnections(owner_->read_only_pref_stores_);
  }

 private:
  PrefStoreManagerImpl* const owner_;
  const service_manager::BindSourceInfo source_info_;
};

PrefStoreManagerImpl::PrefStoreManagerImpl(
    service_manager::mojom::ServiceRequest request,
    PrefStore* managed_prefs,
    PrefStore* supervised_user_prefs,
    PrefStore* extension_prefs,
    PrefStore* command_line_prefs,
    PersistentPrefStore* user_prefs,
    PersistentPrefStore* incognito_user_prefs_underlay,
    PrefStore* recommended_prefs,
    PrefRegistry* pref_registry,
    std::vector<const char*> persistent_perf_names)
    : service_binding_(this, std::move(request)),
      shared_pref_registry_(std::make_unique<SharedPrefRegistry>(
          base::WrapRefCounted(pref_registry))) {
  // This store is done in-process so it's already "registered":
  registry_.AddInterface<prefs::mojom::PrefStoreConnector>(
      base::Bind(&PrefStoreManagerImpl::BindPrefStoreConnectorReceiver,
                 base::Unretained(this)));
  persistent_pref_store_ = std::make_unique<PersistentPrefStoreImpl>(
      base::WrapRefCounted(user_prefs),
      base::BindOnce(&PrefStoreManagerImpl::OnPersistentPrefStoreReady,
                     base::Unretained(this)));
  if (incognito_user_prefs_underlay) {
    incognito_persistent_pref_store_underlay_ =
        std::make_unique<PersistentPrefStoreImpl>(
            base::WrapRefCounted(incognito_user_prefs_underlay),
            base::BindOnce(
                &PrefStoreManagerImpl::OnIncognitoPersistentPrefStoreReady,
                base::Unretained(this)));
    persistent_perf_names_ = std::move(persistent_perf_names);
  } else {
    DCHECK(persistent_perf_names.empty());
  }
  RegisterPrefStore(PrefValueStore::MANAGED_STORE, managed_prefs);
  RegisterPrefStore(PrefValueStore::SUPERVISED_USER_STORE,
                    supervised_user_prefs);
  RegisterPrefStore(PrefValueStore::EXTENSION_STORE, extension_prefs);
  RegisterPrefStore(PrefValueStore::COMMAND_LINE_STORE, command_line_prefs);
  RegisterPrefStore(PrefValueStore::RECOMMENDED_STORE, recommended_prefs);
}

PrefStoreManagerImpl::~PrefStoreManagerImpl() = default;

base::OnceClosure PrefStoreManagerImpl::ShutDownClosure() {
  return base::BindOnce(&PrefStoreManagerImpl::ShutDown,
                        weak_factory_.GetWeakPtr());
}

void PrefStoreManagerImpl::BindPrefStoreConnectorReceiver(
    mojo::PendingReceiver<prefs::mojom::PrefStoreConnector> receiver,
    const service_manager::BindSourceInfo& source_info) {
  connector_receivers_.Add(
      std::make_unique<ConnectorConnection>(this, source_info),
      std::move(receiver));
}

void PrefStoreManagerImpl::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe),
                          source_info);
}

void PrefStoreManagerImpl::OnPersistentPrefStoreReady() {
  DVLOG(1) << "PersistentPrefStore ready";
  for (const auto& connection : pending_persistent_connections_)
    connection->ProvidePersistentPrefStore(persistent_pref_store_.get());
  pending_persistent_connections_.clear();
}

void PrefStoreManagerImpl::OnIncognitoPersistentPrefStoreReady() {
  DVLOG(1) << "Incognito PersistentPrefStore ready";
  for (const auto& connection : pending_persistent_connections_)
    connection->ProvideIncognitoPersistentPrefStoreUnderlay(
        incognito_persistent_pref_store_underlay_.get(),
        persistent_perf_names_);
  pending_persistent_incognito_connections_.clear();
}

void PrefStoreManagerImpl::RegisterPrefStore(PrefValueStore::PrefStoreType type,
                                             PrefStore* pref_store) {
  if (!pref_store)
    return;

  read_only_pref_stores_.emplace(
      type, std::make_unique<PrefStoreImpl>(base::WrapRefCounted(pref_store)));
}

void PrefStoreManagerImpl::ShutDown() {
  read_only_pref_stores_.clear();
  persistent_pref_store_.reset();
  Terminate();
}

}  // namespace prefs
