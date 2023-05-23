// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_FEATURES_H_
#define EXTENSIONS_COMMON_EXTENSION_FEATURES_H_

#include "base/feature_list.h"

namespace extensions_features {

///////////////////////////////////////////////////////////////////////////////
// README!
// * Please keep these features alphabetized. One exception: API features go
//   at the top so that they are visibly grouped together.
// * Adding a new feature for an extension API? Great!
//   Please use the naming style `kApi<Namespace><Method>`, e.g.
//   `kApiTabsCreate`.
//   Note that if you are using the features.json files to restrict your
//   API with the feature (which is usually best practice if you are introducing
//   any new features), you will also have to add the feature entry to the list
//   in extensions/common/features/feature_flags.cc so the features system can
//   detect it.
// * Naming Tips: Even though this file is unique to extensions, base::Features
//   have to be globally unique. Thus, it's often best to give features very
//   specific names (often including "Extension", unlike many C++ class names)
//   since namespacing doesn't otherwise exist.
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// API Features
///////////////////////////////////////////////////////////////////////////////

// NOTE(devlin): If there are consistently enough of these in flux, it might
// make sense to have their own file.

BASE_DECLARE_FEATURE(kApiRuntimeGetContexts);
BASE_DECLARE_FEATURE(kApiSidePanelOpen);

///////////////////////////////////////////////////////////////////////////////
// Other Features
///////////////////////////////////////////////////////////////////////////////

// For historical reasons, this includes some APIs. Please don't add more.

BASE_DECLARE_FEATURE(kAllowSharedArrayBuffersUnconditionally);

BASE_DECLARE_FEATURE(kAllowWithholdingExtensionPermissionsOnInstall);

BASE_DECLARE_FEATURE(kAvoidEarlyExtensionScriptContextCreation);

BASE_DECLARE_FEATURE(kCheckingNoExtensionIdInExtensionIpcs);

BASE_DECLARE_FEATURE(kContentScriptsMatchOriginAsFallback);

BASE_DECLARE_FEATURE(kExtensionDynamicURLRedirection);

BASE_DECLARE_FEATURE(kExtensionSidePanelIntegration);

BASE_DECLARE_FEATURE(kExtensionSourceUrlEnforcement);

BASE_DECLARE_FEATURE(kExtensionWebFileHandlers);

BASE_DECLARE_FEATURE(kExtensionsManifestV3Only);

BASE_DECLARE_FEATURE(kExtensionsMenuAccessControl);

BASE_DECLARE_FEATURE(kExtensionsMenuAccessControlWithPermittedSites);

BASE_DECLARE_FEATURE(kForceWebRequestProxyForTest);

BASE_DECLARE_FEATURE(kLaunchWindowsNativeHostsDirectly);

BASE_DECLARE_FEATURE(kNewExtensionFaviconHandling);

BASE_DECLARE_FEATURE(kNewWebstoreDomain);

BASE_DECLARE_FEATURE(kReportKeepaliveUkm);

BASE_DECLARE_FEATURE(kRestrictDeveloperModeAPIs);

BASE_DECLARE_FEATURE(kSafeBrowsingCrxAllowlistAutoDisable);

BASE_DECLARE_FEATURE(kSafeBrowsingCrxAllowlistShowWarnings);

BASE_DECLARE_FEATURE(kStructuredCloningForMV3Messaging);

BASE_DECLARE_FEATURE(kTelemetryExtensionPendingApprovalApi);

BASE_DECLARE_FEATURE(kWebviewTagMPArchBehavior);

///////////////////////////////////////////////////////////////////////////////
// STOP!
// Please don't just add your new feature down here.
// See the guidance at the top of this file.
///////////////////////////////////////////////////////////////////////////////

}  // namespace extensions_features

#endif  // EXTENSIONS_COMMON_EXTENSION_FEATURES_H_
