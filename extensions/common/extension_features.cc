// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_features.h"
#include "base/feature_list.h"

namespace extensions_features {

// Controls whether we show an install friction dialog when an Enhanced Safe
// Browsing user tries to install an extension that is not included in the
// Safe Browsing CRX allowlist. This feature also controls if we show a warning
// in 'chrome://extensions' for extensions not included in the allowlist.
BASE_FEATURE(kSafeBrowsingCrxAllowlistShowWarnings,
             "SafeBrowsingCrxAllowlistShowWarnings",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Automatically disable extensions not included in the Safe Browsing CRX
// allowlist if the user has turned on Enhanced Safe Browsing (ESB). The
// extensions can be disabled at ESB opt-in time or when an extension is moved
// out of the allowlist.
BASE_FEATURE(kSafeBrowsingCrxAllowlistAutoDisable,
             "SafeBrowsingCrxAllowlistAutoDisable",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Forces requests to go through WebRequestProxyingURLLoaderFactory.
BASE_FEATURE(kForceWebRequestProxyForTest,
             "ForceWebRequestProxyForTest",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the UI in the install prompt which lets a user choose to withhold
// requested host permissions by default.
BASE_FEATURE(kAllowWithholdingExtensionPermissionsOnInstall,
             "AllowWithholdingExtensionPermissionsOnInstall",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables support for the "match_origin_as_fallback" property in content
// scripts.
BASE_FEATURE(kContentScriptsMatchOriginAsFallback,
             "ContentScriptsMatchOriginAsFallback",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Reports Extensions.WebRequest.KeepaliveRequestFinished when enabled.
BASE_FEATURE(kReportKeepaliveUkm,
             "ReportKeepaliveUkm",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether extension contexts can use SharedArrayBuffers unconditionally (i.e.
// without requiring cross origin isolation).
// TODO(crbug.com/1184892): Flip this in M95.
BASE_FEATURE(kAllowSharedArrayBuffersUnconditionally,
             "AllowSharedArrayBuffersUnconditionally",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Loads the CryptoToken component extension, which implements the deprecated
// U2F Security Key API.
// TODO(1224886): Delete together with CryptoToken code.
BASE_FEATURE(kLoadCryptoTokenExtension,
             "LoadCryptoTokenExtension",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the CryptoToken component extension to receive messages. This flag
// has no effect unless `kLoadCryptoTokenExtension` is also enabled.
// TODO(1224886): Delete together with CryptoToken code.
BASE_FEATURE(kU2FSecurityKeyAPI,
             "U2FSecurityKeyAPI",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, causes Manifest V3 (and greater) extensions to use structured
// cloning (instead of JSON serialization) for extension messaging, except when
// communicating with native messaging hosts.
BASE_FEATURE(kStructuredCloningForMV3Messaging,
             "StructuredCloningForMV3Messaging",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, causes extensions to allow access to certain APIs only if the
// user is in the developer mode.
BASE_FEATURE(kRestrictDeveloperModeAPIs,
             "RestrictDeveloperModeAPIs",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, then bad_message::ReceivedBadMessage will be called when
// browser receives an IPC from a content script and the IPC that unexpectedly
// claims to act on behalf of a given extension id, (i.e. even if the browser
// process things that renderer process never run content scripts from the
// extension).
BASE_FEATURE(kCheckingNoExtensionIdInExtensionIpcs,
             "EMF_NO_EXTENSION_ID_FOR_EXTENSION_SOURCE",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether extensions can use the new favicon fetching in Manifest V3.
BASE_FEATURE(kNewExtensionFaviconHandling,
             "ExtensionsNewFaviconHandling",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Determine if dynamic extension URLs are handled and redirected.
BASE_FEATURE(kExtensionDynamicURLRedirection,
             "kExtensionDynamicURLRedirection",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables enhanced site control for extensions and allowing the user to control
// site permissions.
BASE_FEATURE(kExtensionsMenuAccessControl,
             "ExtensionsMenuAccessControl",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, calls RenderFrame::SetAllowsCrossBrowsingInstanceFrameLookup() in
// DidCreateScriptContext() instead of DidCommitProvisionalLoad() to avoid
// creating the script context too early which can be bad for performance.
BASE_FEATURE(kAvoidEarlyExtensionScriptContextCreation,
             "AvoidEarlyExtensionScriptContextCreation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The feature enabling offscreen documents in Manifest V3 extensions.
BASE_FEATURE(kExtensionsOffscreenDocuments,
             "ExtensionsOffscreenDocuments",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, allows APIs used by the webstore to be exposed on the URL for the
// new webstore.
// TODO(crbug.com/1338235): Before this starts to be rolled out to end users, we
// need to ensure the new domain has all the special handling we do for the
// current webstore enabled on it.
BASE_FEATURE(kNewWebstoreDomain,
             "NewWebstoreDomain",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Side panel API availability.
BASE_FEATURE(kExtensionSidePanelIntegration,
             "ExtensionSidePanelIntegration",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable fileSystemProvider and fileSystemProviderInternal APIs in service
// workers.
BASE_FEATURE(kExtensionsFSPInServiceWorkers,
             "ExtensionsFSPInServiceWorkers",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace extensions_features
