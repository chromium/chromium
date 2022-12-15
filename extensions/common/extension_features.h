// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_FEATURES_H_
#define EXTENSIONS_COMMON_EXTENSION_FEATURES_H_

#include "base/feature_list.h"

namespace extensions_features {

BASE_DECLARE_FEATURE(kSafeBrowsingCrxAllowlistShowWarnings);
BASE_DECLARE_FEATURE(kSafeBrowsingCrxAllowlistAutoDisable);

BASE_DECLARE_FEATURE(kForceWebRequestProxyForTest);

BASE_DECLARE_FEATURE(kAllowWithholdingExtensionPermissionsOnInstall);

BASE_DECLARE_FEATURE(kContentScriptsMatchOriginAsFallback);

BASE_DECLARE_FEATURE(kReportKeepaliveUkm);

BASE_DECLARE_FEATURE(kAllowSharedArrayBuffersUnconditionally);

BASE_DECLARE_FEATURE(kStructuredCloningForMV3Messaging);

BASE_DECLARE_FEATURE(kRestrictDeveloperModeAPIs);

BASE_DECLARE_FEATURE(kCheckingNoExtensionIdInExtensionIpcs);

BASE_DECLARE_FEATURE(kNewExtensionFaviconHandling);

BASE_DECLARE_FEATURE(kExtensionDynamicURLRedirection);

BASE_DECLARE_FEATURE(kExtensionsMenuAccessControl);

BASE_DECLARE_FEATURE(kAvoidEarlyExtensionScriptContextCreation);

BASE_DECLARE_FEATURE(kExtensionsOffscreenDocuments);

BASE_DECLARE_FEATURE(kNewWebstoreDomain);

BASE_DECLARE_FEATURE(kExtensionSidePanelIntegration);

BASE_DECLARE_FEATURE(kFileHandlersMV3);

BASE_DECLARE_FEATURE(kExtensionSourceUrlEnforcement);

BASE_DECLARE_FEATURE(kWebviewTagMPArchBehavior);

BASE_DECLARE_FEATURE(kExtensionsManifestV3Only);

BASE_DECLARE_FEATURE(kMinimumMV3CSPWithInlineSpeculationRules);

}  // namespace extensions_features

#endif  // EXTENSIONS_COMMON_EXTENSION_FEATURES_H_
