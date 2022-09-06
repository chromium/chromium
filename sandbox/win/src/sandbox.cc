// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sandbox.h"

#include <windows.h>

#include "sandbox/win/src/broker_services.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/target_services.h"

namespace sandbox {
// The section for IPC and policy.
SANDBOX_INTERCEPT HANDLE g_shared_section;
static bool s_is_broker = false;

// GetBrokerServices: the current implementation relies on a shared section
// that is created by the broker and opened by the target.
BrokerServices* SandboxFactory::GetBrokerServices() {
  // Can't be the broker if the shared section is open.
  if (g_shared_section)
    return nullptr;
  // If the shared section does not exist we are the broker, then create
  // the broker object.
  s_is_broker = true;
  return BrokerServicesBase::GetInstance();
}

// GetTargetServices implementation must follow the same technique as the
// GetBrokerServices, but in this case the logic is the opposite.
TargetServices* SandboxFactory::GetTargetServices() {
  // Can't be the target if the section handle is not valid.
  if (!g_shared_section)
    return nullptr;
  // We are the target
  s_is_broker = false;
  // Creates and returns the target services implementation.
  return TargetServicesBase::GetInstance();
}

}  // namespace sandbox

// Allows querying for whether the current process has been sandboxed.
extern "C" bool __declspec(dllexport) IsSandboxedProcess() {
  return !!sandbox::g_shared_section;
}
