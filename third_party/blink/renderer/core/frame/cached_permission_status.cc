// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/cached_permission_status.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

using mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;
using mojom::blink::PermissionObserver;
using mojom::blink::PermissionService;
using mojom::blink::PermissionStatus;

// static
const char CachedPermissionStatus::kSupplementName[] = "CachedPermissionStatus";

// static
CachedPermissionStatus* CachedPermissionStatus::From(LocalDOMWindow* window) {
  CachedPermissionStatus* cache =
      Supplement<LocalDOMWindow>::From<CachedPermissionStatus>(window);
  if (!cache) {
    cache = MakeGarbageCollected<CachedPermissionStatus>(window);
    ProvideTo(*window, cache);
  }
  return cache;
}

CachedPermissionStatus::CachedPermissionStatus(LocalDOMWindow* local_dom_window)
    : Supplement<LocalDOMWindow>(*local_dom_window),
      permission_service_(local_dom_window),
      permission_observer_receivers_(this, local_dom_window) {
  CHECK(local_dom_window);
  CHECK(RuntimeEnabledFeatures::PermissionElementEnabled(local_dom_window) ||
        RuntimeEnabledFeatures::GeolocationElementEnabled(local_dom_window) ||
        RuntimeEnabledFeatures::UserMediaElementEnabled(local_dom_window));
}

void CachedPermissionStatus::Trace(Visitor* visitor) const {
  visitor->Trace(permission_service_);
  visitor->Trace(permission_observer_receivers_);
  visitor->Trace(clients_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

void CachedPermissionStatus::RegisterClient(
    Client* client,
    const Vector<PermissionDescriptorPtr>& permissions) {
  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus>
      initialized_map;
  for (const PermissionDescriptorPtr& descriptor : permissions) {
    auto status_it = permission_status_map_.find(descriptor->name);
    PermissionStatus status = status_it != permission_status_map_.end()
                                  ? status_it->value
                                  : PermissionStatus::ASK;
    initialized_map.insert(descriptor->name, status);
    auto client_it = clients_.find(descriptor->name);
    if (client_it != clients_.end()) {
      auto inserted = client_it->value.insert(client);
      CHECK(inserted.is_new_entry);
      continue;
    }

    HeapHashSet<WeakMember<Client>> client_set;
    client_set.insert(client);
    clients_.insert(descriptor->name, std::move(client_set));
    RegisterPermissionObserver(descriptor, status);
  }

  client->OnPermissionStatusInitialized(std::move(initialized_map));
}

void CachedPermissionStatus::UnregisterClient(
    Client* client,
    const Vector<PermissionDescriptorPtr>& permissions) {
  for (const PermissionDescriptorPtr& descriptor : permissions) {
    auto it = clients_.find(descriptor->name);
    if (it == clients_.end()) {
      continue;
    }
    HeapHashSet<WeakMember<Client>>& client_set = it->value;
    auto client_set_it = client_set.find(client);
    if (client_set_it == client_set.end()) {
      continue;
    }
    client_set.erase(client_set_it);
    if (!client_set.empty()) {
      continue;
    }

    clients_.erase(it);

    // Stop listening changes in permissions for a permission name, if there's
    // no client that matches that name.
    auto receiver_it = permission_to_receivers_map_.find(descriptor->name);
    CHECK(receiver_it != permission_to_receivers_map_.end());
    permission_observer_receivers_.Remove(receiver_it->value);
    permission_to_receivers_map_.erase(receiver_it);
  }
}

void CachedPermissionStatus::RegisterPermissionObserver(
    const PermissionDescriptorPtr& descriptor,
    PermissionStatus current_status) {
  mojo::PendingRemote<PermissionObserver> observer;
  mojo::ReceiverId id = permission_observer_receivers_.Add(
      observer.InitWithNewPipeAndPassReceiver(), descriptor->name,
      GetTaskRunner());
  GetPermissionService()->AddPageEmbeddedPermissionObserver(
      descriptor.Clone(), current_status, std::move(observer));
  auto inserted = permission_to_receivers_map_.insert(descriptor->name, id);
  CHECK(inserted.is_new_entry);
}

void CachedPermissionStatus::OnPermissionStatusChange(PermissionStatus status) {
  auto permission_name = permission_observer_receivers_.current_context();
  permission_status_map_.Set(permission_name, status);
  auto it = clients_.find(permission_name);
  if (it == clients_.end()) {
    return;
  }
  const auto client_set = it->value;
  for (auto const& client : client_set) {
    client->OnPermissionStatusChange(permission_name, status);
  }
}

PermissionService* CachedPermissionStatus::GetPermissionService() {
  if (!permission_service_.is_bound()) {
    GetSupplementable()->GetBrowserInterfaceBroker().GetInterface(
        permission_service_.BindNewPipeAndPassReceiver(GetTaskRunner()));
  }

  return permission_service_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
CachedPermissionStatus::GetTaskRunner() {
  return GetSupplementable()->GetTaskRunner(TaskType::kInternalDefault);
}

}  // namespace blink
