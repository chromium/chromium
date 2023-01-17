// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/runtime_features.h"

#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/content_switches_internal.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "device/base/features.h"
#include "device/fido/features.h"
#include "device/gamepad/public/cpp/gamepad_features.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "media/base/media_switches.h"
#include "net/base/features.h"
#include "services/device/public/cpp/device_features.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/blink/blink_features.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/native_theme/native_theme_features.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

using blink::WebRuntimeFeatures;

namespace {

// Sets blink runtime features for specific platforms.
// This should be a last resort vs runtime_enabled_features.json5.
void SetRuntimeFeatureDefaultsForPlatform(
    const base::CommandLine& command_line) {
  // Please consider setting up feature defaults for different platforms
  // in runtime_enabled_features.json5 instead of here
  // TODO(rodneyding): Move the more common cases here
  // to baseFeature/switch functions below and move more complex
  // ones to special case functions.
#if defined(USE_AURA)
  WebRuntimeFeatures::EnableCompositedSelectionUpdate(true);
#endif
#if BUILDFLAG(IS_WIN)
  if (base::win::GetVersion() >= base::win::Version::WIN10)
    WebRuntimeFeatures::EnableWebBluetooth(true);
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_LACROS)
  const bool enable_canvas_2d_image_chromium =
      command_line.HasSwitch(
          blink::switches::kEnableGpuMemoryBufferCompositorResources) &&
      !command_line.HasSwitch(switches::kDisable2dCanvasImageChromium) &&
      !command_line.HasSwitch(switches::kDisableGpu) &&
      base::FeatureList::IsEnabled(features::kCanvas2DImageChromium);
#else
  constexpr bool enable_canvas_2d_image_chromium = false;
#endif
  WebRuntimeFeatures::EnableCanvas2dImageChromium(
      enable_canvas_2d_image_chromium);

#if BUILDFLAG(IS_MAC)
  const bool enable_web_gl_image_chromium =
      command_line.HasSwitch(
          blink::switches::kEnableGpuMemoryBufferCompositorResources) &&
      !command_line.HasSwitch(switches::kDisableWebGLImageChromium) &&
      !command_line.HasSwitch(switches::kDisableGpu) &&
      base::FeatureList::IsEnabled(features::kWebGLImageChromium);
#else
  const bool enable_web_gl_image_chromium =
      command_line.HasSwitch(switches::kEnableWebGLImageChromium);
#endif
  WebRuntimeFeatures::EnableWebGLImageChromium(enable_web_gl_image_chromium);

#if BUILDFLAG(IS_ANDROID)
  if (command_line.HasSwitch(switches::kDisableMediaSessionAPI))
    WebRuntimeFeatures::EnableMediaSession(false);
#endif

#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_P) {
    // Display Cutout is limited to Android P+.
    WebRuntimeFeatures::EnableDisplayCutoutAPI(true);
  }
#endif

#if BUILDFLAG(IS_ANDROID)
  WebRuntimeFeatures::EnableMediaControlsExpandGesture(
      base::FeatureList::IsEnabled(media::kMediaControlsExpandGesture));
#endif
}

enum RuntimeFeatureEnableOptions {
  // - If the base::Feature default is overridden by field trial or command
  //   line, set Blink feature to the state of the base::Feature;
  // - Otherwise if the base::Feature is enabled, enable the Blink feature.
  // - Otherwise no change.
  kDefault,
  // Enables the Blink feature when the base::Feature is overridden by field
  // trial or command line. Otherwise no change. Its difference from kDefault is
  // that the Blink feature isn't affected by the default state of the
  // base::Feature. This is useful for Blink origin trial features especially
  // those implemented in both Chromium and Blink. As origin trial only controls
  // the Blink features, for now we require the base::Feature to be enabled by
  // default, but we don't want the default enabled status affect the Blink
  // feature. See also https://crbug.com/1048656#c10.
  // This can also be used for features that are enabled by default in Chromium
  // but not in Blink on all platforms and we want to use the Blink status.
  // However, we would prefer consistent Chromium and Blink status to this.
  kSetOnlyIfOverridden,
};

