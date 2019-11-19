// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/session_storage_namespace_impl_mojo.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "content/browser/child_process_security_policy_impl.h"

namespace content {

namespace {

void SessionStorageResponse(base::OnceClosure callback, bool success) {
  std::move(callback).Run();
}

}  // namespace

SessionStorageNamespaceImplMojo::SessionStorageNamespaceImplMojo(
    std::string namespace_id,
    SessionStorageDataMap::Listener* data_map_listener,
    SessionStorageAreaImpl::RegisterNewAreaMap register_new_map_callback,
    Delegate* delegate)
    : namespace_id_(std::move(namespace_id)),
      data_map_listener_(data_map_listener),
      register_new_map_callback_(std::move(register_new_map_callback)),
      delegate_(delegate) {}

SessionStorageNamespaceImplMojo::~SessionStorageNamespaceImplMojo() {
  DCHECK(child_namespaces_waiting_for_clone_call_.empty());
}

void SessionStorageNamespaceImplMojo::SetPendingPopulationFromParentNamespace(
    const std::string& from_namespace) {
  pending_population_from_parent_namespace_ = from_namespace;
  state_ = State::kNotPopulatedAndPendingClone;
}

void SessionStorageNamespaceImplMojo::AddChildNamespaceWaitingForClone(
    const std::string& namespace_id) {
  child_namespaces_waiting_for_clone_call_.insert(namespace_id);
}
bool SessionStorageNamespaceImplMojo::HasChildNamespacesWaitingForClone()
    const {
  return !child_namespaces_waiting_for_clone_call_.empty();
}
void SessionStorageNamespaceImplMojo::ClearChildNamespacesWaitingForClone() {
  child_namespaces_waiting_for_clone_call_.clear();
}

bool SessionStorageNamespaceImplMojo::HasAreaForOrigin(
    const url::Origin& origin) const {
  return origin_areas_.find(origin) != origin_areas_.end();
}

void SessionStorageNamespaceImplMojo::PopulateFromMetadata(
    storage::AsyncDomStorageDatabase* database,
    SessionStorageMetadata::NamespaceEntry namespace_metadata) {
  DCHECK(!IsPopulated());
  database_ = database;
  state_ = State::kPopulated;
  pending_population_from_parent_namespace_.clear();
  namespace_entry_ = namespace_metadata;
  for (const auto& pair : namespace_entry_->second) {
    scoped_refptr<SessionStorageDataMap> data_map =
        delegate_->MaybeGetExistingDataMapForId(
            pair.second->MapNumberAsBytes());
    if (!data_map) {
      data_map = SessionStorageDataMap::CreateFromDisk(data_map_listener_,
                                                       pair.second, database_);
    }
    origin_areas_[pair.first] = std::make_unique<SessionStorageAreaImpl>(
        namespace_entry_, pair.first, std::move(data_map),
        register_new_map_callback_);
  }
  if (!run_after_population_.empty()) {
    for (base::OnceClosure& callback : run_after_population_)
      std::move(callback).Run();
    run_after_population_.clear();
  }
}

void SessionStorageNamespaceImplMojo::PopulateAsClone(
    storage::AsyncDomStorageDatabase* database,
    SessionStorageMetadata::NamespaceEntry namespace_metadata,
    const OriginAreas& areas_to_clone) {
  DCHECK(!IsPopulated());
  database_ = database;
  state_ = State::kPopulated;
  pending_population_from_parent_namespace_.clear();
  namespace_entry_ = namespace_metadata;
  std::transform(areas_to_clone.begin(), areas_to_clone.end(),
                 std::inserter(origin_areas_, origin_areas_.begin()),
                 [namespace_metadata](const auto& source) {
                   return std::make_pair(
                       source.first, source.second->Clone(namespace_metadata));
                 });
  if (!run_after_population_.empty()) {
    for (base::OnceClosure& callback : run_after_population_)
      std::move(callback).Run();
    run_after_population_.clear();
  }
}

void SessionStorageNamespaceImplMojo::Reset() {
  namespace_entry_ = SessionStorageMetadata::NamespaceEntry();
  database_ = nullptr;
  pending_population_from_parent_namespace_.clear();
  bind_waiting_on_population_ = false;
  run_after_population_.clear();
  state_ = State::kNotPopulated;
  child_namespaces_waiting_for_clone_call_.clear();
  origin_areas_.clear();
  receivers_.Clear();
}

void SessionStorageNamespaceImplMojo::Bind(
    mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver,
    int process_id) {
  if (!IsPopulated()) {
    bind_waiting_on_population_ = true;
    run_after_population_.push_back(base::BindOnce(
        &SessionStorageNamespaceImplMojo::Bind, base::Unretained(this),
        std::move(receiver), process_id));
    return;
  }
  DCHECK(IsPopulated());
  receivers_.Add(this, std::move(receiver), process_id);
  bind_waiting_on_population_ = false;
}

void SessionStorageNamespaceImplMojo::PurgeUnboundAreas() {
  auto it = origin_areas_.begin();
  while (it != origin_areas_.end()) {
    if (!it->second->IsBound())
      it = origin_areas_.erase(it);
    else
      ++it;
  }
}

void SessionStorageNamespaceImplMojo::RemoveOriginData(
    const url::Origin& origin,
    base::OnceClosure callback) {
  DCHECK_NE(state_, State::kNotPopulated);
  if (!IsPopulated()) {
    run_after_population_.push_back(
        base::BindOnce(&SessionStorageNamespaceImplMojo::RemoveOriginData,
                       base::Unretained(this), origin, std::move(callback)));
    return;
  }
  DCHECK(IsPopulated());
  auto it = origin_areas_.find(origin);
  if (it == origin_areas_.end()) {
    std::move(callback).Run();
    return;
  }
  // Renderer process expects |source| to always be two newline separated
  // strings.
  it->second->DeleteAll(
      "\n", base::BindOnce(&SessionStorageResponse, std::move(callback)));
  it->second->NotifyObserversAllDeleted();
  it->second->data_map()->storage_area()->ScheduleImmediateCommit();
}

void SessionStorageNamespaceImplMojo::OpenArea(
    const url::Origin& origin,
    mojo::PendingAssociatedReceiver<blink::mojom::StorageArea> receiver) {
  DCHECK(IsPopulated());
  DCHECK(!receivers_.empty());
  int process_id = receivers_.current_context();
  // TODO(943887): Replace HasSecurityState() call with something that can
  // preserve security state after process shutdown. The security state check
  // is a temporary solution to avoid crashes when this method is run after the
  // process associated with |process_id| has been destroyed.
  // It temporarily restores the old behavior of always allowing access if the
  // process is gone.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessDataForOrigin(process_id, origin) &&
      policy->HasSecurityState(process_id)) {
    receivers_.ReportBadMessage("Access denied for sessionStorage request");
    return;
  }
  auto it = origin_areas_.find(origin);
  if (it == origin_areas_.end()) {
    // The area may have been purged due to lack of bindings, so check the
    // metadata for the map.
    scoped_refptr<SessionStorageDataMap> data_map;
    auto map_data_it = namespace_entry_->second.find(origin);
    if (map_data_it != namespace_entry_->second.end()) {
      // The map exists already, either on disk or being used by another
      // namespace.
      scoped_refptr<SessionStorageMetadata::MapData> map_data =
          map_data_it->second;
      data_map =
          delegate_->MaybeGetExistingDataMapForId(map_data->MapNumberAsBytes());
      if (!data_map) {
        data_map = SessionStorageDataMap::CreateFromDisk(data_map_listener_,
                                                         map_data, database_);
      }
    } else {
      // The map doesn't exist yet.
      data_map = SessionStorageDataMap::CreateEmpty(
          data_map_listener_,
          register_new_map_callback_.Run(namespace_entry_, origin), database_);
    }
    it = origin_areas_
             .emplace(std::make_pair(
                 origin, std::make_unique<SessionStorageAreaImpl>(
                             namespace_entry_, origin, std::move(data_map),
                             register_new_map_callback_)))
             .first;
  }
  it->second->Bind(std::move(receiver));
}

