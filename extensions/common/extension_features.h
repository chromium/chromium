// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_FEATURES_H_
#define EXTENSIONS_COMMON_EXTENSION_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "extensions/buildflags/buildflags.h"

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
// * Example: --enable-features=Feature1,Feature2. Info: //base/feature_list.h.
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// API Features
///////////////////////////////////////////////////////////////////////////////

// NOTE(devlin): If there are consistently enough of these in flux, it might
// make sense to have their own file.

// Controls the availability of action.openPopup().
BASE_DECLARE_FEATURE(kApiActionOpenPopup);

// Controls the limit for alarms.create() API input.
BASE_DECLARE_FEATURE(kApiAlarmsCreateLengthLimit);

// Controls the availability of contentSettings.clipboard.
BASE_DECLARE_FEATURE(kApiContentSettingsClipboard);

// Controls the availability of the enterprise.kioskInput API.
BASE_DECLARE_FEATURE(kApiEnterpriseKioskInput);

// Controls the availability of registering public MIME handlers via
// the mimeHandler manifest key.
BASE_DECLARE_FEATURE(kApiMimeHandler);

// Controls the availability of the runtime.actionData API.
// TODO(crbug.com/376354347): Remove this when the experiment is finished.
BASE_DECLARE_FEATURE(kApiRuntimeActionData);

// Controls the availability of adding and removing site access requests with
// the permissions API.
BASE_DECLARE_FEATURE(kApiPermissionsHostAccessRequests);

// Controls the availability of executing user scripts programmatically using
// the userScripts API.
BASE_DECLARE_FEATURE(kApiUserScriptsExecute);

// Controls the availability of specifying different world IDs in the
// userScripts API.
BASE_DECLARE_FEATURE(kApiUserScriptsMultipleWorlds);

// Controls the availability of the odfsConfigPrivate API.
BASE_DECLARE_FEATURE(kApiOdfsConfigPrivate);

// Controls the availability of the glicPrivate API.
BASE_DECLARE_FEATURE(kApiGlicPrivate);

// Controls the availability of the
// `enterprise.reportingPrivate.onDataMaskingRulesTriggered` API.
BASE_DECLARE_FEATURE(kApiEnterpriseReportingPrivateOnDataMaskingRulesTriggered);

// Controls the availability of Glic access from Google webpages.
BASE_DECLARE_FEATURE(kApiGlicAccessFromGoogleWebpage);
// Controls the availability of Glic access from Chrome promotion pages.
BASE_DECLARE_FEATURE(kApiGlicAccessFromPromotionPage);
extern const base::FeatureParam<std::string> kProdPromptEndpointUrlParam;
extern const base::FeatureParam<std::string> kGlicInvokeApiOAuth2ScopeParam;
extern const base::FeatureParam<bool> kGlicRequireConsentForInvokeParam;

enum class GlicOpenNewTabDisposition {
  kForeground,                // Always open in foreground.
  kBackground,                // Always open in background.
  kForegroundIfNotConsented,  // Open in foreground if user has not consented,
                              // else in background.
};
extern const base::FeatureParam<GlicOpenNewTabDisposition>
    kGlicOpenNewTabDispositionParam;

// String constants for GlicOpenNewTabDisposition.
inline constexpr char kGlicOpenNewTabDispositionForeground[] = "foreground";
inline constexpr char kGlicOpenNewTabDispositionBackground[] = "background";
inline constexpr char kGlicOpenNewTabDispositionForegroundIfNotConsented[] =
    "foreground_if_not_consented";

// Controls the availability of the new `proxyOverrideRulesPrivate` API.
BASE_DECLARE_FEATURE(kApiProxyOverrideRulesPrivate);

// Controls the availability of the deprecated nacl_arch in
// runtime.getPlatformInfo() API.
BASE_DECLARE_FEATURE(kApiRuntimeGetPlatformInfoNaClArch);

///////////////////////////////////////////////////////////////////////////////
// Other Features
///////////////////////////////////////////////////////////////////////////////

// For historical reasons, this includes some APIs. Please don't add more APIs.

// Enables the UI in the install prompt which lets a user choose to withhold
// requested host permissions by default.
BASE_DECLARE_FEATURE(kAllowWithholdingExtensionPermissionsOnInstall);

// When enabled, then bad_message::ReceivedBadMessage will be called when
// browser receives an IPC from a content script and the IPC that unexpectedly
// claims to act on behalf of a given extension id, (i.e. even if the browser
// process things that renderer process never run content scripts from the
// extension).
BASE_DECLARE_FEATURE(kCheckingNoExtensionIdInExtensionIpcs);

// Controls whether component extensions are allowed to use chrome://resources/
// URLs in worker scripts and subresources.
BASE_DECLARE_FEATURE(kComponentExtensionAllowWorkerChromeResources);

