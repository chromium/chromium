// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/process_map.h"

#include "base/process/process_handle.h"
#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/mojom/constants.mojom.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"

namespace memory_instrumentation {

ProcessMap::ProcessMap(service_manager::Connector* connector) : binding_(this) {
  if (!connector)
    return;  // Happens in unittests.
  service_manager::mojom::ServiceManagerPtr service_manager;
  connector->BindInterface(service_manager::mojom::kServiceName,
                           &service_manager);
  service_manager::mojom::ServiceManagerListenerPtr listener;
  service_manager::mojom::ServiceManagerListenerRequest request(
      mojo::MakeRequest(&listener));
  service_manager->AddListener(std::move(listener));
  binding_.Bind(std::move(request));
}

ProcessMap::~ProcessMap() {}

void ProcessMap::OnInit(std::vector<RunningServiceInfoPtr> instances) {
  for (const RunningServiceInfoPtr& instance : instances) {
    if (instance->pid == base::kNullProcessId)
      continue;

    const service_manager::Identity& identity = instance->identity;

    // TODO(https://crbug.com/818593): The listener interface is racy, so the
    // map may contain spurious entries. If so, remove the existing entry before
    // adding a new one.
    if (base::ContainsKey(instances_, identity))
      OnServiceStopped(identity);

    auto it_and_inserted = instances_.emplace(identity, instance->pid);
    DCHECK(it_and_inserted.second);
  }
}

void ProcessMap::OnServiceCreated(RunningServiceInfoPtr instance) {
}

void ProcessMap::OnServiceStarted(const service_manager::Identity& identity,
                                  uint32_t pid) {
}

void ProcessMap::OnServiceFailedToStart(const service_manager::Identity&) {}

void ProcessMap::OnServiceStopped(const service_manager::Identity& identity) {
  instances_.erase(identity);
}

void ProcessMap::OnServicePIDReceived(const service_manager::Identity& identity,
                                      uint32_t pid) {
  if (pid == base::kNullProcessId)
    return;
  auto it_and_inserted = instances_.emplace(identity, pid);
  DCHECK(it_and_inserted.second);
}

base::ProcessId ProcessMap::GetProcessId(
    const service_manager::Identity& identity) const {
  auto it = instances_.find(identity);
  return it != instances_.end() ? it->second : base::kNullProcessId;
}

}  // namespace memory_instrumentation
