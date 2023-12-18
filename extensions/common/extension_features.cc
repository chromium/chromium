// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_features.h"
#include "base/feature_list.h"

namespace extensions_features {

///////////////////////////////////////////////////////////////////////////////
// API Features
///////////////////////////////////////////////////////////////////////////////

BASE_FEATURE(kApiContentSettingsClipboard,
             "ApiContentSettingsClipboard",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApiEnterpriseKioskInput,
             "ApiEnterpriseKioskInput",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApiReadingList,
             "ApiReadingList",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApiUserScripts,
             "ApiUserScripts",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kApiOdfsConfigPrivate,
             "ApiOdfsConfigPrivate",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRestrictFileURLNavigation,
             "RestrictFileURLNavigation",
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

BASE_FEATURE(kExtensionDynamicURLRedirection,
             "ExtensionDynamicURLRedirection",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionSidePanelIntegration,
             "ExtensionSidePanelIntegration",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

BASE_FEATURE(kForceWebRequestProxyForTest,
             "ForceWebRequestProxyForTest",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLaunchWindowsNativeHostsDirectly,
             "LaunchWindowsNativeHostsDirectly",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNewExtensionFaviconHandling,
             "ExtensionsNewFaviconHandling",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNewWebstoreDomain,
             "NewWebstoreDomain",
             base::FEATURE_ENABLED_BY_DEFAULT);

// To investigate signal beacon loss in crrev.com/c/2262402.
BASE_FEATURE(kReportKeepaliveUkm,
             "ReportKeepaliveUkm",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRestrictDeveloperModeAPIs,
             "RestrictDeveloperModeAPIs",
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

BASE_FEATURE(kUsePerBrowserContextWebRequestEventRouter,
             "kUsePerBrowserContextWebRequestEventRouter",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebviewTagMPArchBehavior,
             "WebviewTagMPArchBehavior",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionsZipFileInstalledInProfileDir,
             "ExtensionsZipFileInstalledInProfileDir",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionsServiceWorkerOptimizedEventDispatch,
             "ExtensionsServiceWorkerOptimizedEventDispatch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNewWebstoreURL,
             "NewWebstoreURL",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDeclarativeNetRequestSafeRuleLimits,
             "DeclarativeNetRequestSafeDynamicRules",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDeclarativeNetRequestResponseHeaderMatching,
             "DeclarativeNetRequestResponseHeaderMatching",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace extensions_features
