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

// Controls the availability of contentSettings.clipboard.
BASE_DECLARE_FEATURE(kApiContentSettingsClipboard);

// Controls the availability of the enterprise.kioskInput API.
BASE_DECLARE_FEATURE(kApiEnterpriseKioskInput);

// Controls the availability of the ReadingList API.
BASE_DECLARE_FEATURE(kApiReadingList);

// Controls the availability of the userScripts API.
BASE_DECLARE_FEATURE(kApiUserScripts);

// Controls the availability of the odfsConfigPrivate API.
BASE_DECLARE_FEATURE(kApiOdfsConfigPrivate);

// Controls the availability of navigation to file URLs.
BASE_DECLARE_FEATURE(kRestrictFileURLNavigation);

///////////////////////////////////////////////////////////////////////////////
// Other Features
///////////////////////////////////////////////////////////////////////////////

// For historical reasons, this includes some APIs. Please don't add more.

// Whether extension contexts can use SharedArrayBuffers unconditionally (i.e.
// without requiring cross origin isolation).
// TODO(crbug.com/1184892): Flip this in M95.
BASE_DECLARE_FEATURE(kAllowSharedArrayBuffersUnconditionally);

// Enables the UI in the install prompt which lets a user choose to withhold
// requested host permissions by default.
BASE_DECLARE_FEATURE(kAllowWithholdingExtensionPermissionsOnInstall);

// If enabled, calls RenderFrame::SetAllowsCrossBrowsingInstanceFrameLookup() in
// DidCreateScriptContext() instead of DidCommitProvisionalLoad() to avoid
// creating the script context too early which can be bad for performance.
BASE_DECLARE_FEATURE(kAvoidEarlyExtensionScriptContextCreation);

// When enabled, then bad_message::ReceivedBadMessage will be called when
// browser receives an IPC from a content script and the IPC that unexpectedly
// claims to act on behalf of a given extension id, (i.e. even if the browser
// process things that renderer process never run content scripts from the
// extension).
BASE_DECLARE_FEATURE(kCheckingNoExtensionIdInExtensionIpcs);

// Determine if dynamic extension URLs are handled and redirected.
BASE_DECLARE_FEATURE(kExtensionDynamicURLRedirection);

// Side panel API availability.
BASE_DECLARE_FEATURE(kExtensionSidePanelIntegration);

// IsValidSourceUrl enforcement for ExtensionHostMsg_OpenChannelToExtension IPC.
BASE_DECLARE_FEATURE(kExtensionSourceUrlEnforcement);

// File Handlers.
BASE_DECLARE_FEATURE(kExtensionWebFileHandlers);

// If enabled, only manifest v3 extensions is allowed while v2 will be disabled.
// Note that this feature is now only checked by `ExtensionManagement` which
// represents enterprise extension configurations. Flip the feature will block
// mv2 extension by default but the error messages will improperly mention
// enterprise policy.
BASE_DECLARE_FEATURE(kExtensionsManifestV3Only);

// Enables enhanced site control for extensions and allowing the user to control
// site permissions.
BASE_DECLARE_FEATURE(kExtensionsMenuAccessControl);

// If enabled, user permitted sites are granted access. This should only happen
// if kExtensionsMenuAccessControl is enabled, since it's the only entry point
// where user could set permitted sites.
BASE_DECLARE_FEATURE(kExtensionsMenuAccessControlWithPermittedSites);

// Forces requests to go through WebRequestProxyingURLLoaderFactory.
BASE_DECLARE_FEATURE(kForceWebRequestProxyForTest);

// Launches Native Host executables directly on Windows rather than using a
// cmd.exe process as a proxy.
BASE_DECLARE_FEATURE(kLaunchWindowsNativeHostsDirectly);

// Controls whether extensions can use the new favicon fetching in Manifest V3.
BASE_DECLARE_FEATURE(kNewExtensionFaviconHandling);

// If enabled, allows APIs used by the webstore to be exposed on the URL for the
// new webstore.
BASE_DECLARE_FEATURE(kNewWebstoreDomain);

