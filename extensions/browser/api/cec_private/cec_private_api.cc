// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cec_private/cec_private_api.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "extensions/browser/api/cec_private/cec_private_delegate.h"
#include "extensions/common/api/cec_private.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"

namespace {

const char kKioskOnlyError[] =
    "Only kiosk enabled extensions are allowed to use this function.";

}  // namespace

namespace extensions {
namespace api {

CecPrivateFunction::CecPrivateFunction()
    : delegate_(CecPrivateDelegate::CreateInstance()) {}

CecPrivateFunction::~CecPrivateFunction() = default;

// Only allow calls from kiosk mode extensions.
bool CecPrivateFunction::PreRunValidation(std::string* error) {
  if (!ExtensionFunction::PreRunValidation(error)) {
    return false;
  }

  if (KioskModeInfo::IsKioskEnabled(extension())) {
    return true;
  }

  *error = kKioskOnlyError;
  return false;
}

void CecPrivateFunction::RespondWithNoArguments(void) {
  Respond(NoArguments());
}

CecPrivateSendStandByFunction::CecPrivateSendStandByFunction() = default;

CecPrivateSendStandByFunction::~CecPrivateSendStandByFunction() = default;

ExtensionFunction::ResponseAction CecPrivateSendStandByFunction::Run() {
  delegate_->SendStandBy(base::BindOnce(
      &CecPrivateSendStandByFunction::RespondWithNoArguments, this));
  return RespondLater();
}

CecPrivateSendWakeUpFunction::CecPrivateSendWakeUpFunction() = default;

CecPrivateSendWakeUpFunction::~CecPrivateSendWakeUpFunction() = default;

ExtensionFunction::ResponseAction CecPrivateSendWakeUpFunction::Run() {
  delegate_->SendWakeUp(base::BindOnce(
      &CecPrivateSendWakeUpFunction::RespondWithNoArguments, this));
  return RespondLater();
}

CecPrivateQueryDisplayCecPowerStateFunction::
    CecPrivateQueryDisplayCecPowerStateFunction() = default;

CecPrivateQueryDisplayCecPowerStateFunction::
    ~CecPrivateQueryDisplayCecPowerStateFunction() = default;

ExtensionFunction::ResponseAction
CecPrivateQueryDisplayCecPowerStateFunction::Run() {
  delegate_->QueryDisplayCecPowerState(base::BindOnce(
      &CecPrivateQueryDisplayCecPowerStateFunction::HandlePowerStates, this));
  return RespondLater();
}

void CecPrivateQueryDisplayCecPowerStateFunction::HandlePowerStates(
    const std::vector<cec_private::DisplayCecPowerState>& power_states) {
  Respond(ArgumentList(
      cec_private::QueryDisplayCecPowerState::Results::Create(power_states)));
}

}  // namespace api
}  // namespace extensions