template <typename T>
// Helper class that describes the desired actions for the runtime feature
// depending on a check for chromium base::Feature.
struct RuntimeFeatureToChromiumFeatureMap {
  // This can be either an enabler function defined in web_runtime_features.cc
  // or the string name of the feature in runtime_enabled_features.json5.
  T feature_enabler;
  // The chromium base::Feature to check.
  const base::Feature& chromium_feature;
  const RuntimeFeatureEnableOptions option = kDefault;
};

template <typename Enabler>
void SetRuntimeFeatureFromChromiumFeature(const base::Feature& chromium_feature,
                                          RuntimeFeatureEnableOptions option,
                                          const Enabler& enabler) {
  using FeatureList = base::FeatureList;
  const bool feature_enabled = FeatureList::IsEnabled(chromium_feature);
  const bool is_overridden =
      FeatureList::GetInstance()->IsFeatureOverridden(chromium_feature.name);
  switch (option) {
    case kSetOnlyIfOverridden:
      if (is_overridden)
        enabler(feature_enabled);
      break;
    case kDefault:
      if (feature_enabled || is_overridden)
        enabler(feature_enabled);
      break;
    default:
      NOTREACHED();
  }
}

// Sets blink runtime features that are either directly
// controlled by Chromium base::Feature or are overridden
// by base::Feature states.
void SetRuntimeFeaturesFromChromiumFeatures() {
  using wf = WebRuntimeFeatures;
  // To add a runtime feature control, add a new
  // RuntimeFeatureToChromiumFeatureMap entry here if there is a custom
  // enabler function defined. Otherwise add the entry with string name
  // in the next list.
  const RuntimeFeatureToChromiumFeatureMap<void (*)(bool)>
      blinkFeatureToBaseFeatureMapping[] =
  { {wf::EnableAccessibilityAriaVirtualContent,
     features::kEnableAccessibilityAriaVirtualContent},
    {wf::EnableAccessibilityExposeHTMLElement,
     features::kEnableAccessibilityExposeHTMLElement},
    {wf::EnableAccessibilityExposeIgnoredNodes,
     features::kEnableAccessibilityExposeIgnoredNodes},
#if BUILDFLAG(IS_ANDROID)
    {wf::EnableAccessibilityPageZoom, features::kAccessibilityPageZoom},
    {wf::EnableAutoDisableAccessibilityV2,
     features::kAutoDisableAccessibilityV2},
#endif
    {wf::EnableAccessibilityUseAXPositionForDocumentMarkers,
     features::kUseAXPositionForDocumentMarkers},
    {wf::EnableAOMAriaRelationshipProperties,
     features::kEnableAriaElementReflection},
    {wf::EnableAutoplayIgnoresWebAudio, media::kAutoplayIgnoreWebAudio},
    {wf::EnableBackgroundFetch, features::kBackgroundFetch},
    {wf::EnableBrowserVerifiedUserActivationKeyboard,
     features::kBrowserVerifiedUserActivationKeyboard},
    {wf::EnableBrowserVerifiedUserActivationMouse,
     features::kBrowserVerifiedUserActivationMouse},
    {wf::EnableCompositeBGColorAnimation, features::kCompositeBGColorAnimation},
    {wf::EnableCompositeClipPathAnimation,
     features::kCompositeClipPathAnimation},
    {wf::EnableConsolidatedMovementXY, features::kConsolidatedMovementXY},
    {wf::EnableCooperativeScheduling, features::kCooperativeScheduling},
    {wf::EnableDevicePosture, features::kDevicePosture},
    {wf::EnableDigitalGoods, features::kDigitalGoodsApi, kSetOnlyIfOverridden},
    {wf::EnableDirectSockets, features::kIsolatedWebApps},
    {wf::EnableDocumentPolicy, features::kDocumentPolicy},
    {wf::EnableDocumentPolicyNegotiation, features::kDocumentPolicyNegotiation},
    {wf::EnableFedCm, features::kFedCm, kSetOnlyIfOverridden},
    {wf::EnableFedCmIframeSupport, features::kFedCmIframeSupport,
     kSetOnlyIfOverridden},
    {wf::EnableFedCmMultipleIdentityProviders,
     features::kFedCmMultipleIdentityProviders, kDefault},
    {wf::EnableFedCmUserInfo, features::kFedCmUserInfo, kDefault},
    {wf::EnableFencedFrames, features::kPrivacySandboxAdsAPIsOverride,
     kSetOnlyIfOverridden},
    {wf::EnableSharedStorageAPI, features::kPrivacySandboxAdsAPIsOverride,
     kSetOnlyIfOverridden},
    {wf::EnableForcedColors, features::kForcedColors},
    {wf::EnableFractionalScrollOffsets, features::kFractionalScrollOffsets},
    {wf::EnableSensorExtraClasses, features::kGenericSensorExtraClasses},
#if BUILDFLAG(IS_ANDROID)
    {wf::EnableGetDisplayMedia, features::kUserMediaScreenCapturing},
#endif
    {wf::EnableIdleDetection, features::kIdleDetection, kSetOnlyIfOverridden},
    {wf::EnableInstalledApp, features::kInstalledApp},
    {wf::EnableLazyInitializeMediaControls,
     features::kLazyInitializeMediaControls},
    {wf::EnableLazyFrameLoading, features::kLazyFrameLoading},
    {wf::EnableLazyFrameVisibleLoadTimeMetrics,
     features::kLazyFrameVisibleLoadTimeMetrics},
    {wf::EnableLazyImageLoading, features::kLazyImageLoading},
    {wf::EnableLazyImageVisibleLoadTimeMetrics,
     features::kLazyImageVisibleLoadTimeMetrics},
    {wf::EnableMediaCastOverlayButton, media::kMediaCastOverlayButton},
    {wf::EnableMediaEngagementBypassAutoplayPolicies,
     media::kMediaEngagementBypassAutoplayPolicies},
    {wf::EnableMouseSubframeNoImplicitCapture,
     features::kMouseSubframeNoImplicitCapture},
    {wf::EnableNeverSlowMode, features::kNeverSlowMode},
    {wf::EnableNotificationContentImage, features::kNotificationContentImage,
     kSetOnlyIfOverridden},
    {wf::EnablePaymentApp, features::kServiceWorkerPaymentApps},
    {wf::EnableWebPaymentAPICSP, features::kWebPaymentAPICSP},
    {wf::EnablePaymentRequest, features::kWebPayments},
    {wf::EnablePercentBasedScrolling, features::kWindowsScrollingPersonality},
    {wf::EnablePeriodicBackgroundSync, features::kPeriodicBackgroundSync},
    {wf::EnablePointerLockOptions, features::kPointerLockOptions},
    {wf::EnablePushMessagingSubscriptionChange,
     features::kPushSubscriptionChangeEvent},
    {wf::EnableRestrictGamepadAccess, features::kRestrictGamepadAccess},
    {wf::EnableSecurePaymentConfirmation, features::kSecurePaymentConfirmation},
    {wf::EnableSecurePaymentConfirmationDebug,
     features::kSecurePaymentConfirmationDebug},
    {wf::EnableSendBeaconThrowForBlobWithNonSimpleType,
     features::kSendBeaconThrowForBlobWithNonSimpleType},
    {wf::EnableSharedArrayBuffer, features::kSharedArrayBuffer},
    {wf::EnableSharedArrayBufferOnDesktop,
     features::kSharedArrayBufferOnDesktop},
    {wf::EnableSharedAutofill, autofill::features::kAutofillSharedAutofill},
    {wf::EnableTouchDragAndContextMenu, features::kTouchDragAndContextMenu},
    {wf::EnableUserActivationSameOriginVisibility,
     features::kUserActivationSameOriginVisibility},
    {wf::EnableVideoPlaybackQuality, features::kVideoPlaybackQuality},
    {wf::EnableVideoWakeLockOptimisationHiddenMuted,
     media::kWakeLockOptimisationHiddenMuted},
    {wf::EnableWebBluetooth, features::kWebBluetooth, kSetOnlyIfOverridden},
    {wf::EnableWebBluetoothGetDevices,
     features::kWebBluetoothNewPermissionsBackend, kSetOnlyIfOverridden},
    {wf::EnableWebBluetoothWatchAdvertisements,
     features::kWebBluetoothNewPermissionsBackend, kSetOnlyIfOverridden},
#if BUILDFLAG(IS_ANDROID)
    {wf::EnableWebNFC, features::kWebNfc, kSetOnlyIfOverridden},
#endif
    {wf::EnableWebOTP, features::kWebOTP, kSetOnlyIfOverridden},
    {wf::EnableWebOTPAssertionFeaturePolicy,
     features::kWebOTPAssertionFeaturePolicy, kSetOnlyIfOverridden},
    {wf::EnableWebUSB, features::kWebUsb},
    {wf::EnableWebXR, features::kWebXr},
    {wf::EnableWebXRARModule, features::kWebXrArModule},
    {wf::EnableWebXRCameraAccess, device::features::kWebXrIncubations},
    {wf::EnableWebXRHandInput, device::features::kWebXrHandInput},
    {wf::EnableWebXRHitTest, device::features::kWebXrHitTest},
    {wf::EnableWebXRImageTracking, device::features::kWebXrIncubations},
    {wf::EnableWebXRLayers, device::features::kWebXrLayers},
    {wf::EnableWebXRPlaneDetection, device::features::kWebXrIncubations},
    {wf::EnableRemoveMobileViewportDoubleTap,
     features::kRemoveMobileViewportDoubleTap},
    {wf::EnableGetDisplayMediaSet, features::kGetDisplayMediaSet},
    {wf::EnableGetDisplayMediaSetAutoSelectAllScreens,
     features::kGetDisplayMediaSetAutoSelectAllScreens},
    {wf::EnableServiceWorkerBypassFetchHandler,
     features::kServiceWorkerBypassFetchHandler},
  };
  for (const auto& mapping : blinkFeatureToBaseFeatureMapping) {
    SetRuntimeFeatureFromChromiumFeature(
        mapping.chromium_feature, mapping.option, mapping.feature_enabler);
  }

  // TODO(crbug/832393): Cleanup the inconsistency between custom WRF enabler
  // function and using feature string name with EnableFeatureFromString.
  const RuntimeFeatureToChromiumFeatureMap<const char*>
      runtimeFeatureNameToChromiumFeatureMapping[] = {
          {"AllowContentInitiatedDataUrlNavigations",
           features::kAllowContentInitiatedDataUrlNavigations},
          {"AttributionReporting", features::kPrivacySandboxAdsAPIsOverride,
           kSetOnlyIfOverridden},
          {"AndroidDownloadableFontsMatching",
           features::kAndroidDownloadableFontsMatching},
          {"FirstPartySets", features::kFirstPartySets},
          {"Fledge", blink::features::kFledge, kSetOnlyIfOverridden},
          {"Fledge", features::kPrivacySandboxAdsAPIsOverride,
           kSetOnlyIfOverridden},
          {"FontSrcLocalMatching", features::kFontSrcLocalMatching},
          {"LegacyWindowsDWriteFontFallback",
           features::kLegacyWindowsDWriteFontFallback},
          {"OriginIsolationHeader", features::kOriginIsolationHeader},
          {"PartitionedCookies", net::features::kPartitionedCookies},
          {"ReduceAcceptLanguage", network::features::kReduceAcceptLanguage},
          {"StorageAccessAPI", net::features::kStorageAccessAPI},
          {"TopicsAPI", features::kPrivacySandboxAdsAPIsOverride,
           kSetOnlyIfOverridden},
          {"TrustedTypesFromLiteral", features::kTrustedTypesFromLiteral},
          {"WebAppTabStrip", features::kDesktopPWAsTabStrip},
          {"WebAuthenticationConditionalUI", features::kWebAuthConditionalUI},
          {"WGIGamepadTriggerRumble",
           features::kEnableWindowsGamingInputDataFetcher},
          {"UserAgentFull", blink::features::kFullUserAgent},
          {"MediaStreamTrackTransfer", features::kMediaStreamTrackTransfer}};
  for (const auto& mapping : runtimeFeatureNameToChromiumFeatureMapping) {
    SetRuntimeFeatureFromChromiumFeature(
        mapping.chromium_feature, mapping.option, [&mapping](bool enabled) {
          wf::EnableFeatureFromString(mapping.feature_enabler, enabled);
        });
  }

  WebRuntimeFeatures::UpdateStatusFromBaseFeatures();
}

