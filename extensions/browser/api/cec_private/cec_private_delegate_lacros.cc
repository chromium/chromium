// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chromeos/crosapi/mojom/cec_private.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "extensions/browser/api/cec_private/cec_private_delegate.h"
#include "extensions/common/api/cec_private.h"

namespace {

extensions::api::cec_private::DisplayCecPowerState
ConvertCecServiceClientPowerState(crosapi::mojom::PowerState power_state) {
  switch (power_state) {
    case crosapi::mojom::PowerState::kError:
      return extensions::api::cec_private::DisplayCecPowerState::kError;
    case crosapi::mojom::PowerState::kAdapterNotConfigured:
      return extensions::api::cec_private::DisplayCecPowerState::
          kAdapterNotConfigured;
    case crosapi::mojom::PowerState::kNoDevice:
      return extensions::api::cec_private::DisplayCecPowerState::kNoDevice;
    case crosapi::mojom::PowerState::kOn:
      return extensions::api::cec_private::DisplayCecPowerState::kOn;
    case crosapi::mojom::PowerState::kStandBy:
      return extensions::api::cec_private::DisplayCecPowerState::kStandby;
    case crosapi::mojom::PowerState::kTransitioningToOn:
      return extensions::api::cec_private::DisplayCecPowerState::
          kTransitioningToOn;
    case crosapi::mojom::PowerState::kTransitioningToStandBy:
      return extensions::api::cec_private::DisplayCecPowerState::
          kTransitioningToStandby;
    case crosapi::mojom::PowerState::kUnknown:
      return extensions::api::cec_private::DisplayCecPowerState::kUnknown;
  }

  NOTREACHED();
}

mojo::Remote<crosapi::mojom::CecPrivate>* GetCecPrivateRemote() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service &&
      lacros_service->IsAvailable<crosapi::mojom::CecPrivate>()) {
    return &lacros_service->GetRemote<crosapi::mojom::CecPrivate>();
  }
  return nullptr;
}

class CecPrivateDelegateLacros : public extensions::api::CecPrivateDelegate {
 public:
  CecPrivateDelegateLacros() = default;
  ~CecPrivateDelegateLacros() override = default;

  void SendStandBy(base::OnceClosure callback) override;
  void SendWakeUp(base::OnceClosure callback) override;
  void QueryDisplayCecPowerState(
      base::OnceCallback<void(
          const std::vector<extensions::api::cec_private::DisplayCecPowerState>&
              power_states)> callback) override;
};

void CecPrivateDelegateLacros::SendStandBy(base::OnceClosure callback) {
  auto* remote = GetCecPrivateRemote();
  if (remote) {
    (*remote)->SendStandBy(std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void CecPrivateDelegateLacros::SendWakeUp(base::OnceClosure callback) {
  auto* remote = GetCecPrivateRemote();
  if (remote) {
    (*remote)->SendWakeUp(std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void CecPrivateDelegateLacros::QueryDisplayCecPowerState(
    CecPrivateDelegate::QueryDisplayPowerStateCallback callback) {
  auto* remote = GetCecPrivateRemote();
  if (remote) {
    (*remote)->QueryDisplayCecPowerState(base::BindOnce(
        [](CecPrivateDelegate::QueryDisplayPowerStateCallback delegate_callback,
           const std::vector<crosapi::mojom::PowerState>& power_states) {
          std::vector<extensions::api::cec_private::DisplayCecPowerState>
              result_power_states;
          base::ranges::transform(power_states,
                                  std::back_inserter(result_power_states),
                                  ConvertCecServiceClientPowerState);
          std::move(delegate_callback).Run(result_power_states);
        },
        std::move(callback)));
  } else {
    std::move(callback).Run(
        std::vector<extensions::api::cec_private::DisplayCecPowerState>());
  }
}
}  // namespace

namespace extensions::api {

std::unique_ptr<CecPrivateDelegate> CecPrivateDelegate::CreateInstance() {
  return std::make_unique<CecPrivateDelegateLacros>();
}

}  // namespace extensions::api
