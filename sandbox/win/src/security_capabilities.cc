// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/security_capabilities.h"
#include "base/numerics/safe_conversions.h"
#include "base/win/security_util.h"

namespace sandbox {

SecurityCapabilities::SecurityCapabilities(
    const base::win::Sid& package_sid,
    const std::vector<base::win::Sid>& capabilities)
    : SECURITY_CAPABILITIES(),
      capabilities_(base::win::CloneSidVector(capabilities)),
      package_sid_(package_sid.Clone()) {
  AppContainerSid = package_sid_.GetPSID();
  if (capabilities_.empty())
    return;

  capability_sids_.resize(capabilities_.size());
  for (size_t index = 0; index < capabilities_.size(); ++index) {
    capability_sids_[index].Sid = capabilities_[index].GetPSID();
    capability_sids_[index].Attributes = SE_GROUP_ENABLED;
  }
  CapabilityCount = base::checked_cast<DWORD>(capability_sids_.size());
  Capabilities = capability_sids_.data();
}

SecurityCapabilities::SecurityCapabilities(const base::win::Sid& package_sid)
    : SecurityCapabilities(package_sid, std::vector<base::win::Sid>()) {}

SecurityCapabilities::~SecurityCapabilities() {}

}  // namespace sandbox
