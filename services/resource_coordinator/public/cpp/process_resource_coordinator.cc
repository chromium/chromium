// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/process_resource_coordinator.h"

namespace resource_coordinator {

ProcessResourceCoordinator::ProcessResourceCoordinator(
    service_manager::Connector* connector)
    : ResourceCoordinatorInterface(), weak_ptr_factory_(this) {
  CoordinationUnitID new_cu_id(CoordinationUnitType::kProcess,
                               CoordinationUnitID::RANDOM_ID);
  ResourceCoordinatorInterface::ConnectToService(connector, new_cu_id);
}

ProcessResourceCoordinator::~ProcessResourceCoordinator() = default;

void ProcessResourceCoordinator::SetCPUUsage(double cpu_usage) {
  if (!service_)
    return;
  service_->SetCPUUsage(cpu_usage);
}

void ProcessResourceCoordinator::SetLaunchTime(base::Time launch_time) {
  if (!service_)
    return;
  service_->SetLaunchTime(launch_time);
}

void ProcessResourceCoordinator::SetPID(base::ProcessId pid) {
  if (!service_)
    return;
  service_->SetPID(pid);
}

void ProcessResourceCoordinator::AddFrame(
    const FrameResourceCoordinator& frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!service_)
    return;
  // We could keep the ID around ourselves, but this hop ensures that the child
  // has been created on the service-side.
  frame.service()->GetID(
      base::BindOnce(&ProcessResourceCoordinator::AddFrameByID,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProcessResourceCoordinator::ConnectToService(
    mojom::CoordinationUnitProviderPtr& provider,
    const CoordinationUnitID& cu_id) {
  provider->CreateProcessCoordinationUnit(mojo::MakeRequest(&service_), cu_id);
}

void ProcessResourceCoordinator::AddFrameByID(const CoordinationUnitID& cu_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  service_->AddFrame(cu_id);
}

}  // namespace resource_coordinator
