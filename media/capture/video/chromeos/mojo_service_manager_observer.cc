// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/mojo_service_manager_observer.h"

#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "components/device_event_log/device_event_log.h"

using chromeos::mojo_service_manager::mojom::ErrorOrServiceState;
using chromeos::mojo_service_manager::mojom::ServiceState;

namespace media {

std::unique_ptr<MojoServiceManagerObserver> MojoServiceManagerObserver::Create(
    const std::string& service_name,
    base::RepeatingClosure on_register_callback,
    base::RepeatingClosure on_unregister_callback) {
  if (!ash::mojo_service_manager::IsServiceManagerBound()) {
    LOG(ERROR) << "The endpoint for Mojo Service Manager is not bound yet.";
    return nullptr;
  }
  return base::WrapUnique<MojoServiceManagerObserver>(
      new MojoServiceManagerObserver(service_name,
                                     std::move(on_register_callback),
                                     std::move(on_unregister_callback)));
}

MojoServiceManagerObserver::MojoServiceManagerObserver(
    const std::string& service_name,
    base::RepeatingClosure on_register_callback,
    base::RepeatingClosure on_unregister_callback)
    : service_name_(service_name),
      on_register_callback_(std::move(on_register_callback)),
      on_unregister_callback_(std::move(on_unregister_callback)) {
  auto* proxy = ash::mojo_service_manager::GetServiceManagerProxy();
  proxy->AddServiceObserver(observer_receiver_.BindNewPipeAndPassRemote());
  proxy->Query(service_name_,
               base::BindOnce(&MojoServiceManagerObserver::QueryCallback,
                              weak_ptr_factory_.GetWeakPtr()));
}

MojoServiceManagerObserver::~MojoServiceManagerObserver() = default;

void MojoServiceManagerObserver::OnServiceEvent(
    chromeos::mojo_service_manager::mojom::ServiceEventPtr event) {
  if (event->service_name != service_name_) {
    return;
  }

  switch (event->type) {
    case chromeos::mojo_service_manager::mojom::ServiceEvent::Type::kRegistered:
      CAMERA_LOG(EVENT) << service_name_ << " is registered.";
      on_register_callback_.Run();
      return;

    case chromeos::mojo_service_manager::mojom::ServiceEvent::Type::
        kUnRegistered:
      CAMERA_LOG(EVENT) << service_name_ << " is unregistered.";
      on_unregister_callback_.Run();
      return;

    case chromeos::mojo_service_manager::mojom::ServiceEvent::Type::
        kDefaultValue:
      return;
  }
}

void MojoServiceManagerObserver::QueryCallback(
    chromeos::mojo_service_manager::mojom::ErrorOrServiceStatePtr result) {
  switch (result->which()) {
    case ErrorOrServiceState::Tag::kState:
      switch (result->get_state()->which()) {
        case ServiceState::Tag::kRegisteredState:
          CAMERA_LOG(EVENT)
              << service_name_ << " has been registered during query.";
          on_register_callback_.Run();
          break;

        case ServiceState::Tag::kUnregisteredState:
          CAMERA_LOG(EVENT)
              << service_name_ << " has not been registered during query.";
          break;

        case ServiceState::Tag::kDefaultType:
          break;
      }
      break;

    case ErrorOrServiceState::Tag::kError:
      LOG(ERROR) << "Error code: " << result->get_error()->code
                 << ", message: " << result->get_error()->message;
      break;

    case ErrorOrServiceState::Tag::kDefaultType:
      LOG(ERROR) << "Unknown type: " << result->get_default_type();
      break;
  }
}

}  // namespace media
