// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "media" command-line switches.

#ifndef MEDIA_BASE_MEDIA_SWITCHES_H_
#define MEDIA_BASE_MEDIA_SWITCHES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/media_export.h"
#include "media/gpu/buildflags.h"
#include "media/media_buildflags.h"
#include "ui/gl/angle_implementation.h"

namespace base {
class CommandLine;
}

namespace switches {

MEDIA_EXPORT extern const char kAudioBufferSize[];

#if BUILDFLAG(ENABLE_PASSTHROUGH_AUDIO_CODECS)
MEDIA_EXPORT extern const char kAudioCodecsFromEDID[];
#endif  // BUILDFLAG(ENABLE_PASSTHROUGH_AUDIO_CODECS)

MEDIA_EXPORT extern const char kAutoplayPolicy[];

MEDIA_EXPORT extern const char kDisableAudioInput[];
MEDIA_EXPORT extern const char kDisableAudioOutput[];

MEDIA_EXPORT extern const char kFailAudioStreamCreation[];

MEDIA_EXPORT extern const char kVideoThreads[];

MEDIA_EXPORT extern const char kDisableBackgroundMediaSuspend[];

MEDIA_EXPORT extern const char kReportVp9AsAnUnsupportedMimeType[];

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FREEBSD) || \
    BUILDFLAG(IS_SOLARIS)
MEDIA_EXPORT extern const char kAlsaInputDevice[];
MEDIA_EXPORT extern const char kAlsaOutputDevice[];
#endif

#if BUILDFLAG(IS_WIN)
MEDIA_EXPORT extern const char kEnableExclusiveAudio[];
MEDIA_EXPORT extern const char kForceWaveAudio[];
MEDIA_EXPORT extern const char kTrySupportedChannelLayouts[];
MEDIA_EXPORT extern const char kWaveOutBuffers[];
#endif

#if BUILDFLAG(IS_FUCHSIA)
MEDIA_EXPORT extern const char kEnableProtectedVideoBuffers[];
MEDIA_EXPORT extern const char kForceProtectedVideoOutputBuffers[];
MEDIA_EXPORT extern const char kMinVideoDecoderOutputBufferSize[];
MEDIA_EXPORT extern const char kAudioCapturerWithEchoCancellation[];
#endif

#if BUILDFLAG(USE_CRAS)
MEDIA_EXPORT extern const char kUseCras[];
MEDIA_EXPORT extern const char kSystemAecEnabled[];
#endif

MEDIA_EXPORT extern const char
    kUnsafelyAllowProtectedMediaIdentifierForDomain[];

MEDIA_EXPORT extern const char kAutoGrantCapturedSurfaceControlPrompt[];
MEDIA_EXPORT extern const char kUseFakeDeviceForMediaStream[];
MEDIA_EXPORT extern const char kUseFileForFakeVideoCapture[];
MEDIA_EXPORT extern const char kUseFileForFakeAudioCapture[];
MEDIA_EXPORT extern const char kUseFakeMjpegDecodeAccelerator[];
MEDIA_EXPORT extern const char kDisableAcceleratedMjpegDecode[];

MEDIA_EXPORT extern const char kMuteAudio[];

MEDIA_EXPORT extern const char kDisableRTCSmoothnessAlgorithm[];

MEDIA_EXPORT extern const char kForceVideoOverlays[];

MEDIA_EXPORT extern const char kMSEAudioBufferSizeLimitMb[];
MEDIA_EXPORT extern const char kMSEVideoBufferSizeLimitMb[];

MEDIA_EXPORT extern const char kClearKeyCdmPathForTesting[];
MEDIA_EXPORT extern const char kOverrideEnabledCdmInterfaceVersion[];
MEDIA_EXPORT extern const char kOverrideHardwareSecureCodecsForTesting[];
MEDIA_EXPORT extern const char kEnableLiveCaptionPrefForTesting[];

