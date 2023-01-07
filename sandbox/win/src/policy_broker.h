// Copyright 2006-2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_POLICY_BROKER_H_
#define SANDBOX_WIN_SRC_POLICY_BROKER_H_

#include "sandbox/win/src/interception.h"

namespace sandbox {

class TargetProcess;

// Sets up interceptions not controlled by explicit policies.
bool SetupBasicInterceptions(InterceptionManager* manager,
                             bool is_csrss_connected);

// Sets up imports from NTDLL for the given target process so the interceptions
// can work.
bool SetupNtdllImports(TargetProcess& child);

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_POLICY_BROKER_H_