void SessionStorageNamespaceImplMojo::Clone(
    const std::string& clone_to_namespace) {
  DCHECK(IsPopulated());
  child_namespaces_waiting_for_clone_call_.erase(clone_to_namespace);
  delegate_->RegisterShallowClonedNamespace(namespace_entry_,
                                            clone_to_namespace, origin_areas_);
}

void SessionStorageNamespaceImplMojo::CloneAllNamespacesWaitingForClone(
    storage::AsyncDomStorageDatabase* database,
    SessionStorageMetadata* metadata,
    const std::map<std::string,
                   std::unique_ptr<SessionStorageNamespaceImplMojo>>&
        namespaces_map) {
  SessionStorageNamespaceImplMojo* parent = this;
  // If the current state is kNotPopulatedAndPendingClone, then the children can
  // all be cloned from our parent instead of us.
  if (state() ==
      SessionStorageNamespaceImplMojo::State::kNotPopulatedAndPendingClone) {
    auto parent_it =
        namespaces_map.find(pending_population_from_parent_namespace_);
    // The parent must be in the map, because the only way to remove something
    // from the map is to call DeleteSessionNamespace, which would have called
    // this method on the parent if there were children, and resolved our clone
    // dependency.
    DCHECK(parent_it != namespaces_map.end());
    parent = parent_it->second.get();
  }

  if (parent->state() ==
      SessionStorageNamespaceImplMojo::State::kNotPopulated) {
    // Populate the namespace to prepare for copy.
    parent->PopulateFromMetadata(
        database, metadata->GetOrCreateNamespaceEntry(parent->namespace_id_));
  }

  auto* delegate = parent->delegate_;
  for (const std::string& destination_namespace :
       child_namespaces_waiting_for_clone_call_) {
    if (parent->IsPopulated()) {
      delegate->RegisterShallowClonedNamespace(parent->namespace_entry(),
                                               destination_namespace,
                                               parent->origin_areas_);
    } else {
      parent->AddChildNamespaceWaitingForClone(destination_namespace);
      parent->run_after_population_.push_back(
          base::BindOnce(&SessionStorageNamespaceImplMojo::Clone,
                         base::Unretained(parent), destination_namespace));
      auto child_it = namespaces_map.find(destination_namespace);
      // The child must be in the map, as the only way to add it to
      // |child_namespaces_waiting_for_clone_call_| is to call
      // CloneSessionNamespace, which always adds it to the map.
      DCHECK(child_it != namespaces_map.end());
      child_it->second->SetPendingPopulationFromParentNamespace(
          parent->namespace_id_);
    }
  }
  child_namespaces_waiting_for_clone_call_.clear();
}

void SessionStorageNamespaceImplMojo::FlushOriginForTesting(
    const url::Origin& origin) {
  if (!IsPopulated())
    return;
  auto it = origin_areas_.find(origin);
  if (it == origin_areas_.end())
    return;
  it->second->data_map()->storage_area()->ScheduleImmediateCommit();
}

}  // namespace content
