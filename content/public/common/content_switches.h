// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "content" command-line switches.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/common/content_export.h"
#include "media/media_buildflags.h"
#include "tools/v8_context_snapshot/buildflags.h"

namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
CONTENT_EXPORT extern const char kAllowFileAccessFromFiles[];
CONTENT_EXPORT extern const char kAllowInsecureLocalhost[];
CONTENT_EXPORT extern const char kAllowLoopbackInPeerConnection[];
CONTENT_EXPORT extern const char kAllowCommandLinePlugins[];
CONTENT_EXPORT extern const char kAttributionReportingDebugMode[];
CONTENT_EXPORT extern const char kAutoAcceptCameraAndMicrophoneCapture[];
CONTENT_EXPORT extern const char kBrowserCrashTest[];
CONTENT_EXPORT extern const char kBrowserStartupDialog[];
CONTENT_EXPORT extern const char kBrowserSubprocessPath[];
CONTENT_EXPORT extern const char kBrowserTest[];
CONTENT_EXPORT extern const char kChangeStackGuardOnFork[];
CONTENT_EXPORT extern const char kChangeStackGuardOnForkEnabled[];
CONTENT_EXPORT extern const char kChangeStackGuardOnForkDisabled[];
CONTENT_EXPORT extern const char kDisable2dCanvasAntialiasing[];
CONTENT_EXPORT extern const char kDisable2dCanvasImageChromium[];
CONTENT_EXPORT extern const char kDisable3DAPIs[];
CONTENT_EXPORT extern const char kDisableAccelerated2dCanvas[];
CONTENT_EXPORT extern const char kDisableAcceleratedVideoDecode[];
CONTENT_EXPORT extern const char kDisableYUVImageDecoding[];
CONTENT_EXPORT extern const char kDisableAcceleratedVideoEncode[];
extern const char kDisableBackingStoreLimit[];
CONTENT_EXPORT extern const char
    kDisableBackgroundingOccludedWindowsForTesting[];
