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
#include "media/base/media_export.h"
#include "media/gpu/buildflags.h"
#include "media/media_buildflags.h"
#include "ui/gl/angle_implementation.h"

namespace gpu {
class GpuDriverBugWorkarounds;
}

namespace base {
class CommandLine;
}

// NOTE: Generally you should not add new switches, instead preferring to add
// base::Feature entries. If you really must add a switch, you'll need to update
// other places in the code to ensure it's passed to subprocesses correctly.
//
// When adding new switches ensure they are added alphabetically and that the
// order in the .cc file matches the order here.
namespace switches {

namespace autoplay {
MEDIA_EXPORT extern const char kDocumentUserActivationRequiredPolicy[];
MEDIA_EXPORT extern const char kNoUserGestureRequiredPolicy[];
MEDIA_EXPORT extern const char kUserGestureRequiredPolicy[];
}  // namespace autoplay

MEDIA_EXPORT extern const char kAudioBufferSize[];
MEDIA_EXPORT extern const char kAutoGrantCapturedSurfaceControlPrompt[];
MEDIA_EXPORT extern const char kAutoplayPolicy[];
MEDIA_EXPORT extern const char kCastStreamingForceDisableHardwareAv1[];
MEDIA_EXPORT extern const char kCastStreamingForceDisableHardwareH264[];
MEDIA_EXPORT extern const char kCastStreamingForceDisableHardwareHevc[];
MEDIA_EXPORT extern const char kCastStreamingForceDisableHardwareVp8[];
MEDIA_EXPORT extern const char kCastStreamingForceDisableHardwareVp9[];
MEDIA_EXPORT extern const char kCastStreamingForceEnableHardwareAv1[];
MEDIA_EXPORT extern const char kCastStreamingForceEnableHardwareH264[];
MEDIA_EXPORT extern const char kCastStreamingForceEnableHardwareHevc[];
MEDIA_EXPORT extern const char kCastStreamingForceEnableHardwareVp8[];
MEDIA_EXPORT extern const char kCastStreamingForceEnableHardwareVp9[];
MEDIA_EXPORT extern const char kClearKeyCdmPathForTesting[];
MEDIA_EXPORT extern const char kDisableAcceleratedMjpegDecode[];
MEDIA_EXPORT extern const char kDisableAudioInput[];
MEDIA_EXPORT extern const char kDisableAudioOutput[];
MEDIA_EXPORT extern const char kDisableBackgroundMediaSuspend[];
MEDIA_EXPORT extern const char kDisableRTCSmoothnessAlgorithm[];
MEDIA_EXPORT extern const char kEnableLiveCaptionPrefForTesting[];
MEDIA_EXPORT extern const char kFailAudioStreamCreation[];
MEDIA_EXPORT extern const char kFakeBackgroundBlurTogglePeriod[];
MEDIA_EXPORT extern const char kForceVideoOverlays[];
MEDIA_EXPORT extern const char kMSEAudioBufferSizeLimitMb[];
MEDIA_EXPORT extern const char kMSEVideoBufferSizeLimitMb[];
MEDIA_EXPORT extern const char kMuteAudio[];
MEDIA_EXPORT extern const char kOverrideEnabledCdmInterfaceVersion[];
MEDIA_EXPORT extern const char kOverrideHardwareSecureCodecsForTesting[];
MEDIA_EXPORT extern const char kReportVp9AsAnUnsupportedMimeType[];
MEDIA_EXPORT extern const char
    kUnsafelyAllowProtectedMediaIdentifierForDomain[];
MEDIA_EXPORT extern const char kUseFakeDeviceForMediaStream[];
MEDIA_EXPORT extern const char kUseFakeMjpegDecodeAccelerator[];
MEDIA_EXPORT extern const char kUseFileForFakeAudioCapture[];
MEDIA_EXPORT extern const char kUseFileForFakeVideoCapture[];
MEDIA_EXPORT extern const char kVideoThreads[];

#if BUILDFLAG(ENABLE_PASSTHROUGH_AUDIO_CODECS)
MEDIA_EXPORT extern const char kAudioCodecsFromEDID[];
#endif  // BUILDFLAG(ENABLE_PASSTHROUGH_AUDIO_CODECS)

#if BUILDFLAG(IS_CHROMEOS)
MEDIA_EXPORT extern const char kAllowRAInDevMode[];
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_FUCHSIA)
MEDIA_EXPORT extern const char kAudioCapturerWithEchoCancellation[];
MEDIA_EXPORT extern const char kEnableProtectedVideoBuffers[];
MEDIA_EXPORT extern const char kForceProtectedVideoOutputBuffers[];
MEDIA_EXPORT extern const char kMinVideoDecoderOutputBufferSize[];
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FREEBSD) || \
    BUILDFLAG(IS_SOLARIS)
