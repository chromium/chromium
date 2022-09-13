// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_FEATURES_H_
#define EXTENSIONS_COMMON_EXTENSION_FEATURES_H_

#include "base/feature_list.h"

namespace extensions_features {

extern const base::Feature kSafeBrowsingCrxAllowlistShowWarnings;
extern const base::Feature kSafeBrowsingCrxAllowlistAutoDisable;

extern const base::Feature kForceWebRequestProxyForTest;

extern const base::Feature kAllowWithholdingExtensionPermissionsOnInstall;

extern const base::Feature kContentScriptsMatchOriginAsFallback;

extern const base::Feature kReportKeepaliveUkm;

extern const base::Feature kAllowSharedArrayBuffersUnconditionally;

extern const base::Feature kLoadCryptoTokenExtension;

extern const base::Feature kU2FSecurityKeyAPI;

extern const base::Feature kStructuredCloningForMV3Messaging;

extern const base::Feature kRestrictDeveloperModeAPIs;

extern const base::Feature kCheckingUnexpectedExtensionIdInContentScriptIpcs;
extern const base::Feature kCheckingNoExtensionIdInExtensionIpcs;

extern const base::Feature kNewExtensionFaviconHandling;

extern const base::Feature kExtensionDynamicURLRedirection;

extern const base::Feature kExtensionsMenuAccessControl;

extern const base::Feature kAvoidEarlyExtensionScriptContextCreation;

extern const base::Feature kExtensionsOffscreenDocuments;

extern const base::Feature kNewWebstoreDomain;

extern const base::Feature kExtensionSidePanelIntegration;

}  // namespace extensions_features

#endif  // EXTENSIONS_COMMON_EXTENSION_FEATURES_H_
