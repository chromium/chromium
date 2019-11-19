// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cec_private/cec_private_api.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "chromeos/dbus/cec_service_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "extensions/common/api/cec_private.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"

namespace {

const char kKioskOnlyError[] =
    "Only kiosk enabled extensions are allowed to use this function.";

extensions::api::cec_private::DisplayCecPowerState
ConvertCecServiceClientPowerState(
    chromeos::CecServiceClient::PowerState power_state) {
  switch (power_state) {
    case chromeos::CecServiceClient::PowerState::kError:
      return extensions::api::cec_private::DISPLAY_CEC_POWER_STATE_ERROR;
    case chromeos::CecServiceClient::PowerState::kAdapterNotConfigured:
      return extensions::api::cec_private::
          DISPLAY_CEC_POWER_STATE_ADAPTERNOTCONFIGURED;
    case chromeos::CecServiceClient::PowerState::kNoDevice:
      return extensions::api::cec_private::DISPLAY_CEC_POWER_STATE_NODEVICE;
    case chromeos::CecServiceClient::PowerState::kOn:
      return extensions::api::cec_private::DISPLAY_CEC_POWER_STATE_ON;
    case chromeos::CecServiceClient::PowerState::kStandBy:
      return extensions::api::cec_private::DISPLAY_CEC_POWER_STATE_STANDBY;
    case chromeos::CecServiceClient::PowerState::kTransitioningToOn:
      return extensions::api::cec_private::
          DISPLAY_CEC_POWER_STATE_TRANSITIONINGTOON;
    case chromeos::CecServiceClient::PowerState::kTransitioningToStandBy:
      return extensions::api::cec_private::
          DISPLAY_CEC_POWER_STATE_TRANSITIONINGTOSTANDBY;
    case chromeos::CecServiceClient::PowerState::kUnknown:
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
  chromeos::DBusThreadManager::Get()->GetCecServiceClient()->SendStandBy();
  return RespondNow(NoArguments());
}

CecPrivateSendWakeUpFunction::CecPrivateSendWakeUpFunction() = default;

CecPrivateSendWakeUpFunction::~CecPrivateSendWakeUpFunction() = default;

ExtensionFunction::ResponseAction CecPrivateSendWakeUpFunction::Run() {
  chromeos::DBusThreadManager::Get()->GetCecServiceClient()->SendWakeUp();
  return RespondNow(NoArguments());
}

CecPrivateQueryDisplayCecPowerStateFunction::
    CecPrivateQueryDisplayCecPowerStateFunction() = default;

CecPrivateQueryDisplayCecPowerStateFunction::
    ~CecPrivateQueryDisplayCecPowerStateFunction() = default;

ExtensionFunction::ResponseAction
CecPrivateQueryDisplayCecPowerStateFunction::Run() {
  chromeos::DBusThreadManager::Get()
      ->GetCecServiceClient()
      ->QueryDisplayCecPowerState(base::BindOnce(
          &CecPrivateQueryDisplayCecPowerStateFunction::HandlePowerStates,
          this));
  return RespondLater();
}

void CecPrivateQueryDisplayCecPowerStateFunction::HandlePowerStates(
    const std::vector<chromeos::CecServiceClient::PowerState>& power_states) {
  std::vector<cec_private::DisplayCecPowerState> result_power_states;

  for (const chromeos::CecServiceClient::PowerState& state : power_states) {
    result_power_states.push_back(ConvertCecServiceClientPowerState(state));
  }

  Respond(ArgumentList(cec_private::QueryDisplayCecPowerState::Results::Create(
      result_power_states)));
}

}  // namespace api
}  // namespace extensions
