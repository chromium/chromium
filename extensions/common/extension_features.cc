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

BASE_FEATURE(ApiActionOpenPopup, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(ApiContentSettingsClipboard, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(ApiEnterpriseKioskInput, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(ApiRuntimeActionData, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(ApiPermissionsHostAccessRequests,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(ApiUserScriptsExecute, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(ApiUserScriptsMultipleWorlds, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(ApiOdfsConfigPrivate, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(ApiEnterpriseReportingPrivateOnDataMaskingRulesTriggered,
             base::FEATURE_ENABLED_BY_DEFAULT);

///////////////////////////////////////////////////////////////////////////////
// Other Features
///////////////////////////////////////////////////////////////////////////////

// For historical reasons, this includes some APIs. Please don't add more.

BASE_FEATURE(AllowWithholdingExtensionPermissionsOnInstall,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCheckingNoExtensionIdInExtensionIpcs,
             "EMF_NO_EXTENSION_ID_FOR_EXTENSION_SOURCE",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(SkipResetServiceWorkerURLLoaderFactories,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(EnableWebHidInWebView, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(ExtensionDisableUnsupportedDeveloper,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(ExtensionIconVariants, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(ExtensionManifestV2Unsupported, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(ExtensionManifestV2ExceptionList,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(ExtensionManifestV2Disabled, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kExtensionManifestV2ExceptionListParam(
    &kExtensionManifestV2ExceptionList,
    /*name=*/"mv2_exception_list",
    /*default_value=*/"");

BASE_FEATURE(AllowLegacyMV2Extensions, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(ExtensionProtocolHandlers, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(ExtensionsManifestV3Only, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(ExtensionsMenuAccessControl, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionsMenuAccessControlWithPermittedSites,
             "ExtensionsMenuAccessControlWithPermittedSitesName",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(ExtensionsToolbarZeroState, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(ForceWebRequestProxyForTest, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(LaunchWindowsNativeHostsDirectly,
             base::FEATURE_DISABLED_BY_DEFAULT);

// To investigate signal beacon loss in crrev.com/c/2262402.
BASE_FEATURE(ReportKeepaliveUkm, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(SafeBrowsingCrxAllowlistAutoDisable,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(SafeBrowsingCrxAllowlistShowWarnings,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(StructuredCloningForMV3Messaging,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(TelemetryExtensionPendingApprovalApi,
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/399447642): Clean up this feature after confirming the fix is
// sufficient.
BASE_FEATURE(WebstoreInstallerUserGestureKillSwitch,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDeclarativeNetRequestSafeRuleLimits,
             "DeclarativeNetRequestSafeDynamicRules",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(ExperimentalOmniboxLabs, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(DeclarativeNetRequestResponseHeaderMatching,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(IncludeJSCallStackInExtensionApiRequest,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(UseNewServiceWorkerTaskQueue, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(DeclarativeNetRequestHeaderSubstitution,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(DisableDisableExtensionsExceptCommandLineSwitch,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
);

BASE_FEATURE(DisableLoadExtensionCommandLineSwitch,
// --load-extension is disabled for chrome-branded release builds except on
// ChromeOS where it is required for testing, and is not a security risk
// since it cannot be controlled by users.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
);

BASE_FEATURE(DisableExtensionsOnChromeUrlsSwitch,
// TODO (crbug.com/426554244): Determine if this switch should be
// removed for desktop-android builds as well.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS) && \
    !BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(UserScriptUserExtensionToggle, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(DebuggerAPIRestrictedToDevMode, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(ExtensionBrowserNamespaceAlternative,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(RuntimeOnMessagePromiseReturnSupport,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(OptimizeServiceWorkerStartRequests,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(AvoidCloneArgsOnExtensionFunctionDispatch,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(OneTimeMessageUnserializableResponseClosesChannel,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(ContentVerifyJobUseJobVersionForHashing,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace extensions_features