// Helper class that describes the desired enable/disable action
// for a runtime feature when a command line switch exists.
struct SwitchToFeatureMap {
  // The enabler function defined in web_runtime_features.cc.
  void (*feature_enabler)(bool);
  // The switch to check for on command line.
  const char* switch_name;
  // This is the desired state for the runtime feature if the
  // switch exists on command line.
  bool target_enabled_state;
};

// Sets blink runtime features controlled by command line switches.
void SetRuntimeFeaturesFromCommandLine(const base::CommandLine& command_line) {
  // To add a new switch-controlled runtime feature, add a new
  // SwitchToFeatureMap entry to the initializer list below.
  // Note: command line switches are now discouraged, please consider
  // using base::Feature instead.
  // https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/configuration.md#switches
  using wrf = WebRuntimeFeatures;
  const SwitchToFeatureMap switchToFeatureMapping[] = {
      // Stable Features
      {wrf::EnablePermissions, switches::kDisablePermissionsAPI, false},
      {wrf::EnablePresentation, switches::kDisablePresentationAPI, false},
      {wrf::EnableRemotePlayback, switches::kDisableRemotePlaybackAPI, false},
      {wrf::EnableTimerThrottlingForBackgroundTabs,
       switches::kDisableBackgroundTimerThrottling, false},
      // End of Stable Features
      {wrf::EnableAutomationControlled, switches::kEnableAutomation, true},
      {wrf::EnableAutomationControlled, switches::kHeadless, true},
      {wrf::EnableAutomationControlled, switches::kRemoteDebuggingPipe, true},
      {wrf::EnableDatabase, switches::kDisableDatabases, false},
      {wrf::EnableFileSystem, switches::kDisableFileSystem, false},
      {wrf::EnableFileSystemSyncAccessHandleAsyncInterfaceOverride,
       switches::kFileSystemSyncAccessHandleAsyncInterfaceEnabled, true},
      {wrf::EnableNetInfoDownlinkMax,
       switches::kEnableNetworkInformationDownlinkMax, true},
      {wrf::EnableNotifications, switches::kDisableNotifications, false},
      {wrf::EnablePreciseMemoryInfo, switches::kEnablePreciseMemoryInfo, true},
      {wrf::EnablePrefixedStorageInfo,
       blink::switches::kPrefixedStorageInfoEnabled, true},
      // Chrome's Push Messaging implementation relies on Web Notifications.
      {wrf::EnablePushMessaging, switches::kDisableNotifications, false},
      {wrf::EnableScriptedSpeechRecognition, switches::kDisableSpeechAPI,
       false},
      {wrf::EnableScriptedSpeechSynthesis, switches::kDisableSpeechAPI, false},
      {wrf::EnableScriptedSpeechSynthesis, switches::kDisableSpeechSynthesisAPI,
       false},
      {wrf::EnableSharedWorker, switches::kDisableSharedWorkers, false},
      {wrf::EnableTextFragmentIdentifiers,
       switches::kDisableScrollToTextFragment, false},
      {wrf::EnableWebAuthenticationRemoteDesktopSupport,
       switches::kWebAuthRemoteDesktopSupport, true},
      {wrf::EnableWebGLDeveloperExtensions,
       switches::kEnableWebGLDeveloperExtensions, true},
      {wrf::EnableWebGLDraftExtensions, switches::kEnableWebGLDraftExtensions,
       true},
      {wrf::EnableWebGPUDeveloperFeatures,
       switches::kEnableWebGPUDeveloperFeatures, true},
      {wrf::EnableDirectSockets, switches::kIsolatedAppOrigins, true},
  };
  for (const auto& mapping : switchToFeatureMapping) {
    if (command_line.HasSwitch(mapping.switch_name))
      mapping.feature_enabler(mapping.target_enabled_state);
  }

  // Set EnableAutomationControlled if the caller passes
  // --remote-debugging-port=0 on the command line. This means
  // the caller has requested an ephemeral port which is how ChromeDriver
  // launches the browser by default.
  // If the caller provides a specific port number, this is
  // more likely for attaching a debugger, so we should leave
  // EnableAutomationControlled unset to ensure the browser behaves as it does
  // when not under automation control.
  if (command_line.HasSwitch(switches::kRemoteDebuggingPort)) {
    std::string port_str =
        command_line.GetSwitchValueASCII(::switches::kRemoteDebuggingPort);
    int port;
    if (base::StringToInt(port_str, &port) && port == 0) {
      WebRuntimeFeatures::EnableAutomationControlled(true);
    }
  }

  // Enable or disable EventPath if Enterprise Policy passes
  // --event-path-policy=0 or =1 on the command line. This overrides any
  // existing setting via base::Feature.
  if (command_line.HasSwitch(blink::switches::kEventPathPolicy)) {
    const std::string value =
        command_line.GetSwitchValueASCII(blink::switches::kEventPathPolicy);
    if (value == blink::switches::kEventPathPolicy_ForceEnable)
      WebRuntimeFeatures::EnableEventPath(true);
    if (value == blink::switches::kEventPathPolicy_ForceDisable)
      WebRuntimeFeatures::EnableEventPath(false);
  }

  // Enable or disable OffsetParentNewSpecBehavior for Enterprise Policy. This
  // overrides any existing settings via base::Feature.
  if (command_line.HasSwitch(
          blink::switches::kOffsetParentNewSpecBehaviorPolicy)) {
    const std::string value = command_line.GetSwitchValueASCII(
        blink::switches::kOffsetParentNewSpecBehaviorPolicy);
    if (value ==
        blink::switches::kOffsetParentNewSpecBehaviorPolicy_ForceEnable)
      WebRuntimeFeatures::EnableOffsetParentNewSpecBehavior(true);
    if (value ==
        blink::switches::kOffsetParentNewSpecBehaviorPolicy_ForceDisable)
      WebRuntimeFeatures::EnableOffsetParentNewSpecBehavior(false);
  }

  // Enable or disable SendMouseEventsDisabledFormControls for Enterprise
  // Policy. This overrides any existing settings via base::Feature.
  if (command_line.HasSwitch(
          blink::switches::kSendMouseEventsDisabledFormControlsPolicy)) {
    const std::string value = command_line.GetSwitchValueASCII(
        blink::switches::kSendMouseEventsDisabledFormControlsPolicy);
    if (value ==
        blink::switches::kSendMouseEventsDisabledFormControlsPolicy_ForceEnable)
      WebRuntimeFeatures::EnableSendMouseEventsDisabledFormControls(true);
    if (value == blink::switches::
                     kSendMouseEventsDisabledFormControlsPolicy_ForceDisable)
      WebRuntimeFeatures::EnableSendMouseEventsDisabledFormControls(false);
  }
}