// To investigate signal beacon loss in crrev.com/c/2262402.
BASE_DECLARE_FEATURE(kReportKeepaliveUkm);

// When enabled, causes extensions to allow access to certain APIs only if the
// user is in the developer mode.
BASE_DECLARE_FEATURE(kRestrictDeveloperModeAPIs);

// Reports Extensions.WebRequest.KeepaliveRequestFinished when enabled.
// Automatically disable extensions not included in the Safe Browsing CRX
// allowlist if the user has turned on Enhanced Safe Browsing (ESB). The
// extensions can be disabled at ESB opt-in time or when an extension is moved
// out of the allowlist.
BASE_DECLARE_FEATURE(kSafeBrowsingCrxAllowlistAutoDisable);

// Controls whether we show an install friction dialog when an Enhanced Safe
// Browsing user tries to install an extension that is not included in the
// Safe Browsing CRX allowlist. This feature also controls if we show a warning
// in 'chrome://extensions' for extensions not included in the allowlist.
BASE_DECLARE_FEATURE(kSafeBrowsingCrxAllowlistShowWarnings);

// When enabled, causes Manifest V3 (and greater) extensions to use structured
// cloning (instead of JSON serialization) for extension messaging, except when
// communicating with native messaging hosts.
BASE_DECLARE_FEATURE(kStructuredCloningForMV3Messaging);

// If enabled, APIs of the Telemetry Extension platform that have pending
// approval will be enabled. Read more about the platform here:
// https://chromium.googlesource.com/chromium/src/+/master/docs/telemetry_extension/README.md.
BASE_DECLARE_FEATURE(kTelemetryExtensionPendingApprovalApi);

// If enabled, calling WebRequestEventRouter::Get will return an instance of the
// per-BrowserContext WebRequestEventRouter instead of the global singleton
// ExtensionWebRequestEventRouter.
BASE_DECLARE_FEATURE(kUsePerBrowserContextWebRequestEventRouter);

// Controls the <webview> tag behaviour changes proposed as part of the guest
// view MPArch migration. See
// https://docs.google.com/document/d/1RVbtvklXUg9QCNvMT0r-1qDwJNeQFGoTCOD1Ur9mDa4/edit?usp=sharing
// for details.
BASE_DECLARE_FEATURE(kWebviewTagMPArchBehavior);

///////////////////////////////////////////////////////////////////////////////
// STOP!
// Please don't just add your new feature down here.
// See the guidance at the top of this file.
///////////////////////////////////////////////////////////////////////////////

// If enabled, extensions installed from .zip files (from dev mode) are changed
// from installing in base::TEMP_DIR to .../<profile_dir>/UnpackedExtensions and
// persist until removed by the user.
BASE_DECLARE_FEATURE(kExtensionsZipFileInstalledInProfileDir);

// If enabled, extensions with service workers use an optimized event
// dispatching flow that does not start the worker for every event. It only
// starts a worker if it is not already running.
BASE_DECLARE_FEATURE(kExtensionsServiceWorkerOptimizedEventDispatch);

// If enabled, the button for visiting the chrome webstore in both the
// extensions menu in the app menu and the chrome://extensions sidebar will send
// the user to the new chrome webstore URL.
BASE_DECLARE_FEATURE(kNewWebstoreURL);

// Enables a relaxed rule count for "safe" dynqmic or session scoped rules above
// the current limit. If disabled, all dynamic and session scoped rules are
// treated as "safe" but the rule limit's value will be the stricter "unsafe"
// limit.
BASE_DECLARE_FEATURE(kDeclarativeNetRequestSafeRuleLimits);

// Enables declarative net request rules to specify response headers as a
// matching condition.
BASE_DECLARE_FEATURE(kDeclarativeNetRequestResponseHeaderMatching);

}  // namespace extensions_features

#endif  // EXTENSIONS_COMMON_EXTENSION_FEATURES_H_
