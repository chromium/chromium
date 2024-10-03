// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_features.h"
#include "base/feature_list.h"

namespace extensions_features {

///////////////////////////////////////////////////////////////////////////////
// API Features
///////////////////////////////////////////////////////////////////////////////

BASE_FEATURE(kApiActionOpenPopup,
             "ApiActionOpenPopup",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApiContentSettingsClipboard,
             "ApiContentSettingsClipboard",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApiEnterpriseKioskInput,
             "ApiEnterpriseKioskInput",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApiPermissionsSiteAccessRequests,
             "ApiPermissionsSiteAccessRequests",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kApiUserScriptsMultipleWorlds,
             "ApiUserScriptsMultipleWorlds",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kApiOdfsConfigPrivate,
             "ApiOdfsConfigPrivate",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApiEnterpriseReportingPrivateReportDataMaskingEvent,
             "ApiEnterpriseReportingPrivateReportDataMaskingEvent",
             base::FEATURE_ENABLED_BY_DEFAULT);

///////////////////////////////////////////////////////////////////////////////
// Other Features
///////////////////////////////////////////////////////////////////////////////

// For historical reasons, this includes some APIs. Please don't add more.

BASE_FEATURE(kAllowSharedArrayBuffersUnconditionally,
             "AllowSharedArrayBuffersUnconditionally",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAllowWithholdingExtensionPermissionsOnInstall,
             "AllowWithholdingExtensionPermissionsOnInstall",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAvoidEarlyExtensionScriptContextCreation,
             "AvoidEarlyExtensionScriptContextCreation",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCheckingNoExtensionIdInExtensionIpcs,
             "EMF_NO_EXTENSION_ID_FOR_EXTENSION_SOURCE",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableWebHidInWebView,
             "EnableWebHidInWebView",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionDisableUnsupportedDeveloper,
             "ExtensionDisableUnsupportedDeveloper",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionDynamicURLRedirection,
             "ExtensionDynamicURLRedirection",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionIconVariants,
             "ExtensionIconVariants",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionManifestV2DeprecationWarning,
             "ExtensionManifestV2DeprecationWarning",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionManifestV2Unsupported,
             "ExtensionManifestV2Unsupported",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionManifestV2ExceptionList,
             "ExtensionManifestV2ExceptionList",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionManifestV2Disabled,
             "ExtensionManifestV2Disabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kExtensionManifestV2ExceptionListParam(
    &kExtensionManifestV2ExceptionList,
    /*name=*/"mv2_exception_list",
    /*default_value=*/"");

BASE_FEATURE(kAllowLegacyMV2Extensions,
             "AllowLegacyMV2Extensions",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionSourceUrlEnforcement,
             "ExtensionSourceUrlEnforcement",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionWebFileHandlers,
             "ExtensionWebFileHandlers",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionsManifestV3Only,
             "ExtensionsManifestV3Only",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionsMenuAccessControl,
             "ExtensionsMenuAccessControl",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionsMenuAccessControlWithPermittedSites,
             "ExtensionsMenuAccessControlWithPermittedSitesName",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionsToolbarZeroState,
             "ExtensionsToolbarZeroState",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kForceWebRequestProxyForTest,
             "ForceWebRequestProxyForTest",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLaunchWindowsNativeHostsDirectly,
             "LaunchWindowsNativeHostsDirectly",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/357636604): Remove this feature flag in M132.
BASE_FEATURE(kMacRejectFilePathsEndingWithSeparator,
             "MacRejectFilePathsEndingWithSeparator",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kNewExtensionFaviconHandling,
             "ExtensionsNewFaviconHandling",
             base::FEATURE_ENABLED_BY_DEFAULT);

// To investigate signal beacon loss in crrev.com/c/2262402.
BASE_FEATURE(kReportKeepaliveUkm,
             "ReportKeepaliveUkm",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSafeBrowsingCrxAllowlistAutoDisable,
             "SafeBrowsingCrxAllowlistAutoDisable",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSafeBrowsingCrxAllowlistShowWarnings,
             "SafeBrowsingCrxAllowlistShowWarnings",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kStructuredCloningForMV3Messaging,
             "StructuredCloningForMV3Messaging",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTelemetryExtensionPendingApprovalApi,
             "TelemetryExtensionPendingApprovalApi",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNewWebstoreURL,
             "NewWebstoreURL",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDeclarativeNetRequestSafeRuleLimits,
             "DeclarativeNetRequestSafeDynamicRules",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDeclarativeNetRequestResponseHeaderMatching,
             "DeclarativeNetRequestResponseHeaderMatching",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIncludeJSCallStackInExtensionApiRequest,
             "IncludeJSCallStackInExtensionApiRequest",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseItemSnippetsAPI,
             "UseItemSnippetsAPI",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUseNewServiceWorkerTaskQueue,
             "UseNewServiceWorkerTaskQueue",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDeclarativeNetRequestHeaderSubstitution,
             "DeclarativeNetRequestHeaderSubstitution",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSilentDebuggerExtensionAPI,
             "SilentDebuggerExtensionAPI",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace extensions_features
