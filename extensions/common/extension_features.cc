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
    "DisableMalwareExtensionsRemotely", base::FEATURE_DISABLED_BY_DEFAULT};

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

// Reports Extensions.WebRequest.KeepaliveRequestFinished when enabled.
const base::Feature kReportKeepaliveUkm{"ReportKeepaliveUkm",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Enables callers of the GetAuthToken API to request for the unbundled consent
// UI and populates the scopes parameter in the GetAuthToken callback function.
const base::Feature kReturnScopesInGetAuthToken{
    "ReturnScopesInGetAuthToken", base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, allows the GetAuthToken API to provide the "selected_user_id"
// parameter to the server, indicating which account to request permissions
// from.
const base::Feature kSelectedUserIdInGetAuthToken{
    "SelectedUserIdInGetAuthToken", base::FEATURE_ENABLED_BY_DEFAULT};

// Feature used mostly for exposing a field-trial-param-based mechanism for
// adding remaining strugglers to the CORB/CORS allowlist which has been
// deprecated in Chrome 87.
const base::Feature kCorbCorsAllowlist{"CorbCorsAllowlist",
                                       base::FEATURE_DISABLED_BY_DEFAULT};
const char kCorbCorsAllowlistParamName[] =
    "CorbCorsAllowlistDeprecationParamName";

}  // namespace extensions_features