CONTENT_EXPORT extern const char kDisableBackgroundTimerThrottling[];
CONTENT_EXPORT extern const char kDisableBackForwardCache[];
CONTENT_EXPORT extern const char kDisableBlinkFeatures[];
CONTENT_EXPORT extern const char kDisableDatabases[];
CONTENT_EXPORT extern const char kDisableDisplayList2dCanvas[];
extern const char kDisableDomainBlockingFor3DAPIs[];
CONTENT_EXPORT extern const char kDisableInProcessStackTraces[];
CONTENT_EXPORT extern const char kDisableWebGL[];
CONTENT_EXPORT extern const char kDisableWebGL2[];
CONTENT_EXPORT extern const char kDisableFileSystem[];
CONTENT_EXPORT extern const char kDisableGestureRequirementForPresentation[];
CONTENT_EXPORT extern const char kDisableGpu[];
CONTENT_EXPORT extern const char kDisableGpuCompositing[];
CONTENT_EXPORT extern const char kDisableGpuEarlyInit[];
CONTENT_EXPORT extern const char kDisableGpuMemoryBufferCompositorResources[];
CONTENT_EXPORT extern const char kDisableGpuMemoryBufferVideoFrames[];
extern const char kDisableGpuProcessCrashLimit[];
CONTENT_EXPORT extern const char kDisableScrollToTextFragment[];
CONTENT_EXPORT extern const char kDisableSoftwareCompositingFallback[];
CONTENT_EXPORT extern const char kDisableGpuWatchdog[];
CONTENT_EXPORT extern const char kDisableIpcFloodingProtection[];
CONTENT_EXPORT extern const char kDisableJavaScriptHarmonyShipping[];
CONTENT_EXPORT extern const char kDisableLowLatencyDxva[];
CONTENT_EXPORT extern const char kDisableHangMonitor[];
extern const char kDisableHistogramCustomizer[];
CONTENT_EXPORT extern const char kDisableLCDText[];
CONTENT_EXPORT extern const char kDisableKillAfterBadIPC[];
CONTENT_EXPORT extern const char kDisableLocalStorage[];
CONTENT_EXPORT extern const char kDisableLogging[];
CONTENT_EXPORT extern const char kDisableMojoBroker[];
CONTENT_EXPORT extern const char kDisableNewContentRenderingTimeout[];
CONTENT_EXPORT extern const char kDisableNotifications[];
CONTENT_EXPORT extern const char kDisableNv12DxgiVideo[];
CONTENT_EXPORT extern const char kDisableOriginTrialControlledBlinkFeatures[];
extern const char kDisablePepper3d[];
CONTENT_EXPORT extern const char kDisablePinch[];
CONTENT_EXPORT extern const char kDisablePresentationAPI[];
CONTENT_EXPORT extern const char kDisablePushStateThrottle[];
CONTENT_EXPORT extern const char kDisableReadingFromCanvas[];
extern const char kDisableRemoteFonts[];
CONTENT_EXPORT extern const char kDisableRemotePlaybackAPI[];
CONTENT_EXPORT extern const char kDisableRendererBackgrounding[];
CONTENT_EXPORT extern const char kDisableResourceScheduler[];
CONTENT_EXPORT extern const char kDisableSharedWorkers[];
CONTENT_EXPORT extern const char kDisableSkiaRuntimeOpts[];
CONTENT_EXPORT extern const char kDisableSmoothScrolling[];
CONTENT_EXPORT extern const char kDisableSoftwareRasterizer[];
CONTENT_EXPORT extern const char kDisableSpeechAPI[];
CONTENT_EXPORT extern const char kDisableSpeechSynthesisAPI[];
CONTENT_EXPORT extern const char kDisableThreadedCompositing[];
extern const char kDisableV8IdleTasks[];
CONTENT_EXPORT extern const char kDisableWebRtcEncryption[];
CONTENT_EXPORT extern const char kDisableWebGLImageChromium[];
CONTENT_EXPORT extern const char kDisableWebSecurity[];
CONTENT_EXPORT extern const char kDisableZeroCopyDxgiVideo[];
CONTENT_EXPORT extern const char kDomAutomationController[];
extern const char kDisable2dCanvasClipAntialiasing[];
CONTENT_EXPORT extern const char kEnableAggressiveDOMStorageFlushing[];
CONTENT_EXPORT extern const char kEnableAutomation[];
CONTENT_EXPORT extern const char kEnableBlinkFeatures[];
CONTENT_EXPORT extern const char kEnableCaretBrowsing[];
CONTENT_EXPORT extern const char kEnableDisplayList2dCanvas[];
CONTENT_EXPORT extern const char kEnableExperimentalCookieFeatures[];
CONTENT_EXPORT extern const char kEnableExperimentalWebAssemblyFeatures[];
CONTENT_EXPORT extern const char kEnableExperimentalWebPlatformFeatures[];
CONTENT_EXPORT extern const char kEnableBlinkTestFeatures[];
CONTENT_EXPORT extern const char kEnableGpuMemoryBufferVideoFrames[];
CONTENT_EXPORT extern const char kEnableIsolatedWebAppsInRenderer[];
CONTENT_EXPORT extern const char kEnableLCDText[];
CONTENT_EXPORT extern const char kEnableLogging[];
CONTENT_EXPORT extern const char kEnableNetworkInformationDownlinkMax[];
CONTENT_EXPORT extern const char kEnableCanvas2DLayers[];
CONTENT_EXPORT extern const char kEnablePluginPlaceholderTesting[];
CONTENT_EXPORT extern const char kEnablePreciseMemoryInfo[];
CONTENT_EXPORT extern const char kEnablePrivacySandboxAdsApis[];
CONTENT_EXPORT extern const char kEnableServiceBinaryLauncher[];
extern const char kEnableSkiaBenchmarking[];
CONTENT_EXPORT extern const char kEnableSmoothScrolling[];
CONTENT_EXPORT extern const char kEnableSpatialNavigation[];
CONTENT_EXPORT extern const char kEnableStrictMixedContentChecking[];
CONTENT_EXPORT extern const char kEnableStrictPowerfulFeatureRestrictions[];
CONTENT_EXPORT extern const char kEnableTracingFraction[];
CONTENT_EXPORT extern const char kEnableUserMediaScreenCapturing[];
CONTENT_EXPORT extern const char kEnableViewport[];
CONTENT_EXPORT extern const char kEnableVtune[];
CONTENT_EXPORT extern const char kEnableWebAuthDeprecatedMojoTestingApi[];
CONTENT_EXPORT extern const char kEnableWebGLDeveloperExtensions[];
CONTENT_EXPORT extern const char kEnableWebGLDraftExtensions[];
CONTENT_EXPORT extern const char kEnableWebGLImageChromium[];
CONTENT_EXPORT extern const char kEnableWebRtcSrtpEncryptedHeaders[];
CONTENT_EXPORT extern const char kEnforceWebRtcIPPermissionCheck[];
CONTENT_EXPORT extern const char kEnableWebVR[];
CONTENT_EXPORT extern const char kFileUrlPathAlias[];
CONTENT_EXPORT extern const char kForceDisplayList2dCanvas[];
CONTENT_EXPORT extern const char kForcePresentationReceiverForTesting[];
CONTENT_EXPORT extern const char kForceWebRtcIPHandlingPolicy[];
extern const char kGpuLauncher[];
CONTENT_EXPORT extern const char kGpuProcess[];
CONTENT_EXPORT extern const char kGpuSandboxStartEarly[];
CONTENT_EXPORT extern const char kGpuStartupDialog[];
CONTENT_EXPORT extern const char kHideScrollbars[];
CONTENT_EXPORT extern const char kInProcessGPU[];
CONTENT_EXPORT extern const char kIPCConnectionTimeout[];
CONTENT_EXPORT extern const char kIsolateOrigins[];
CONTENT_EXPORT extern const char kIsolationByDefault[];
CONTENT_EXPORT extern const char kJavaScriptHarmony[];
CONTENT_EXPORT extern const char kLaunchAsBrowser[];
CONTENT_EXPORT extern const char kLogGpuControlListDecisions[];
CONTENT_EXPORT extern const char kLoggingLevel[];
CONTENT_EXPORT extern const char kLogFile[];
CONTENT_EXPORT extern const char kLogMissingUnloadACK[];
extern const char kMaxActiveWebGLContexts[];
CONTENT_EXPORT extern const char kMaxDecodedImageSizeMb[];
CONTENT_EXPORT extern const char kMaxWebMediaPlayerCount[];
CONTENT_EXPORT extern const char kMessageLoopTypeUi[];
CONTENT_EXPORT extern const char kMHTMLGeneratorOption[];
CONTENT_EXPORT extern const char kMHTMLSkipNostoreMain[];
CONTENT_EXPORT extern const char kMHTMLSkipNostoreAll[];
CONTENT_EXPORT extern const char kMockCertVerifierDefaultResultForTesting[];
CONTENT_EXPORT extern const char kMojoLocalStorage[];
CONTENT_EXPORT extern const char kNoUnsandboxedZygote[];
CONTENT_EXPORT extern const char kNoZygote[];
CONTENT_EXPORT extern const char kOverrideLanguageDetection[];
CONTENT_EXPORT extern const char kPdfRenderer[];
CONTENT_EXPORT extern const char kPpapiInProcess[];
extern const char kPpapiPluginLauncher[];
CONTENT_EXPORT extern const char kPpapiPluginProcess[];
extern const char kPpapiStartupDialog[];
CONTENT_EXPORT extern const char kPrivateAggregationDeveloperMode[];
CONTENT_EXPORT extern const char kProcessPerSite[];
CONTENT_EXPORT extern const char kProcessPerTab[];
CONTENT_EXPORT extern const char kProcessType[];
CONTENT_EXPORT extern const char kProtectedAudiencesConsentedDebugToken[];
CONTENT_EXPORT extern const char kPullToRefresh[];
CONTENT_EXPORT extern const char kQuotaChangeEventInterval[];
CONTENT_EXPORT extern const char kReduceAcceptLanguage[];
CONTENT_EXPORT extern const char kReduceUserAgentMinorVersion[];
CONTENT_EXPORT extern const char kReduceUserAgentPlatformOsCpu[];
CONTENT_EXPORT extern const char kRegisterPepperPlugins[];
CONTENT_EXPORT extern const char kRemoteDebuggingPipe[];
CONTENT_EXPORT extern const char kRemoteDebuggingPort[];
CONTENT_EXPORT extern const char kRemoteAllowOrigins[];
CONTENT_EXPORT extern const char kRendererClientId[];
extern const char kRendererCmdPrefix[];
CONTENT_EXPORT extern const char kRendererProcess[];
CONTENT_EXPORT extern const char kRendererProcessLaunchTimeTicks[];
CONTENT_EXPORT extern const char kRendererProcessLimit[];
CONTENT_EXPORT extern const char kRendererStartupDialog[];
CONTENT_EXPORT extern const char kRunManualTestsFlag[];
extern const char kSandboxIPCProcess[];
#if !BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT extern const char kSharedArrayBufferUnrestrictedAccessAllowed[];
#endif
CONTENT_EXPORT extern const char kSharedFiles[];
CONTENT_EXPORT extern const char kSingleProcess[];
CONTENT_EXPORT extern const char kSitePerProcess[];
CONTENT_EXPORT extern const char kDisableSiteIsolation[];
CONTENT_EXPORT extern const char kStartFullscreen[];
CONTENT_EXPORT extern const char kStatsCollectionController[];
extern const char kSkiaFontCacheLimitMb[];
extern const char kSkiaResourceCacheLimitMb[];
CONTENT_EXPORT extern const char kTestType[];
CONTENT_EXPORT extern const char kTimeTicksAtUnixEpoch[];
CONTENT_EXPORT extern const char kTouchEventFeatureDetection[];
CONTENT_EXPORT extern const char kTouchEventFeatureDetectionAuto[];
CONTENT_EXPORT extern const char kTouchEventFeatureDetectionEnabled[];
CONTENT_EXPORT extern const char kTouchEventFeatureDetectionDisabled[];
CONTENT_EXPORT extern const char kUseFakeCodecForPeerConnection[];
CONTENT_EXPORT extern const char kUseFakeUIForDigitalIdentity[];
CONTENT_EXPORT extern const char kUseFakeUIForFedCM[];
CONTENT_EXPORT extern const char kUseFakeUIForMediaStream[];
CONTENT_EXPORT extern const char kVideoImageTextureTarget[];
#if BUILDFLAG(IS_WIN)
CONTENT_EXPORT extern const char kUseSkiaFontManager[];
#endif
#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(INCLUDE_BOTH_V8_SNAPSHOTS)
CONTENT_EXPORT extern const char kUseContextSnapshotSwitch[];
#endif
CONTENT_EXPORT extern const char kUseMobileUserAgent[];
CONTENT_EXPORT extern const char kUseMockCertVerifierForTesting[];
extern const char kUtilityCmdPrefix[];
CONTENT_EXPORT extern const char kUtilityProcess[];
CONTENT_EXPORT extern const char kUtilityStartupDialog[];
CONTENT_EXPORT extern const char kUtilitySubType[];
CONTENT_EXPORT extern const char kV8CacheOptions[];
CONTENT_EXPORT extern const char kVerifyPixels[];
CONTENT_EXPORT extern const char kWaitForDebuggerChildren[];
CONTENT_EXPORT extern const char kWaitForDebuggerOnNavigation[];
CONTENT_EXPORT extern const char kWaitForDebuggerWebUI[];
CONTENT_EXPORT extern const char kWebAuthRemoteDesktopSupport[];
CONTENT_EXPORT extern const char kWebglAntialiasingMode[];
CONTENT_EXPORT extern const char kWebglMSAASampleCount[];
CONTENT_EXPORT extern const char kWebOtpBackend[];
CONTENT_EXPORT extern const char kWebOtpBackendSmsVerification[];
CONTENT_EXPORT extern const char kWebOtpBackendUserConsent[];
CONTENT_EXPORT extern const char kWebOtpBackendAuto[];
CONTENT_EXPORT extern const char kWebRtcLocalEventLogging[];
extern const char kWebRtcMaxCaptureFramerate[];
CONTENT_EXPORT extern const char kWebXrForceRuntime[];
CONTENT_EXPORT extern const char kWebXrRuntimeNone[];
CONTENT_EXPORT extern const char kWebXrRuntimeArCore[];
CONTENT_EXPORT extern const char kWebXrRuntimeCardboard[];
CONTENT_EXPORT extern const char kWebXrRuntimeOrientationSensors[];
CONTENT_EXPORT extern const char kWebXrRuntimeOpenXr[];
CONTENT_EXPORT extern const char kZygoteCmdPrefix[];
CONTENT_EXPORT extern const char kZygoteProcess[];