MEDIA_EXPORT extern const char kAlsaInputDevice[];
MEDIA_EXPORT extern const char kAlsaOutputDevice[];
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_FREEBSD) || BUILDFLAG(IS_SOLARIS)

#if BUILDFLAG(IS_WIN)
MEDIA_EXPORT extern const char kEnableExclusiveAudio[];
MEDIA_EXPORT extern const char kForceWaveAudio[];
MEDIA_EXPORT extern const char kTrySupportedChannelLayouts[];
MEDIA_EXPORT extern const char kWaveOutBuffers[];
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(USE_CRAS)
MEDIA_EXPORT extern const char kSystemAecEnabled[];
MEDIA_EXPORT extern const char kUseCras[];
#endif  // BUILDFLAG(USE_CRAS)

#if BUILDFLAG(USE_V4L2_CODEC)
MEDIA_EXPORT extern const char kHardwareVideoDecodeFrameRate[];
#endif  // BUILDFLAG(USE_V4L2_CODEC)

#if BUILDFLAG(USE_V4L2_CODEC) || BUILDFLAG(USE_VAAPI)
MEDIA_EXPORT extern const char kEnablePrimaryNodeAccessForVkmsTesting[];
#endif  // BUILDFLAG(USE_V4L2_CODEC) || BUILDFLAG(USE_VAAPI)

#if BUILDFLAG(USE_VAAPI)
MEDIA_EXPORT extern const char kHardwareVideoDevicePath[];
#endif  // BUILDFLAG(USE_VAAPI)

}  // namespace switches

namespace media {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

MEDIA_EXPORT BASE_DECLARE_FEATURE(kAVDColorSpaceChanges);
// TODO(crbug.com/467555325): Remove after M153 reaches stable.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAccurateVideoFrameConverterColorSpace);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAomVpxUsePresentationThreadType);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAudioFocusDuckFlash);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAutoPictureInPictureOnWindowOccluded);
// Enables showing auto picture-in-picture permission details in page info.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAutoPictureInPicturePageInfoDetails);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAutoPictureInPictureSurveys);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAutoplayBypassForMicCamera);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAutoplayDisableSettings);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAutoplayPoliciesAndroid);
MEDIA_EXPORT BASE_DECLARE_FEATURE(
    kBrowserInitiatedAutomaticPictureInPictureDryRun);
// NOTE: callers should always use the free functions in
// /media/cast/encoding/encoding_support.h instead of accessing these features
// directly.
// TODO(https://crbug.com/1453388): Guard Cast Sender flags with !IS_ANDROID.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCastStreamingAv1);
MEDIA_EXPORT BASE_DECLARE_FEATURE(
    kCastStreamingExponentialVideoBitrateAlgorithm);
MEDIA_EXPORT extern const base::FeatureParam<int>
    kCastStreamingExponentialVideoBitrateAlgorithmWindowSize;
MEDIA_EXPORT extern const base::FeatureParam<int>
    kCastStreamingExponentialVideoBitrateAlgorithmDropThreshold;
MEDIA_EXPORT extern const base::FeatureParam<double>
    kCastStreamingExponentialVideoBitrateAlgorithmIncreaseFactor;
MEDIA_EXPORT extern const base::FeatureParam<double>
    kCastStreamingExponentialVideoBitrateAlgorithmDecreaseFactor;
MEDIA_EXPORT extern const base::FeatureParam<double>
    kCastStreamingExponentialVideoBitrateAlgorithmDynamicWindowMultiplier;
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCastStreamingHardwareHevc);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCastStreamingMaxVideoBitrate);
MEDIA_EXPORT extern const base::FeatureParam<int>
    kCastStreamingMaxVideoBitrateMbps;
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCastStreamingPerformanceOverlay);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCastStreamingVp8);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCastStreamingVp9);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCastVideoEncoderFrameDrop);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCdmHostVerification);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCdmProcessPriorityElevation);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCdmThreadPriorityElevation);
MEDIA_EXPORT BASE_DECLARE_FEATURE(
    kClearPipCachedBoundsWhenPermissionPromptVisible);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kContextMenuCopyVideoFrame);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kContextMenuSaveVideoFrameAs);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kContextMenuSearchForVideoFrame);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kD3D11VideoDecoderForceSingleTexture);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kD3D11VideoDecoderUseSharedHandle);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kDedicatedMediaServiceThread);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kDeferAudioFocusUntilAudible);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kDirectOpusAudioDecoding);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kDocumentPictureInPictureAnimateResize);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kDocumentPictureInPictureCapture);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kDocumentPictureInPictureNavigation);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kDocumentPictureInPictureReparenting);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kEnableRtcpReporting);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kEnableTabMuting);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kEncryptedMediaOcclusionTracking);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kExtendedVideoBitstreamValidation);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kExternalClearKeyForTesting);