#if BUILDFLAG(IS_CHROMEOS)
MEDIA_EXPORT extern const char kLacrosEnablePlatformHevc[];
MEDIA_EXPORT extern const char kLacrosUseChromeosProtectedMedia[];
MEDIA_EXPORT extern const char kLacrosUseChromeosProtectedAv1[];
MEDIA_EXPORT extern const char kAllowRAInDevMode[];
MEDIA_EXPORT extern const char kCrosWidevineBundledDir[];
MEDIA_EXPORT extern const char kCrosWidevineComponentUpdatedHintFile[];
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace autoplay {

MEDIA_EXPORT extern const char kDocumentUserActivationRequiredPolicy[];
MEDIA_EXPORT extern const char kNoUserGestureRequiredPolicy[];
MEDIA_EXPORT extern const char kUserGestureRequiredPolicy[];

}  // namespace autoplay

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
MEDIA_EXPORT extern const char kHardwareVideoDecodeFrameRate[];
#endif

#if BUILDFLAG(USE_V4L2_CODEC)
MEDIA_EXPORT extern const char kEnablePrimaryNodeAccessForVkmsTesting[];
#endif

// NOTE: callers should always use the free functions in
// /media/cast/encoding/encoding_support.h instead of accessing these features
// directly.
//
// TODO(https://crbug.com/1453388): Guard Cast Sender flags with !IS_ANDROID.
//
// If enabled, completely disables use of H264 hardware encoding for Cast
// Streaming sessions. Takes precedence over
// kCastStreamingForceEnableHardwareH264.
MEDIA_EXPORT extern const char kCastStreamingForceDisableHardwareH264[];

// If enabled, completely disables use of VP8 hardware encoding for Cast
// Streaming sessions. Takes precedence over
// kCastStreamingForceEnableHardwareVp8.
MEDIA_EXPORT extern const char kCastStreamingForceDisableHardwareVp8[];

// If enabled, completely disables use of VP9 hardware encoding for Cast
// Streaming sessions. Takes precedence over
// kCastStreamingForceEnableHardwareVp9.
MEDIA_EXPORT extern const char kCastStreamingForceDisableHardwareVp9[];

// If enabled, allows use of H264 hardware encoding for Cast Streaming sessions,
// even on platforms where it is disabled due to performance and reliability
// issues. kCastStreamingForceDisableHardwareH264 must be disabled for this flag
// to take effect.
MEDIA_EXPORT extern const char kCastStreamingForceEnableHardwareH264[];

// If enabled, allows use of VP8 hardware encoding for Cast Streaming sessions,
// even on platforms where it is disabled due to performance and reliability
// issues. kCastStreamingForceDisableHardwareVp8 must be disabled for this flag
// to take effect.
MEDIA_EXPORT extern const char kCastStreamingForceEnableHardwareVp8[];

// If enabled, allows use of VP9 hardware encoding for Cast Streaming sessions,
// even on platforms where it is disabled due to performance and reliability
// issues. kCastStreamingForceDisableHardwareVp9 must be disabled for this flag
// to take effect.
MEDIA_EXPORT extern const char kCastStreamingForceEnableHardwareVp9[];

#if !BUILDFLAG(IS_ANDROID)
// If enabled, overrides the target playout delay for a casting mirroring
// session. The value will be parsed as milliseconds. Lowering this value will
// result in a lower end to end latency, but could come at the cost of other
// quality standards such as dropped frames or FPS.
MEDIA_EXPORT extern const char kCastMirroringTargetPlayoutDelay[];
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace switches

namespace media {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

MEDIA_EXPORT BASE_DECLARE_FEATURE(kAudioFocusDuckFlash);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAudioFocusLossSuspendMediaSession);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAudioRendererAlgorithmParameters);
MEDIA_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kAudioRendererAlgorithmStartingCapacityForEncrypted;
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAutoPictureInPictureForVideoPlayback);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAutoplayDisableSettings);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAVDColorSpaceChanges);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kBuiltInH264Decoder);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_FUCHSIA)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCameraMicEffects);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS) &&
        // !BUILDFLAG(IS_FUCHSIA)

MEDIA_EXPORT BASE_DECLARE_FEATURE(kCastLoopbackAudioToAudioReceivers);