// If enabled, <webview>s will be allowed to request permission from an
// embedding Chrome App to request access to Human Interface Devices.
BASE_DECLARE_FEATURE(kEnableWebHidInWebView);

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
// If enabled, extensions will be enabled for @google.com and @managedchrome.com
// users on desktop Android. Otherwise they will be blocked.
BASE_DECLARE_FEATURE(kEnableExtensionsForCorpDesktopAndroid);
#endif

// If enabled, JS content scripts injected at document start will be compiled
// in a background thread.
BASE_DECLARE_FEATURE(kExtensionsBackgroundCompilation);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kBackgroundCompilationTimeout);
BASE_DECLARE_FEATURE_PARAM(size_t, kMinScriptSizeForBackgroundCompilation);
BASE_DECLARE_FEATURE_PARAM(size_t, kMaxScriptSizeForBackgroundCompilation);

// If enabled, disables unpacked extensions if developer mode is off.
BASE_DECLARE_FEATURE(kExtensionDisableUnsupportedDeveloper);

// Allow e.g. .css files to use default_locale messages in WAR files via GUID.
// TODO(crbug.com/435609878): Remove after m142. It's for safe m141 back merge.
BASE_DECLARE_FEATURE(kExtensionLocalizationGuid);

// A replacement key for declaring icons, in addition to supporting dark mode.
BASE_DECLARE_FEATURE(kExtensionIconVariants);

// A feature to allow legacy MV2 extensions, even if they are not supported by
// the browser or experiment configuration. This is important to allow
// developers of MV2 extensions to continue loading, running, and testing their
// extensions for as long as MV2 is supported in any variant.
// TODO(https://crbug.com/431097630): Remove this feature.
BASE_DECLARE_FEATURE(kAllowLegacyMV2Extensions);

// If enabled, allows an extension to specify protocol_handlers keys in the
// Manifest, registering a group of custom handlers so that the browser can
// handle navigation requests to URLs with unknown schemes. This feature
// provides similar behavior and capabilities than the one implemented by
// the 'registerProtocolHandler' Web API, defined in the Custom Handlers
// section of the HTML specification.
BASE_DECLARE_FEATURE(kExtensionProtocolHandlers);

// Enables extension support for the "tab" context menu, allowing extensions
// to add custom items when right-clicking a tab.
BASE_DECLARE_FEATURE(kExtensionTabContextMenu);

// Enables enhanced site control for extensions and allowing the user to control
// site permissions.
BASE_DECLARE_FEATURE(kExtensionsMenuAccessControl);

// If enabled, user permitted sites are granted access. This should only happen
// if kExtensionsMenuAccessControl is enabled, since it's the only entry point
// where user could set permitted sites.
BASE_DECLARE_FEATURE(kExtensionsMenuAccessControlWithPermittedSites);

// If enabled, guide users with zero extensions installed to explore the
// benefits of extensions.
// Displays an IPH anchored to the Extensions Toolbar Button, and replaces the
// extensions submenu with an alternative submenu to recommend extensions.
BASE_DECLARE_FEATURE(kExtensionsToolbarZeroState);

// Retries starting a service worker if it fails with a transient error.
BASE_DECLARE_FEATURE(kExtensionsServiceWorkerStartRetry);

// Forces requests to go through WebRequestProxyingURLLoaderFactory.
BASE_DECLARE_FEATURE(kForceWebRequestProxyForTest);

// Launches Native Host executables directly on Windows rather than using a
// cmd.exe process as a proxy.
BASE_DECLARE_FEATURE(kLaunchWindowsNativeHostsDirectly);

// Controls whether omnibox extensions can use the new capability to intercept
// input without needing keyword mode.
BASE_DECLARE_FEATURE(kExperimentalOmniboxLabs);

// Reports Extensions.WebRequest.KeepaliveRequestFinished when enabled.
// Automatically disable extensions not included in the Safe Browsing CRX
// allowlist if the user has turned on Enhanced Safe Browsing (ESB). The
// extensions can be disabled at ESB opt-in time or when an extension is moved
// out of the allowlist.
BASE_DECLARE_FEATURE(kSafeBrowsingCrxAllowlistAutoDisable);

// When enabled, cause extensions to use structured cloning (instead of JSON
// serialization) for extension messaging, except when communicating with native
// messaging hosts.
BASE_DECLARE_FEATURE(kStructuredCloningForMessaging);

// Controls whether the component webstore hosted app is loaded.
BASE_DECLARE_FEATURE(kWebstoreHostedApp);

// Used to control whether downloads initiated by `WebstoreInstaller` are marked
// as having a corresponding user gesture or not.
BASE_DECLARE_FEATURE(kWebstoreInstallerUserGestureKillSwitch);

///////////////////////////////////////////////////////////////////////////////
// STOP!
// Please don't just add your new feature down here.
// See the guidance at the top of this file.
///////////////////////////////////////////////////////////////////////////////

// Enables declarative net request rules to specify response headers as a
// matching condition.
BASE_DECLARE_FEATURE(kDeclarativeNetRequestResponseHeaderMatching);