MEDIA_EXPORT extern const base::FeatureParam<std::string>
    kMediaFoundationClearKeyCdmPathForTesting;
MEDIA_EXPORT BASE_DECLARE_FEATURE(kFailUrlProvisionFetcherForTesting);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kFallbackAfterDecodeError);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kFeatureManagementLiveTranslateCrOS);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kForceSoftwareForRtcLowResolutions);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kGetDisplayMediaConfersActivation);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kGlobalMediaControlsAutoDismiss);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kGlobalMediaControlsSaveVideoFrame);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kGlobalMediaControlsSeamlessTransfer);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kGlobalVaapiLock);
// When enabled, H.264 keyframe detection becomes stricter for samples whose avc
// config does not provide SPS/PPS. In that case, an IDR alone is not
// sufficient, SPS+PPS must appear in-band to mark it as a keyframe.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kH264IDRKeyframeRequiresParameterSets);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kHardwareMediaKeyHandling);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kHardwareSecureDecryption);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kHardwareSecureDecryptionAv1);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kHardwareSecureDecryptionFallback);
MEDIA_EXPORT extern const base::FeatureParam<bool>
    kHardwareSecureDecryptionFallbackPerSite;
MEDIA_EXPORT extern const base::FeatureParam<int>
    kHardwareSecureDecryptionFallbackMinDisablingDays;
MEDIA_EXPORT extern const base::FeatureParam<int>
    kHardwareSecureDecryptionFallbackMaxDisablingDays;
MEDIA_EXPORT extern const base::FeatureParam<bool>
    kHardwareSecureDecryptionFallbackOnHardwareContextReset;
MEDIA_EXPORT BASE_DECLARE_FEATURE(kHardwareSecureDecryptionVp9);
// If enabled, Glic will start captioning as soon as a profile is loaded.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kHeadlessCaptionEarlyStart);
// Enable experimental headless captions.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kHeadlessLiveCaption);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kInternalMediaSession);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLiveCaption);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLiveCaptionAutomaticLanguageDownload);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLiveCaptionExperimentalLanguages);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLiveCaptionLogFlickerRate);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLiveCaptionRightClick);
MEDIA_EXPORT BASE_DECLARE_FEATURE(
    kLiveCaptionSpeechRecognitionSmallExpertModel);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLiveCaptionUseGreedyTextStabilizer);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLiveCaptionUseWaitK);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLiveCaptionWebAudio);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLogSodaLoadFailures);