// NOTE: callers should always use the free functions in
// /media/cast/encoding/encoding_support.h instead of accessing these features
// directly.
// TODO(https://crbug.com/1453388): Guard Cast Sender flags with !IS_ANDROID.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCastStreamingAv1);
MEDIA_EXPORT BASE_DECLARE_FEATURE(
    kCastStreamingExponentialVideoBitrateAlgorithm);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCastStreamingPerformanceOverlay);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCastStreamingVp8);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCastStreamingVp9);
#if BUILDFLAG(IS_MAC)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCastStreamingMacHardwareH264);
#endif
#if BUILDFLAG(IS_WIN)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCastStreamingWinHardwareH264);
#endif
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCdmHostVerification);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCdmProcessSiteIsolation);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kContextMenuCopyVideoFrame);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kContextMenuSaveVideoFrameAs);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kContextMenuSearchForVideoFrame);
#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kChromeWideEchoCancellation);
MEDIA_EXPORT extern const base::FeatureParam<bool>
    kChromeWideEchoCancellationMinimizeResampling;
MEDIA_EXPORT extern const base::FeatureParam<double>
    kChromeWideEchoCancellationDynamicMixingTimeout;
MEDIA_EXPORT extern const base::FeatureParam<bool>
    kChromeWideEchoCancellationAllowAllSampleRates;
MEDIA_EXPORT BASE_DECLARE_FEATURE(kDecreaseProcessingAudioFifoSize);
MEDIA_EXPORT extern const base::FeatureParam<int>
    kDecreaseProcessingAudioFifoSizeValue;
#endif
#if BUILDFLAG(IS_CHROMEOS)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSSystemAEC);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSSystemAECDeactivatedGroups);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSEnforceSystemAecNsAgc);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSEnforceSystemAecNs);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSEnforceSystemAecAgc);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSEnforceSystemAec);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSDspBasedAecDeactivatedGroups);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSDspBasedNsDeactivatedGroups);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSDspBasedAgcDeactivatedGroups);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSDspBasedAecAllowed);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSDspBasedNsAllowed);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSDspBasedAgcAllowed);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kIgnoreUiGains);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kShowForceRespectUiGainsToggle);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSSystemVoiceIsolationOption);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAudioFlexibleLoopbackForSystemLoopback);
#endif
MEDIA_EXPORT BASE_DECLARE_FEATURE(kD3D11VideoDecoderUseSharedHandle);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kDedicatedMediaServiceThread);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kDeferAudioFocusUntilAudible);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kDocumentPictureInPictureAnimateResize);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kDocumentPictureInPictureCapture);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kEnableTabMuting);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kExternalClearKeyForTesting);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kFailUrlProvisionFetcherForTesting);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kFallbackAfterDecodeError);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kFeatureManagementLiveTranslateCrOS);
#if !BUILDFLAG(IS_ANDROID)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kFileDialogsBlockPictureInPicture);
#endif  // !BUILDFLAG(IS_ANDROID)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kGlobalMediaControls);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kGlobalMediaControlsAutoDismiss);
#if !BUILDFLAG(IS_CHROMEOS)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kGlobalMediaControlsUpdatedUI);
#endif  // !BUILDFLAG(IS_CHROMEOS)
#if !BUILDFLAG(IS_ANDROID)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaRemotingWithoutFullscreen);
#endif
MEDIA_EXPORT BASE_DECLARE_FEATURE(kGlobalMediaControlsPictureInPicture);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kGlobalMediaControlsSeamlessTransfer);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kHardwareMediaKeyHandling);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kHardwareSecureDecryption);
MEDIA_EXPORT extern const base::FeatureParam<bool>
    kHardwareSecureDecryptionForceSupportClearLead;
MEDIA_EXPORT BASE_DECLARE_FEATURE(kHardwareSecureDecryptionExperiment);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kHardwareSecureDecryptionFallback);
MEDIA_EXPORT extern const base::FeatureParam<bool>
    kHardwareSecureDecryptionFallbackPerSite;
MEDIA_EXPORT extern const base::FeatureParam<int>
    kHardwareSecureDecryptionFallbackMinDisablingDays;
