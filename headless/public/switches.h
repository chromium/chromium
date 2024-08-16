// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_SWITCHES_H_
#define HEADLESS_PUBLIC_SWITCHES_H_

namespace headless {
namespace switches {

// All switches should be in alphabetical order.

// Specifies Accept-Language to send to servers and expose to JavaScript via the
// navigator.language DOM property. language[-country] where language is the 2
// letter code from ISO-639.
inline constexpr char kAcceptLang[] = "accept-lang";

// A comma-separated, case-insenitive list of video codecs to allow. If
// specified, codecs not matching the list will not be used. '*' will match
// everything,
// '-' at the start of an entry means codec is disallowed. First entry that
// matches determines the outcome. Codec names are as returned by
// `GetCodecName()` in media/base/video_codecs.cc
inline constexpr char kAllowVideoCodecs[] = "allow-video-codecs";

// Allowlist for Negotiate Auth servers.
inline constexpr char kAuthServerAllowlist[] = "auth-server-allowlist";

// If true, then all pop-ups and calls to window.open will fail.
inline constexpr char kBlockNewWebContents[] = "block-new-web-contents";

// The directory breakpad should store minidumps in.
inline constexpr char kCrashDumpsDir[] = "crash-dumps-dir";

// A meta flag. This sets a number of flags which put the browser into
// deterministic mode where begin frames should be issued over DevToolsProtocol
// (experimental).
inline constexpr char kDeterministicMode[] = "deterministic-mode";

// Whether cookies stored as part of user profile are encrypted.
inline constexpr char kDisableCookieEncryption[] = "disable-cookie-encryption";

// Disable crash reporter for headless. It is enabled by default in official
// builds.
inline constexpr char kDisableCrashReporter[] = "disable-crash-reporter";

// Disables lazy loading of images and frames.
inline constexpr char kDisableLazyLoading[] = "disable-lazy-loading";

// Use a specific disk cache location, rather than one derived from the
// UserDatadir.
inline constexpr char kDiskCacheDir[] = "disk-cache-dir";

// Enable Back Forward Cache support
inline constexpr char kEnableBackForwardCache[] = "enable-bfcache";

// Whether or not begin frames should be issued over DevToolsProtocol
// (experimental).
inline constexpr char kEnableBeginFrameControl[] = "enable-begin-frame-control";

// Enable crash reporter for headless.
inline constexpr char kEnableCrashReporter[] = "enable-crash-reporter";

// Enable hardware GPU support.
// Headless uses swiftshader by default for consistency across headless
// environments. This flag just turns forcing of swiftshader off and lets
// us revert to regular driver selection logic. Alternatively, specific
// drivers may be forced with --use-gl or --use-angle. Nethier approach
// guarantees that hardware GPU support will be enabled, as this is still
// conditional on headless having access to X display etc.
inline constexpr char kEnableGPU[] = "enable-gpu";

// Allows overriding the list of restricted ports by passing a comma-separated
// list of port numbers.
inline constexpr char kExplicitlyAllowedPorts[] = "explicitly-allowed-ports";

// Sets font render hinting when running headless, affects Skia rendering and
// whether glyph subpixel positioning is enabled.
// Possible values: none|slight|medium|full|max. Default: full.
inline constexpr char kFontRenderHinting[] = "font-render-hinting";

// Forces each navigation to use a new BrowsingInstance.
inline constexpr char kForceNewBrowsingInstance[] =
    "force-new-browsing-instance";

// Force reporting destination attested for headless shell.
inline constexpr char kForceReportingDestinationAttested[] =
    "force-reporting-destination-attested";

// Forces Incognito mode even if user data directory is specified using the
// --user-data-dir switch.
inline constexpr char kIncognito[] = "incognito";

// Do not use system proxy configuration service.
inline constexpr char kNoSystemProxyConfigService[] =
    "no-system-proxy-config-service";

// Specifies which encryption storage backend to use. Possible values are
// kwallet, kwallet5, gnome-libsecret, basic. Any other value will lead to
// Chrome detecting the best backend automatically.
// TODO(crbug.com/40449930): Once PasswordStore no longer uses KWallet for
// storing passwords, rename this flag to stop referencing passwords. Do not
// rename it sooner, though; developers and testers might rely on it keeping
// large amounts of testing passwords out of their KWallets.
inline constexpr char kPasswordStore[] = "password-store";

// Specifies a list of hosts for whom we bypass proxy settings and use direct
// connections. Ignored unless --proxy-server is also specified. This is a
// comma-separated list of bypass rules. See:
// "net/proxy_resolution/proxy_bypass_rules.h" for the format of these rules.
inline constexpr char kProxyBypassList[] = "proxy-bypass-list";

// Uses a specified proxy server, overrides system settings. This switch only
// affects HTTP and HTTPS requests.
inline constexpr char kProxyServer[] = "proxy-server";

// A string used to override the default user agent with a custom one.
inline constexpr char kUserAgent[] = "user-agent";

// Directory where the browser stores the user profile. Note that if this switch
// is added, the session will no longer be Incognito, unless Incognito mode is
// forced with --incognito switch.
inline constexpr char kUserDataDir[] = "user-data-dir";

// Prints version information and quits.
inline constexpr char kVersion[] = "version";

// Sets the initial window size. Provided as string in the format "800,600".
inline constexpr char kWindowSize[] = "window-size";

// No! Please don't just add your switches at the end of the list.
// Please maintain the switch list sorted.

}  // namespace switches
}  // namespace headless

#endif  // HEADLESS_PUBLIC_SWITCHES_H_
