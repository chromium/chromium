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

BASE_FEATURE(kApiActionOpenPopup, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApiContentSettingsClipboard, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApiEnterpriseKioskInput, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApiRuntimeActionData, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kApiPermissionsHostAccessRequests,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApiUserScriptsExecute, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApiUserScriptsMultipleWorlds, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApiOdfsConfigPrivate, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApiEnterpriseReportingPrivateOnDataMaskingRulesTriggered,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApiRuntimeGetPlatformInfoNaClArch,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebRequestSecurityInfo, base::FEATURE_DISABLED_BY_DEFAULT);

///////////////////////////////////////////////////////////////////////////////
// Other Features
///////////////////////////////////////////////////////////////////////////////

// For historical reasons, this includes some APIs. Please don't add more.

BASE_FEATURE(kAllowWithholdingExtensionPermissionsOnInstall,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCheckingNoExtensionIdInExtensionIpcs,
             "EMF_NO_EXTENSION_ID_FOR_EXTENSION_SOURCE",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSkipResetServiceWorkerURLLoaderFactories,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableWebHidInWebView, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
// Disabled by default because on first-run we don't have a Finch seed yet, so
// we want to default to the safe behavior of no extensions.
BASE_FEATURE(kEnableExtensionsForCorpDesktopAndroid,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kExtensionDisableUnsupportedDeveloper,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionLocalizationGuid, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionIconVariants, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionManifestV2Unsupported, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionManifestV2ExceptionList,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionManifestV2Disabled, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kExtensionManifestV2ExceptionListParam(
    &kExtensionManifestV2ExceptionList,
    /*name=*/"mv2_exception_list",
    /*default_value=*/"");

BASE_FEATURE(kAllowLegacyMV2Extensions, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionProtocolHandlers, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionsManifestV3Only, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionsMenuAccessControl, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionsMenuAccessControlWithPermittedSites,
             "ExtensionsMenuAccessControlWithPermittedSitesName",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionsServiceWorkerStartRetry,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionsToolbarZeroState, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kForceWebRequestProxyForTest, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLaunchWindowsNativeHostsDirectly,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSafeBrowsingCrxAllowlistAutoDisable,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSafeBrowsingCrxAllowlistShowWarnings,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kStructuredCloningForMessaging, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTelemetryExtensionPendingApprovalApi,
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/399447642): Clean up this feature after confirming the fix is
// sufficient.
BASE_FEATURE(kWebstoreInstallerUserGestureKillSwitch,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDeclarativeNetRequestSafeRuleLimits,
             "DeclarativeNetRequestSafeDynamicRules",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExperimentalOmniboxLabs, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDeclarativeNetRequestResponseHeaderMatching,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIncludeJSCallStackInExtensionApiRequest,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseNewServiceWorkerTaskQueue, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDeclarativeNetRequestHeaderSubstitution,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDisableDisableExtensionsExceptCommandLineSwitch,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
);

BASE_FEATURE(kDisableExtensionsOnChromeUrlsSwitch,
// TODO (crbug.com/426554244): Determine if this switch should be
// removed for desktop-android builds as well.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS) && \
    !BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kUserScriptUserExtensionToggle, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDebuggerAPIRestrictedToDevMode,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionBrowserNamespaceAlternative,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOptimizeServiceWorkerStartRequests,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAvoidCloneArgsOnExtensionFunctionDispatch,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContentVerifyJobUseJobVersionForHashing,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRuntimeOnMessageWebExtensionPolyfillSupport,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableShouldShowPromotion, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebRequestPersistFilteredEvents,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace extensions_features