MEDIA_EXPORT extern const base::FeatureParam<int>
    kHardwareSecureDecryptionFallbackMaxDisablingDays;
MEDIA_EXPORT extern const base::FeatureParam<bool>
    kHardwareSecureDecryptionFallbackOnHardwareContextReset;
MEDIA_EXPORT BASE_DECLARE_FEATURE(kHideIncognitoMediaMetadata);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kInternalMediaSession);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kKeyPressMonitoring);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kOnDeviceWebSpeech);
#if BUILDFLAG(IS_CHROMEOS)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLacrosUseAshWidevine);
#endif
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLiveCaption);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLiveCaptionAutomaticLanguageDownload);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLiveCaptionRightClick);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLiveCaptionLogFlickerRate);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLiveCaptionMultiLanguage);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLiveCaptionExperimentalLanguages);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLiveCaptionUseGreedyTextStabilizer);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLiveCaptionUseWaitK);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLiveCaptionWebAudio);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLiveTranslate);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLowDelayVideoRenderingOnLiveStream);
#if BUILDFLAG(IS_MAC)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMacLoopbackAudioForScreenShare);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseSCContentSharingPicker);
#endif  // BUILDFLAG(IS_MAC)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaCapabilitiesQueryGpuFactories);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaCapabilitiesWithParameters);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaCastOverlayButton);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaEngagementBypassAutoplayPolicies);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaEngagementHTTPSOnly);
#if BUILDFLAG(IS_WIN)
MEDIA_EXPORT extern const base::FeatureParam<std::string>
    kMediaFoundationClearKeyCdmPathForTesting;
MEDIA_EXPORT BASE_DECLARE_FEATURE(kEnableFaultyGPUForMediaFoundation);
#endif  // BUILDFLAG(IS_WIN)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaLearningExperiment);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaLearningFramework);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaLearningSmoothnessExperiment);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaOptimizer);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaPowerExperiment);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMemoryPressureBasedSourceBufferGC);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kOverlayFullscreenVideo);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPauseBackgroundMutedAudio);
#if !BUILDFLAG(IS_ANDROID)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPictureInPictureOcclusionTracking);
#endif  // !BUILDFLAG(IS_ANDROID)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPlatformAudioEncoder);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kEnableRtcpReporting);
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPlatformHEVCDecoderSupport);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPlatformHEVCEncoderSupport);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPlaybackSpeedButton);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPreloadMediaEngagementData);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPreloadMetadataSuspend);
#if BUILDFLAG(IS_LINUX)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPulseaudioLoopbackForCast);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPulseaudioLoopbackForScreenShare);
#endif  // BUILDFLAG(IS_LINUX)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kRecordMediaEngagementScores);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kRecordWebAudioEngagement);
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kReduceHardwareVideoDecoderBuffers);
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kResumeBackgroundVideo);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kRevokeMediaSourceObjectURLOnAttach);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kShareThisTabInsteadButtonGetDisplayMedia);
MEDIA_EXPORT BASE_DECLARE_FEATURE(
    kShareThisTabInsteadButtonGetDisplayMediaAudio);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kSpeakerChangeDetection);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kSpecCompliantCanPlayThrough);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kSuspendMutedAudio);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUnifiedAutoplay);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseAndroidOverlayForSecureOnly);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseFakeDeviceForMediaStream);
#if BUILDFLAG(IS_LINUX)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAcceleratedVideoDecodeLinux);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAcceleratedVideoDecodeLinuxGL);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAcceleratedVideoEncodeLinux);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVaapiIgnoreDriverChecks);
#endif  // BUILDFLAG(IS_LINUX)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVaapiOnNvidiaGPUs);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVaapiLowPowerEncoderGen9x);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVaapiEnforceVideoMinMaxResolution);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVaapiVideoMinResolutionForPerformance);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVaapiVP8Encoder);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVaapiVP9Encoder);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVaapiAV1Encoder);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kGlobalVaapiLock);
#if defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVaapiH264TemporalLayerHWEncoding);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVaapiH264SWBitrateController);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVaapiVp8TemporalLayerHWEncoding);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVaapiVp9SModeHWEncoding);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVSyncMjpegDecoding);
#endif  // defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kV4L2FlatStatefulVideoDecoder);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kV4L2H264TemporalLayerHWEncoding);
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVideoBlitColorAccuracy);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVideoPictureInPictureControlsUpdate2024);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCastVideoEncoderFrameDrop);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kWebCodecsVideoEncoderFrameDrop);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kWebRTCHardwareVideoEncoderFrameDrop);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kWebRTCColorAccuracy);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kWebContentsCaptureHiDpi);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kWebrtcMediaCapabilitiesParameters);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kResolutionBasedDecoderPriority);

