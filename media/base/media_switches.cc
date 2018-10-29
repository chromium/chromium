// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_switches.h"

#include "base/command_line.h"
#include "build/build_config.h"

namespace switches {

// Allow users to specify a custom buffer size for debugging purpose.
const char kAudioBufferSize[] = "audio-buffer-size";

// Set a timeout (in milliseconds) for the audio service to quit if there are no
// client connections to it. If the value is zero the service never quits.
const char kAudioServiceQuitTimeoutMs[] = "audio-service-quit-timeout-ms";

// Command line flag name to set the autoplay policy.
const char kAutoplayPolicy[] = "autoplay-policy";

const char kDisableAudioOutput[] = "disable-audio-output";

// Causes the AudioManager to fail creating audio streams. Used when testing
// various failure cases.
const char kFailAudioStreamCreation[] = "fail-audio-stream-creation";

// Set number of threads to use for video decoding.
const char kVideoThreads[] = "video-threads";

// Suspend media pipeline on background tabs.
const char kEnableMediaSuspend[] = "enable-media-suspend";
const char kDisableMediaSuspend[] = "disable-media-suspend";

// Force to report VP9 as an unsupported MIME type.
const char kReportVp9AsAnUnsupportedMimeType[] =
    "report-vp9-as-an-unsupported-mime-type";

#if defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_SOLARIS)
// The Alsa device to use when opening an audio input stream.
const char kAlsaInputDevice[] = "alsa-input-device";
// The Alsa device to use when opening an audio stream.
const char kAlsaOutputDevice[] = "alsa-output-device";
#endif

#if defined(OS_WIN)
// Use exclusive mode audio streaming for Windows Vista and higher.
// Leads to lower latencies for audio streams which uses the
// AudioParameters::AUDIO_PCM_LOW_LATENCY audio path.
// See http://msdn.microsoft.com/en-us/library/windows/desktop/dd370844.aspx
// for details.
const char kEnableExclusiveAudio[] = "enable-exclusive-audio";

// Use Windows WaveOut/In audio API even if Core Audio is supported.
const char kForceWaveAudio[] = "force-wave-audio";

// Instead of always using the hardware channel layout, check if a driver
// supports the source channel layout.  Avoids outputting empty channels and
// permits drivers to enable stereo to multichannel expansion.  Kept behind a
// flag since some drivers lie about supported layouts and hang when used.  See
// http://crbug.com/259165 for more details.
const char kTrySupportedChannelLayouts[] = "try-supported-channel-layouts";

// Number of buffers to use for WaveOut.
const char kWaveOutBuffers[] = "waveout-buffers";
#endif

#if defined(USE_CRAS)
// Use CRAS, the ChromeOS audio server.
const char kUseCras[] = "use-cras";
#endif

// For automated testing of protected content, this switch allows specific
// domains (e.g. example.com) to skip asking the user for permission to share
// the protected media identifier. In this context, domain does not include the
// port number. User's content settings will not be affected by enabling this
// switch.
// Reference: http://crbug.com/718608
// Example:
// --unsafely-allow-protected-media-identifier-for-domain=a.com,b.ca
const char kUnsafelyAllowProtectedMediaIdentifierForDomain[] =
    "unsafely-allow-protected-media-identifier-for-domain";

#if BUILDFLAG(ENABLE_RUNTIME_MEDIA_RENDERER_SELECTION)
// Rather than use the renderer hosted remotely in the media service, fall back
// to the default renderer within content_renderer. Does not change the behavior
// of the media service.
const char kDisableMojoRenderer[] = "disable-mojo-renderer";
#endif  // BUILDFLAG(ENABLE_RUNTIME_MEDIA_RENDERER_SELECTION)

// Use fake device for Media Stream to replace actual camera and microphone.
const char kUseFakeDeviceForMediaStream[] = "use-fake-device-for-media-stream";

// Use an .y4m file to play as the webcam. See the comments in
// media/capture/video/file_video_capture_device.h for more details.
const char kUseFileForFakeVideoCapture[] = "use-file-for-fake-video-capture";

// Play a .wav file as the microphone. Note that for WebRTC calls we'll treat
// the bits as if they came from the microphone, which means you should disable
// audio processing (lest your audio file will play back distorted). The input
// file is converted to suit Chrome's audio buses if necessary, so most sane
// .wav files should work. You can pass either <path> to play the file looping
// or <path>%noloop to stop after playing the file to completion.
const char kUseFileForFakeAudioCapture[] = "use-file-for-fake-audio-capture";

// Use fake device for accelerated decoding of JPEG. This allows, for example,
// testing of the communication to the GPU service without requiring actual
// accelerator hardware to be present.
const char kUseFakeJpegDecodeAccelerator[] = "use-fake-jpeg-decode-accelerator";

// Disable hardware acceleration of mjpeg decode for captured frame, where
// available.
const char kDisableAcceleratedMjpegDecode[] =
    "disable-accelerated-mjpeg-decode";

// When running tests on a system without the required hardware or libraries,
// this flag will cause the tests to fail. Otherwise, they silently succeed.
const char kRequireAudioHardwareForTesting[] =
    "require-audio-hardware-for-testing";

// Mutes audio sent to the audio device so it is not audible during
// automated testing.
const char kMuteAudio[] = "mute-audio";

// Allows clients to override the threshold for when the media renderer will
// declare the underflow state for the video stream when audio is present.
// TODO(dalecurtis): Remove once experiments for http://crbug.com/470940 finish.
const char kVideoUnderflowThresholdMs[] = "video-underflow-threshold-ms";

// Disables the new rendering algorithm for webrtc, which is designed to improve
// the rendering smoothness.
const char kDisableRTCSmoothnessAlgorithm[] =
    "disable-rtc-smoothness-algorithm";

// Force media player using SurfaceView instead of SurfaceTexture on Android.
const char kForceVideoOverlays[] = "force-video-overlays";

// Allows explicitly specifying MSE audio/video buffer sizes as megabytes.
// Default values are 150M for video and 12M for audio.
const char kMSEAudioBufferSizeLimitMb[] = "mse-audio-buffer-size-limit-mb";
const char kMSEVideoBufferSizeLimitMb[] = "mse-video-buffer-size-limit-mb";

// Specifies the path to the Clear Key CDM for testing, which is necessary to
// support External Clear Key key system when library CDM is enabled. Note that
// External Clear Key key system support is also controlled by feature
// kExternalClearKeyForTesting.
const char kClearKeyCdmPathForTesting[] = "clear-key-cdm-path-for-testing";

// Overrides the default enabled library CDM interface version(s) with the one
// specified with this switch, which will be the only version enabled. For
// example, on a build where CDM 8, CDM 9 and CDM 10 are all supported
// (implemented), but only CDM 8 and CDM 9 are enabled by default:
//  --override-enabled-cdm-interface-version=8 : Only CDM 8 is enabled
//  --override-enabled-cdm-interface-version=9 : Only CDM 9 is enabled
//  --override-enabled-cdm-interface-version=10 : Only CDM 10 is enabled
//  --override-enabled-cdm-interface-version=11 : No CDM interface is enabled
// This can be used for local testing and debugging. It can also be used to
// enable an experimental CDM interface (which is always disabled by default)
// for testing while it's still in development.
const char kOverrideEnabledCdmInterfaceVersion[] =
    "override-enabled-cdm-interface-version";

// Overrides hardware secure codecs support for testing. If specified, real
// platform hardware secure codecs check will be skipped. Codecs are separated
// by comma. Valid codecs are "vp8", "vp9" and "avc1". For example:
//  --override-hardware-secure-codecs-for-testing=vp8,vp9
//  --override-hardware-secure-codecs-for-testing=avc1
// CENC encryption scheme is assumed to be supported for the specified codecs.
// If no valid codecs specified, no hardware secure codecs are supported. This
// can be used to disable hardware secure codecs support:
//  --override-hardware-secure-codecs-for-testing
const char kOverrideHardwareSecureCodecsForTesting[] =
    "override-hardware-secure-codecs-for-testing";

namespace autoplay {

// Autoplay policy that requires a document user activation.
const char kDocumentUserActivationRequiredPolicy[] =
    "document-user-activation-required";

// Autoplay policy that does not require any user gesture.
const char kNoUserGestureRequiredPolicy[] = "no-user-gesture-required";

// Autoplay policy to require a user gesture in order to play.
const char kUserGestureRequiredPolicy[] = "user-gesture-required";

// Autoplay policy to require a user gesture in order to play for cross origin
// iframes.
const char kUserGestureRequiredForCrossOriginPolicy[] =
    "user-gesture-required-for-cross-origin";

}  // namespace autoplay

}  // namespace switches