// Flag to enable or disable parsing of MP4 timed metadata tracks.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMP4TimedMetadataTrack);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMatchSourceAudioChannelLayout);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaCapabilitiesQueryGpuFactories);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaCapabilitiesWithParameters);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaEngagementBypassAutoplayPolicies);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaEngagementHTTPSOnly);
// Enable site-specific media link helpers.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaLinkHelpers);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaLogToConsole);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaOptimizer);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaPowerExperiment);
// When enabled, non-IDR H.264 frames with SEI recovery points
// (recovery_frame_cnt=0) are promoted to keyframes for MSE random access,
// and SPS/PPS parameter sets are injected for these frames so the hardware
// decoder can initialize after a seek/reset. This enables playback of
// open-GOP content in SourceBuffer. Limited to clear content to work
// around hardware decoders that don't handle non-IDR keyframes (some older
// Intel/AMD devices mishandle SEI + SPS/PPS); see https://crbug.com/451536366.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaSourceSeiRecoveryPointKeyframe);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaStreamAccurateDroppedFrameCount);
// If enabled, chrome would inform Glic once it starts trasncribing, if Glic
// requested to be informed.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaTrasncriptsFlagInPageMetadata);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kOnDeviceWebSpeech);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kOnDeviceWebSpeechGeminiNano);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kOnDeviceWebSpeechSmallExpertModel);
MEDIA_EXPORT BASE_DECLARE_FEATURE(
    kOnDeviceWebSpeechSmallExpertModelMultiLanguage);
MEDIA_EXPORT extern const base::FeatureParam<std::string>
    kOnDeviceWebSpeechSmallExpertModelLanguages;
MEDIA_EXPORT BASE_DECLARE_FEATURE(kOverlayFullscreenVideo);
// Causes the AVC parser to output Treats H.264 SEI recovery points with a
// `recovery_frame_cnt=0` as keyframes.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kParseSEIRecoveryPoints);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPauseBackgroundTimer);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPauseMutedBackgroundAudio);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPictureInPictureMuteControl);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPlatformAudioEncoder);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPlaybackSpeedButton);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPreemptiveSodaDownload);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPreloadMediaEngagementData);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPreloadMetadataSuspend);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kRecordMediaEngagementScores);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kRecordWebAudioEngagement);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kRenderMutedAudio);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kResolutionBasedDecoderPriority);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kResumeBackgroundVideo);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kRevokeMediaSourceObjectURLOnAttach);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kRustMpegAudioDataParser);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kSpeakerChangeDetection);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kSpecCompliantCanPlayThrough);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kSuspendMediaForFrozenFrames);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUnifiedAutoplay);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseAndroidOverlayForSecureOnly);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseFakeDeviceForMediaStream);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseSequencedTaskRunnerForMediaService);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseTaskRunnerForMojoAudioDecoderService);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseWindowBoundsForPip);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVaapiEarlyPPSParsingForCENCv1);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVaapiLowPowerEncoderGen9x);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVaapiOnNvidiaGPUs);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVaapiVideoMinResolutionForPerformance);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kValidateEncryptionPatternSize);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVideoBlitColorAccuracy);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVideoDecodeBatching);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kWebCodecsDecoderFlushOptimizations);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kWebCodecsVideoEncoderFrameDrop);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kWebRTCColorAccuracy);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kWebRTCHardwareVideoEncoderFrameDrop);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kWebRTCLogColorSpace);
// When enabled, input audio processing in the audio process may use an ML-based
// residual echo estimator instead of the default heuristics, when applying
// WebRTC echo cancellation.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kWebRtcAudioNeuralResidualEchoEstimation);
// When enabled, input audio processing in the audio process may use an ML-based
// voice isolation denoiser.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kWebRtcVoiceIsolationDenoiser);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kWebrtcMediaCapabilitiesParameters);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kWidevinePersistentLicenseSupport);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kZeroCopyDesktopCapture);

#if !BUILDFLAG(IS_ANDROID)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kFileDialogsBlockPictureInPicture);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kFileDialogsTuckPictureInPicture);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaRemotingWithoutFullscreen);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPictureInPictureOcclusionTracking);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVideoPipDisplaySmoothnessOptimization);
MEDIA_EXPORT BASE_DECLARE_FEATURE(
    kVideoPipForceTrustedForMediaPlaybackForTesting);
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_WIN)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseSequencedTaskRunnerForMojoVEAProvider);
#endif  // !BUILDFLAG(IS_WIN)

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
// Note: please use IsOutOfProcessVideoDecodingEnabled() to determine if OOP-VD
// is enabled instead of directly checking this feature flag. The reason is that
// that function may perform checks beyond the feature flag.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseOutOfProcessVideoDecoding);
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kChromeWideEchoCancellation);
#endif  // BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaRecorderHEVCSupport);
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

