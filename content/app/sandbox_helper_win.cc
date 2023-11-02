// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/sandbox_helper_win.h"

#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"

namespace content {

void InitializeSandboxInfo(sandbox::SandboxInterfaceInfo* info,
                           sandbox::MitigationFlags starting_mitigations) {
  info->broker_services = sandbox::SandboxFactory::GetBrokerServices();
  if (info->broker_services) {
    info->broker_services->SetStartingMitigations(starting_mitigations);

    // Ensure the proper mitigations are enforced for the browser process.
    info->broker_services->RatchetDownSecurityMitigations(
        sandbox::MITIGATION_DEP | sandbox::MITIGATION_DEP_NO_ATL_THUNK |
        sandbox::MITIGATION_HARDEN_TOKEN_IL_POLICY);
    // Note: these mitigations are "post-startup".  Some mitigations that need
    // to be enabled sooner (e.g. MITIGATION_EXTENSION_POINT_DISABLE) are done
    // so in Chrome_ELF.
  } else {
    // There should be no starting mitigations for child processes passed in.
    DCHECK_EQ(starting_mitigations, uint64_t{0});
    info->target_services = sandbox::SandboxFactory::GetTargetServices();
  }
}

}  // namespace content