namespace media {

// Use new audio rendering mixer.
const base::Feature kNewAudioRenderingMixingStrategy{
    "NewAudioRenderingMixingStrategy", base::FEATURE_DISABLED_BY_DEFAULT};

// Only used for disabling overlay fullscreen (aka SurfaceView) in Clank.
const base::Feature kOverlayFullscreenVideo{"overlay-fullscreen-video",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enable Picture-in-Picture.
const base::Feature kPictureInPicture {
  "PictureInPicture",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Only decode preload=metadata elements upon visibility?
const base::Feature kPreloadMetadataLazyLoad{"PreloadMetadataLazyLoad",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Let videos be resumed via remote controls (for example, the notification)
// when in background.
const base::Feature kResumeBackgroundVideo {
  "resume-background-video",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Display the Cast overlay button on the media controls.
const base::Feature kMediaCastOverlayButton{"MediaCastOverlayButton",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Use AndroidOverlay rather than ContentVideoView in clank?
const base::Feature kUseAndroidOverlay{"UseAndroidOverlay",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Use AndroidOverlay for more cases than just player-element fullscreen?  This
// requires that |kUseAndroidOverlay| is true, else it is ignored.
const base::Feature kUseAndroidOverlayAggressively{
    "UseAndroidOverlayAggressively", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables playback of AV1 video files.
const base::Feature kAv1Decoder{"Av1Decoder",
                                base::FEATURE_ENABLED_BY_DEFAULT};

// Let video track be unselected when video is playing in the background.
const base::Feature kBackgroundSrcVideoTrackOptimization{
    "BackgroundSrcVideoTrackOptimization", base::FEATURE_DISABLED_BY_DEFAULT};

// Let video without audio be paused when it is playing in the background.
const base::Feature kBackgroundVideoPauseOptimization{
    "BackgroundVideoPauseOptimization", base::FEATURE_ENABLED_BY_DEFAULT};

// Make MSE garbage collection algorithm more aggressive when we are under
// moderate or critical memory pressure. This will relieve memory pressure by
// releasing stale data from MSE buffers.
const base::Feature kMemoryPressureBasedSourceBufferGC{
    "MemoryPressureBasedSourceBufferGC", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable MojoVideoDecoder.  On Android, we use this by default.  Elsewhere,
// it's experimental.
const base::Feature kMojoVideoDecoder {
  "MojoVideoDecoder",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Enable The D3D11 Video decoder. Must also enable MojoVideoDecoder for
// this to have any effect.
const base::Feature kD3D11VideoDecoder{"D3D11VideoDecoder",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Allow playback of encrypted media through the D3D11 decoder.  Requires
// D3D11VideoDecoder to be enabled also.
const base::Feature kD3D11EncryptedMedia{"D3D11EncryptedMedia",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Enable VP9 decoding in the D3D11VideoDecoder.
const base::Feature kD3D11VP9Decoder{"D3D11VP9Decoder",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Falls back to other decoders after audio/video decode error happens. The
// implementation may choose different strategies on when to fallback. See
// DecoderStream for details. When disabled, playback will fail immediately
// after a decode error happens. This can be useful in debugging and testing
// because the behavior is simpler and more predictable.
const base::Feature kFallbackAfterDecodeError{"FallbackAfterDecodeError",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Manage and report MSE buffered ranges by PTS intervals, not DTS intervals.
const base::Feature kMseBufferByPts{"MseBufferByPts",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enable new cpu load estimator. Intended for evaluation in local
// testing and origin-trial.
// TODO(nisse): Delete once we have switched over to always using the
// new estimator.
const base::Feature kNewEncodeCpuLoadEstimator{
    "NewEncodeCpuLoadEstimator", base::FEATURE_DISABLED_BY_DEFAULT};

// Use the new Remote Playback / media flinging pipeline.
const base::Feature kNewRemotePlaybackPipeline{
    "NewRemotePlaybackPipeline", base::FEATURE_DISABLED_BY_DEFAULT};

// Use the new RTC hardware decode path via RTCVideoDecoderAdapter.
const base::Feature kRTCVideoDecoderAdapter{"RTCVideoDecoderAdapter",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// CanPlayThrough issued according to standard.
const base::Feature kSpecCompliantCanPlayThrough{
    "SpecCompliantCanPlayThrough", base::FEATURE_ENABLED_BY_DEFAULT};

// Use shared block-based buffering for media.
const base::Feature kUseNewMediaCache{"use-new-media-cache",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Use R16 texture for 9-16 bit channel instead of half-float conversion by CPU.
const base::Feature kUseR16Texture{"use-r16-texture",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the Unified Autoplay policy by overriding the platform's default
// autoplay policy.
const base::Feature kUnifiedAutoplay{"UnifiedAutoplay",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, use SurfaceLayer instead of VideoLayer for all playbacks that
// aren't MediaStream.
const base::Feature kUseSurfaceLayerForVideo{"UseSurfaceLayerForVideo",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Use SurfaceLayer instead of VideoLayer when entering Picture-in-Picture mode.
// Does nothing if UseSurfaceLayerForVideo is enabled.  Does not affect
// MediaStream playbacks.
const base::Feature kUseSurfaceLayerForVideoPIP{
    "UseSurfaceLayerForVideoPIP", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable VA-API hardware encode acceleration for VP8.
const base::Feature kVaapiVP8Encoder{"VaapiVP8Encoder",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Inform video blitter of video color space.
const base::Feature kVideoBlitColorAccuracy{"video-blit-color-accuracy",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables support for External Clear Key (ECK) key system for testing on
// supported platforms. On platforms that do not support ECK, this feature has
// no effect.
const base::Feature kExternalClearKeyForTesting{
    "ExternalClearKeyForTesting", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables hardware secure decryption if supported by hardware and CDM.
// TODO(xhwang): Currently this is only used for development of new features.
// Apply this to Android and ChromeOS as well where hardware secure decryption
// is already available.
const base::Feature kHardwareSecureDecryption{
    "HardwareSecureDecryption", base::FEATURE_DISABLED_BY_DEFAULT};

// Limits number of media tags loading in parallel to 6. This speeds up
// preloading of any media that requires multiple requests to preload.
const base::Feature kLimitParallelMediaPreloading{
    "LimitParallelMediaPreloading", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables low-delay video rendering in media pipeline on "live" stream.
const base::Feature kLowDelayVideoRenderingOnLiveStream{
    "low-delay-video-rendering-on-live-stream",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Whether the autoplay policy should ignore Web Audio. When ignored, the
// autoplay policy will be hardcoded to be the legacy one on based on the
// platform
const base::Feature kAutoplayIgnoreWebAudio{"AutoplayIgnoreWebAudio",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Whether we should show a setting to disable autoplay policy.
const base::Feature kAutoplayDisableSettings{"AutoplayDisableSettings",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Whether we should allow autoplay whitelisting via sounds settings.
const base::Feature kAutoplayWhitelistSettings{
    "AutoplayWhitelistSettings", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Enable a gesture to make the media controls expaned into the display cutout.
const base::Feature kMediaControlsExpandGesture{
    "MediaControlsExpandGesture", base::FEATURE_ENABLED_BY_DEFAULT};

// Lock the screen orientation when a video goes fullscreen.
const base::Feature kVideoFullscreenOrientationLock{
    "VideoFullscreenOrientationLock", base::FEATURE_ENABLED_BY_DEFAULT};

// Enter/exit fullscreen when device is rotated to/from the video orientation.
const base::Feature kVideoRotateToFullscreen{"VideoRotateToFullscreen",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// An experimental feature to enable persistent-license type support in MediaDrm
// when using Encrypted Media Extensions (EME) API.
// TODO(xhwang): Remove this after feature launch. See http://crbug.com/493521
const base::Feature kMediaDrmPersistentLicense{
    "MediaDrmPersistentLicense", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the Android MediaRouter implementation using CAF (Cast v3).
const base::Feature kCafMediaRouterImpl{"CafMediaRouterImpl",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the Android Image Reader path for Video decoding(for AVDA and MCVD)
const base::Feature kAImageReaderVideoOutput{"AImageReaderVideoOutput",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_WIN)
// Does NV12->NV12 video copy on the main thread right before the texture's
// used by GL.
const base::Feature kDelayCopyNV12Textures{"DelayCopyNV12Textures",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Enables H264 HW encode acceleration using Media Foundation for Windows.
const base::Feature kMediaFoundationH264Encoding{
    "MediaFoundationH264Encoding", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables MediaFoundation based video capture
const base::Feature kMediaFoundationVideoCapture{
    "MediaFoundationVideoCapture", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables DirectShow GetPhotoState implementation
// Created to act as a kill switch by disabling it, in the case of the
// resurgence of https://crbug.com/722038
const base::Feature kDirectShowGetPhotoState{"DirectShowGetPhotoState",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

#endif  // defined(OS_WIN)

std::string GetEffectiveAutoplayPolicy(const base::CommandLine& command_line) {
  // Return the autoplay policy set in the command line, if any.
  if (command_line.HasSwitch(switches::kAutoplayPolicy))
    return command_line.GetSwitchValueASCII(switches::kAutoplayPolicy);

  if (base::FeatureList::IsEnabled(media::kUnifiedAutoplay))
    return switches::autoplay::kDocumentUserActivationRequiredPolicy;

// The default value is platform dependent.
#if defined(OS_ANDROID)
  return switches::autoplay::kUserGestureRequiredPolicy;
#else
  return switches::autoplay::kNoUserGestureRequiredPolicy;
#endif
}

// Adds icons to the overflow menu on the native media controls.
const base::Feature kOverflowIconsForMediaControls{
    "OverflowIconsForMediaControls", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the new redesigned media controls.
const base::Feature kUseModernMediaControls{"UseModernMediaControls",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables Media Engagement Index recording. This data will be used to determine
// when to bypass autoplay policies. This is recorded on all platforms.
const base::Feature kRecordMediaEngagementScores{
    "RecordMediaEngagementScores", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables Media Engagement Index recording for Web Audio playbacks.
const base::Feature kRecordWebAudioEngagement{"RecordWebAudioEngagement",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// The following Media Engagement flags are not enabled on mobile platforms:
// - MediaEngagementBypassAutoplayPolicies: enables the Media Engagement Index
//   data to be esude to override autoplay policies. An origin with a high MEI
//   will be allowed to autoplay.
// - PreloadMediaEngagementData: enables a list of origins to be considered as
//   having a high MEI until there is enough local data to determine the user's
//   preferred behaviour.
#if defined(OS_ANDROID) || defined(OS_IOS)
const base::Feature kMediaEngagementBypassAutoplayPolicies{
    "MediaEngagementBypassAutoplayPolicies", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kPreloadMediaEngagementData{
    "PreloadMediaEngagementData", base::FEATURE_DISABLED_BY_DEFAULT};
#else
const base::Feature kMediaEngagementBypassAutoplayPolicies{
    "MediaEngagementBypassAutoplayPolicies", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kPreloadMediaEngagementData{
    "PreloadMediaEngagementData", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

bool IsVideoCaptureAcceleratedJpegDecodingEnabled() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAcceleratedMjpegDecode)) {
    return false;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeJpegDecodeAccelerator)) {
    return true;
  }
#if defined(OS_CHROMEOS)
  return true;
#endif
  return false;
}

}  // namespace media
