// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  int threshold = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &threshold));
  threshold = ClampThreshold(threshold);

  ui::IdleState state =
      IdleManagerFactory::GetForBrowserContext(browser_context())
          ->QueryState(threshold);

  return RespondNow(OneArgument(
      base::Value::FromUniquePtrValue(IdleManager::CreateIdleValue(state))));
}

void IdleQueryStateFunction::IdleStateCallback(ui::IdleState state) {
}

IdleSetDetectionIntervalFunction::~IdleSetDetectionIntervalFunction() = default;

ExtensionFunction::ResponseAction IdleSetDetectionIntervalFunction::Run() {
  int threshold = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &threshold));
  threshold = ClampThreshold(threshold);

  IdleManagerFactory::GetForBrowserContext(browser_context())
      ->SetThreshold(extension_id(), threshold);

  return RespondNow(NoArguments());
}

IdleGetAutoLockDelayFunction::~IdleGetAutoLockDelayFunction() = default;

ExtensionFunction::ResponseAction IdleGetAutoLockDelayFunction::Run() {
  const int delay = IdleManagerFactory::GetForBrowserContext(browser_context())
                        ->GetAutoLockDelay()
                        .InSeconds();
  return RespondNow(OneArgument(base::Value(delay)));
}
}  // namespace extensions