#if BUILDFLAG(IS_ANDROID)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAllowNonSecureOverlays);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaCodecBlockModel);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaCodecCodedSizeGuessing);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaCodecElideEOS);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaControlsExpandGesture);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaDrmPersistentLicense);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaDrmPreprovisioning);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaDrmPreprovisioningAtStartup);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaDrmGetStatusForPolicy);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kHlsPlayer);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kRequestSystemAudioFocus);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseAudioLatencyFromHAL);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUsePooledSharedImageVideoProvider);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAllowMediaCodecSoftwareDecoder);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAllowMediaCodecCallsInSeparateProcess);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_HLS_DEMUXER)
// The feature |kHlsPlayer| enables the use of Android's builtin media-player
// based HLS implementation, which chrome currently relies on when playing
// on android, while this feature enabled chrome's built-in HLS parser and
// demuxer. When this feature is enabled, the media-player based HLS player
// will NOT be used. This will roll out first on android, but will eventually
// land in desktop chrome as well.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kBuiltInHlsPlayer);

// This feature enables the buildin hls player to play and demux additional
// media containers, including Fragmented and unfragmented MP4, as well as
// raw AAC bytestreams. It does nothing if kBuiltInHlsPlayer is disabled.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kBuiltInHlsMP4);

// This feature enables statistics to be collected from MediaPlayer-based HLS
// playbacks using the builtin HLS manifest parser. It is enabled by default
// to act as a kill switch in the event of crashes.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaPlayerHlsStatistics);
#endif  // BUILDFLAG(ENABLE_HLS_DEMUXER)

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kChromeOSHWAV1Decoder);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kChromeOSHWVBREncoding);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLimitConcurrentDecoderInstances);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUSeSequencedTaskRunnerForVEA);
#if defined(ARCH_CPU_ARM_FAMILY)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseGLForScaling);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPreferGLImageProcessor);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPreferSoftwareMT21);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kEnableProtectedVulkanDetiling);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kEnableArmHwdrm10bitOverlays);
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kEnableArmHwdrm);
#endif  // BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
#endif  // defined(ARCH_CPU_ARM_FAMILY)
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

#if BUILDFLAG(IS_WIN)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kDirectShowGetPhotoState);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kIncludeIRCamerasInDeviceEnumeration);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationVideoCapture);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationVP8Decoding);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationUseSoftwareRateCtrl);

// For feature check of kMediaFoundationD3D11VideoCapture at runtime,
// please use IsMediaFoundationD3D11VideoCaptureEnabled() instead.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationD3D11VideoCapture);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationD3D11VideoCaptureZeroCopy);

MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationClearPlayback);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAllowMediaFoundationFrameServerMode);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kD3D11Vp9kSVCHWDecoding);

// Strategy affecting how Media Foundation Renderer determines its rendering
// mode when used with clear video media. This strategy does not impact
// protected media which must always use Direct Composition mode.
enum class MediaFoundationClearRenderingStrategy {
  // The renderer will operate in Direct Composition mode (e.g. windowless
  // swapchain).
  kDirectComposition,
  // The renderer will operate in Frame Server mode.
  kFrameServer,
  // The renderer is allowed to switch between Direct Composition & Frame Server
  // mode at its discretion.
  kDynamic,
};

// Under this feature, a given MediaFoundationClearRenderingStrategy param is
// used by the Media Foundation Renderer for Clear content scenarios.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationClearRendering);
MEDIA_EXPORT extern const base::FeatureParam<
    MediaFoundationClearRenderingStrategy>
    kMediaFoundationClearRenderingStrategyParam;

