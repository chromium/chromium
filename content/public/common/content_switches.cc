// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_switches.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/media_buildflags.h"

namespace switches {

// By default, file:// URIs cannot read other file:// URIs. This is an
// override for developers who need the old behavior for testing.
const char kAllowFileAccessFromFiles[]      = "allow-file-access-from-files";

// Enables TLS/SSL errors on localhost to be ignored (no interstitial,
// no blocking of requests).
const char kAllowInsecureLocalhost[] = "allow-insecure-localhost";

// Allows loopback interface to be added in network list for peer connection.
const char kAllowLoopbackInPeerConnection[] =
    "allow-loopback-in-peer-connection";

// Allows plugins to be loaded in the command line for testing.
const char kAllowCommandLinePlugins[] = "allow-command-line-plugins";

// Causes the Attribution Report API to run without delays or noise.
const char kAttributionReportingDebugMode[] =
    "attribution-reporting-debug-mode";

// Bypasses the dialog prompting the user for permission to capture
// cameras and microphones. Useful in automatic tests of video-conferencing
// Web applications.
// This is nearly identical to kUseFakeUIForMediaStream, with the exception
// being that this flag does NOT affect screen-capture.
const char kAutoAcceptCameraAndMicrophoneCapture[] =
    "auto-accept-camera-and-microphone-capture";

// Causes the browser process to crash on startup.
const char kBrowserCrashTest[]              = "crash-test";

// Causes the browser process to display a dialog on launch.
const char kBrowserStartupDialog[]          = "browser-startup-dialog";

// Path to the exe to run for the renderer and plugin subprocesses.
const char kBrowserSubprocessPath[]         = "browser-subprocess-path";

// Tells whether the code is running browser tests (this changes the startup URL
// used by the content shell and also disables features that can make tests
// flaky [like monitoring of memory pressure]).
const char kBrowserTest[] = "browser-test";

// After a zygote forks a new process, change the stack canary. This switch is
// useful so not all forked processes use the same canary (a secret value),
// which can be vulnerable to information leaks and brute force attacks. See
// https://crbug.com/1206626.
// This requires that all functions on the stack at the time
// content::RunZygote() is called be compiled without stack canaries.
// Valid values are "enable" or "disable".
const char kChangeStackGuardOnFork[] = "change-stack-guard-on-fork";
const char kChangeStackGuardOnForkEnabled[] = "enable";
const char kChangeStackGuardOnForkDisabled[] = "disable";

// Disable antialiasing on 2d canvas.
const char kDisable2dCanvasAntialiasing[]   = "disable-canvas-aa";

// Disables Canvas2D rendering into a scanout buffer for overlay support.
const char kDisable2dCanvasImageChromium[] = "disable-2d-canvas-image-chromium";

// Disables client-visible 3D APIs, in particular WebGL and Pepper 3D.
// This is controlled by policy and is kept separate from the other
// enable/disable switches to avoid accidentally regressing the policy
// support for controlling access to these APIs.
const char kDisable3DAPIs[]                 = "disable-3d-apis";

// Disable gpu-accelerated 2d canvas.
const char kDisableAccelerated2dCanvas[]    = "disable-accelerated-2d-canvas";

// Enable in-progress canvas 2d API methods BeginLayer and EndLayer.
const char kEnableCanvas2DLayers[] = "canvas-2d-layers";

// Disables hardware acceleration of video decode, where available.
// Warning: do not remove or rename this flag, as it is used inside ChromeOS
// code to implement the DeviceHardwareVideoDecodingEnabled policy.
const char kDisableAcceleratedVideoDecode[] =
    "disable-accelerated-video-decode";

// Disables hardware acceleration of video encode, where available.
const char kDisableAcceleratedVideoEncode[] =
    "disable-accelerated-video-encode";

// Disable limits on the number of backing stores. Can prevent blinking for
// users with many windows/tabs and lots of memory.
const char kDisableBackingStoreLimit[]      = "disable-backing-store-limit";

// Disable backgrounding renders for occluded windows. Done for tests to avoid
// nondeterministic behavior.
const char kDisableBackgroundingOccludedWindowsForTesting[] =
    "disable-backgrounding-occluded-windows";

// Disable task throttling of timer tasks from background pages.
const char kDisableBackgroundTimerThrottling[] =
    "disable-background-timer-throttling";

// Disables the BackForwardCache feature.
const char kDisableBackForwardCache[] = "disable-back-forward-cache";

// Disable one or more Blink runtime-enabled features.
// Use names from runtime_enabled_features.json5, separated by commas.
// Applied after kEnableBlinkFeatures, and after other flags that change these
// features.
const char kDisableBlinkFeatures[]          = "disable-blink-features";

// Disables HTML5 DB support.
const char kDisableDatabases[]              = "disable-databases";

// Disable the per-domain blocking for 3D APIs after GPU reset.
// This switch is intended only for tests.
const char kDisableDomainBlockingFor3DAPIs[] =
    "disable-domain-blocking-for-3d-apis";

// Disables the in-process stack traces.
const char kDisableInProcessStackTraces[] = "disable-in-process-stack-traces";

// Disable all versions of WebGL.
const char kDisableWebGL[] = "disable-webgl";

// Disable WebGL2.
const char kDisableWebGL2[] = "disable-webgl2";

// Disable FileSystem API.
const char kDisableFileSystem[]             = "disable-file-system";

// Disable user gesture requirement for presentation.
const char kDisableGestureRequirementForPresentation[] =
    "disable-gesture-requirement-for-presentation";

// Disables GPU hardware acceleration.  If software renderer is not in place,
// then the GPU process won't launch.
const char kDisableGpu[]                    = "disable-gpu";

// Prevent the compositor from using its GPU implementation.
const char kDisableGpuCompositing[]         = "disable-gpu-compositing";

// Disable proactive early init of GPU process.
const char kDisableGpuEarlyInit[]           = "disable-gpu-early-init";

// Do not force that all compositor resources be backed by GPU memory buffers.
const char kDisableGpuMemoryBufferCompositorResources[] =
    "disable-gpu-memory-buffer-compositor-resources";

// Disable GpuMemoryBuffer backed VideoFrames.
const char kDisableGpuMemoryBufferVideoFrames[] =
    "disable-gpu-memory-buffer-video-frames";

// For tests, to disable the limit on the number of times the GPU process may be
// restarted.
const char kDisableGpuProcessCrashLimit[] = "disable-gpu-process-crash-limit";

// For tests, to disable falling back to software compositing if the GPU Process
// has crashed, and reached the GPU Process crash limit.
const char kDisableSoftwareCompositingFallback[] =
    "disable-software-compositing-fallback";

// Disable the thread that crashes the GPU process if it stops responding to
// messages.
const char kDisableGpuWatchdog[] = "disable-gpu-watchdog";

// Disables the IPC flooding protection.
// It is activated by default. Some javascript functions can be used to flood
// the browser process with IPC. This protection limits the rate at which they
// can be used.
const char kDisableIpcFloodingProtection[] = "disable-ipc-flooding-protection";

// Suppresses hang monitor dialogs in renderer processes.  This may allow slow
// unload handlers on a page to prevent the tab from closing, but the Task
// Manager can be used to terminate the offending process in this case.
const char kDisableHangMonitor[]            = "disable-hang-monitor";

// Disable the RenderThread's HistogramCustomizer.
const char kDisableHistogramCustomizer[]    = "disable-histogram-customizer";

// Don't kill a child process when it sends a bad IPC message.  Apart
// from testing, it is a bad idea from a security perspective to enable
// this switch.
const char kDisableKillAfterBadIPC[]        = "disable-kill-after-bad-ipc";

// Disables LCD text.
const char kDisableLCDText[]                = "disable-lcd-text";

// Disable LocalStorage.
const char kDisableLocalStorage[]           = "disable-local-storage";

// Force logging to be disabled.  Logging is enabled by default in debug
// builds.
const char kDisableLogging[]                = "disable-logging";

// Disables using CODECAPI_AVLowLatencyMode when creating DXVA decoders.
const char kDisableLowLatencyDxva[]         = "disable-low-latency-dxva";

// Disables Mojo broker capabilities in the browser during Mojo initialization.
const char kDisableMojoBroker[] = "disable-mojo-broker";

// Disables clearing the rendering output of a renderer when it didn't commit
// new output for a while after a top-frame navigation.
const char kDisableNewContentRenderingTimeout[] =
    "disable-new-content-rendering-timeout";

// Disables the Web Notification and the Push APIs.
const char kDisableNotifications[]          = "disable-notifications";

// Disable Pepper3D.
const char kDisablePepper3d[]               = "disable-pepper-3d";

// Disables compositor-accelerated touch-screen pinch gestures.
const char kDisablePinch[]                  = "disable-pinch";

// Disables the Presentation API.
const char kDisablePresentationAPI[]        = "disable-presentation-api";

// Disables throttling of history.pushState/replaceState calls.
const char kDisablePushStateThrottle[] = "disable-pushstate-throttle";

// Taints all <canvas> elements, regardless of origin.
const char kDisableReadingFromCanvas[]      = "disable-reading-from-canvas";

// Disables remote web font support. SVG font should always work whether this
// option is specified or not.
const char kDisableRemoteFonts[]            = "disable-remote-fonts";

// Disables the RemotePlayback API.
const char kDisableRemotePlaybackAPI[]      = "disable-remote-playback-api";

// Prevent renderer process backgrounding when set.
const char kDisableRendererBackgrounding[]  = "disable-renderer-backgrounding";

// Whether the ResourceScheduler is disabled.  Note this is only useful for C++
// Headless embedders who need to implement their own resource scheduling.
const char kDisableResourceScheduler[] = "disable-resource-scheduler";

// Disable shared workers.
const char kDisableSharedWorkers[]          = "disable-shared-workers";

// Do not use runtime-detected high-end CPU optimizations in Skia.  This is
// useful for forcing a baseline code path for e.g. web tests.
const char kDisableSkiaRuntimeOpts[]        = "disable-skia-runtime-opts";

// Disable smooth scrolling for testing.
const char kDisableSmoothScrolling[]        = "disable-smooth-scrolling";

// Disables the use of a 3D software rasterizer.
const char kDisableSoftwareRasterizer[]     = "disable-software-rasterizer";

// Disables the Web Speech API (both speech recognition and synthesis).
const char kDisableSpeechAPI[]              = "disable-speech-api";

// Disables the speech synthesis part of Web Speech API.
const char kDisableSpeechSynthesisAPI[]     = "disable-speech-synthesis-api";

// Disable multithreaded GPU compositing of web content.
const char kDisableThreadedCompositing[]    = "disable-threaded-compositing";

// Disable V8 idle tasks.
const char kDisableV8IdleTasks[]            = "disable-v8-idle-tasks";

// Disables WebGL rendering into a scanout buffer for overlay support.
const char kDisableWebGLImageChromium[]     = "disable-webgl-image-chromium";

// Don't enforce the same-origin policy; meant for website testing only.
// This switch has no effect unless --user-data-dir (as defined by the content
// embedder) is also present.
const char kDisableWebSecurity[]            = "disable-web-security";

// Disable the video decoder from drawing directly to a texture.
const char kDisableZeroCopyDxgiVideo[]      = "disable-zero-copy-dxgi-video";

// Specifies if the |DOMAutomationController| needs to be bound in the
// renderer. This binding happens on per-frame basis and hence can potentially
// be a performance bottleneck. One should only enable it when automating dom
// based tests.
const char kDomAutomationController[]       = "dom-automation";

// Disable antialiasing on 2d canvas clips
const char kDisable2dCanvasClipAntialiasing[] = "disable-2d-canvas-clip-aa";

// Disable YUV image decoding for those formats and cases where it's supported.
// Has no effect unless GPU rasterization is enabled.
const char kDisableYUVImageDecoding[] = "disable-yuv-image-decoding";

// Enables LCD text.
const char kEnableLCDText[]                 = "enable-lcd-text";

// Enable one or more Blink runtime-enabled features.
// Use names from runtime_enabled_features.json5, separated by commas.
// Applied before kDisableBlinkFeatures, and after other flags that change these
// features.
const char kEnableBlinkFeatures[]           = "enable-blink-features";

// Enable native caret browsing, in which a moveable cursor is placed on a web
// page, allowing a user to select and navigate through non-editable text using
// just a keyboard. See https://crbug.com/977390 for links to i2i.
const char kEnableCaretBrowsing[] = "enable-caret-browsing";

// Flag that turns on a group of experimental/newly added cookie-related
// features together, as a convenience for e.g. testing, to avoid having to set
// multiple switches individually which may be error-prone (not to mention
// tedious). There is not a corresponding switch to disable all these features,
// because that is discouraged, and for testing purposes you'd need to switch
// them off individually to identify the problematic feature anyway.
//
// At present this turns on:
//   net::features::kSameSiteDefaultChecksMethodRigorously
//   net::features::kCookieSameSiteConsidersRedirectChain
//   net::features::kEnablePortBoundCookies
//   net::features::kEnableSchemeBoundCookies
const char kEnableExperimentalCookieFeatures[] =
    "enable-experimental-cookie-features";

// Enables experimental WebAssembly features.
const char kEnableExperimentalWebAssemblyFeatures[] =
    "enable-experimental-webassembly-features";

// Enables Web Platform features that are in development.
const char kEnableExperimentalWebPlatformFeatures[] =
    "enable-experimental-web-platform-features";

// Enables blink runtime enabled features with status:"test" or
// status:"experimental", which are enabled when running web tests.
const char kEnableBlinkTestFeatures[] = "enable-blink-test-features";

// Disables all RuntimeEnabledFeatures that can be enabled via OriginTrials.
const char kDisableOriginTrialControlledBlinkFeatures[] =
    "disable-origin-trial-controlled-blink-features";

// Enable GpuMemoryBuffer backed VideoFrames.
const char kEnableGpuMemoryBufferVideoFrames[] =
    "enable-gpu-memory-buffer-video-frames";

// Enables Isolated Web Apps (IWAs) in a renderer process. There are two ways
// to enable the IWAs: by feature flag and by enterprise policy. If IWAs are
// enabled by any of the mentioned above ways then this flag is passed to
// the renderer process. This flag should not be used from command line.
// To enable IWAs from command line one should use kIsolatedWebApps feature
// flag.
const char kEnableIsolatedWebAppsInRenderer[] =
    "enable-isolated-web-apps-in-renderer";

// Force logging to be enabled.  Logging is disabled by default in release
// builds.
const char kEnableLogging[]                 = "enable-logging";

// Enables the type, downlinkMax attributes of the NetInfo API. Also, enables
// triggering of change attribute of the NetInfo API when there is a change in
// the connection type.
const char kEnableNetworkInformationDownlinkMax[] =
    "enable-network-information-downlink-max";

// Disables the video decoder from drawing to an NV12 textures instead of ARGB.
const char kDisableNv12DxgiVideo[] = "disable-nv12-dxgi-video";

// Enables testing features of the Plugin Placeholder. For internal use only.
const char kEnablePluginPlaceholderTesting[] =
    "enable-plugin-placeholder-testing";

// Make the values returned to window.performance.memory more granular and more
// up to date in shared worker. Without this flag, the memory information is
// still available, but it is bucketized and updated less frequently. This flag
// also applys to workers.
const char kEnablePreciseMemoryInfo[] = "enable-precise-memory-info";

// Enables Privacy Sandbox APIs: Attribution Reporting, Fledge, Topics, Fenced
// Frames, Shared Storage, Private Aggregation, and their associated features.
const char kEnablePrivacySandboxAdsApis[] = "enable-privacy-sandbox-ads-apis";

// Set options to cache V8 data. (none, code, or default)
const char kV8CacheOptions[] = "v8-cache-options";

// If true the ServiceProcessLauncher is used to launch services. This allows
// for service binaries to be loaded rather than using the utility process. This
// is only useful for tests.
const char kEnableServiceBinaryLauncher[] = "enable-service-binary-launcher";

// Enables the Skia benchmarking extension.
const char kEnableSkiaBenchmarking[]        = "enable-skia-benchmarking";

// On platforms that support it, enables smooth scroll animation.
const char kEnableSmoothScrolling[]         = "enable-smooth-scrolling";

// Enable spatial navigation
const char kEnableSpatialNavigation[]       = "enable-spatial-navigation";

// Blocks all insecure requests from secure contexts, and prevents the user
// from overriding that decision.
const char kEnableStrictMixedContentChecking[] =
    "enable-strict-mixed-content-checking";

// Blocks insecure usage of a number of powerful features (device orientation,
// for example) that we haven't yet deprecated for the web at large.
const char kEnableStrictPowerfulFeatureRestrictions[] =
    "enable-strict-powerful-feature-restrictions";

// When specified along with a value in the range (0,1] will --enable-tracing
// for (roughly) that percentage of tests being run. This is done in a stable
// manner such that the same tests are chosen each run, and under the assumption
// that tests hash equally across the range of possible values.
// The flag will enable all tracing categories for those tests, and none for the
// rest. This flag could be used with other tracing switches like
// --enable-tracing-format, but any other switches that will enable tracing will
// turn tracing on for all tests.
const char kEnableTracingFraction[] = "enable-tracing-fraction";

// Enable screen capturing support for MediaStream API.
const char kEnableUserMediaScreenCapturing[] =
    "enable-usermedia-screen-capturing";

// Enables the use of the @viewport CSS rule, which allows
// pages to control aspects of their own layout. This also turns on touch-screen
// pinch gestures.
const char kEnableViewport[]                = "enable-viewport";

// Enable the Vtune profiler support.
const char kEnableVtune[]                   = "enable-vtune-support";

// Enable the WebAuthn Mojo Testing API. This is a way to interact with the
// virtual authenticator environment through a mojo interface and is supported
// only to run web-platform-tests on content shell.
// Removal of this deprecated API is blocked on crbug.com/937369.
const char kEnableWebAuthDeprecatedMojoTestingApi[] =
    "enable-web-auth-deprecated-mojo-testing-api";

// Enables WebGL developer extensions which are not generally exposed
// to the web platform.
const char kEnableWebGLDeveloperExtensions[] =
    "enable-webgl-developer-extensions";

// Enables WebGL extensions not yet approved by the community.
const char kEnableWebGLDraftExtensions[] = "enable-webgl-draft-extensions";

// Enables WebGL rendering into a scanout buffer for overlay support.
const char kEnableWebGLImageChromium[] = "enable-webgl-image-chromium";

// Define an alias root directory which is replaced with the replacement string
// in file URLs. The format is "/alias=/replacement", which would turn
// file:///alias/some/path.html into file:///replacement/some/path.html.
const char kFileUrlPathAlias[] = "file-url-path-alias";

// This forces pages to be loaded as presentation receivers.  Useful for testing
// behavior specific to presentation receivers.
// Spec: https://www.w3.org/TR/presentation-api/#interface-presentationreceiver
const char kForcePresentationReceiverForTesting[] =
    "force-presentation-receiver-for-testing";

// Extra command line options for launching the GPU process (normally used
// for debugging). Use like renderer-cmd-prefix.
const char kGpuLauncher[]                   = "gpu-launcher";

// Makes this process a GPU sub-process.
const char kGpuProcess[]                    = "gpu-process";

// Starts the GPU sandbox before creating a GL context.
const char kGpuSandboxStartEarly[] = "gpu-sandbox-start-early";

// Causes the GPU process to display a dialog on launch.
const char kGpuStartupDialog[]              = "gpu-startup-dialog";

// Prevents creating scrollbars for web content. Useful for taking consistent
// screenshots.
const char kHideScrollbars[] = "hide-scrollbars";

// Run the GPU process as a thread in the browser process.
const char kInProcessGPU[]                  = "in-process-gpu";

// Overrides the timeout, in seconds, that a child process waits for a
// connection from the browser before killing itself.
const char kIPCConnectionTimeout[]          = "ipc-connection-timeout";

// Require dedicated processes for a set of origins, specified as a
// comma-separated list. For example:
//   --isolate-origins=https://www.foo.com,https://www.bar.com
const char kIsolateOrigins[] = "isolate-origins";

// Enables the web-facing behaviors that will enable origin-isolation by default
// at some point in the relatively near future.
//
// https://crbug.com/1140371
const char kIsolationByDefault[] = "isolation-by-default";

// Disable latest shipping ECMAScript 6 features.
const char kDisableJavaScriptHarmonyShipping[] =
    "disable-javascript-harmony-shipping";

// Enables experimental Harmony (ECMAScript 6) features.
const char kJavaScriptHarmony[]             = "javascript-harmony";

// Flag to launch tests in the browser process.
const char kLaunchAsBrowser[] = "as-browser";

// Logs GPU control list decisions when enforcing blocklist rules.
const char kLogGpuControlListDecisions[]    = "log-gpu-control-list-decisions";

// Sets the minimum log level. Valid values are from 0 to 3:
// INFO = 0, WARNING = 1, LOG_ERROR = 2, LOG_FATAL = 3.
const char kLoggingLevel[]                  = "log-level";

// Overrides the default file name to use for general-purpose logging (does not
// affect which events are logged).
const char kLogFile[] = "log-file";

// Log an error whenever the unload timeout for a render frame is exceeded.
const char kLogMissingUnloadACK[] = "log-missing-unload-ack";

// Allows user to override maximum number of active WebGL contexts per
// renderer process.
const char kMaxActiveWebGLContexts[] = "max-active-webgl-contexts";

// Sets the maximium decoded image size limitation.
const char kMaxDecodedImageSizeMb[] = "max-decoded-image-size-mb";

// Sets the maximum number of WebMediaPlayers allowed per frame.
const char kMaxWebMediaPlayerCount[] = "max-web-media-player-count";

// Indicates the utility process should run with a message loop type of UI.
const char kMessageLoopTypeUi[] = "message-loop-type-ui";

// Set the default result for MockCertVerifier. This only works in test code.
const char kMockCertVerifierDefaultResultForTesting[] =
    "mock-cert-verifier-default-result-for-testing";

// Use a Mojo-based LocalStorage implementation.
const char kMojoLocalStorage[]              = "mojo-local-storage";

// Disables the unsandboxed zygote.
// Note: this flag should not be used on most platforms. It is introduced
// because some platforms (e.g. Cast) have very limited memory and binaries
// won't be updated when the browser process is running.
const char kNoUnsandboxedZygote[] = "no-unsandboxed-zygote";

// Disables the use of a zygote process for forking child processes. Instead,
// child processes will be forked and exec'd directly. Note that --no-sandbox
// should also be used together with this flag because the sandbox needs the
// zygote to work.
const char kNoZygote[] = "no-zygote";

// Overrides the language detection result determined based on the page
// contents.
const char kOverrideLanguageDetection[] = "override-language-detection";

// Renderer process that runs the non-PPAPI PDF plugin.
const char kPdfRenderer[] = "pdf-renderer";

// Runs PPAPI (Pepper) plugins in-process.
const char kPpapiInProcess[]                = "ppapi-in-process";

// Specifies a command that should be used to launch the ppapi plugin process.
// Useful for running the plugin process through purify or quantify.  Ex:
//   --ppapi-plugin-launcher="path\to\purify /Run=yes"
const char kPpapiPluginLauncher[]           = "ppapi-plugin-launcher";

// Argument to the process type that indicates a PPAPI plugin process type.
const char kPpapiPluginProcess[]            = "ppapi";

// Causes the PPAPI sub process to display a dialog on launch. Be sure to use
// --no-sandbox as well or the sandbox won't allow the dialog to display.
const char kPpapiStartupDialog[]            = "ppapi-startup-dialog";

// Causes the Private Aggregation API to run without reporting delays.
const char kPrivateAggregationDeveloperMode[] =
    "private-aggregation-developer-mode";

// Enable the "Process Per Site" process model for all domains. This mode
// consolidates same-site pages so that they share a single process.
//
// More details here:
// - https://www.chromium.org/developers/design-documents/process-models
// - The class comment in site_instance.h, listing the supported process models.
//
// IMPORTANT: This isn't to be confused with --site-per-process (which is about
// isolation, not consolidation). You probably want the other one.
const char kProcessPerSite[]                = "process-per-site";

// Runs each set of script-connected tabs (i.e., a BrowsingInstance) in its own
// renderer process.  We default to using a renderer process for each
// site instance (i.e., group of pages from the same registered domain with
// script connections to each other).
// TODO(creis): This flag is currently a no-op.  We should refactor it to avoid
// "unnecessary" process swaps for cross-site navigations but still swap when
// needed for security (e.g., isolated origins).
const char kProcessPerTab[]                 = "process-per-tab";

// The value of this switch determines whether the process is started as a
// renderer or plugin host.  If it's empty, it's the browser.
const char kProcessType[]                   = "type";

// Causes Protected Audiences Bidding and Auction API to supply the provided
// debugging key to the trusted auction server. This tells the server that it
// okay to log information about this user's auction to help with debugging.
const char kProtectedAudiencesConsentedDebugToken[] =
    "protected-audiences-consented-debug-token";

// Enables or disables pull-to-refresh gesture in response to vertical
// overscroll.
// Set the value to '0' to disable the feature, set to '1' to enable it for both
// touchpad and touchscreen, and set to '2' to enable it only for touchscreen.
// Defaults to disabled.
const char kPullToRefresh[] = "pull-to-refresh";

// Specifies the minimum amount of time, in seconds, that must pass before
// consecutive quota change events can be fired. Set the value to '0' to disable
// the debounce mechanimsm.
const char kQuotaChangeEventInterval[] = "quota-change-event-interval";

// Reduce the accept-language http header, and only send one language in the
// request header: https://github.com/Tanych/accept-language.
const char kReduceAcceptLanguage[] = "reduce-accept-language";

// Reduce the minor version number in the User-Agent string. This flag
// implements phase 4 of User-Agent reduction:
// https://blog.chromium.org/2021/09/user-agent-reduction-origin-trial-and-dates.html.
const char kReduceUserAgentMinorVersion[] = "reduce-user-agent-minor-version";

// Reduce the platform and oscpu in the desktop User-Agent string. This flag
// implements phase 5 of User-Agent reduction:
// https://blog.chromium.org/2021/09/user-agent-reduction-origin-trial-and-dates.html.
const char kReduceUserAgentPlatformOsCpu[] = "reduce-user-agent-platform-oscpu";

// Register Pepper plugins (see pepper_plugin_list.cc for its format).
const char kRegisterPepperPlugins[]         = "register-pepper-plugins";

// Enables remote debug over stdio pipes [in=3, out=4] or over the remote pipes
// specified in the 'remote-debugging-io-pipes' switch.
// Optionally, specifies the format for the protocol messages, can be either
// "JSON" (the default) or "CBOR".
const char kRemoteDebuggingPipe[] = "remote-debugging-pipe";

// Enables remote debug over HTTP on the specified port.
const char kRemoteDebuggingPort[]           = "remote-debugging-port";

// Enables web socket connections from the specified origins only. '*' allows
// any origin.
const char kRemoteAllowOrigins[] = "remote-allow-origins";

const char kRendererClientId[] = "renderer-client-id";

// The contents of this flag are prepended to the renderer command line.
// Useful values might be "valgrind" or "xterm -e gdb --args".
const char kRendererCmdPrefix[]             = "renderer-cmd-prefix";

// Causes the process to run as renderer instead of as browser.
const char kRendererProcess[]               = "renderer";

// Time the browser launched the renderer process (in TimeTicks).
const char kRendererProcessLaunchTimeTicks[] = "launch-time-ticks";

// Overrides the default/calculated limit to the number of renderer processes.
// Very high values for this setting can lead to high memory/resource usage
// or instability.
const char kRendererProcessLimit[]          = "renderer-process-limit";

// Causes the renderer process to display a dialog on launch. Passing this flag
// also adds sandbox::policy::kNoSandbox on Windows non-official builds, since
// that's needed to show a dialog.
const char kRendererStartupDialog[]         = "renderer-startup-dialog";

// Manual tests only run when --run-manual is specified. This allows writing
// tests that don't run automatically but are still in the same test binary.
// This is useful so that a team that wants to run a few tests doesn't have to
// add a new binary that must be compiled on all builds.
const char kRunManualTestsFlag[] = "run-manual";

// Causes the process to run as a sandbox IPC subprocess.
const char kSandboxIPCProcess[]             = "sandbox-ipc";

// Enables shared array buffer on desktop, gated by an Enterprise Policy.
// TODO(crbug.com/40155376) Remove when migration to COOP+COEP is complete.
#if !BUILDFLAG(IS_ANDROID)
const char kSharedArrayBufferUnrestrictedAccessAllowed[] =
    "shared-array-buffer-unrestricted-access-allowed";
#endif

// Describes the file descriptors passed to a child process in the following
// list format:
//
//     <file_id>:<descriptor_id>,<file_id>:<descriptor_id>,...
//
// where <file_id> is an ID string from the manifest of the service being
// launched and <descriptor_id> is the numeric identifier of the descriptor for
// the child process can use to retrieve the file descriptor from the
// global descriptor table.
const char kSharedFiles[] = "shared-files";

// Runs the renderer and plugins in the same process as the browser
const char kSingleProcess[]                 = "single-process";

// Enforces a one-site-per-process security policy:
//  * Each renderer process, for its whole lifetime, is dedicated to rendering
//    pages for just one site.
//  * Thus, pages from different sites are never in the same process.
//  * A renderer process's access rights are restricted based on its site.
//  * All cross-site navigations force process swaps.
//  * <iframe>s are rendered out-of-process whenever the src= is cross-site.
//
// More details here:
// - https://www.chromium.org/developers/design-documents/site-isolation
// - https://www.chromium.org/developers/design-documents/process-models
// - The class comment in site_instance.h, listing the supported process models.
//
// IMPORTANT: this isn't to be confused with --process-per-site (which is about
// process consolidation, not isolation). You probably want this one.
const char kSitePerProcess[]                = "site-per-process";

// Disables site isolation.
//
// Note that the opt-in (to site-per-process, isolate-origins, etc.) via
// enterprise policy and/or cmdline takes precedence over the
// kDisableSiteIsolation switch (i.e. the opt-in takes effect despite potential
// presence of kDisableSiteIsolation switch).
//
// Note that for historic reasons the name of the switch misleadingly mentions
// "trials", but the switch also disables the default site isolation that ships
// on desktop since M67.  The name of the switch is preserved for
// backcompatibility of chrome://flags.
const char kDisableSiteIsolation[] = "disable-site-isolation-trials";

// Specifies if the browser should start in fullscreen mode, like if the user
// had pressed F11 right after startup.
const char kStartFullscreen[] = "start-fullscreen";

// Specifies if the |StatsCollectionController| needs to be bound in the
// renderer. This binding happens on per-frame basis and hence can potentially
// be a performance bottleneck. One should only enable it when running a test
// that needs to access the provided statistics.
const char kStatsCollectionController[] =
    "enable-stats-collection-bindings";

// Specifies the max number of bytes that should be used by the skia font cache.
// If the cache needs to allocate more, skia will purge previous entries.
const char kSkiaFontCacheLimitMb[] = "skia-font-cache-limit-mb";

// Specifies the max number of bytes that should be used by the skia resource
// cache. The previous entries are purged from the cache when the memory useage
// exceeds this limit.
const char kSkiaResourceCacheLimitMb[] = "skia-resource-cache-limit-mb";

// Type of the current test harness ("browser" or "ui" or "gpu").
const char kTestType[]                      = "test-type";

// Enable support for touch event feature detection.
const char kTouchEventFeatureDetection[] = "touch-events";

// The values the kTouchEventFeatureDetection switch may have, as in
// --touch-events=disabled.
//   auto: enabled at startup when an attached touchscreen is present.
const char kTouchEventFeatureDetectionAuto[] = "auto";
//   enabled: touch events always enabled.
const char kTouchEventFeatureDetectionEnabled[] = "enabled";
//   disabled: touch events are disabled.
const char kTouchEventFeatureDetectionDisabled[] = "disabled";

// Accepts a number representing the time-ticks value at the Unix epoch.
// Since different processes can produce a different value for this due to
// system clock changes, this allows synchronizing them to a single value.
const char kTimeTicksAtUnixEpoch[] = "time-ticks-at-unix-epoch";

// Replaces the existing codecs supported in peer connection with a single fake
// codec entry that create a fake video encoder and decoder.
const char kUseFakeCodecForPeerConnection[] =
    "use-fake-codec-for-peer-connection";

// Bypass the digital-identity-credential OS call. Simulate the user
// accepting the OS-presented dialog.
const char kUseFakeUIForDigitalIdentity[] = "use-fake-ui-for-digital-identity";

// Bypass the FedCM account selection dialog. If a value is provided for
// this switch, that account ID is selected, otherwise the first account
// is chosen.
const char kUseFakeUIForFedCM[] = "use-fake-ui-for-fedcm";

// Bypass the media stream infobar by selecting the default device for media
// streams (e.g. WebRTC). Works with --use-fake-device-for-media-stream.
// Prefer --auto-accept-camera-and-microphone-capture which does not interact
// with screen/tab capture.
const char kUseFakeUIForMediaStream[]     = "use-fake-ui-for-media-stream";

#if BUILDFLAG(IS_WIN)
// This will replace the existing font manager with SkiaFontManager in the
// renderer.
const char kUseSkiaFontManager[] = "use-skia-font-manager";
#endif

// Texture target for CHROMIUM_image backed video frame textures.
const char kVideoImageTextureTarget[] = "video-image-texture-target";

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(INCLUDE_BOTH_V8_SNAPSHOTS)
// Switch supplied to the renderer if the feature `kUseContextSnapshot` is
// enabled. A switch is used as at the time the renderer needs this information
// features have not yet been loaded.
const char kUseContextSnapshotSwitch[] = "use-context-snapshot";
#endif

// Set when Chromium should use a mobile user agent.
const char kUseMobileUserAgent[] = "use-mobile-user-agent";

// Use the MockCertVerifier. This only works in test code.
const char kUseMockCertVerifierForTesting[] =
    "use-mock-cert-verifier-for-testing";

// The contents of this flag are prepended to the utility process command line.
// Useful values might be "valgrind" or "xterm -e gdb --args".
const char kUtilityCmdPrefix[]              = "utility-cmd-prefix";

// Causes the process to run as a utility subprocess.
const char kUtilityProcess[]                = "utility";

// Causes the utility process to display a dialog on launch.
const char kUtilityStartupDialog[] = "utility-startup-dialog";

// This switch indicates the type of a utility process. It does not affect the
// services offered by the process, but is added to the command line for
// debugging and profiling purposes.
const char kUtilitySubType[] = "utility-sub-type";

// Causes tests to attempt to verify pixel output.
const char kVerifyPixels[] = "browser-ui-tests-verify-pixels";

// Will add kWaitForDebugger to every child processes. If a value is passed, it
// will be used as a filter to determine if the child process should have the
// kWaitForDebugger flag passed on or not.
const char kWaitForDebuggerChildren[]       = "wait-for-debugger-children";

// On every navigation a message with the renderer's URL will be logged and the
// renderer will wait for a debugger to be attached or SIGUSR1 to be sent to
// continue execution.
const char kWaitForDebuggerOnNavigation[] = "wait-for-debugger-on-navigation";

// Flag used by WebUI test runners to wait for debugger to be attached.
const char kWaitForDebuggerWebUI[] = "wait-for-debugger-webui";

// Allows trusted remote desktop clients to make WebAuthn requests on behalf of
// other origins. This switch only controls availability of the
// `remoteDesktopClientOverride` extension but doesn't by itself enable any
// origin to use it.
const char kWebAuthRemoteDesktopSupport[] = "webauthn-remote-desktop-support";

// Set the antialiasing method used for webgl. (none, explicit, implicit)
const char kWebglAntialiasingMode[] = "webgl-antialiasing-mode";

// Set a default sample count for webgl if msaa is enabled.
const char kWebglMSAASampleCount[] = "webgl-msaa-sample-count";

// The prefix used when starting the zygote process. (i.e. 'gdb --args')
const char kZygoteCmdPrefix[] = "zygote-cmd-prefix";

// Causes the process to run as a zygote.
const char kZygoteProcess[] = "zygote";

// Enables specified backend for the Web OTP API.
const char kWebOtpBackend[] = "web-otp-backend";

// Enables Sms Verification backend for Web OTP API which requires app hash in
// SMS body.
const char kWebOtpBackendSmsVerification[] = "web-otp-backend-sms-verification";

// Enables User Consent backend for Web OTP API.
const char kWebOtpBackendUserConsent[] = "web-otp-backend-user-consent";

// Enables auto backend selection for Web OTP API.
const char kWebOtpBackendAuto[] = "web-otp-backend-auto";

// Disables encryption of RTP Media for WebRTC. When Chrome embeds Content, it
// ignores this switch on its stable and beta channels.
const char kDisableWebRtcEncryption[]      = "disable-webrtc-encryption";

// Enables negotiation of encrypted header extensions from RFC 6904 for SRTP
// in WebRTC.
// See https://tools.ietf.org/html/rfc6904 for further information.
// TODO(crbug.com/40623740): Remove this.
const char kEnableWebRtcSrtpEncryptedHeaders[] =
    "enable-webrtc-srtp-encrypted-headers";

// Enforce IP Permission check. TODO(guoweis): Remove this once the feature is
// not under finch and becomes the default.
const char kEnforceWebRtcIPPermissionCheck[] =
    "enforce-webrtc-ip-permission-check";

// Override WebRTC IP handling policy to mimic the behavior when WebRTC IP
// handling policy is specified in Preferences.
const char kForceWebRtcIPHandlingPolicy[] = "force-webrtc-ip-handling-policy";

// Override the maximum framerate as can be specified in calls to getUserMedia.
// This flag expects a value.  Example: --max-gum-fps=17.5
const char kWebRtcMaxCaptureFramerate[] = "max-gum-fps";

// Enable capture and local storage of WebRTC event logs without visiting
// chrome://webrtc-internals. This is useful for automated testing. It accepts
// the path to which the local logs would be stored. Disabling is not possible
// without restarting the browser and relaunching without this flag.
const char kWebRtcLocalEventLogging[] = "webrtc-event-logging";

// This switch disables the ScrollToTextFragment feature.
const char kDisableScrollToTextFragment[] = "disable-scroll-to-text-fragment";

// Forcibly enable and select the specified runtime for webxr.
// Note that this provides an alternative means of enabling a runtime, and will
// also functionally disable all other runtimes.
const char kWebXrForceRuntime[] = "force-webxr-runtime";

// Tell WebXr to assume that it does not support any runtimes.
const char kWebXrRuntimeNone[] = "no-xr-runtime";

const char kWebXrRuntimeOrientationSensors[] = "orientation-sensors";

// The following are the runtimes that WebXr supports.
const char kWebXrRuntimeArCore[] = "arcore";
const char kWebXrRuntimeCardboard[] = "cardboard";
const char kWebXrRuntimeOpenXr[] = "openxr";

#if BUILDFLAG(IS_ANDROID)
// Disable Media Session API
const char kDisableMediaSessionAPI[] = "disable-media-session-api";

// Disable the locking feature of the screen orientation API.
const char kDisableScreenOrientationLock[]  = "disable-screen-orientation-lock";

// Just like kDisableSiteIsolation, but doesn't show the "stability and security
// will suffer" butter bar warning.
const char kDisableSiteIsolationForPolicy[] =
    "disable-site-isolation-for-policy";

// Disable timeouts that may cause the browser to die when running slowly. This
// is useful if running with profiling (such as debug malloc).
const char kDisableTimeoutsForProfiling[] = "disable-timeouts-for-profiling";

// Enable inverting of selection handles so that they are not clipped by the
// viewport boundaries.
const char kEnableAdaptiveSelectionHandleOrientation[] =
    "enable-adaptive-selection-handle-orientation";

// Enable drag manipulation of longpress-triggered text selections.
const char kEnableLongpressDragSelection[]  = "enable-longpress-drag-selection";

// Prevent the offline indicator from showing.
const char kForceOnlineConnectionStateForIndicator[] =
    "force-online-connection-state-for-indicator";

// Enables remote debug over HTTP on the specified socket name.
const char kRemoteDebuggingSocketName[]     = "remote-debugging-socket-name";

// Block ChildProcessMain thread of the renderer's ChildProcessService until a
// Java debugger is attached.
const char kRendererWaitForJavaDebugger[] = "renderer-wait-for-java-debugger";

// Disables debug crash dumps for OOPR.
const char kDisableOoprDebugCrashDump[] = "disable-oopr-debug-crash-dump";
#endif  // BUILDFLAG(IS_ANDROID)

// Enable the aggressive flushing of DOM Storage to minimize data loss.
const char kEnableAggressiveDOMStorageFlushing[] =
    "enable-aggressive-domstorage-flushing";

// Enable indication that browser is controlled by automation.
const char kEnableAutomation[] = "enable-automation";

#if BUILDFLAG(IS_IOS)
// For mobile devices, tests should include a viewport meta tag to specify page
// dimension adjustments. Omitting the tag can lead to automatic resizing to
// the standard mobile fallback size (980), which results in content shrinking
// as it first expands to 980, then scales down to 800 to fit the screen, as
// observed in the issue at https://crrev.com/c/4615623.
// This flag is intended for use in tests that do not include a viewport meta
// tag. When enabled, it ensures the viewport size matches the standard mobile
// fallback size, thereby helping to prevent content resizing in such tests.
const char kPreventResizingContentsForTesting[] =
    "prevent-resizing-contents-for-testing";
#endif

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
// Allows sending text-to-speech requests to speech-dispatcher, a common
// Linux speech service. Because it's buggy, the user must explicitly
// enable it so that visiting a random webpage can't cause instability.
const char kEnableSpeechDispatcher[] = "enable-speech-dispatcher";

// For lacros, we do not use environment variable to pass values. Instead we
// use a command line flag to pass the path to the device.
const char kLLVMProfileFile[] = "llvm-profile-file";
#endif

#if BUILDFLAG(IS_WIN)
// Device scale factor passed to certain processes like renderers, etc.
const char kDeviceScaleFactor[]     = "device-scale-factor";

// Disable the Legacy Window which corresponds to the size of the WebContents.
const char kDisableLegacyIntermediateWindow[] = "disable-legacy-window";

// DirectWrite FontCache is shared by browser to renderers using shared memory.
// This switch allows us to pass the shared memory handle to the renderer.
const char kFontCacheSharedHandle[] = "font-cache-shared-handle";

// The boolean value (0/1) of FontRenderParams::antialiasing to be passed to
// Ppapi processes.
const char kPpapiAntialiasedTextEnabled[] = "ppapi-antialiased-text-enabled";

// The enum value of FontRenderParams::subpixel_rendering to be passed to Ppapi
// processes.
const char kPpapiSubpixelRenderingSetting[] =
    "ppapi-subpixel-rendering-setting";

// Raise the timer interrupt frequency in all Chrome processes, for experimental
// purposes. This feature is needed because as of Windows 10 2004 the scheduling
// effects of changing the timer interrupt frequency are not global, and this
// lets us prove/disprove whether this matters. See https://crbug.com/1128917
const char kRaiseTimerFrequency[] = "raise-timer-frequency";

// Causes the second GPU process used for gpu info collection to display a
// dialog on launch.
const char kGpu2StartupDialog[] = "gpu2-startup-dialog";

// Use high priority for the audio process.
const char kAudioProcessHighPriority[] = "audio-process-high-priority";

// Specifies pipe names for the incoming and outbound messages on the Windows
// platform. This is a comma separated list of two pipe handles serialized as
// unsigned integers, e.g. "--remote-debugging-io-pipes=3,4".
const char kRemoteDebuggingIoPipes[] = "remote-debugging-io-pipes";
#endif

#if defined(ENABLE_IPC_FUZZER)
// Dumps IPC messages sent from renderer processes to the browser process to
// the given directory. Used primarily to gather samples for IPC fuzzing.
const char kIpcDumpDirectory[] = "ipc-dump-directory";

// Specifies the testcase used by the IPC fuzzer.
const char kIpcFuzzerTestcase[] = "ipc-fuzzer-testcase";
#endif

// Don't dump stuff here, follow the same order as the header.

}  // namespace switches
