// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_features.h"
#include "base/feature_list.h"

namespace extensions_features {

// Controls whether we disable extensions for malware.
const base::Feature kDisableMalwareExtensionsRemotely{
    "DisableMalwareExtensionsRemotely", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether we disable extensions that are marked as policy violation
// by the Omaha attribute.
const base::Feature kDisablePolicyViolationExtensionsRemotely{
    "DisablePolicyViolationExtensionsRemotely",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether we disable extensions that are marked as potentially
// unwanted by the Omaha attribute.
const base::Feature kDisablePotentiallyUwsExtensionsRemotely{
    "DisablePotentiallyUwsExtensionsRemotely",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether we show an install friction dialog when an Enhanced Safe
// Browsing user tries to install an extension that is not included in the
// Safe Browsing CRX allowlist. This feature also controls if we show a warning
// in 'chrome://extensions' for extensions not included in the allowlist.
const base::Feature kSafeBrowsingCrxAllowlistShowWarnings{
    "SafeBrowsingCrxAllowlistShowWarnings", base::FEATURE_DISABLED_BY_DEFAULT};

// Automatically disable extensions not included in the Safe Browsing CRX
// allowlist if the user has turned on Enhanced Safe Browsing (ESB). The
// extensions can be disabled at ESB opt-in time or when an extension is moved
// out of the allowlist.
const base::Feature kSafeBrowsingCrxAllowlistAutoDisable{
    "SafeBrowsingCrxAllowlistAutoDisable", base::FEATURE_DISABLED_BY_DEFAULT};

// Forces requests to go through WebRequestProxyingURLLoaderFactory.
const base::Feature kForceWebRequestProxyForTest{
    "ForceWebRequestProxyForTest", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the UI in the install prompt which lets a user choose to withhold
// requested host permissions by default.
const base::Feature kAllowWithholdingExtensionPermissionsOnInstall{
    "AllowWithholdingExtensionPermissionsOnInstall",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables support for the "match_origin_as_fallback" property in content
// scripts.
const base::Feature kContentScriptsMatchOriginAsFallback{
    "ContentScriptsMatchOriginAsFallback", base::FEATURE_DISABLED_BY_DEFAULT};

// Whether Manifest Version 3-based extensions are supported.
const base::Feature kMv3ExtensionsSupported{"Mv3ExtensionsSupported",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Reports Extensions.WebRequest.KeepaliveRequestFinished when enabled.
const base::Feature kReportKeepaliveUkm{"ReportKeepaliveUkm",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether every extension will require a locked process, preventing
// process sharing between extensions. See https://crbug.com/1209417.
const base::Feature kStrictExtensionIsolation{
    "StrictExtensionIsolation", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace extensions_features