// Sets blink runtime features that depend on a combination
// of args rather than a single check of base::Feature or switch.
// This can be a combination of both or custom checking logic
// not covered by other functions. In short, this should be used
// as a last resort.
void SetCustomizedRuntimeFeaturesFromCombinedArgs(
    const base::CommandLine& command_line) {
  // CAUTION: Only add custom enabling logic here if it cannot
  // be covered by the other functions.

  if (!command_line.HasSwitch(switches::kDisableYUVImageDecoding) &&
      base::FeatureList::IsEnabled(
          blink::features::kDecodeJpeg420ImagesToYUV)) {
    WebRuntimeFeatures::EnableDecodeJpeg420ImagesToYUV(true);
  }
  if (!command_line.HasSwitch(switches::kDisableYUVImageDecoding) &&
      base::FeatureList::IsEnabled(
          blink::features::kDecodeLossyWebPImagesToYUV)) {
    WebRuntimeFeatures::EnableDecodeLossyWebPImagesToYUV(true);
  }

  // These checks are custom wrappers around base::FeatureList::IsEnabled
  // They're moved here to distinguish them from actual base checks
  WebRuntimeFeatures::EnableOverlayScrollbars(ui::IsOverlayScrollbarEnabled());
  WebRuntimeFeatures::EnableFluentScrollbars(ui::IsFluentScrollbarEnabled());

  // TODO(rodneyding): This is a rare case for a stable feature
  // Need to investigate more to determine whether to refactor it.
  if (command_line.HasSwitch(switches::kDisableV8IdleTasks))
    WebRuntimeFeatures::EnableV8IdleTasks(false);
  else
    WebRuntimeFeatures::EnableV8IdleTasks(true);

  WebRuntimeFeatures::EnableBackForwardCache(
      content::IsBackForwardCacheEnabled());

  if (base::FeatureList::IsEnabled(network::features::kPrivateStateTokens)) {
    // See https://bit.ly/configuring-trust-tokens.
    using network::features::TrustTokenOriginTrialSpec;
    switch (
        network::features::kTrustTokenOperationsRequiringOriginTrial.Get()) {
      case TrustTokenOriginTrialSpec::kOriginTrialNotRequired:
        // Setting PrivateStateTokens=true enables the Trust Tokens interface;
        // PrivateStateTokensAlwaysAllowIssuance disables a runtime check
        // during issuance that the origin trial is active (see
        // blink/.../trust_token_issuance_authorization.h).
        WebRuntimeFeatures::EnablePrivateStateTokens(true);
        WebRuntimeFeatures::EnablePrivateStateTokensAlwaysAllowIssuance(true);
        break;
      case TrustTokenOriginTrialSpec::kAllOperationsRequireOriginTrial:
        // The origin trial itself will be responsible for enabling the
        // PrivateStateTokens RuntimeEnabledFeature.
        WebRuntimeFeatures::EnablePrivateStateTokens(false);
        WebRuntimeFeatures::EnablePrivateStateTokensAlwaysAllowIssuance(false);
        break;
      case TrustTokenOriginTrialSpec::kOnlyIssuanceRequiresOriginTrial:
        // At issuance, a runtime check will be responsible for checking that
        // the origin trial is present.
        WebRuntimeFeatures::EnablePrivateStateTokens(true);
        WebRuntimeFeatures::EnablePrivateStateTokensAlwaysAllowIssuance(false);
        break;
    }
  }

  // Enables the Blink feature only when the base feature variation is enabled.
  if (base::FeatureList::IsEnabled(features::kFedCm)) {
    if (base::GetFieldTrialParamByFeatureAsBool(
            features::kFedCm, features::kFedCmIdpSignoutFieldTrialParamName,
            false)) {
      WebRuntimeFeatures::EnableFedCmIdpSignout(true);
    }
    if (base::GetFieldTrialParamByFeatureAsBool(
            features::kFedCm,
            features::kFedCmIdpSigninStatusFieldTrialParamName, false)) {
      WebRuntimeFeatures::EnableFedCmIdpSigninStatus(true);
    }
  }

  // (b/239679616) kWebGPUService can be controlled by finch. So switching off
  // WebGPU based on it can help remotely control origin trial usage. Local
  // command switches --enable-unsafe-webgpu can still enable WebGPU.
  if (!base::FeatureList::IsEnabled(features::kWebGPUService)) {
    WebRuntimeFeatures::EnableWebGPU(false);
  }
  if (command_line.HasSwitch(switches::kEnableUnsafeWebGPU)) {
    WebRuntimeFeatures::EnableWebGPU(true);
  }

  if (base::FeatureList::IsEnabled(blink::features::kPendingBeaconAPI)) {
    // The Chromium flag `kPendingBeaconAPI` is true, which enables the
    // parts of the API's implementation in Chromium.
    if (blink::features::kPendingBeaconAPIRequiresOriginTrial.Get()) {
      // `kPendingBeaconAPIRequiresOriginTrial`=true specifies that
      // execution context needs to have an origin trial token in order to use
      // the PendingBeacon web API.
      // So disable the RuntimeEnabledFeature flag PendingBeaconAPI here and let
      // the existence of OT token to decide whether the web API is enabled.
      WebRuntimeFeatures::EnablePendingBeaconAPI(false);
    } else {
      WebRuntimeFeatures::EnablePendingBeaconAPI(true);
    }
  }
}

