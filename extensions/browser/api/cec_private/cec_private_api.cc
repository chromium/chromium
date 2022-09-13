// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cec_private/cec_private_api.h"

#include <vector>

#include "base/bind.h"
#include "base/notreached.h"
#include "chromeos/ash/components/dbus/cec_service/cec_service_client.h"
#include "extensions/common/api/cec_private.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"

namespace {

const char kKioskOnlyError[] =
    "Only kiosk enabled extensions are allowed to use this function.";

extensions::api::cec_private::DisplayCecPowerState
ConvertCecServiceClientPowerState(
    ash::CecServiceClient::PowerState power_state) {
  switch (power_state) {
    case ash::CecServiceClient::PowerState::kError:
      return extensions::api::cec_private::DISPLAY_CEC_POWER_STATE_ERROR;
    case ash::CecServiceClient::PowerState::kAdapterNotConfigured:
      return extensions::api::cec_private::
          DISPLAY_CEC_POWER_STATE_ADAPTERNOTCONFIGURED;
    case ash::CecServiceClient::PowerState::kNoDevice:
      return extensions::api::cec_private::DISPLAY_CEC_POWER_STATE_NODEVICE;
    case ash::CecServiceClient::PowerState::kOn:
      return extensions::api::cec_private::DISPLAY_CEC_POWER_STATE_ON;
    case ash::CecServiceClient::PowerState::kStandBy:
      return extensions::api::cec_private::DISPLAY_CEC_POWER_STATE_STANDBY;
    case ash::CecServiceClient::PowerState::kTransitioningToOn:
      return extensions::api::cec_private::
          DISPLAY_CEC_POWER_STATE_TRANSITIONINGTOON;
    case ash::CecServiceClient::PowerState::kTransitioningToStandBy:
      return extensions::api::cec_private::
          DISPLAY_CEC_POWER_STATE_TRANSITIONINGTOSTANDBY;
    case ash::CecServiceClient::PowerState::kUnknown:
      return extensions::api::cec_private::DISPLAY_CEC_POWER_STATE_UNKNOWN;
  }

  NOTREACHED();
  return extensions::api::cec_private::DISPLAY_CEC_POWER_STATE_UNKNOWN;
}

}  // namespace

namespace extensions {
namespace api {

CecPrivateFunction::CecPrivateFunction() = default;

CecPrivateFunction::~CecPrivateFunction() = default;

// Only allow calls from kiosk mode extensions.
bool CecPrivateFunction::PreRunValidation(std::string* error) {
  if (!ExtensionFunction::PreRunValidation(error))
    return false;

  if (KioskModeInfo::IsKioskEnabled(extension()))
    return true;

  *error = kKioskOnlyError;
  return false;
}

CecPrivateSendStandByFunction::CecPrivateSendStandByFunction() = default;

CecPrivateSendStandByFunction::~CecPrivateSendStandByFunction() = default;

ExtensionFunction::ResponseAction CecPrivateSendStandByFunction::Run() {
  ash::CecServiceClient::Get()->SendStandBy();
  return RespondNow(NoArguments());
}

CecPrivateSendWakeUpFunction::CecPrivateSendWakeUpFunction() = default;

CecPrivateSendWakeUpFunction::~CecPrivateSendWakeUpFunction() = default;

ExtensionFunction::ResponseAction CecPrivateSendWakeUpFunction::Run() {
  ash::CecServiceClient::Get()->SendWakeUp();
  return RespondNow(NoArguments());
}

CecPrivateQueryDisplayCecPowerStateFunction::
    CecPrivateQueryDisplayCecPowerStateFunction() = default;

CecPrivateQueryDisplayCecPowerStateFunction::
    ~CecPrivateQueryDisplayCecPowerStateFunction() = default;

ExtensionFunction::ResponseAction
CecPrivateQueryDisplayCecPowerStateFunction::Run() {
  ash::CecServiceClient::Get()->QueryDisplayCecPowerState(base::BindOnce(
      &CecPrivateQueryDisplayCecPowerStateFunction::HandlePowerStates, this));
  return RespondLater();
}

void CecPrivateQueryDisplayCecPowerStateFunction::HandlePowerStates(
    const std::vector<ash::CecServiceClient::PowerState>& power_states) {
  std::vector<cec_private::DisplayCecPowerState> result_power_states;

  for (const ash::CecServiceClient::PowerState& state : power_states) {
    result_power_states.push_back(ConvertCecServiceClientPowerState(state));
  }

  Respond(ArgumentList(cec_private::QueryDisplayCecPowerState::Results::Create(
      result_power_states)));
}

}  // namespace api
}  // namespace extensions