// Enables the batch audio/video buffers reading for media playback.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationBatchRead);

// Specify the batch read count between client renderer and remote renderer,
// default value is 1.
MEDIA_EXPORT extern const base::FeatureParam<int> kBatchReadCount;

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPlatformEncryptedDolbyVision);
MEDIA_EXPORT BASE_DECLARE_FEATURE(
    kAllowClearDolbyVisionInMseWhenPlatformEncryptedDvEnabled);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kExposeOutOfProcessVideoDecodingToLacros);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kBackgroundListening);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
// Note: please use GetOutOfProcessVideoDecodingMode() to determine if OOP-VD is
// enabled instead of directly checking this feature flag. The reason is that
// that function may perform checks beyond the feature flag.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseOutOfProcessVideoDecoding);

// Note: please use GetOutOfProcessVideoDecodingMode() to determine if GTFO
// OOP-VD is enabled instead of directly checking this feature flag. The reason
// is that that function may perform checks beyond the feature flag.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseGTFOOutOfProcessVideoDecoding);
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseOutOfProcessVideoEncoding);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseSequencedTaskRunnerForMediaService);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseSequencedTaskRunnerForMojoVEAProvider);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseTaskRunnerForMojoVEAService);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseTaskRunnerForMojoAudioDecoderService);

#if BUILDFLAG(IS_FUCHSIA)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kFuchsiaMediacodecVideoEncoder);
#endif  // BUILDFLAG(IS_FUCHSIA)

MEDIA_EXPORT BASE_DECLARE_FEATURE(kVideoDecodeBatching);

MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseWindowBoundsForPip);

MEDIA_EXPORT BASE_DECLARE_FEATURE(kFFmpegAllowLists);

MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaLogToConsole);

MEDIA_EXPORT BASE_DECLARE_FEATURE(kLibvpxUseChromeThreads);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLibaomUseChromeThreads);

#if BUILDFLAG(IS_WIN)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kD3D12VideoDecoder);

MEDIA_EXPORT extern const base::FeatureParam<double> kAudioOffloadBufferTimeMs;
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAudioOffload);
#endif

#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationAcceleratedEncodeOnArm64);
#endif

MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaSharedBitmapToSharedImage);

#if BUILDFLAG(IS_WIN)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationD3DVideoProcessing);
#endif

// Based on a |command_line| and the current platform, returns the effective
// autoplay policy. In other words, it will take into account the default policy
// if none is specified via the command line and options passed for testing.
// Returns one of the possible autoplay policy switches from the
// switches::autoplay namespace.
MEDIA_EXPORT std::string GetEffectiveAutoplayPolicy(
    const base::CommandLine& command_line);

MEDIA_EXPORT bool IsChromeWideEchoCancellationEnabled();
MEDIA_EXPORT bool IsDedicatedMediaServiceThreadEnabled(
    gl::ANGLEImplementation impl);
MEDIA_EXPORT int GetProcessingAudioFifoSize();
MEDIA_EXPORT bool IsHardwareSecureDecryptionEnabled();
MEDIA_EXPORT bool IsLiveTranslateEnabled();
MEDIA_EXPORT bool IsVideoCaptureAcceleratedJpegDecodingEnabled();

#if BUILDFLAG(IS_WIN)
MEDIA_EXPORT bool IsMediaFoundationD3D11VideoCaptureEnabled();
#endif

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
enum class OOPVDMode {
  kDisabled,
  kEnabledWithGpuProcessAsProxy,     // AKA "regular" OOP-VD; go/oop-vd-dd.
  kEnabledWithoutGpuProcessAsProxy,  // AKA GTFO OOP-VD; go/oopvd-gtfo-dd.
};
MEDIA_EXPORT OOPVDMode GetOutOfProcessVideoDecodingMode();
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

// Return bitmask of audio formats supported by EDID.
MEDIA_EXPORT uint32_t GetPassthroughAudioFormats();

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_SWITCHES_H_
