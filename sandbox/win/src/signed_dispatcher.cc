// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/signed_dispatcher.h"

#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/signed_interception.h"
#include "sandbox/win/src/signed_policy.h"

namespace sandbox {

SignedDispatcher::SignedDispatcher(PolicyBase* policy_base)
    : policy_base_(policy_base) {
}

bool SignedDispatcher::SetupService(InterceptionManager* manager,
                                    IpcTag service) {
  return service == IpcTag::NTCREATESECTION
             ? INTERCEPT_NT(manager, NtCreateSection, CREATE_SECTION_ID, 32)
             : false;
}

}  // namespace sandbox