#if BUILDFLAG(IS_ANDROID)
CONTENT_EXPORT extern const char kDisableMediaSessionAPI[];
CONTENT_EXPORT extern const char kDisableOoprDebugCrashDump[];
CONTENT_EXPORT extern const char kDisableScreenOrientationLock[];
CONTENT_EXPORT extern const char kDisableSiteIsolationForPolicy[];
CONTENT_EXPORT extern const char kDisableTimeoutsForProfiling[];
CONTENT_EXPORT extern const char kEnableAdaptiveSelectionHandleOrientation[];
CONTENT_EXPORT extern const char kEnableLongpressDragSelection[];
CONTENT_EXPORT extern const char kForceOnlineConnectionStateForIndicator[];
CONTENT_EXPORT extern const char kRemoteDebuggingSocketName[];
CONTENT_EXPORT extern const char kRendererWaitForJavaDebugger[];
#endif

#if BUILDFLAG(IS_IOS)
CONTENT_EXPORT extern const char kPreventResizingContentsForTesting[];
#endif

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
CONTENT_EXPORT extern const char kEnableSpeechDispatcher[];
CONTENT_EXPORT extern const char kLLVMProfileFile[];
#endif

#if BUILDFLAG(IS_WIN)
CONTENT_EXPORT extern const char kPrefetchArgumentRenderer[];
CONTENT_EXPORT extern const char kPrefetchArgumentGpu[];
CONTENT_EXPORT extern const char kPrefetchArgumentPpapi[];
CONTENT_EXPORT extern const char kPrefetchArgumentPpapiBroker[];
CONTENT_EXPORT extern const char kPrefetchArgumentOther[];
// This switch contains the device scale factor passed to certain processes
// like renderers, etc.
CONTENT_EXPORT extern const char kDeviceScaleFactor[];
CONTENT_EXPORT extern const char kDisableLegacyIntermediateWindow[];
// Switch to pass the font cache shared memory handle to the renderer.
CONTENT_EXPORT extern const char kFontCacheSharedHandle[];
CONTENT_EXPORT extern const char kPpapiAntialiasedTextEnabled[];
CONTENT_EXPORT extern const char kPpapiSubpixelRenderingSetting[];
CONTENT_EXPORT extern const char kRaiseTimerFrequency[];
CONTENT_EXPORT extern const char kGpu2StartupDialog[];
CONTENT_EXPORT extern const char kAudioProcessHighPriority[];
// Pipe names for the incoming and outbound messages.
CONTENT_EXPORT extern const char kRemoteDebuggingIoPipes[];
#endif

#if defined(ENABLE_IPC_FUZZER)
extern const char kIpcDumpDirectory[];
extern const char kIpcFuzzerTestcase[];
#endif

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace switches

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_SWITCHES_H_
