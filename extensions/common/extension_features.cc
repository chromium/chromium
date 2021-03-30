// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_features.h"

namespace extensions_features {

// Controls whether we redirect the NTP to the chrome://extensions page or show
// a middle slot promo, and which of the the three checkup banner messages
// (performance focused, privacy focused or neutral) to show.
const base::Feature kExtensionsCheckup{"ExtensionsCheckup",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether we disable extensions for malware.
const base::Feature kDisableMalwareExtensionsRemotely{
    "DisableMalwareExtensionsRemotely", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether we disable extensions not allowlisted for Enhanced Safe
// Browsing users.
const base::Feature kEnforceSafeBrowsingExtensionAllowlist{
    "EnforceSafeBrowsingExtensionAllowlist", base::FEATURE_DISABLED_BY_DEFAULT};

// Parameters for ExtensionsCheckup feature.
const char kExtensionsCheckupEntryPointParameter[] = "entry_point";
const char kExtensionsCheckupBannerMessageParameter[] = "banner_message_type";

// Constants for ExtensionsCheckup parameters.
// Indicates that the user should be shown the chrome://extensions page on
// startup.
const char kStartupEntryPoint[] = "startup";
// Indicates that the user should be shown a promo on the NTP leading to the
// chrome://extensions page.
const char kNtpPromoEntryPoint[] = "promo";
// Indicates the focus of the message shown on chrome://the extensions page
// banner and the NTP promo.
const char kPerformanceMessage[] = "0";
const char kPrivacyMessage[] = "1";
const char kNeutralMessage[] = "2";

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

// Enables default Chrome apps on Chrome OS to sync uninstallation across
// devices.
const base::Feature kDefaultChromeAppUninstallSync{
    "DefaultChromeAppUninstallSync", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace extensions_features
