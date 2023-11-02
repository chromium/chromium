// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/win/lpac_capability.h"

namespace sandbox {
namespace policy {

// WARNING: Renaming a capability could cause backward compatible issues!

// Capability used by the Media Foundation CDM to grant read and write access to
// a folder under the Chrome user's profile.
const wchar_t kMediaFoundationCdmData[] = L"lpacMediaFoundationCdmData";

// Capability for Media Foundation CDM files that needs read and execute access.
const wchar_t kMediaFoundationCdmFiles[] = L"mediaFoundationCdmFiles";

}  // namespace policy
}  // namespace sandbox
