// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
#include "extensions/browser/api/cec_private/cec_private_delegate.h"
// clang-format on

#include <utility>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/components/dbus/cec_service/cec_service_client.h"
#include "extensions/common/api/cec_private.h"

namespace {

extensions::api::cec_private::DisplayCecPowerState
ConvertCecServiceClientPowerState(
    ash::CecServiceClient::PowerState power_state) {
  switch (power_state) {
    case ash::CecServiceClient::PowerState::kError:
      return extensions::api::cec_private::DisplayCecPowerState::kError;
    case ash::CecServiceClient::PowerState::kAdapterNotConfigured:
      return extensions::api::cec_private::DisplayCecPowerState::
          kAdapterNotConfigured;
    case ash::CecServiceClient::PowerState::kNoDevice:
      return extensions::api::cec_private::DisplayCecPowerState::kNoDevice;
    case ash::CecServiceClient::PowerState::kOn:
      return extensions::api::cec_private::DisplayCecPowerState::kOn;
    case ash::CecServiceClient::PowerState::kStandBy:
      return extensions::api::cec_private::DisplayCecPowerState::kStandby;
    case ash::CecServiceClient::PowerState::kTransitioningToOn:
      return extensions::api::cec_private::DisplayCecPowerState::
          kTransitioningToOn;
    case ash::CecServiceClient::PowerState::kTransitioningToStandBy:
      return extensions::api::cec_private::DisplayCecPowerState::
          kTransitioningToStandby;
    case ash::CecServiceClient::PowerState::kUnknown:
      return extensions::api::cec_private::DisplayCecPowerState::kUnknown;
  }

  NOTREACHED();
}

class CecPrivateDelegateAsh : public extensions::api::CecPrivateDelegate {
 public:
  CecPrivateDelegateAsh() = default;
  ~CecPrivateDelegateAsh() override = default;

  void SendStandBy(base::OnceClosure callback) override;
  void SendWakeUp(base::OnceClosure callback) override;
  void QueryDisplayCecPowerState(
      base::OnceCallback<void(
          const std::vector<extensions::api::cec_private::DisplayCecPowerState>&
              power_states)> callback) override;
};

void CecPrivateDelegateAsh::SendStandBy(base::OnceClosure callback) {
  ash::CecServiceClient* dbus_client = ash::CecServiceClient::Get();
  if (dbus_client) {
    dbus_client->SendStandBy();
  }
  std::move(callback).Run();
}

void CecPrivateDelegateAsh::SendWakeUp(base::OnceClosure callback) {
  ash::CecServiceClient* dbus_client = ash::CecServiceClient::Get();
  if (dbus_client) {
    dbus_client->SendWakeUp();
  }
  std::move(callback).Run();
}

void CecPrivateDelegateAsh::QueryDisplayCecPowerState(
    CecPrivateDelegate::QueryDisplayPowerStateCallback callback) {
  ash::CecServiceClient* dbus_client = ash::CecServiceClient::Get();
  if (!dbus_client) {
    std::move(callback).Run({});
    return;
  }
  dbus_client->QueryDisplayCecPowerState(base::BindOnce(
      [](CecPrivateDelegate::QueryDisplayPowerStateCallback service_callback,
         const std::vector<ash::CecServiceClient::PowerState>& power_states) {
        std::vector<extensions::api::cec_private::DisplayCecPowerState>
            result_power_states;
        base::ranges::transform(power_states,
                                std::back_inserter(result_power_states),
                                ConvertCecServiceClientPowerState);
        std::move(service_callback).Run(result_power_states);
      },
      std::move(callback)));
}
}  // namespace

namespace extensions::api {

std::unique_ptr<CecPrivateDelegate> CecPrivateDelegate::CreateInstance() {
  return std::make_unique<CecPrivateDelegateAsh>();
}

}  // namespace extensions::api