#if BUILDFLAG(ENABLE_IAMF_TOOLS)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kIamfAudioDecoding);
#endif  // BUILDFLAG(ENABLE_IAMF_TOOLS)

#if BUILDFLAG(ENABLE_OPENH264)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kOpenH264SoftwareEncoder);
#endif  // BUILDFLAG(ENABLE_OPENH264)

#if BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)
MEDIA_EXPORT BASE_DECLARE_FEATURE(
    kAllowClearDolbyVisionInMseWhenPlatformEncryptedDvEnabled);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAllowClearDolbyVisionViaMFT);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPlatformEncryptedDolbyVision);
#endif  // BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPlatformHEVCDecoderSupport);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPlatformHEVCEncoderSupport);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_APPLE)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPlatformHEVCHbdEncoderSupport);
#endif  // BUILDFLAG(IS_APPLE)
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)

#if BUILDFLAG(ENABLE_SYMPHONIA)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kSymphoniaAudioDecoding);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kSymphoniaMp3Decoding);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kSymphoniaPcmDecoding);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kSymphoniaVorbisDecoding);
#endif  // BUILDFLAG(ENABLE_SYMPHONIA)

#if BUILDFLAG(ENABLE_SYMPHONIA_DEMUXER)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kSymphoniaDemuxing);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kSymphoniaAacDemuxing);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kSymphoniaFlacDemuxing);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kSymphoniaIsomDemuxing);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kSymphoniaMkvDemuxing);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kSymphoniaMp3Demuxing);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kSymphoniaOggDemuxing);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kSymphoniaRiffDemuxing);
#endif  // BUILDFLAG(ENABLE_SYMPHONIA_DEMUXER)

#if BUILDFLAG(IS_ANDROID)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAllowAudioPlaybackCapture);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAllowDelayedAudioFocusGainAndroid);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAllowEnhancedPipTransition);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAllowMediaCodecSoftwareDecoder);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAndroidEnableBackgroundMediaCapturing);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAndroidSuspendWebRtcOnScreenOff);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAndroidZeroCopyVideoCapture);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAutoDocPiPPermissionPromptAndroid);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAutoPictureInPictureAndroid);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kContextMenuPictureInPictureAndroid);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kFullscreenVideoPictureInPicture);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaCodecBlockModel);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaCodecBlockModelOutput);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaCodecColorSpaceCleanup);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaCodecLowDelayMode);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaDrmGetStatusForPolicy);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaDrmPersistentLicense);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaDrmPreprovisioning);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaDrmPreprovisioningAtStartup);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaDrmQueryInSeparateProcess);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kNdkVideoEncodeAcceleratorBitrateLayering);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kNdkVideoEncodeAcceleratorNativeSvc);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kNoPauseMediaOnHeadphoneUnplug);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPauseMediaOnSystemSleepAndroid);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kRequestSystemAudioFocus);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kSkipMediaCodecReallocation);
MEDIA_EXPORT BASE_DECLARE_FEATURE(
    kUseMediaCryptoRequiresSecureDecoderComponent);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseAudioLatencyFromHAL);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseAudioManagerMaxChannelLayout);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseMediaFormatCodedSize);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_APPLE)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVTVideoEncodeAcceleratorCalculatePSNR);
