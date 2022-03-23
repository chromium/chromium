// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "media/media_buildflags.h"

namespace base {
class CommandLine;
}

namespace switches {

MEDIA_EXPORT extern const char kAudioBufferSize[];

MEDIA_EXPORT extern const char kAudioServiceQuitTimeoutMs[];

MEDIA_EXPORT extern const char kAutoplayPolicy[];

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
MEDIA_EXPORT extern const char kDisableAudioInput[];
MEDIA_EXPORT extern const char kUseOverlaysForVideo[];
#endif

#if defined(USE_CRAS)
MEDIA_EXPORT extern const char kUseCras[];
#endif

MEDIA_EXPORT extern const char
    kUnsafelyAllowProtectedMediaIdentifierForDomain[];

MEDIA_EXPORT extern const char kUseFakeDeviceForMediaStream[];
MEDIA_EXPORT extern const char kUseFileForFakeVideoCapture[];
MEDIA_EXPORT extern const char kUseFileForFakeAudioCapture[];
MEDIA_EXPORT extern const char kUseFakeMjpegDecodeAccelerator[];
MEDIA_EXPORT extern const char kDisableAcceleratedMjpegDecode[];

MEDIA_EXPORT extern const char kRequireAudioHardwareForTesting[];
MEDIA_EXPORT extern const char kMuteAudio[];

MEDIA_EXPORT extern const char kVideoUnderflowThresholdMs[];

MEDIA_EXPORT extern const char kDisableRTCSmoothnessAlgorithm[];

MEDIA_EXPORT extern const char kForceVideoOverlays[];

MEDIA_EXPORT extern const char kMSEAudioBufferSizeLimitMb[];
MEDIA_EXPORT extern const char kMSEVideoBufferSizeLimitMb[];

MEDIA_EXPORT extern const char kClearKeyCdmPathForTesting[];
MEDIA_EXPORT extern const char kOverrideEnabledCdmInterfaceVersion[];
MEDIA_EXPORT extern const char kOverrideHardwareSecureCodecsForTesting[];
MEDIA_EXPORT extern const char kEnableLiveCaptionPrefForTesting[];

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
MEDIA_EXPORT extern const char kEnableClearHevcForTesting[];
#endif

#if BUILDFLAG(IS_CHROMEOS)
MEDIA_EXPORT extern const char kLacrosEnablePlatformEncryptedHevc[];
MEDIA_EXPORT extern const char kLacrosEnablePlatformHevc[];
MEDIA_EXPORT extern const char kLacrosUseChromeosProtectedMedia[];
MEDIA_EXPORT extern const char kLacrosUseChromeosProtectedAv1[];
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace autoplay {

MEDIA_EXPORT extern const char kDocumentUserActivationRequiredPolicy[];
MEDIA_EXPORT extern const char kNoUserGestureRequiredPolicy[];
MEDIA_EXPORT extern const char kUserGestureRequiredPolicy[];

}  // namespace autoplay

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
MEDIA_EXPORT extern const char kHardwareVideoDecodeFrameRate[];
#endif

}  // namespace switches

