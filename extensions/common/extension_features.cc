// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_features.h"

#include "base/feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"

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

BASE_FEATURE(kApiRuntimeActionData,
             "ApiRuntimeActionData",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kApiPermissionsHostAccessRequests,
             "ApiPermissionsHostAccessRequests",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApiUserScriptsExecute,
             "ApiUserScriptsExecute",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApiUserScriptsMultipleWorlds,
             "ApiUserScriptsMultipleWorlds",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApiOdfsConfigPrivate,
             "ApiOdfsConfigPrivate",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApiEnterpriseReportingPrivateOnDataMaskingRulesTriggered,
             "ApiEnterpriseReportingPrivateOnDataMaskingRulesTriggered",
             base::FEATURE_DISABLED_BY_DEFAULT);

///////////////////////////////////////////////////////////////////////////////
// Other Features
///////////////////////////////////////////////////////////////////////////////

// For historical reasons, this includes some APIs. Please don't add more.

BASE_FEATURE(kAllowWithholdingExtensionPermissionsOnInstall,
             "AllowWithholdingExtensionPermissionsOnInstall",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
BASE_FEATURE(kBlockInstallingExtensionsOnDesktopAndroid,
             "BlockInstallingExtensionsOnDesktopAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)

BASE_FEATURE(kCheckingNoExtensionIdInExtensionIpcs,
             "EMF_NO_EXTENSION_ID_FOR_EXTENSION_SOURCE",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDeferResetURLLoaderFactories,
             "DeferResetURLLoaderFactories",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSkipResetServiceWorkerURLLoaderFactories,
             "SkipResetServiceWorkerURLLoaderFactories",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableWebHidInWebView,
             "EnableWebHidInWebView",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionDisableUnsupportedDeveloper,
             "ExtensionDisableUnsupportedDeveloper",
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
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kExtensionManifestV2ExceptionListParam(
    &kExtensionManifestV2ExceptionList,
    /*name=*/"mv2_exception_list",
    /*default_value=*/"");

BASE_FEATURE(kAllowLegacyMV2Extensions,
             "AllowLegacyMV2Extensions",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionWARForRedirect,
             "ExtensionWARForRedirect",
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

// TODO(crbug.com/399447642): Clean up this feature after confirming the fix is
// sufficient.
BASE_FEATURE(kWebstoreInstallerUserGestureKillSwitch,
             "WebstoreInstallerUserGestureKillSwitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// TODO(https://crbug.com/400119351): Remove this feature flag in M138.
BASE_FEATURE(kWinRejectDotSpaceSuffixFilePaths,
             "WinRejectDotSpaceSuffixFilePaths",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kDeclarativeNetRequestSafeRuleLimits,
             "DeclarativeNetRequestSafeDynamicRules",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExperimentalOmniboxLabs,
             "ExperimentalOmniboxLabs",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDeclarativeNetRequestResponseHeaderMatching,
             "DeclarativeNetRequestResponseHeaderMatching",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIncludeJSCallStackInExtensionApiRequest,
             "IncludeJSCallStackInExtensionApiRequest",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseNewServiceWorkerTaskQueue,
             "UseNewServiceWorkerTaskQueue",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDeclarativeNetRequestHeaderSubstitution,
             "DeclarativeNetRequestHeaderSubstitution",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSilentDebuggerExtensionAPI,
             "SilentDebuggerExtensionAPI",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRemoveCoreSiteInstance,
             "RemoveCoreSiteInstance",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDisableLoadExtensionCommandLineSwitch,
             "DisableLoadExtensionCommandLineSwitch",
// --load-extension is disabled for chrome-branded release builds except on
// ChromeOS where it is required for testing, and is not a security risk
// since it cannot be controlled by users.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
);

BASE_FEATURE(kUserScriptUserExtensionToggle,
             "UserScriptUserExtensionToggle",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDebuggerAPIRestrictedToDevMode,
             "DebuggerAPIRestrictedToDevMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionBrowserNamespaceAlternative,
             "ExtensionBrowserNamespaceAlternative",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace extensions_features