MEDIA_EXPORT BASE_DECLARE_FEATURE(
    kVTVideoEncodeAcceleratorOpaqueSharedImageEncode);
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_CHROMEOS)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAudioFlexibleLoopbackForSystemLoopback);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kBackgroundListening);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSDspBasedAecDeactivatedGroups);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSDspBasedAgcDeactivatedGroups);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSDspBasedNsDeactivatedGroups);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSEnforceMonoAudioCapture);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSEnforceSystemAec);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSEnforceSystemAecAgc);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSEnforceSystemAecNs);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSEnforceSystemAecNsAgc);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSSystemAEC);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSSystemAECDeactivatedGroups);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCrOSSystemVoiceIsolationOption);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kIgnoreUiGains);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kShowForceRespectUiGainsToggle);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kReduceHardwareVideoDecoderBuffers);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseOutOfProcessVideoEncoding);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kV4L2H264TemporalLayerHWEncoding);
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_FUCHSIA)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kFuchsiaCdmStoragePathMigration);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kFuchsiaMediacodecVideoEncoder);
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_LINUX)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAcceleratedVideoDecodeLinux);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAcceleratedVideoDecodeLinuxGL);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAcceleratedVideoEncodeLinux);
// When both VA-API and V4L2 are compiled in, selects the active backend:
// disabled (default) => VA-API, enabled => V4L2. Toggle via
// --enable-features=PreferV4L2VideoAcceleration.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPreferV4L2VideoAcceleration);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPulseaudioLoopbackForCast);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPulseaudioLoopbackForScreenShare);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVaapiIgnoreDriverChecks);
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_MAC)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kApplicationAudioCaptureMac);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCastMacForceBaselineProfile);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCastStreamingMacHardwareH264);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMacCatapLoopbackAudioForCast);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseSCContentSharingPicker);
#endif  // BUILDFLAG(IS_MAC)

#if (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN))
MEDIA_EXPORT BASE_DECLARE_FEATURE(kEnforceSystemEchoCancellation);
MEDIA_EXPORT extern const base::FeatureParam<bool>
    kEnforceSystemEchoCancellationAllowAgcInTandem;
MEDIA_EXPORT extern const base::FeatureParam<bool>
    kEnforceSystemEchoCancellationAllowNsInTandem;
#endif  // (BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN))

#if BUILDFLAG(IS_WIN)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAllowMediaFoundationFrameServerMode);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kApplicationAudioCaptureWin);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAudioDuckingWin);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kAudioOffload);
MEDIA_EXPORT extern const base::FeatureParam<double> kAudioOffloadBufferTimeMs;
MEDIA_EXPORT BASE_DECLARE_FEATURE(kCastStreamingWinHardwareH264);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kD3D11Vp9kSVCHWDecoding);
// Enables D3D12 video encode accelerator taking shared image as input.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kD3D12SharedImageEncode);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kD3D12VideoDecoder);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kD3D12VideoEncodeAccelerator);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kD3D12VideoEncodeAcceleratorL1T3);
MEDIA_EXPORT BASE_DECLARE_FEATURE(
    kD3D12VideoEncodeAcceleratorSharedHandleCaching);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kDirectShowGetPhotoState);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kHardwareSecureDecryptionRequireServerCert);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kIncludeIRCamerasInDeviceEnumeration);
// Enables the batch audio/video buffers reading for media playback.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationBatchRead);
// Specify the batch read count between client renderer and remote renderer,
// default value is 1.
MEDIA_EXPORT extern const base::FeatureParam<int> kBatchReadCount;
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationClearPlayback);
// For feature check of kMediaFoundationD3D11VideoCapture at runtime,
// please use IsMediaFoundationD3D11VideoCaptureEnabled() instead.
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationD3D11VideoCapture);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationD3D11VideoCaptureZeroCopy);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationD3DVideoProcessing);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationMultiGpuAdapterSelection);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationSharedImageEncode);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationUseSoftwareRateCtrl);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationVideoCapture);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kMediaFoundationVideoEncodeAccelerator);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kProtectedMediaIdentifierIndicator);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(SYSTEM_LOOPBACK_AS_AEC_REFERENCE)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kSystemLoopbackAsAecReference);
MEDIA_EXPORT extern const base::FeatureParam<bool>
    kSystemLoopbackAsAecReferenceForcedOn;
MEDIA_EXPORT extern const base::FeatureParam<int> kAddedProcessingDelayMs;
MEDIA_EXPORT extern const base::FeatureParam<int> kAecDelayNumFilters;
#endif  // BUILDFLAG(SYSTEM_LOOPBACK_AS_AEC_REFERENCE)