namespace media {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

MEDIA_EXPORT extern const base::Feature kAudioFocusDuckFlash;
MEDIA_EXPORT extern const base::Feature kAudioFocusLossSuspendMediaSession;
MEDIA_EXPORT extern const base::Feature kAutoplayIgnoreWebAudio;
MEDIA_EXPORT extern const base::Feature kAutoplayDisableSettings;
MEDIA_EXPORT extern const base::Feature kBackgroundVideoPauseOptimization;
MEDIA_EXPORT extern const base::Feature kBresenhamCadence;
MEDIA_EXPORT extern const base::Feature kCdmHostVerification;
MEDIA_EXPORT extern const base::Feature kCdmProcessSiteIsolation;
MEDIA_EXPORT extern const base::Feature kChromeWideEchoCancellationEnabled;
MEDIA_EXPORT extern const base::Feature kD3D11VideoDecoderUseSharedHandle;
MEDIA_EXPORT extern const base::Feature kEnableMediaInternals;
MEDIA_EXPORT extern const base::Feature kEnableTabMuting;
MEDIA_EXPORT extern const base::Feature kExposeSwDecodersToWebRTC;
MEDIA_EXPORT extern const base::Feature kExternalClearKeyForTesting;
MEDIA_EXPORT extern const base::Feature kFFmpegDecodeOpaqueVP8;
MEDIA_EXPORT extern const base::Feature kFailUrlProvisionFetcherForTesting;
MEDIA_EXPORT extern const base::Feature kFallbackAfterDecodeError;
MEDIA_EXPORT extern const base::Feature kGav1VideoDecoder;
MEDIA_EXPORT extern const base::Feature kGlobalMediaControls;
MEDIA_EXPORT extern const base::Feature kGlobalMediaControlsAutoDismiss;
MEDIA_EXPORT extern const base::Feature kGlobalMediaControlsForChromeOS;
MEDIA_EXPORT extern const base::Feature kGlobalMediaControlsPictureInPicture;
MEDIA_EXPORT extern const base::Feature kGlobalMediaControlsSeamlessTransfer;
MEDIA_EXPORT extern const base::Feature kGlobalMediaControlsModernUI;
MEDIA_EXPORT extern const base::Feature kHardwareMediaKeyHandling;
MEDIA_EXPORT extern const base::Feature kHardwareSecureDecryption;
MEDIA_EXPORT extern const base::Feature kHardwareSecureDecryptionExperiment;
MEDIA_EXPORT extern const base::Feature kInternalMediaSession;
MEDIA_EXPORT extern const base::Feature kKeepRvfcFrameAlive;
MEDIA_EXPORT extern const base::Feature kKeyPressMonitoring;
MEDIA_EXPORT extern const base::Feature kLiveCaption;
MEDIA_EXPORT extern const base::Feature kLiveCaptionMultiLanguage;
MEDIA_EXPORT extern const base::Feature kLiveCaptionSystemWideOnChromeOS;
MEDIA_EXPORT extern const base::Feature kLowDelayVideoRenderingOnLiveStream;
MEDIA_EXPORT extern const base::Feature kMediaCapabilitiesQueryGpuFactories;
MEDIA_EXPORT extern const base::Feature kMediaCapabilitiesWithParameters;
MEDIA_EXPORT extern const base::Feature kMediaCastOverlayButton;
MEDIA_EXPORT extern const base::Feature kMediaEngagementBypassAutoplayPolicies;
MEDIA_EXPORT extern const base::Feature kMediaEngagementHTTPSOnly;
MEDIA_EXPORT extern const base::Feature kMediaLearningExperiment;
MEDIA_EXPORT extern const base::Feature kMediaLearningFramework;
MEDIA_EXPORT extern const base::Feature kMediaLearningSmoothnessExperiment;
MEDIA_EXPORT extern const base::Feature kMediaOptimizer;
MEDIA_EXPORT extern const base::Feature kMediaPowerExperiment;
MEDIA_EXPORT extern const base::Feature kMediaSessionWebRTC;
MEDIA_EXPORT extern const base::Feature kMemoryPressureBasedSourceBufferGC;
MEDIA_EXPORT extern const base::Feature kMultiPlaneVideoCaptureSharedImages;
MEDIA_EXPORT extern const base::Feature kOverlayFullscreenVideo;
MEDIA_EXPORT extern const base::Feature kPictureInPicture;
MEDIA_EXPORT extern const base::Feature kPlaybackSpeedButton;
MEDIA_EXPORT extern const base::Feature kPreloadMediaEngagementData;
MEDIA_EXPORT extern const base::Feature kPreloadMetadataLazyLoad;
MEDIA_EXPORT extern const base::Feature kPreloadMetadataSuspend;
MEDIA_EXPORT extern const base::Feature kRecordMediaEngagementScores;
MEDIA_EXPORT extern const base::Feature kRecordWebAudioEngagement;
MEDIA_EXPORT extern const base::Feature kResumeBackgroundVideo;
MEDIA_EXPORT extern const base::Feature kReuseMediaPlayer;
MEDIA_EXPORT extern const base::Feature kRevokeMediaSourceObjectURLOnAttach;
MEDIA_EXPORT extern const base::Feature
    kShareThisTabInsteadButtonGetDisplayMedia;
MEDIA_EXPORT extern const base::Feature kSpeakerChangeDetection;
MEDIA_EXPORT extern const base::Feature kSpecCompliantCanPlayThrough;
MEDIA_EXPORT extern const base::Feature kSurfaceLayerForMediaStreams;
MEDIA_EXPORT extern const base::Feature kSuspendMutedAudio;
MEDIA_EXPORT extern const base::Feature kUnifiedAutoplay;
MEDIA_EXPORT extern const base::Feature kUseAndroidOverlayForSecureOnly;
MEDIA_EXPORT extern const base::Feature kUseDecoderStreamForWebRTC;
MEDIA_EXPORT extern const base::Feature kUseFakeDeviceForMediaStream;
MEDIA_EXPORT extern const base::Feature kUseMediaHistoryStore;
MEDIA_EXPORT extern const base::Feature kUseR16Texture;
#if BUILDFLAG(IS_LINUX)
MEDIA_EXPORT extern const base::Feature kVaapiVideoDecodeLinux;
MEDIA_EXPORT extern const base::Feature kVaapiVideoEncodeLinux;
#endif  // BUILDFLAG(IS_LINUX)
MEDIA_EXPORT extern const base::Feature kVaapiAV1Decoder;
MEDIA_EXPORT extern const base::Feature kVaapiLowPowerEncoderGen9x;
MEDIA_EXPORT extern const base::Feature kVaapiEnforceVideoMinMaxResolution;
MEDIA_EXPORT extern const base::Feature kVaapiVideoMinResolutionForPerformance;
MEDIA_EXPORT extern const base::Feature kVaapiVP8Encoder;
MEDIA_EXPORT extern const base::Feature kVaapiVP9Encoder;
MEDIA_EXPORT extern const base::Feature kGlobalVaapiLock;
#if defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS)
MEDIA_EXPORT extern const base::Feature kVaapiH264TemporalLayerHWEncoding;
MEDIA_EXPORT extern const base::Feature kVaapiVp8TemporalLayerHWEncoding;
MEDIA_EXPORT extern const base::Feature kVaapiVp9kSVCHWEncoding;
#endif  // defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS)
MEDIA_EXPORT extern const base::Feature kVideoBlitColorAccuracy;
MEDIA_EXPORT extern const base::Feature kVp9kSVCHWDecoding;
MEDIA_EXPORT extern const base::Feature kWakeLockOptimisationHiddenMuted;
MEDIA_EXPORT extern const base::Feature kWebrtcMediaCapabilitiesParameters;
MEDIA_EXPORT extern const base::Feature kResolutionBasedDecoderPriority;
MEDIA_EXPORT extern const base::Feature kForceHardwareVideoDecoders;
MEDIA_EXPORT extern const base::Feature kForceHardwareAudioDecoders;

#if BUILDFLAG(IS_ANDROID)
MEDIA_EXPORT extern const base::Feature kAllowNonSecureOverlays;
MEDIA_EXPORT extern const base::Feature kMediaControlsExpandGesture;
MEDIA_EXPORT extern const base::Feature kMediaDrmPersistentLicense;
MEDIA_EXPORT extern const base::Feature kMediaDrmPreprovisioning;
MEDIA_EXPORT extern const base::Feature kMediaDrmPreprovisioningAtStartup;
MEDIA_EXPORT extern const base::Feature kCanPlayHls;
MEDIA_EXPORT extern const base::Feature kPictureInPictureAPI;
MEDIA_EXPORT extern const base::Feature kHlsPlayer;
MEDIA_EXPORT extern const base::Feature kRequestSystemAudioFocus;
MEDIA_EXPORT extern const base::Feature kUseAudioLatencyFromHAL;
MEDIA_EXPORT extern const base::Feature kUsePooledSharedImageVideoProvider;
MEDIA_EXPORT extern const base::Feature kUseRealColorSpaceForAndroidVideo;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
MEDIA_EXPORT extern const base::Feature kUseChromeOSDirectVideoDecoder;

#if BUILDFLAG(IS_CHROMEOS)
MEDIA_EXPORT extern const base::Feature kUseAlternateVideoDecoderImplementation;
#endif  // BUILDFLAG(IS_CHROMEOS)
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

#if BUILDFLAG(IS_MAC)
#if BUILDFLAG(ENABLE_PLATFORM_HEVC_DECODING)
MEDIA_EXPORT extern const base::Feature kVideoToolboxHEVCDecoding;
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC_DECODING)
MEDIA_EXPORT extern const base::Feature kMultiPlaneVideoToolboxSharedImages;
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
MEDIA_EXPORT extern const base::Feature kDelayCopyNV12Textures;
MEDIA_EXPORT extern const base::Feature kDirectShowGetPhotoState;
MEDIA_EXPORT extern const base::Feature kIncludeIRCamerasInDeviceEnumeration;
MEDIA_EXPORT extern const base::Feature kMediaFoundationAV1Encoding;
MEDIA_EXPORT extern const base::Feature kMediaFoundationH264CbpEncoding;
MEDIA_EXPORT extern const base::Feature kMediaFoundationVideoCapture;
MEDIA_EXPORT extern const base::Feature kMediaFoundationVP8Decoding;
MEDIA_EXPORT extern const base::Feature kMediaFoundationD3D11VideoCapture;
MEDIA_EXPORT extern const base::Feature kMediaFoundationClearPlayback;
MEDIA_EXPORT extern const base::Feature kWasapiRawAudioCapture;
MEDIA_EXPORT extern const base::Feature kD3D11HEVCDecoding;
MEDIA_EXPORT extern const base::Feature kD3D11Vp9kSVCHWDecoding;
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
MEDIA_EXPORT extern const base::Feature kDeprecateLowUsageCodecs;
#endif

// Based on a |command_line| and the current platform, returns the effective
// autoplay policy. In other words, it will take into account the default policy
// if none is specified via the command line and options passed for testing.
// Returns one of the possible autoplay policy switches from the
// switches::autoplay namespace.
MEDIA_EXPORT std::string GetEffectiveAutoplayPolicy(
    const base::CommandLine& command_line);

MEDIA_EXPORT bool IsChromeWideEchoCancellationEnabled();
MEDIA_EXPORT bool IsHardwareSecureDecryptionEnabled();
MEDIA_EXPORT bool IsLiveCaptionFeatureEnabled();
MEDIA_EXPORT bool IsVideoCaptureAcceleratedJpegDecodingEnabled();

enum class kCrosGlobalMediaControlsPinOptions {
  kPin,
  kNotPin,
  kHeuristic,
};

// Feature param used to force default pin/unpin for global media controls in
// CrOS.
MEDIA_EXPORT extern const base::FeatureParam<kCrosGlobalMediaControlsPinOptions>
    kCrosGlobalMediaControlsPinParam;

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_SWITCHES_H_
