// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/forcedark/forcedark_switches.h"

namespace blink {
namespace features {

// Enable intervention for download that was initiated from or occurred in an ad
// frame without user activation.
const base::Feature kBlockingDownloadsInAdFrameWithoutUserActivation{
    "BlockingDownloadsInAdFrameWithoutUserActivation",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enable defer commits to avoid flash of unstyled content.
const base::Feature kPaintHolding{"PaintHolding",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// Enable eagerly setting up a CacheStorage interface pointer and
// passing it to service workers on startup as an optimization.
// TODO(crbug/1077916): Re-enable once the issue with COOP/COEP is fixed.
const base::Feature kEagerCacheStorageSetupForServiceWorkers{
    "EagerCacheStorageSetupForServiceWorkers",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls script streaming.
const base::Feature kScriptStreaming{"ScriptStreaming",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Allow streaming small (<30kB) scripts.
const base::Feature kSmallScriptStreaming{"SmallScriptStreaming",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables user level memory pressure signal generation on Android.
const base::Feature kUserLevelMemoryPressureSignal{
    "UserLevelMemoryPressureSignal", base::FEATURE_DISABLED_BY_DEFAULT};

// Perform memory purges after freezing only if all pages are frozen.
const base::Feature kFreezePurgeMemoryAllPagesFrozen{
    "FreezePurgeMemoryAllPagesFrozen", base::FEATURE_DISABLED_BY_DEFAULT};

// Freezes the user-agent as part of https://github.com/WICG/ua-client-hints.
const base::Feature kFreezeUserAgent{"FreezeUserAgent",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, use the maximum possible bounds in compositing overlap testing
// for fixed position elements.
const base::Feature kMaxOverlapBoundsForFixed{
    "MaxOverlapBoundsForFixed", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable Display Locking JavaScript APIs.
const base::Feature kDisplayLocking{"DisplayLocking",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kJSONModules{"JSONModules",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kForceSynchronousHTMLParsing{
    "ForceSynchronousHTMLParsing", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables top-level await in modules.
const base::Feature kTopLevelAwait{"TopLevelAwait",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enable LayoutNG.
const base::Feature kLayoutNG{"LayoutNG", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kMixedContentAutoupgrade{"AutoupgradeMixedContent",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Used to control the collection of anchor element metrics (crbug.com/856683).
// If kNavigationPredictor is enabled, then metrics of anchor elements
// in the first viewport after the page load and the metrics of the clicked
// anchor element will be extracted and recorded. Additionally, navigation
// predictor may preconnect/prefetch to resources/origins to make the
// future navigations faster.
const base::Feature kNavigationPredictor {
  "NavigationPredictor",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enable browser-initiated dedicated worker script loading
// (PlzDedicatedWorker). https://crbug.com/906991
const base::Feature kPlzDedicatedWorker{"PlzDedicatedWorker",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enable Portals. https://crbug.com/865123.
const base::Feature kPortals{"Portals", base::FEATURE_DISABLED_BY_DEFAULT};

// When kPortals is enabled, allow portals to load content that is third-party
// (cross-origin) to the hosting page. Otherwise has no effect.
//
// This will be disabled by default by the time Portals is generally available,
// either in origin trial or shipped.
//
// https://crbug.com/1013389
const base::Feature kPortalsCrossOrigin{"PortalsCrossOrigin",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Enable limiting previews loading hints to specific resource types.
const base::Feature kPreviewsResourceLoadingHintsSpecificResourceTypes{
    "PreviewsResourceLoadingHintsSpecificResourceTypes",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Perform a memory purge after a renderer is backgrounded. Formerly labelled as
// the "PurgeAndSuspend" experiment.
//
// TODO(adityakeerthi): Disabled by default on Mac and Android for historical
// reasons. Consider enabling by default if experiment results are positive.
// https://crbug.com/926186
const base::Feature kPurgeRendererMemoryWhenBackgrounded {
  "PurgeRendererMemoryWhenBackgrounded",
#if defined(OS_MACOSX) || defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Enable Implicit Root Scroller. https://crbug.com/903260.
// TODO(bokan): Temporarily disabled on desktop platforms to address issues
// with non-overlay scrollbars. https://crbug.com/948059.
const base::Feature kImplicitRootScroller {
  "ImplicitRootScroller",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enable CSSOM View Scroll Coordinates. https://crbug.com/721759.
const base::Feature kCSSOMViewScrollCoordinates{
    "CSSOMViewScrollCoordinates", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Raw Clipboard. https://crbug.com/897289.
const base::Feature kRawClipboard{"RawClipboard",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Enables usage of getDisplayMedia() that allows capture of web content, see
// https://crbug.com/865060.
const base::Feature kRTCGetDisplayMedia{"RTCGetDisplayMedia",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Changes the default RTCPeerConnection constructor behavior to use Unified
// Plan as the SDP semantics. When the feature is enabled, Unified Plan is used
// unless the default is overridden (by passing {sdpSemantics:'plan-b'} as the
// argument).
const base::Feature kRTCUnifiedPlanByDefault{"RTCUnifiedPlanByDefault",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Determines if the SDP attrbute extmap-allow-mixed should be offered by
// default or not. The default value can be overridden by passing
// {offerExtmapAllowMixed:true} as an argument to the RTCPeerConnection
// constructor.
const base::Feature kRTCOfferExtmapAllowMixed{
    "RTCOfferExtmapAllowMixed", base::FEATURE_DISABLED_BY_DEFAULT};

// Prevents workers from sending IsolateInBackgroundNotification to V8
// and thus instructs V8 to favor performance over memory on workers.
const base::Feature kV8OptimizeWorkersForPerformance{
    "V8OptimizeWorkersForPerformance", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables negotiation of experimental multiplex codec in SDP.
const base::Feature kWebRtcMultiplexCodec{"WebRTC-MultiplexCodec",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Causes WebRTC to replace host ICE candidate IP addresses with generated
// names ending in ".local" and resolve them using mDNS.
// http://crbug.com/878465
const base::Feature kWebRtcHideLocalIpsWithMdns{
    "WebRtcHideLocalIpsWithMdns", base::FEATURE_ENABLED_BY_DEFAULT};

#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
// Run-time feature for the |rtc_use_h264| encoder/decoder.
const base::Feature kWebRtcH264WithOpenH264FFmpeg{
    "WebRTC-H264WithOpenH264FFmpeg", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)

// Experiment of the delay from navigation to starting an update of a service
// worker's script.
const base::Feature kServiceWorkerUpdateDelay{
    "ServiceWorkerUpdateDelay", base::FEATURE_DISABLED_BY_DEFAULT};

// Freeze scheduler task queues in background after allowed grace time.
// "stop" is a legacy name.
const base::Feature kStopInBackground {
  "stop-in-background",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Freeze scheduler task queues in background on network idle.
// This feature only works if stop-in-background is enabled.
const base::Feature kFreezeBackgroundTabOnNetworkIdle{
    "freeze-background-tab-on-network-idle", base::FEATURE_DISABLED_BY_DEFAULT};

// Freeze non-timer task queues in background, after allowed grace time.
// "stop" is a legacy name.
const base::Feature kStopNonTimersInBackground{
    "stop-non-timers-in-background", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable the Storage Access API. https://crbug.com/989663.
const base::Feature kStorageAccessAPI{"StorageAccessAPI",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enable text snippets in URL fragments. https://crbug.com/919204.
const base::Feature kTextFragmentAnchor{"TextFragmentAnchor",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Writable files and native file system access. https://crbug.com/853326
const base::Feature kNativeFileSystemAPI{"NativeFileSystemAPI",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// File handling integration. https://crbug.com/829689
const base::Feature kFileHandlingAPI{"FileHandlingAPI",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Allows for synchronous XHR requests during page dismissal
const base::Feature kAllowSyncXHRInPageDismissal{
    "AllowSyncXHRInPageDismissal", base::FEATURE_DISABLED_BY_DEFAULT};

// Font enumeration and table access. https://crbug.com/535764 and
// https://crbug.com/982054.
const base::Feature kFontAccess{"FontAccess",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// Prefetch request properties are updated to be privacy-preserving. See
// crbug.com/988956.
const base::Feature kPrefetchPrivacyChanges{"PrefetchPrivacyChanges",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const char kMixedContentAutoupgradeModeParamName[] = "mode";
const char kMixedContentAutoupgradeModeAllPassive[] = "all-passive";

// Decodes jpeg 4:2:0 formatted images to YUV instead of RGBX and stores in this
// format in the image decode cache. See crbug.com/919627 for details on the
// feature.
const base::Feature kDecodeJpeg420ImagesToYUV{"DecodeJpeg420ImagesToYUV",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Decodes lossy WebP images to YUV instead of RGBX and stores in this format
// in the image decode cache. See crbug.com/900264 for details on the feature.
const base::Feature kDecodeLossyWebPImagesToYUV{
    "DecodeLossyWebPImagesToYUV", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables cache-aware WebFonts loading. See https://crbug.com/570205.
// The feature is disabled on Android for WebView API issue discussed at
// https://crbug.com/942440.
const base::Feature kWebFontsCacheAwareTimeoutAdaption {
  "WebFontsCacheAwareTimeoutAdaption",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Enabled to block programmatic focus in subframes when not triggered by user
// activation (see htpps://crbug.com/954349).
const base::Feature kBlockingFocusWithoutUserActivation{
    "BlockingFocusWithoutUserActivation", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAudioWorkletRealtimeThread{
    "AudioWorkletRealtimeThread", base::FEATURE_DISABLED_BY_DEFAULT};

// A feature to reduce the set of resources fetched by No-State Prefetch.
const base::Feature kLightweightNoStatePrefetch {
  "LightweightNoStatePrefetch",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Automatically convert light-themed pages to use a Blink-generated dark theme
const base::Feature kForceWebContentsDarkMode{
    "WebContentsForceDark", base::FEATURE_DISABLED_BY_DEFAULT};

// A feature to enable using the smallest image specified within image srcset
// for users with Save Data enabled.
const base::Feature kSaveDataImgSrcset{"SaveDataImgSrcset",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Which algorithm should be used for color inversion?
const base::FeatureParam<ForceDarkInversionMethod>::Option
    forcedark_inversion_method_options[] = {
        {ForceDarkInversionMethod::kUseBlinkSettings,
         "use_blink_settings_for_method"},
        {ForceDarkInversionMethod::kHslBased, "hsl_based"},
        {ForceDarkInversionMethod::kCielabBased, "cielab_based"},
        {ForceDarkInversionMethod::kRgbBased, "rgb_based"}};

const base::FeatureParam<ForceDarkInversionMethod>
    kForceDarkInversionMethodParam{&kForceWebContentsDarkMode,
                                   "inversion_method",
                                   ForceDarkInversionMethod::kUseBlinkSettings,
                                   &forcedark_inversion_method_options};

// Should images be inverted?
const base::FeatureParam<ForceDarkImageBehavior>::Option
    forcedark_image_behavior_options[] = {
        {ForceDarkImageBehavior::kUseBlinkSettings,
         "use_blink_settings_for_images"},
        {ForceDarkImageBehavior::kInvertNone, "none"},
        {ForceDarkImageBehavior::kInvertSelectively, "selective"}};

const base::FeatureParam<ForceDarkImageBehavior> kForceDarkImageBehaviorParam{
    &kForceWebContentsDarkMode, "image_behavior",
    ForceDarkImageBehavior::kUseBlinkSettings,
    &forcedark_image_behavior_options};

// Do not invert text lighter than this.
// Range: 0 (do not invert any text) to 256 (invert all text)
// Can also set to -1 to let Blink's internal settings control the value
const base::FeatureParam<int> kForceDarkTextLightnessThresholdParam{
    &kForceWebContentsDarkMode, "text_lightness_threshold", -1};

// Do not invert backgrounds darker than this.
// Range: 0 (invert all backgrounds) to 256 (invert no backgrounds)
// Can also set to -1 to let Blink's internal settings control the value
const base::FeatureParam<int> kForceDarkBackgroundLightnessThresholdParam{
    &kForceWebContentsDarkMode, "background_lightness_threshold", -1};

// Instructs WebRTC to honor the Min/Max Video Encode Accelerator dimensions.
const base::Feature kWebRtcUseMinMaxVEADimensions {
  "WebRtcUseMinMaxVEADimensions",
  // TODO(crbug.com/1008491): enable other platforms.
#if defined(OS_CHROMEOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Blink garbage collection.
// Enables compaction of backing stores on Blink's heap.
const base::Feature kBlinkHeapCompaction{"BlinkHeapCompaction",
                                         base::FEATURE_ENABLED_BY_DEFAULT};
// Enables concurrently marking Blink's heap.
const base::Feature kBlinkHeapConcurrentMarking{
    "BlinkHeapConcurrentMarking", base::FEATURE_ENABLED_BY_DEFAULT};
// Enables concurrently sweeping Blink's heap.
const base::Feature kBlinkHeapConcurrentSweeping{
    "BlinkHeapConcurrentSweeping", base::FEATURE_ENABLED_BY_DEFAULT};
// Enables incrementally marking Blink's heap.
const base::Feature kBlinkHeapIncrementalMarking{
    "BlinkHeapIncrementalMarking", base::FEATURE_ENABLED_BY_DEFAULT};
// Enables a marking stress mode that schedules more garbage collections and
// also adds additional verification passes.
const base::Feature kBlinkHeapIncrementalMarkingStress{
    "BlinkHeapIncrementalMarkingStress", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables removing AppCache delays when triggering requests when the HTML was
// not fetched from AppCache.
const base::Feature kVerifyHTMLFetchedFromAppCacheBeforeDelay{
    "VerifyHTMLFetchedFromAppCacheBeforeDelay",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether we use ThreadPriority::DISPLAY for renderer
// compositor & IO threads.
const base::Feature kBlinkCompositorUseDisplayThreadPriority {
  "BlinkCompositorUseDisplayThreadPriority",
#if defined(OS_ANDROID) || defined(OS_CHROMEOS) || defined(OS_WIN)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Ignores cross origin windows in the named property interceptor of Window.
// https://crbug.com/538562
const base::Feature kIgnoreCrossOriginWindowWhenNamedAccessOnWindow{
    "IgnoreCrossOriginWindowWhenNamedAccessOnWindow",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, loading priority of JavaScript requests is lowered when they
// are force deferred by the intervention.
const base::Feature kLowerJavaScriptPriorityWhenForceDeferred{
    "LowerJavaScriptPriorityWhenForceDeferred",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, scripts in iframes are not force deferred by the DeferAllScript
// intervention.
const base::Feature kDisableForceDeferInChildFrames{
    "DisableForceDeferInChildFrames", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables redirecting subresources in the page to better compressed and
// optimized versions to provide data savings.
const base::Feature kSubresourceRedirect{"SubresourceRedirect",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// When 'enabled', all cross-origin iframes will get a compositing layer.
const base::Feature kCompositeCrossOriginIframes{
    "CompositeCrossOriginIframes", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, enforces new interoperable semantics for 3D transforms.
// See crbug.com/1008483.
const base::Feature kTransformInterop{"TransformInterop",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, beacons (and friends) have ResourceLoadPriority::kLow,
// not ResourceLoadPriority::kVeryLow.
const base::Feature kSetLowPriorityForBeacon{"SetLowPriorityForBeacon",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled allows the header name used in the blink
// CacheStorageCodeCacheHint runtime feature to be modified.  This runtime
// feature disables generating full code cache for responses stored in
// cache_storage during a service worker install event.  The runtime feature
// must be enabled via the blink runtime feature mechanism, however.
const base::Feature kCacheStorageCodeCacheHintHeader{
    "CacheStorageCodeCacheHintHeader", base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<std::string> kCacheStorageCodeCacheHintHeaderName{
    &kCacheStorageCodeCacheHintHeader, "name", "x-CacheStorageCodeCacheHint"};

// When enabled, the beforeunload handler is dispatched when a frame is frozen.
// This allows the browser to know whether discarding the frame could result in
// lost user data, at the cost of extra CPU usage. The feature will be removed
// once we have determine whether the CPU cost is acceptable.
const base::Feature kDispatchBeforeUnloadOnFreeze{
    "DispatchBeforeUnloadOnFreeze", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the use of GpuMemoryBuffer images for low latency 2d canvas.
// TODO(khushalsagar): Enable this if we're using SurfaceControl and GMBs allow
// us to overlay these resources.
const base::Feature kLowLatencyCanvas2dImageChromium {
  "LowLatencyCanvas2dImageChromium",
#if defined(OS_CHROMEOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // OS_CHROMEOS
};

// Enables the use of shared image swap chains for low latency 2d canvas.
const base::Feature kLowLatencyCanvas2dSwapChain{
    "LowLatencyCanvas2dSwapChain", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the use of shared image swap chains for low latency webgl canvas.
const base::Feature kLowLatencyWebGLSwapChain{"LowLatencyWebGLSwapChain",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables Dawn-accelerated 2D canvas.
const base::Feature kDawn2dCanvas{"Dawn2dCanvas",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCSSReducedFontLoadingInvalidations{
    "CSSReducedFontLoadingInvalidations", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kCSSReducedFontLoadingLayoutInvalidations{
    "CSSReducedFontLoadingLayoutInvalidations",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, frees up CachedMetadata after consumption by script resources
// and modules. Needed for the experiment in http://crbug.com/1045052.
const base::Feature kDiscardCodeCacheAfterFirstUse{
    "DiscardCodeCacheAfterFirstUse", base::FEATURE_DISABLED_BY_DEFAULT};

// The kill-switch for the fix for https://crbug.com/1051439.
// TODO(crbug.com/1053369): Remove this around M84.
const base::Feature kSuppressContentTypeForBeaconMadeWithArrayBufferView{
    "SuppressContentTypeForBeaconMadeWithArrayBufferView",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kBlockFlowHandlesWebkitLineClamp{
    "BlockFlowHandlesWebkitLineClamp", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kBlockHTMLParserOnStyleSheets{
    "BlockHTMLParserOnStyleSheets", base::FEATURE_DISABLED_BY_DEFAULT};

// Kill switch for the new <link disabled> behavior.
// TODO(crbug.com/1087043): Remove this once the feature has
// landed and no compat issues are reported.
const base::Feature kLinkDisabledNewSpecBehavior{
    "LinkDisabledNewSpecBehavior", base::FEATURE_ENABLED_BY_DEFAULT};

// Slightly delays rendering if there are fonts being preloaded, so that
// they don't miss the first paint if they can be loaded fast enough (e.g.,
// from the disk cache)
const base::Feature kFontPreloadingDelaysRendering{
    "FontPreloadingDelaysRendering", base::FEATURE_DISABLED_BY_DEFAULT};

// Set to be over 90th-percentile of HttpCache.AccessToDone.Used on all
// platforms, and also to allow some time for IPC and scheduling.
// TODO(xiaochengh): Tune it for the best performance.
const base::FeatureParam<int> kFontPreloadingDelaysRenderingParam{
    &kFontPreloadingDelaysRendering, "delay-in-ms", 100};

const base::Feature kFlexGaps{"FlexGaps", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kFlexNG{"FlexNG", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kKeepScriptResourceAlive{"KeepScriptResourceAlive",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kDelayAsyncScriptExecution{
    "DelayAsyncScriptExecution", base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<DelayAsyncScriptDelayType>::Option
    delay_async_script_execution_delay_types[] = {
        {DelayAsyncScriptDelayType::kFinishedParsing, "finished_parsing"},
        {DelayAsyncScriptDelayType::kFirstPaintOrFinishedParsing,
         "first_paint_or_finished_parsing"}};
const base::FeatureParam<DelayAsyncScriptDelayType>
    kDelayAsyncScriptExecutionDelayParam{
        &kDelayAsyncScriptExecution, "delay_type",
        DelayAsyncScriptDelayType::kFinishedParsing,
        &delay_async_script_execution_delay_types};

// The AppCache feature is a kill-switch for the entire AppCache feature,
// both backend and API.  If disabled, then it will turn off the backend and
// api, regardless of the presence of valid origin trial tokens.
const base::Feature kAppCache{"AppCache", base::FEATURE_ENABLED_BY_DEFAULT};
// If AppCacheRequireOriginTrial is enabled, then the AppCache backend in the
// browser will require origin trial tokens in order to load or store manifests
// and their contents.
const base::Feature kAppCacheRequireOriginTrial{
    "AppCacheRequireOriginTrial", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the AV1 Image File Format (AVIF).
const base::Feature kAVIF{"AVIF", base::FEATURE_DISABLED_BY_DEFAULT};

// Make all pending 'display: auto' web fonts enter the swap or failure period
// immediately before reaching the LCP time limit (~2500ms), so that web fonts
// do not become a source of bad LCP.
const base::Feature kAlignFontDisplayAutoTimeoutWithLCPGoal{
    "AlignFontDisplayAutoTimeoutWithLCPGoal",
    base::FEATURE_DISABLED_BY_DEFAULT};

// The amount of time allowed for 'display: auto' web fonts to load without
// intervention, counted from navigation start.
const base::FeatureParam<int>
    kAlignFontDisplayAutoTimeoutWithLCPGoalTimeoutParam{
        &kAlignFontDisplayAutoTimeoutWithLCPGoal, "lcp-limit-in-ms", 2000};

const base::FeatureParam<AlignFontDisplayAutoTimeoutWithLCPGoalMode>::Option
    align_font_display_auto_timeout_with_lcp_goal_modes[] = {
        {AlignFontDisplayAutoTimeoutWithLCPGoalMode::kToFailurePeriod,
         "failure"},
        {AlignFontDisplayAutoTimeoutWithLCPGoalMode::kToSwapPeriod, "swap"}};
const base::FeatureParam<AlignFontDisplayAutoTimeoutWithLCPGoalMode>
    kAlignFontDisplayAutoTimeoutWithLCPGoalModeParam{
        &kAlignFontDisplayAutoTimeoutWithLCPGoal, "intervention-mode",
        AlignFontDisplayAutoTimeoutWithLCPGoalMode::kToFailurePeriod,
        &align_font_display_auto_timeout_with_lcp_goal_modes};

// Enable throttling of fetch() requests from service workers in the
// installing state.
const base::Feature kThrottleInstallingServiceWorker{
    "ThrottleInstallingServiceWorker", base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<int> kInstallingServiceWorkerOutstandingThrottledLimit{
    &kThrottleInstallingServiceWorker, "limit", 5};

const base::Feature kInputPredictorTypeChoice{
    "InputPredictorTypeChoice", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kResamplingInputEvents{"ResamplingInputEvents",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kResamplingScrollEvents{"ResamplingScrollEvents",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the device-memory, resource-width, viewport-width and DPR client
// hints to be sent to third-party origins if the first-party has opted in to
// receiving client hints, regardless of Feature Policy.
#if defined(OS_ANDROID)
const base::Feature kAllowClientHintsToThirdParty{
    "AllowClientHintsToThirdParty", base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kAllowClientHintsToThirdParty{
    "AllowClientHintsToThirdParty", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const char kScrollPredictorNameLsq[] = "lsq";
const char kScrollPredictorNameKalman[] = "kalman";
const char kScrollPredictorNameLinearFirst[] = "linear_first";
const char kScrollPredictorNameLinearSecond[] = "linear_second";
const char kScrollPredictorNameLinearResampling[] = "linear_resampling";
const char kScrollPredictorNameEmpty[] = "empty";

const base::Feature kFilteringScrollPrediction{
    "FilteringScrollPrediction", base::FEATURE_DISABLED_BY_DEFAULT};

const char kFilterNameEmpty[] = "empty_filter";
const char kFilterNameOneEuro[] = "one_euro_filter";

const base::Feature kKalmanHeuristics{"KalmanHeuristics",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kKalmanDirectionCutOff{"KalmanDirectionCutOff",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSkipTouchEventFilter{"SkipTouchEventFilter",
                                          base::FEATURE_ENABLED_BY_DEFAULT};
const char kSkipTouchEventFilterTypeParamName[] = "type";
const char kSkipTouchEventFilterTypeParamValueDiscrete[] = "discrete";
const char kSkipTouchEventFilterTypeParamValueAll[] = "all";
const char kSkipTouchEventFilterFilteringProcessParamName[] =
    "skip_filtering_process";
const char kSkipTouchEventFilterFilteringProcessParamValueBrowser[] = "browser";
const char kSkipTouchEventFilterFilteringProcessParamValueBrowserAndRenderer[] =
    "browser_and_renderer";

// Improves support for WebXR on computers with multiple GPUs.
const base::Feature kWebXrMultiGpu{"WebXRMultiGpu",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables dependency support in blink::MatchedPropertiesCache, which allows
// caching of previously uncachable objects.
const base::Feature kCSSMatchedPropertiesCacheDependencies{
    "CSSMatchedPropertiesCacheDependencies", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace blink