#if BUILDFLAG(USE_LINUX_VIDEO_ACCELERATION)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kChromeOSHWVBREncoding);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kLimitConcurrentDecoderInstances);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseSequencedTaskRunnerForVEA);
#if defined(ARCH_CPU_ARM_FAMILY)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kEnableArmHwdrm10bitOverlays);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kEnableProtectedVulkanDetiling);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPreferGLImageProcessor);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kPreferSoftwareMT21);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kUseGLForScaling);
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kEnableArmHwdrm);
#endif  // BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
#endif  // defined(ARCH_CPU_ARM_FAMILY)
#endif  // BUILDFLAG(USE_LINUX_VIDEO_ACCELERATION)

#if defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS)
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVSyncMjpegDecoding);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVaapiAV1TemporalLayerHWEncoding);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVaapiH264SWBitrateController);
MEDIA_EXPORT BASE_DECLARE_FEATURE(kVaapiVp9SModeHWEncoding);
#endif  // defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS)

// switches::autoplay namespace.
MEDIA_EXPORT std::string GetEffectiveAutoplayPolicy(
    const base::CommandLine& command_line);

// Return bitmask of audio formats supported by EDID.
MEDIA_EXPORT uint32_t GetPassthroughAudioFormats();

// Returns true if application audio loopback capture is implemented for the
// current OS.
MEDIA_EXPORT bool IsApplicationLoopbackCaptureSupported();

// Controls a global feature for sending ML model updates from the Optimization
// Guide framework in the browser process to the audio process.
MEDIA_EXPORT bool IsAudioProcessMlModelUsageEnabled();

MEDIA_EXPORT bool IsChromeWideEchoCancellationEnabled();

MEDIA_EXPORT bool IsDedicatedMediaServiceThreadEnabled(
    gl::ANGLEImplementation impl);

MEDIA_EXPORT bool IsHardwareSecureDecryptionEnabled();

MEDIA_EXPORT bool IsIamfAudioDecodingSupported();

MEDIA_EXPORT bool IsLiveTranslateEnabled();

MEDIA_EXPORT bool IsRestrictOwnAudioSupported();

MEDIA_EXPORT bool IsSystemEchoCancellationEnforced();

MEDIA_EXPORT bool IsSystemEchoCancellationEnforcedAndAllowAgcInTandem();

MEDIA_EXPORT bool IsSystemEchoCancellationEnforcedAndAllowNsInTandem();

// Returns true if loopback-based AEC can be used for audio input streams that
// are configured to do so.
MEDIA_EXPORT bool IsSystemLoopbackAsAecReferenceEnabled();

// Returns true if loopback-based AEC is enabled and its usage is forced, which
// means that loopback-based AEC will be used instead of chrome-wide AEC.
MEDIA_EXPORT bool IsSystemLoopbackAsAecReferenceForcedOn();

// Returns true if system audio loopback capture is implemented for the current
// OS.
MEDIA_EXPORT bool IsSystemLoopbackCaptureSupported();

MEDIA_EXPORT bool IsVideoCaptureAcceleratedJpegDecodingEnabled();

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
MEDIA_EXPORT bool IsOutOfProcessVideoDecodingEnabled();
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

#if BUILDFLAG(IS_ANDROID)
MEDIA_EXPORT bool IsAndroidZeroCopyVideoCaptureEnabled(
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
MEDIA_EXPORT bool IsMacCatapSystemLoopbackCaptureSupported();
MEDIA_EXPORT bool IsMacSckSystemLoopbackCaptureSupported();
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
MEDIA_EXPORT bool IsMediaFoundationD3D11VideoCaptureEnabled();
MEDIA_EXPORT bool IsWindowsProcessLoopbackCaptureSupported();
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(SYSTEM_LOOPBACK_AS_AEC_REFERENCE)
MEDIA_EXPORT base::TimeDelta GetAecAddedDelay();
MEDIA_EXPORT int GetAecDelayNumFilters();
#endif  // BUILDFLAG(SYSTEM_LOOPBACK_AS_AEC_REFERENCE)
}  // namespace media

#endif  // MEDIA_BASE_MEDIA_SWITCHES_H_