// Ensures that the various ways of enabling/disabling features do not produce
// an invalid configuration.
void ResolveInvalidConfigurations() {
  // Portals cannot be enabled without the support of the browser process.
  if (!base::FeatureList::IsEnabled(blink::features::kPortals)) {
    LOG_IF(WARNING, WebRuntimeFeatures::IsPortalsEnabled())
        << "Portals cannot be enabled in this configuration. Use --"
        << switches::kEnableFeatures << "=" << blink::features::kPortals.name
        << " instead.";
    WebRuntimeFeatures::EnablePortals(false);
  }

  // Fenced frames, like Portals, cannot be enabled without the support of the
  // browser process.
  if (base::FeatureList::IsEnabled(features::kPrivacySandboxAdsAPIsOverride) &&
      !base::FeatureList::IsEnabled(blink::features::kFencedFrames)) {
    LOG_IF(WARNING, WebRuntimeFeatures::IsFencedFramesEnabled())
        << "Fenced frames cannot be enabled in this configuration. Use --"
        << switches::kEnableFeatures << "="
        << blink::features::kFencedFrames.name << " instead.";
    WebRuntimeFeatures::EnableFencedFrames(false);
  }

  // Topics API cannot be enabled without the support of the browser process.
  if (base::FeatureList::IsEnabled(features::kPrivacySandboxAdsAPIsOverride) &&
      !base::FeatureList::IsEnabled(blink::features::kBrowsingTopics)) {
    LOG_IF(WARNING, WebRuntimeFeatures::IsTopicsAPIEnabled())
        << "Topics cannot be enabled in this configuration. Use --"
        << switches::kEnableFeatures << "="
        << blink::features::kBrowsingTopics.name << " instead.";
    WebRuntimeFeatures::EnableTopicsAPI(false);
  }

  // Storage Access API ForSite cannot be enabled unless the larger Storage
  // Access API is also enabled.
  if (base::FeatureList::IsEnabled(
          blink::features::kStorageAccessAPIForOriginExtension) &&
      !base::FeatureList::IsEnabled(net::features::kStorageAccessAPI)) {
    LOG_IF(WARNING,
           WebRuntimeFeatures::IsStorageAccessAPIForOriginExtensionEnabled())
        << "requestStorageAccessForOrigin cannot be enabled in this "
           "configuration. Use --"
        << switches::kEnableFeatures << "="
        << net::features::kStorageAccessAPI.name << " in addition.";
    WebRuntimeFeatures::EnableStorageAccessAPIForOriginExtension(false);
  }
}

}  // namespace

