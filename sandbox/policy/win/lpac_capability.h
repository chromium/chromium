// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_WIN_LPAC_CAPABILITY_H_
#define SANDBOX_POLICY_WIN_LPAC_CAPABILITY_H_

namespace sandbox {
namespace policy {

// WARNING: Renaming a capability could cause backward compatible issues!

// Capability used by the Media Foundation CDM to grant read and write access to
// a folder under the Chrome user's profile.
constexpr wchar_t kMediaFoundationCdmData[] = L"lpacMediaFoundationCdmData";

// Capability for Media Foundation CDM files that needs read and execute access.
constexpr wchar_t kMediaFoundationCdmFiles[] = L"mediaFoundationCdmFiles";

// Capabilities for accessing installation chrome files.
constexpr wchar_t kChromeInstallFiles[] = L"chromeInstallFiles";
constexpr wchar_t kLpacChromeInstallFiles[] = L"lpacChromeInstallFiles";

// Capabilities defined by the system.
constexpr wchar_t kLpacAppExperience[] = L"lpacAppExperience";
constexpr wchar_t kLpacCom[] = L"lpacCom";
constexpr wchar_t kLpacCryptoServices[] = L"lpacCryptoServices";
constexpr wchar_t kLpacEnterprisePolicyChangeNotifications[] =
    L"lpacEnterprisePolicyChangeNotifications";
constexpr wchar_t kLpacIdentityServices[] = L"lpacIdentityServices";
constexpr wchar_t kLpacInstrumentation[] = L"lpacInstrumentation";
constexpr wchar_t kLpacMedia[] = L"lpacMedia";
constexpr wchar_t kLpacPnPNotifications[] = L"lpacPnPNotifications";
constexpr wchar_t kLpacPnpNotifications[] = L"lpacPnpNotifications";
constexpr wchar_t kLpacServicesManagement[] = L"lpacServicesManagement";
constexpr wchar_t kLpacSessionManagement[] = L"lpacSessionManagement";
constexpr wchar_t kRegistryRead[] = L"registryRead";

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_WIN_LPAC_CAPABILITY_H_