// Enables a relaxed rule count for "safe" dynamic or session scoped rules above
// the current limit. If disabled, all dynamic and session scoped rules are
// treated as "safe" but the rule limit's value will be the stricter "unsafe"
// limit.
BASE_DECLARE_FEATURE(kDeclarativeNetRequestSafeRuleLimits);

// If enabled, include JS call stack data in the extension API request
// sent to the browser process. This data is used for telemetry purpose
// only.
BASE_DECLARE_FEATURE(kIncludeJSCallStackInExtensionApiRequest);

// If enabled, use the new CWS itemSnippets API to fetch extension info.
BASE_DECLARE_FEATURE(kUseItemSnippetsAPI);

// If enabled, use the new simpler, more efficient service worker task queue.
BASE_DECLARE_FEATURE(kUseNewServiceWorkerTaskQueue);

// Enables declarative net request rules to specify a header substitution action
// type for modifying headers.
BASE_DECLARE_FEATURE(kDeclarativeNetRequestHeaderSubstitution);

// Disables loading extensions via the `--disable-extensions-except` command
// line switch.
BASE_DECLARE_FEATURE(kDisableDisableExtensionsExceptCommandLineSwitch);

// Disables the `--extensions-on-chrome-urls` flag's functionality on
// `chrome://` URLs. Extension can still run on extension URLs using the new
// flag `--extensions-on-extension-urls` flag.
BASE_DECLARE_FEATURE(kDisableExtensionsOnChromeUrlsSwitch);

// If enabled, high-risk extension DOM activity is collected and reported
// for enterprise auditing.
BASE_DECLARE_FEATURE(kEnterpriseExtensionDOMActivityTelemetry);

// Forces the debugger API/feature to always be restricted by developer mode.
// This ensures we're always testing the developer mode API/feature restriction
// capability, even when no other API/feature might be restricted by it.
BASE_DECLARE_FEATURE(kDebuggerAPIRestrictedToDevMode);

// Creates a `browser` object that can be used in place of `chrome` where
// extension APIs are available. It does not include non-extension APIs like
// `loadTimes`, `csi`, etc. or deprecated APIs (e.g. `app`).
// Also aligns one-time message (e.g. runtime.sendMessage) behavior more closely
// with the mozilla/webextension-polyfill. This includes supporting
// chrome.runtime.onMessage() listeners returning a Promise. Also in more error
// cases (like listeners sending unserializable responses or throwing errors
// during execution) the error is passed back to the sender.
BASE_DECLARE_FEATURE(kExtensionBrowserNamespaceAndPolyfillSupport);

// When enabled, a call to base::ListValue::Clone is avoided when dispatching an
// extension function. Behind a feature to assess impact
// (go/chrome-performance-work-should-be-finched).
// TODO(crbug.com/424432184): Clean up when experiment is complete.
BASE_DECLARE_FEATURE(kAvoidCloneArgsOnExtensionFunctionDispatch);

// If enabled, the ContentVerifier cache key will include the extension root
// path. This prevents collisions when an extension is updated or reloaded
// to a new directory while keeping the same version ID.
// This also controls content verifier behavior when starting new
// ContentVerifyJobs: it will ensure that only jobs matching the currently
// loaded extension's root directory are allowed to start. This helps avoid
// memory leaks from stale cache entries and false-positive corruption reports.
BASE_DECLARE_FEATURE(kExtensionContentVerificationUsesExtensionRoot);

// Enables the shouldShowPromotion API to determine which promotion to show for
// Chrome Enterprise on CWS.
BASE_DECLARE_FEATURE(kEnableShouldShowPromotion);

// When enabled, web searches with a newly-installed search engine-changing
// extension will be blocked behind a new explicit-choice dialog. The dialog
// must be used to confirm the choice of using the new search engine, or
// returning to the previous provider.
BASE_DECLARE_FEATURE(kSearchEngineExplicitChoiceDialog);
BASE_DECLARE_FEATURE_PARAM(bool, kSearchEngineExplicitChoiceDialogEscapable);

// If true, the dialog is re-shown until a choice is made. If false, the
// dialog is limited to once per session, as the original dialog works.
BASE_DECLARE_FEATURE_PARAM(bool,
                           kSearchEngineExplicitChoiceDialogUnlimitedShows);

// When enabled, all search extensions will unconditionally get the search
// engine override dialog.
BASE_DECLARE_FEATURE(kSearchEngineUnconditionalDialog);

// Enables the securityInfo in chrome.webRequest API for extensions.
// Allowing them to retrieve certificate information from web requests.
BASE_DECLARE_FEATURE(kWebRequestSecurityInfo);

// When enabled, optimizes WebRequest proxying by strictly limiting it to
// requests that are subject to interception. This ensures that the 'webview'
// permission only triggers proxying for its own guest frames (e.g., <webview>
// or Controlled Frame), rather than globally proxying all requests. This
// avoids unnecessary performance overhead and restores navigation
// optimizations like preconnect.
BASE_DECLARE_FEATURE(kOptimizeWebRequestProxy);

}  // namespace extensions_features

#endif  // EXTENSIONS_COMMON_EXTENSION_FEATURES_H_