namespace content {

void SetRuntimeFeaturesDefaultsAndUpdateFromArgs(
    const base::CommandLine& command_line) {
  // Sets experimental features.
  bool enable_experimental_web_platform_features =
      command_line.HasSwitch(switches::kEnableExperimentalWebPlatformFeatures);
  bool enable_blink_test_features =
      command_line.HasSwitch(switches::kEnableBlinkTestFeatures);

  if (enable_blink_test_features) {
    enable_experimental_web_platform_features = true;
    WebRuntimeFeatures::EnableTestOnlyFeatures(true);
  }

  if (enable_experimental_web_platform_features)
    WebRuntimeFeatures::EnableExperimentalFeatures(true);

  SetRuntimeFeatureDefaultsForPlatform(command_line);

  // Sets origin trial features.
  if (command_line.HasSwitch(
          switches::kDisableOriginTrialControlledBlinkFeatures)) {
    WebRuntimeFeatures::EnableOriginTrialControlledFeatures(false);
  }

  // TODO(rodneyding): add doc explaining ways to add new runtime features
  // controls in the following functions.

  SetRuntimeFeaturesFromChromiumFeatures();

  SetRuntimeFeaturesFromCommandLine(command_line);

  SetCustomizedRuntimeFeaturesFromCombinedArgs(command_line);

  // Enable explicitly enabled features, and then disable explicitly disabled
  // ones.
  for (const std::string& feature :
       FeaturesFromSwitch(command_line, switches::kEnableBlinkFeatures)) {
    WebRuntimeFeatures::EnableFeatureFromString(feature, true);
  }
  for (const std::string& feature :
       FeaturesFromSwitch(command_line, switches::kDisableBlinkFeatures)) {
    WebRuntimeFeatures::EnableFeatureFromString(feature, false);
  }

  ResolveInvalidConfigurations();
}

}  // namespace content
