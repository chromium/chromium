// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/idle/idle_api.h"

#include "base/values.h"
#include "extensions/browser/api/idle/idle_api_constants.h"
#include "extensions/browser/api/idle/idle_manager.h"
#include "extensions/browser/api/idle/idle_manager_factory.h"

namespace extensions {

namespace {

// In seconds. Set >1 sec for security concerns.
const int kMinThreshold = 15;

// Four hours, in seconds. Not set arbitrarily high for security concerns.
const int kMaxThreshold = 4 * 60 * 60;

int ClampThreshold(int threshold) {
  if (threshold < kMinThreshold) {
    threshold = kMinThreshold;
  } else if (threshold > kMaxThreshold) {
    threshold = kMaxThreshold;
  }

  return threshold;
}

}  // namespace

IdleQueryStateFunction::~IdleQueryStateFunction() = default;

ExtensionFunction::ResponseAction IdleQueryStateFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  const auto& threshold_value = args()[0];
  EXTENSION_FUNCTION_VALIDATE(threshold_value.is_int());
  int threshold = ClampThreshold(threshold_value.GetInt());

  ui::IdleState state =
      IdleManagerFactory::GetForBrowserContext(browser_context())
          ->QueryState(threshold);

  return RespondNow(WithArguments(IdleManager::CreateIdleValue(state)));
}

void IdleQueryStateFunction::IdleStateCallback(ui::IdleState state) {
}

IdleSetDetectionIntervalFunction::~IdleSetDetectionIntervalFunction() = default;

ExtensionFunction::ResponseAction IdleSetDetectionIntervalFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  const auto& threshold_value = args()[0];
  EXTENSION_FUNCTION_VALIDATE(threshold_value.is_int());
  int threshold = ClampThreshold(threshold_value.GetInt());

  IdleManagerFactory::GetForBrowserContext(browser_context())
      ->SetThreshold(extension_id(), threshold);

  return RespondNow(NoArguments());
}

IdleGetAutoLockDelayFunction::~IdleGetAutoLockDelayFunction() = default;

ExtensionFunction::ResponseAction IdleGetAutoLockDelayFunction::Run() {
  const int delay = IdleManagerFactory::GetForBrowserContext(browser_context())
                        ->GetAutoLockDelay()
                        .InSeconds();
  return RespondNow(WithArguments(delay));
}
}  // namespace extensions
