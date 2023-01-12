// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/com_security.h"

#include <objidl.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "remoting/host/win/security_descriptor.h"

namespace remoting {

bool InitializeComSecurity(const std::string& security_descriptor,
                           const std::string& mandatory_label,
                           bool activate_as_activator) {
  std::string sddl = security_descriptor + mandatory_label;

  // Convert the SDDL description into a security descriptor in absolute format.
  ScopedSd relative_sd = ConvertSddlToSd(sddl);
  if (!relative_sd) {
    PLOG(ERROR) << "Failed to create a security descriptor";
    return false;
  }
  ScopedSd absolute_sd;
  ScopedAcl dacl;
  ScopedSid group;
  ScopedSid owner;
  ScopedAcl sacl;
  if (!MakeScopedAbsoluteSd(relative_sd, &absolute_sd, &dacl, &group, &owner,
                            &sacl)) {
    PLOG(ERROR) << "MakeScopedAbsoluteSd() failed";
    return false;
  }

  DWORD capabilities = EOAC_DYNAMIC_CLOAKING;
  if (!activate_as_activator) {
    capabilities |= EOAC_DISABLE_AAA;
  }

  // Apply the security descriptor and default security settings. See
  // InitializeComSecurity's declaration for details.
  HRESULT result = CoInitializeSecurity(
      absolute_sd.get(),
      -1,       // Let COM choose which authentication services to register.
      nullptr,  // See above.
      nullptr,  // Reserved, must be nullptr.
      RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IDENTIFY,
      nullptr,  // Default authentication information is not provided.
      capabilities,
      nullptr);  // Reserved, must be nullptr
  if (FAILED(result)) {
    LOG(ERROR) << "CoInitializeSecurity() failed, result=0x" << std::hex
               << result << std::dec << ".";
    return false;
  }

  return true;
}

}  // namespace remoting
