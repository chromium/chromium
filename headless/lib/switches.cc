// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/switches.h"

namespace headless {
namespace switches {

// Specifies Accept-Language to send to servers and expose to JavaScript via the
// navigator.language DOM property. language[-country] where language is the 2
// letter code from ISO-639.
const char kAcceptLang[] = "accept-lang";

// Allowlist for Negotiate Auth servers.
const char kAuthServerAllowlist[] = "auth-server-allowlist";

// If true, then all pop-ups and calls to window.open will fail.
const char kBlockNewWebContents[] = "block-new-web-contents";

// The directory breakpad should store minidumps in.
const char kCrashDumpsDir[] = "crash-dumps-dir";

// A meta flag. This sets a number of flags which put the browser into
// deterministic mode where begin frames should be issued over DevToolsProtocol
// (experimental).
const char kDeterministicMode[] = "deterministic-mode";

// Disable crash reporter for headless. It is enabled by default in official
// builds.
const char kDisableCrashReporter[] = "disable-crash-reporter";

// Whether cookies stored as part of user profile are encrypted.
const char kDisableCookieEncryption[] = "disable-cookie-encryption";

// Disables lazy loading of images and frames.
const char kDisableLazyLoading[] = "disable-lazy-loading";

// Use a specific disk cache location, rather than one derived from the
// UserDatadir.
const char kDiskCacheDir[] = "disk-cache-dir";

// Whether or not begin frames should be issued over DevToolsProtocol
// (experimental).
const char kEnableBeginFrameControl[] = "enable-begin-frame-control";

// Enable crash reporter for headless.
const char kEnableCrashReporter[] = "enable-crash-reporter";

// Enable hardware GPU support.
// Headless uses swiftshader by default for consistency across headless
// environments. This flag just turns forcing of swiftshader off and lets
// us revert to regular driver selection logic. Alternatively, specific
// drivers may be forced with --use-gl or --use-angle. Nethier approach
// guarantees that hardware GPU support will be enabled, as this is still
// conditional on headless having access to X display etc.
const char kEnableGPU[] = "enable-gpu";

// Allows overriding the list of restricted ports by passing a comma-separated
// list of port numbers.
const char kExplicitlyAllowedPorts[] = "explicitly-allowed-ports";

// Sets font render hinting when running headless, affects Skia rendering and
// whether glyph subpixel positioning is enabled.
// Possible values: none|slight|medium|full|max. Default: full.
const char kFontRenderHinting[] = "font-render-hinting";

// Forces Incognito mode even if user data directory is specified using the
// --user-data-dir switch.
const char kIncognito[] = "incognito";

// Do not use system proxy configuration service.
const char kNoSystemProxyConfigService[] = "no-system-proxy-config-service";

// Specifies which encryption storage backend to use. Possible values are
// kwallet, kwallet5, gnome-libsecret, basic. Any other value will lead to
// Chrome detecting the best backend automatically.
// TODO(crbug.com/571003): Once PasswordStore no longer uses KWallet for
// storing passwords, rename this flag to stop referencing passwords. Do not
// rename it sooner, though; developers and testers might rely on it keeping
// large amounts of testing passwords out of their KWallets.
const char kPasswordStore[] = "password-store";

// Specifies a list of hosts for whom we bypass proxy settings and use direct
// connections. Ignored unless --proxy-server is also specified. This is a
// comma-separated list of bypass rules. See:
// "net/proxy_resolution/proxy_bypass_rules.h" for the format of these rules.
const char kProxyBypassList[] = "proxy-bypass-list";

// Uses a specified proxy server, overrides system settings. This switch only
// affects HTTP and HTTPS requests.
const char kProxyServer[] = "proxy-server";

// Use the given address instead of the default loopback for accepting remote
// debugging connections. Should be used together with --remote-debugging-port.
// Note that the remote debugging protocol does not perform any authentication,
// so exposing it too widely can be a security risk.
const char kRemoteDebuggingAddress[] = "remote-debugging-address";

// A string used to override the default user agent with a custom one.
const char kUserAgent[] = "user-agent";

// Directory where the browser stores the user profile. Note that if this switch
// is added, the session will no longer be Incognito, unless Incognito mode is
// forced with --incognito switch.
const char kUserDataDir[] = "user-data-dir";

// Sets the initial window size. Provided as string in the format "800,600".
const char kWindowSize[] = "window-size";

// No! Please don't just add your switches at the end of the list.
// Please maintain the switch list sorted.

}  // namespace switches
}  // namespace headless
