// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_switches.h"

#include "build/build_config.h"
#include "media/media_buildflags.h"

namespace switches {

// The number of MSAA samples for canvas2D. Requires MSAA support by GPU to
// have an effect. 0 disables MSAA.
const char kAcceleratedCanvas2dMSAASampleCount[] = "canvas-msaa-sample-count";

// By default, file:// URIs cannot read other file:// URIs. This is an
// override for developers who need the old behavior for testing.
const char kAllowFileAccessFromFiles[]      = "allow-file-access-from-files";

// Enables TLS/SSL errors on localhost to be ignored (no interstitial,
// no blocking of requests).
const char kAllowInsecureLocalhost[] = "allow-insecure-localhost";

// Allows loopback interface to be added in network list for peer connection.
const char kAllowLoopbackInPeerConnection[] =
    "allow-loopback-in-peer-connection";

// Uses the android SkFontManager on linux. The specified directory should
// include the configuration xml file with the name "fonts.xml".
// This is used in blimp to emulate android fonts on linux.
const char kAndroidFontsPath[]          = "android-fonts-path";

// Set blink settings. Format is <name>[=<value],<name>[=<value>],...
// The names are declared in Settings.json5. For boolean type, use "true",
// "false", or omit '=<value>' part to set to true. For enum type, use the int
// value of the enum value. Applied after other command line flags and prefs.
const char kBlinkSettings[]                 = "blink-settings";

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

// Sets the tile size used by composited layers.
const char kDefaultTileWidth[]              = "default-tile-width";
const char kDefaultTileHeight[]             = "default-tile-height";

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

// Disables hardware acceleration of video decode, where available.
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

// Disable one or more Blink runtime-enabled features.
// Use names from runtime_enabled_features.json5, separated by commas.
// Applied after kEnableBlinkFeatures, and after other flags that change these
// features.
const char kDisableBlinkFeatures[]          = "disable-blink-features";

// Disables compositor Ukm recording in browser tests.
// TODO(khushalsagar): Remove once crbug.com/761524 is resolved.
const char kDisableCompositorUkmForTests[] = "disable-compositor-ukm-for-tests";

// Disables HTML5 DB support.
const char kDisableDatabases[]              = "disable-databases";

// Disable the per-domain blocking for 3D APIs after GPU reset.
// This switch is intended only for tests.
const char kDisableDomainBlockingFor3DAPIs[] =
    "disable-domain-blocking-for-3d-apis";

// Disable all versions of WebGL.
const char kDisableWebGL[] = "disable-webgl";

// Disable WebGL2.
const char kDisableWebGL2[] = "disable-webgl2";

// Disable FileSystem API.
const char kDisableFileSystem[]             = "disable-file-system";

// Disable 3D inside of flapper.
const char kDisableFlash3d[]                = "disable-flash-3d";

// Disable Stage3D inside of flapper.
const char kDisableFlashStage3d[]           = "disable-flash-stage3d";

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

// When using CPU rasterizing disable low resolution tiling. This uses
// less power, particularly during animations, but more white may be seen
// during fast scrolling especially on slower devices.
const char kDisableLowResTiling[] = "disable-low-res-tiling";

// Disable the thread that crashes the GPU process if it stops responding to
// messages.
const char kDisableGpuWatchdog[] = "disable-gpu-watchdog";

// Disallow image animations to be reset to the beginning to avoid skipping
// many frames. Only effective if compositor image animations are enabled.
const char kDisableImageAnimationResync[] = "disable-image-animation-resync";

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

// Disables clearing the rendering output of a renderer when it didn't commit
// new output for a while after a top-frame navigation.
const char kDisableNewContentRenderingTimeout[] =
    "disable-new-content-rendering-timeout";

// Disables the Web Notification and the Push APIs.
const char kDisableNotifications[]          = "disable-notifications";

// Disable partial raster in the renderer. Disabling this switch also disables
// the use of persistent gpu memory buffers.
const char kDisablePartialRaster[] = "disable-partial-raster";

// Enable partial raster in the renderer.
const char kEnablePartialRaster[] = "enable-partial-raster";

// Disable Pepper3D.
const char kDisablePepper3d[]               = "disable-pepper-3d";

// Disables the Permissions API.
const char kDisablePermissionsAPI[]         = "disable-permissions-api";

// Disable Image Chromium for Pepper 3d.
const char kDisablePepper3DImageChromium[] = "disable-pepper-3d-image-chromium";

// Disables compositor-accelerated touch-screen pinch gestures.
const char kDisablePinch[]                  = "disable-pinch";

// Disable the creation of compositing layers when it would prevent LCD text.
const char kDisablePreferCompositingToLCDText[] =
    "disable-prefer-compositing-to-lcd-text";

// Disables the Presentation API.
const char kDisablePresentationAPI[]        = "disable-presentation-api";

// Disables throttling of history.pushState/replaceState calls.
const char kDisablePushStateThrottle[] = "disable-pushstate-throttle";

// Disables RGBA_4444 textures.
const char kDisableRGBA4444Textures[]       = "disable-rgba-4444-textures";

// Taints all <canvas> elements, regardless of origin.
const char kDisableReadingFromCanvas[]      = "disable-reading-from-canvas";

// Disables remote web font support. SVG font should always work whether this
// option is specified or not.
const char kDisableRemoteFonts[]            = "disable-remote-fonts";

// Disables the RemotePlayback API.
const char kDisableRemotePlaybackAPI[]      = "disable-remote-playback-api";

// Turns off the accessibility in the renderer.
const char kDisableRendererAccessibility[]  = "disable-renderer-accessibility";

// Prevent renderer process backgrounding when set.
const char kDisableRendererBackgrounding[]  = "disable-renderer-backgrounding";

// Whether the ResourceScheduler is disabled.  Note this is only useful for C++
// Headless embedders who need to implement their own resource scheduling.
const char kDisableResourceScheduler[] = "disable-resource-scheduler";

// Disable shared workers.
const char kDisableSharedWorkers[]          = "disable-shared-workers";

// Do not use runtime-detected high-end CPU optimizations in Skia.  This is
// useful for forcing a baseline code path for e.g. layout tests.
const char kDisableSkiaRuntimeOpts[]        = "disable-skia-runtime-opts";

// Disable smooth scrolling for testing.
const char kDisableSmoothScrolling[]        = "disable-smooth-scrolling";

// Disables the use of a 3D software rasterizer.
const char kDisableSoftwareRasterizer[]     = "disable-software-rasterizer";

// Disables the Web Speech API.
const char kDisableSpeechAPI[]              = "disable-speech-api";

// Disables adding the test certs in the network process.
const char kDisableTestCerts[] = "disable-test-root-certs";

// Disable multithreaded GPU compositing of web content.
const char kDisableThreadedCompositing[]     = "disable-threaded-compositing";

// Disable multithreaded, compositor scrolling of web content.
const char kDisableThreadedScrolling[]      = "disable-threaded-scrolling";

// Disable V8 idle tasks.
const char kDisableV8IdleTasks[]            = "disable-v8-idle-tasks";

// Disables WebGL rendering into a scanout buffer for overlay support.
const char kDisableWebGLImageChromium[]     = "disable-webgl-image-chromium";

// Don't enforce the same-origin policy. (Used by people testing their sites.)
const char kDisableWebSecurity[]            = "disable-web-security";

// Disables Blink's XSSAuditor. The XSSAuditor mitigates reflective XSS.
const char kDisableXSSAuditor[]             = "disable-xss-auditor";

// Disable rasterizer that writes directly to GPU memory associated with tiles.
const char kDisableZeroCopy[]                = "disable-zero-copy";

// Disable the video decoder from drawing directly to a texture.
const char kDisableZeroCopyDxgiVideo[]      = "disable-zero-copy-dxgi-video";

// Specifies if the |DOMAutomationController| needs to be bound in the
// renderer. This binding happens on per-frame basis and hence can potentially
// be a performance bottleneck. One should only enable it when automating dom
// based tests.
const char kDomAutomationController[]       = "dom-automation";

// Disable antialiasing on 2d canvas clips
const char kDisable2dCanvasClipAntialiasing[] = "disable-2d-canvas-clip-aa";

// Disable partially decoding jpeg images using the GPU.
// At least YUV decoding will be accelerated when not using this flag.
// Has no effect unless GPU rasterization is enabled.
const char kDisableAcceleratedJpegDecoding[] =
    "disable-accelerated-jpeg-decoding";

// Logs Runtime Call Stats for Blink. --single-process also needs to be
// used along with this for the stats to be logged.
const char kDumpBlinkRuntimeCallStats[] = "dump-blink-runtime-call-stats";

// Enables LCD text.
const char kEnableLCDText[]                 = "enable-lcd-text";

// Enable the creation of compositing layers when it would prevent LCD text.
const char kEnablePreferCompositingToLCDText[] =
    "enable-prefer-compositing-to-lcd-text";

// Enable one or more Blink runtime-enabled features.
// Use names from runtime_enabled_features.json5, separated by commas.
// Applied before kDisableBlinkFeatures, and after other flags that change these
// features.
const char kEnableBlinkFeatures[]           = "enable-blink-features";

// This is now an alias of "--enable-blink-features=BlinkGenPropertyTrees".
// TODO(pdr): This flag is redundant and should be removed.
const char kEnableBlinkGenPropertyTrees[] = "enable-blink-gen-property-trees";

// Enables Web Platform features that are in development.
const char kEnableExperimentalWebPlatformFeatures[] =
    "enable-experimental-web-platform-features";

// Disables all RuntimeEnabledFeatures that can be enabled via OriginTrials.
const char kDisableOriginTrialControlledBlinkFeatures[] =
    "disable-origin-trial-controlled-blink-features";

// Specify that all compositor resources should be backed by GPU memory buffers.
const char kEnableGpuMemoryBufferCompositorResources[] =
    "enable-gpu-memory-buffer-compositor-resources";

// Enable GpuMemoryBuffer backed VideoFrames.
const char kEnableGpuMemoryBufferVideoFrames[] =
    "enable-gpu-memory-buffer-video-frames";

// When using CPU rasterizing generate low resolution tiling. Low res
// tiles may be displayed during fast scrolls especially on slower devices.
const char kEnableLowResTiling[] = "enable-low-res-tiling";

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

// Enables compositor-accelerated touch-screen pinch gestures.
const char kEnablePinch[]                   = "enable-pinch";

// Enables testing features of the Plugin Placeholder. For internal use only.
const char kEnablePluginPlaceholderTesting[] =
    "enable-plugin-placeholder-testing";

// Make the values returned to window.performance.memory more granular and more
// up to date in shared worker. Without this flag, the memory information is
// still available, but it is bucketized and updated less frequently. This flag
// also applys to workers.
const char kEnablePreciseMemoryInfo[] = "enable-precise-memory-info";

// Enables PrintBrowser mode, in which everything renders as though printed.
const char kEnablePrintBrowser[] = "enable-print-browser";

// Enables RGBA_4444 textures.
const char kEnableRGBA4444Textures[] = "enable-rgba-4444-textures";

// Set options to cache V8 data. (off, preparse data, or code)
const char kV8CacheOptions[] = "v8-cache-options";

// If true the ServiceProcessLauncher is used to launch services. This allows
// for service binaries to be loaded rather than using the utility process. This
// is only useful for tests.
const char kEnableServiceBinaryLauncher[] = "enable-service-binary-launcher";

// Enables the Skia benchmarking extension
const char kEnableSkiaBenchmarking[]        = "enable-skia-benchmarking";

// Enables slimming paint phase 2: https://www.chromium.org/blink/slimming-paint
// This is now an alias of "--enable-blink-features=SlimmingPaintV2".
// TODO(pdr): This flag is redundant should be removed.
const char kEnableSlimmingPaintV2[]         = "enable-slimming-paint-v2";

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

// Enabled threaded compositing for layout tests.
const char kEnableThreadedCompositing[]     = "enable-threaded-compositing";

// Enable tracing during the execution of browser tests.
const char kEnableTracing[]                 = "enable-tracing";

// The filename to write the output of the test tracing to.
const char kEnableTracingOutput[]           = "enable-tracing-output";

// Enable screen capturing support for MediaStream API.
const char kEnableUserMediaScreenCapturing[] =
    "enable-usermedia-screen-capturing";

// Enable the mode that uses zooming to implment device scale factor behavior.
const char kEnableUseZoomForDSF[]            = "enable-use-zoom-for-dsf";

// Enables the use of the @viewport CSS rule, which allows
// pages to control aspects of their own layout. This also turns on touch-screen
// pinch gestures.
const char kEnableViewport[]                = "enable-viewport";

// Enable the Vtune profiler support.
const char kEnableVtune[]                   = "enable-vtune-support";

// Enable Vulkan support, must also have ENABLE_VULKAN defined.
const char kEnableVulkan[] = "enable-vulkan";

// Enable the Web Authentication Testing API.
// https://w3c.github.io/webauthn
const char kEnableWebAuthTestingAPI[] = "enable-web-authentication-testing-api";

// Enable WebGL2 Compute context.
const char kEnableWebGL2ComputeContext[] = "enable-webgl2-compute-context";

// Enables WebGL extensions not yet approved by the community.
const char kEnableWebGLDraftExtensions[] = "enable-webgl-draft-extensions";

// Enables WebGL rendering into a scanout buffer for overlay support.
const char kEnableWebGLImageChromium[] = "enable-webgl-image-chromium";

// Enables interaction with virtual reality devices.
const char kEnableWebVR[] = "enable-webvr";

// Enable rasterizer that writes directly to GPU memory associated with tiles.
const char kEnableZeroCopy[]                = "enable-zero-copy";

// Explicitly allows additional ports using a comma-separated list of port
// numbers.
const char kExplicitlyAllowedPorts[]        = "explicitly-allowed-ports";

// Handle to the shared memory segment containing field trial state that is to
// be shared between processes. The argument to this switch is the handle id
// (pointer on Windows) as a string, followed by a comma, then the size of the
// shared memory segment as a string.
const char kFieldTrialHandle[] = "field-trial-handle";

// Define an alias root directory which is replaced with the replacement string
// in file URLs. The format is "/alias=/replacement", which would turn
// file:///alias/some/path.html into file:///replacement/some/path.html.
const char kFileUrlPathAlias[] = "file-url-path-alias";

// Always use the Skia GPU backend for drawing layer tiles. Only valid with GPU
// accelerated compositing + impl-side painting. Overrides the
// kEnableGpuRasterization flag.
const char kForceGpuRasterization[] = "force-gpu-rasterization";

// Disables OOP rasterization.  Takes precedence over the enable flag.
const char kDisableOopRasterization[] = "disable-oop-rasterization";

// Turns on out of process raster for the renderer whenever gpu raster
// would have been used.  Enables the chromium_raster_transport extension.
const char kEnableOopRasterization[] = "enable-oop-rasterization";

// Turns on skia deferred display list for out of process raster.
const char kEnableOopRasterizationDDL[] = "enable-oop-rasterization-ddl";

// The number of multisample antialiasing samples for GPU rasterization.
// Requires MSAA support on GPU to have an effect. 0 disables MSAA.
const char kGpuRasterizationMSAASampleCount[] =
    "gpu-rasterization-msaa-sample-count";

// Forces use of hardware overlay for fullscreen video playback. Useful for
// testing the Android overlay fullscreen functionality on other platforms.
const char kForceOverlayFullscreenVideo[]   = "force-overlay-fullscreen-video";

// This forces pages to be loaded as presentation receivers.  Useful for testing
// behavior specific to presentation receivers.
// Spec: https://www.w3.org/TR/presentation-api/#interface-presentationreceiver
const char kForcePresentationReceiverForTesting[] =
    "force-presentation-receiver-for-testing";

// Force renderer accessibility to be on instead of enabling it on demand when
// a screen reader is detected. The disable-renderer-accessibility switch
// overrides this if present.
const char kForceRendererAccessibility[]    = "force-renderer-accessibility";

// For development / testing only. When running content_browsertests,
// saves output of failing accessibility tests to their expectations files in
// content/test/data/accessibility/, overwriting existing file content.
const char kGenerateAccessibilityTestExpectations[] =
    "generate-accessibility-test-expectations";

// Extra command line options for launching the GPU process (normally used
// for debugging). Use like renderer-cmd-prefix.
const char kGpuLauncher[]                   = "gpu-launcher";

// Makes this process a GPU sub-process.
const char kGpuProcess[]                    = "gpu-process";

// Starts the GPU sandbox before creating a GL context.
const char kGpuSandboxStartEarly[] = "gpu-sandbox-start-early";

// Causes the GPU process to display a dialog on launch.
const char kGpuStartupDialog[]              = "gpu-startup-dialog";

// Don't allow content to arbitrarily append to the back/forward list.
// The page must prcoess a user gesture before an entry can be added.
const char kHistoryEntryRequiresUserGesture[] =
    "history-entry-requires-user-gesture";

// Start the renderer with an initial virtual time override specified in
// seconds since the epoch.
const char kInitialVirtualTime[] = "initial-virtual-time";

// Run the GPU process as a thread in the browser process.
const char kInProcessGPU[]                  = "in-process-gpu";

// Overrides the timeout, in seconds, that a child process waits for a
// connection from the browser before killing itself.
const char kIPCConnectionTimeout[]          = "ipc-connection-timeout";

// Require dedicated processes for a set of origins, specified as a
// comma-separated list. For example:
//   --isolate-origins=https://www.foo.com,https://www.bar.com
const char kIsolateOrigins[] = "isolate-origins";

// Disable latest shipping ECMAScript 6 features.
const char kDisableJavaScriptHarmonyShipping[] =
    "disable-javascript-harmony-shipping";

// Enables experimental Harmony (ECMAScript 6) features.
const char kJavaScriptHarmony[]             = "javascript-harmony";

// Specifies the flags passed to JS engine
const char kJavaScriptFlags[]               = "js-flags";

// Logs GPU control list decisions when enforcing blacklist rules.
const char kLogGpuControlListDecisions[]    = "log-gpu-control-list-decisions";

// Sets the minimum log level. Valid values are from 0 to 3:
// INFO = 0, WARNING = 1, LOG_ERROR = 2, LOG_FATAL = 3.
const char kLoggingLevel[]                  = "log-level";

// Overrides the default file name to use for general-purpose logging (does not
// affect which events are logged).
const char kLogFile[] = "log-file";

// Resizes of the main frame are caused by changing between landscape and
// portrait mode (i.e. Android) so the page should be rescaled to fit.
const char kMainFrameResizesAreOrientationChanges[] =
    "main-frame-resizes-are-orientation-changes";

// Sets the maximium decoded image size limitation.
const char kMaxDecodedImageSizeMb[] = "max-decoded-image-size-mb";

// Sets the width and height above which a composited layer will get tiled.
const char kMaxUntiledLayerHeight[]         = "max-untiled-layer-height";
const char kMaxUntiledLayerWidth[]          = "max-untiled-layer-width";

// Indicates the utility process should run with a message loop type of UI.
const char kMessageLoopTypeUi[] = "message-loop-type-ui";

// Sets options for MHTML generator to skip no-store resources:
//   "skip-nostore-main" - fails to save a page if main frame is 'no-store'
//   "skip-nostore-all" - also skips no-store subresources.
const char kMHTMLGeneratorOption[]          = "mhtml-generator-option";
const char kMHTMLSkipNostoreMain[]          = "skip-nostore-main";
const char kMHTMLSkipNostoreAll[]           = "skip-nostore-all";

// Use a Mojo-based LocalStorage implementation.
const char kMojoLocalStorage[]              = "mojo-local-storage";

// Sets the timeout seconds of the network-quiet timers in IdlenessDetector.
// Used by embedders who want to change the timeout time in order to run web
// contents on various embedded devices and changeable network bandwidths in
// different regions. For example, it's useful when using FirstMeaningfulPaint
// signal to dismiss a splash screen.
const char kNetworkQuietTimeout[] = "network-quiet-timeout";

// Disables the use of a zygote process for forking child processes. Instead,
// child processes will be forked and exec'd directly. Note that --no-sandbox
// should also be used together with this flag because the sandbox needs the
// zygote to work.
const char kNoZygote[] = "no-zygote";

// Disables V8 mitigations for executing untrusted code.
const char kNoV8UntrustedCodeMitigations[] = "no-v8-untrusted-code-mitigations";

// Number of worker threads used to rasterize content.
const char kNumRasterThreads[]              = "num-raster-threads";

// Override the behavior of plugin throttling for testing.
// By default the throttler is only enabled for a hard-coded list of plugins.
// Set the value to 'always' to always throttle every plugin instance. Set the
// value to 'never' to disable throttling.
const char kOverridePluginPowerSaverForTesting[] =
    "override-plugin-power-saver-for-testing";

// Controls the value of the threshold to start horizontal overscroll relative
// to the default value.
// E.g. set the value to '133' to have the overscroll start threshold be 133%
// of the default threshold.
const char kOverscrollStartThreshold[] = "overscroll-start-threshold";

// Override the default value for the 'passive' field in javascript
// addEventListener calls. Values are defined as:
//  'documentonlytrue' to set the default be true only for document level nodes.
//  'true' to set the default to be true on all nodes (when not specified).
//  'forcealltrue' to force the value on all nodes.
const char kPassiveListenersDefault[] = "passive-listeners-default";

// Argument to the process type that indicates a PPAPI broker process type.
const char kPpapiBrokerProcess[]            = "ppapi-broker";

// "Command-line" arguments for the PPAPI Flash; used for debugging options.
const char kPpapiFlashArgs[]                = "ppapi-flash-args";

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

// Uses a specified proxy server, overrides system settings. This switch only
// affects HTTP and HTTPS requests. ARC-apps use only HTTP proxy server with the
// highest priority.
// TODO(yzshen): Move this switch back to chrome/common/chrome_switches.{h,cc},
// once the network service is able to access the corresponding setting via the
// pref service.
const char kProxyServer[] = "proxy-server";

// Enables or disables pull-to-refresh gesture in response to vertical
// overscroll.
// Set the value to '0' to disable the feature, set to '1' to enable it for both
// touchpad and touchscreen, and set to '2' to enable it only for touchscreen.
// Defaults to disabled.
const char kPullToRefresh[] = "pull-to-refresh";

// Register Pepper plugins (see pepper_plugin_list.cc for its format).
const char kRegisterPepperPlugins[]         = "register-pepper-plugins";

// Enables remote debug over stdio pipes [in=3, out=4].
const char kRemoteDebuggingPipe[] = "remote-debugging-pipe";

// Enables remote debug over HTTP on the specified port.
const char kRemoteDebuggingPort[]           = "remote-debugging-port";

const char kRendererClientId[] = "renderer-client-id";

// The contents of this flag are prepended to the renderer command line.
// Useful values might be "valgrind" or "xterm -e gdb --args".
const char kRendererCmdPrefix[]             = "renderer-cmd-prefix";

// Causes the process to run as renderer instead of as browser.
const char kRendererProcess[]               = "renderer";

// Overrides the default/calculated limit to the number of renderer processes.
// Very high values for this setting can lead to high memory/resource usage
// or instability.
const char kRendererProcessLimit[]          = "renderer-process-limit";

// Causes the renderer process to display a dialog on launch. Passing this flag
// also adds service_manager::kNoSandbox on Windows non-official builds, since
// that's needed to show a dialog.
const char kRendererStartupDialog[]         = "renderer-startup-dialog";

// Reduce the default `referer` header's granularity.
const char kReducedReferrerGranularity[] =
  "reduced-referrer-granularity";

// Enables native memory sampling profiler with a given rate (default 128 KiB).
const char kSamplingHeapProfiler[]          = "sampling-heap-profiler";

// Causes the process to run as a sandbox IPC subprocess.
const char kSandboxIPCProcess[]             = "sandbox-ipc";

// Causes the renderer to keep an old document's cached resources alive until
// the specified point in the next document's lifecycle.
// By default, no explicit attempt to keep the resources alive is made, though
// that doesn't necessarily mean they will be GCed promptly.
const char kSavePreviousDocumentResources[] =
    "save-previous-document-resources";

// Visibly render a border around paint rects in the web page to help debug
// and study painting behavior.
const char kShowPaintRects[]                = "show-paint-rects";

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

// Disables enabling site isolation (i.e., --site-per-process) via field trial.
const char kDisableSiteIsolationTrials[] = "disable-site-isolation-trials";

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

// Type of the current test harness ("browser" or "ui").
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

// Controls how text selection granularity changes when touch text selection
// handles are dragged. Should be "character" or "direction". If not specified,
// the platform default is used.
const char kTouchTextSelectionStrategy[]    = "touch-selection-strategy";

// Bypass the media stream infobar by selecting the default device for media
// streams (e.g. WebRTC). Works with --use-fake-device-for-media-stream.
const char kUseFakeUIForMediaStream[]     = "use-fake-ui-for-media-stream";

// Texture target for CHROMIUM_image backed video frame textures.
const char kVideoImageTextureTarget[] = "video-image-texture-target";

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

// In debug builds, asserts that the stream of input events is valid.
const char kValidateInputEventStream[] = "validate-input-event-stream";

// Will add kWaitForDebugger to every child processes. If a value is passed, it
// will be used as a filter to determine if the child process should have the
// kWaitForDebugger flag passed on or not.
const char kWaitForDebuggerChildren[]       = "wait-for-debugger-children";

// Disables encryption of RTP Media for WebRTC. When Chrome embeds Content, it
// ignores this switch on its stable and beta channels.
const char kDisableWebRtcEncryption[]      = "disable-webrtc-encryption";

// Disables HW decode acceleration for WebRTC.
const char kDisableWebRtcHWDecoding[]       = "disable-webrtc-hw-decoding";

// Disables HW encode acceleration for WebRTC.
const char kDisableWebRtcHWEncoding[] = "disable-webrtc-hw-encoding";

// Enables negotiation of GCM cipher suites from RFC 7714 for SRTP in WebRTC.
// See https://tools.ietf.org/html/rfc7714 for further information.
const char kEnableWebRtcSrtpAesGcm[] = "enable-webrtc-srtp-aes-gcm";

// Enables negotiation of encrypted header extensions from RFC 6904 for SRTP
// in WebRTC.
// See https://tools.ietf.org/html/rfc6904 for further information.
const char kEnableWebRtcSrtpEncryptedHeaders[] =
    "enable-webrtc-srtp-encrypted-headers";

// Enables Origin header in Stun messages for WebRTC.
const char kEnableWebRtcStunOrigin[]        = "enable-webrtc-stun-origin";

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

// Configure the maximum CPU time percentage of a single core that can be
// consumed for desktop capturing. Default is 50. Set 100 to disable the
// throttling of the capture.
const char kWebRtcMaxCpuConsumptionPercentage[] =
    "webrtc-max-cpu-consumption-percentage";

// Renderer process parameter for WebRTC Stun probe trial to determine the
// interval. Please see SetupStunProbeTrial in
// chrome_browser_field_trials_desktop.cc for more detail.
const char kWebRtcStunProbeTrialParameter[] = "webrtc-stun-probe-trial";

// Enable capture and local storage of WebRTC event logs without visiting
// chrome://webrtc-internals. This is useful for automated testing. It accepts
// the path to which the local logs would be stored. Disabling is not possible
// without restarting the browser and relaunching without this flag.
const char kWebRtcLocalEventLogging[] = "webrtc-event-logging";

#if defined(OS_ANDROID)
// Disable Media Session API
const char kDisableMediaSessionAPI[] = "disable-media-session-api";

// Disable overscroll edge effects like those found in Android views.
const char kDisableOverscrollEdgeEffect[]   = "disable-overscroll-edge-effect";

// Disable the pull-to-refresh effect when vertically overscrolling content.
const char kDisablePullToRefreshEffect[]   = "disable-pull-to-refresh-effect";

// Disable the locking feature of the screen orientation API.
const char kDisableScreenOrientationLock[]  = "disable-screen-orientation-lock";

// Disable timeouts that may cause the browser to die when running slowly. This
// is useful if running with profiling (such as debug malloc).
const char kDisableTimeoutsForProfiling[] = "disable-timeouts-for-profiling";

// Enable inverting of selection handles so that they are not clipped by the
// viewport boundaries.
const char kEnableAdaptiveSelectionHandleOrientation[] =
    "enable-adaptive-selection-handle-orientation";

// Enable drag manipulation of longpress-triggered text selections.
const char kEnableLongpressDragSelection[]  = "enable-longpress-drag-selection";

// The telephony region (ISO country code) to use in phone number detection.
const char kNetworkCountryIso[] = "network-country-iso";

// Enables remote debug over HTTP on the specified socket name.
const char kRemoteDebuggingSocketName[]     = "remote-debugging-socket-name";

// Block ChildProcessMain thread of the renderer's ChildProcessService until a
// Java debugger is attached.
const char kRendererWaitForJavaDebugger[] = "renderer-wait-for-java-debugger";

// Enables overscrolling for the OSK on Android.
const char kEnableOSKOverscroll[]               = "enable-osk-overscroll";
#endif

// Enable the experimental Accessibility Object Model APIs in development.
const char kEnableAccessibilityObjectModel[] =
    "enable-accessibility-object-model";

// Enable the aggressive flushing of DOM Storage to minimize data loss.
const char kEnableAggressiveDOMStorageFlushing[] =
    "enable-aggressive-domstorage-flushing";

// Enable indication that browser is controlled by automation.
const char kEnableAutomation[] = "enable-automation";

// Enable audio for desktop share.
const char kDisableAudioSupportForDesktopShare[] =
    "disable-audio-support-for-desktop-share";

#if defined(OS_CHROMEOS)
// Disables panel fitting (used for mirror mode).
const char kDisablePanelFitting[]           = "disable-panel-fitting";
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
// Allows sending text-to-speech requests to speech-dispatcher, a common
// Linux speech service. Because it's buggy, the user must explicitly
// enable it so that visiting a random webpage can't cause instability.
const char kEnableSpeechDispatcher[] = "enable-speech-dispatcher";
#endif

#if defined(OS_WIN)
// /prefetch:# arguments to use when launching various process types. It has
// been observed that when file reads are consistent for 3 process launches with
// the same /prefetch:# argument, the Windows prefetcher starts issuing reads in
// batch at process launch. Because reads depend on the process type, the
// prefetcher wouldn't be able to observe consistent reads if no /prefetch:#
// arguments were used. Note that the browser process has no /prefetch:#
// argument; as such all other processes must have one in order to avoid
// polluting its profile. Note: # must always be in [1, 8]; otherwise it is
// ignored by the Windows prefetcher.
const char kPrefetchArgumentRenderer[] = "/prefetch:1";
const char kPrefetchArgumentGpu[] = "/prefetch:2";
const char kPrefetchArgumentPpapi[] = "/prefetch:3";
const char kPrefetchArgumentPpapiBroker[] = "/prefetch:4";
// /prefetch:5, /prefetch:6 and /prefetch:7 are reserved for content embedders
// and are not to be used by content itself.

// /prefetch:# argument shared by all process types that don't have their own.
// It is likely that the prefetcher won't work for these process types as it
// won't be able to observe consistent file reads across launches. However,
// having a valid prefetch argument for these process types is required to
// prevent them from interfering with the prefetch profile of the browser
// process.
const char kPrefetchArgumentOther[] = "/prefetch:8";

// Device scale factor passed to certain processes like renderers, etc.
const char kDeviceScaleFactor[]     = "device-scale-factor";

// Disable the Legacy Window which corresponds to the size of the WebContents.
const char kDisableLegacyIntermediateWindow[] = "disable-legacy-window";

// Enables experimental hardware acceleration for VP8/VP9 video decoding.
// Bitmask - 0x1=Microsoft, 0x2=AMD, 0x03=Try all.
const char kEnableAcceleratedVpxDecode[] = "enable-accelerated-vpx-decode";

// Enables H264 HW decode acceleration for WebRtc on Win 7.
const char kEnableWin7WebRtcHWH264Decoding[] =
    "enable-win7-webrtc-hw-h264-decoding";

// DirectWrite FontCache is shared by browser to renderers using shared memory.
// This switch allows us to pass the shared memory handle to the renderer.
const char kFontCacheSharedHandle[] = "font-cache-shared-handle";

// Sets the free memory thresholds below which the system is considered to be
// under moderate and critical memory pressure. Used in the browser process,
// and ignored if invalid. Specified as a pair of comma separated integers.
// See base/win/memory_pressure_monitor.cc for defaults.
const char kMemoryPressureThresholdsMb[] = "memory-pressure-thresholds-mb";

// The boolean value (0/1) of FontRenderParams::antialiasing to be passed to
// Ppapi processes.
const char kPpapiAntialiasedTextEnabled[] = "ppapi-antialiased-text-enabled";

// The enum value of FontRenderParams::subpixel_rendering to be passed to Ppapi
// processes.
const char kPpapiSubpixelRenderingSetting[] =
    "ppapi-subpixel-rendering-setting";

// Enables the exporting of the tracing events to ETW. This is only supported on
// Windows Vista and later.
const char kTraceExportEventsToETW[] = "trace-export-events-to-etw";
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
