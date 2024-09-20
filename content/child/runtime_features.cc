// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/runtime_features.h"

#include <string>
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
#include "components/attribution_reporting/features.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/content_switches_internal.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "device/base/features.h"
#include "device/fido/features.h"
#include "device/gamepad/public/cpp/gamepad_features.h"
#include "device/vr/buildflags/buildflags.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "media/base/media_switches.h"
#include "services/device/public/cpp/device_features.h"
#include "services/network/public/cpp/features.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
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

#if BUILDFLAG(ENABLE_VR)
#include "device/vr/public/cpp/features.h"
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

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_CHROMEOS_LACROS)
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

#if BUILDFLAG(IS_APPLE)
  const bool enable_web_gl_image_chromium =
      command_line.HasSwitch(
          blink::switches::kEnableGpuMemoryBufferCompositorResources) &&
      !command_line.HasSwitch(switches::kDisableWebGLImageChromium) &&
      !command_line.HasSwitch(switches::kDisableGpu);
#else
  const bool enable_web_gl_image_chromium =
      command_line.HasSwitch(switches::kEnableWebGLImageChromium);
#endif
  WebRuntimeFeatures::EnableWebGLImageChromium(enable_web_gl_image_chromium);

#if BUILDFLAG(IS_ANDROID)
  if (command_line.HasSwitch(switches::kDisableMediaSessionAPI)) {
    WebRuntimeFeatures::EnableMediaSession(false);
  }
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
  const raw_ref<const base::Feature> chromium_feature;
  const RuntimeFeatureEnableOptions option = kDefault;
};

template <typename Enabler>
void SetRuntimeFeatureFromChromiumFeature(const base::Feature& chromium_feature,
                                          RuntimeFeatureEnableOptions option,
                                          const Enabler& enabler) {
  using FeatureList = base::FeatureList;
  const bool feature_enabled = FeatureList::IsEnabled(chromium_feature);
  const bool is_overridden =
      FeatureList::GetStateIfOverridden(chromium_feature).has_value();
  switch (option) {
    case kSetOnlyIfOverridden:
      if (is_overridden) {
        enabler(feature_enabled);
      }
      break;
    case kDefault:
      if (feature_enabled || is_overridden) {
        enabler(feature_enabled);
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION();
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
      blinkFeatureToBaseFeatureMapping[] = {
          {wf::EnableAccessibilityAriaVirtualContent,
           raw_ref(features::kEnableAccessibilityAriaVirtualContent)},
#if BUILDFLAG(IS_ANDROID)
          {wf::EnableAccessibilityPageZoom,
           raw_ref(features::kAccessibilityPageZoom)},
#endif
          {wf::EnableAccessibilityUseAXPositionForDocumentMarkers,
           raw_ref(features::kUseAXPositionForDocumentMarkers)},
          {wf::EnableAOMAriaRelationshipProperties,
           raw_ref(features::kEnableAriaElementReflection)},
          {wf::EnableBackgroundFetch, raw_ref(features::kBackgroundFetch)},
          {wf::EnableBoundaryEventDispatchTracksNodeRemoval,
           raw_ref(blink::features::kBoundaryEventDispatchTracksNodeRemoval)},
          {wf::EnableBrowserVerifiedUserActivationKeyboard,
           raw_ref(features::kBrowserVerifiedUserActivationKeyboard)},
          {wf::EnableBrowserVerifiedUserActivationMouse,
           raw_ref(features::kBrowserVerifiedUserActivationMouse)},
          {wf::EnableCompositeBGColorAnimation,
           raw_ref(features::kCompositeBGColorAnimation)},
          {wf::EnableCooperativeScheduling,
           raw_ref(features::kCooperativeScheduling)},
          {wf::EnableDigitalGoods, raw_ref(features::kDigitalGoodsApi),
           kSetOnlyIfOverridden},
          {wf::EnableDocumentPolicyNegotiation,
           raw_ref(features::kDocumentPolicyNegotiation)},
          {wf::EnableEyeDropperAPI, raw_ref(features::kEyeDropper),
           kSetOnlyIfOverridden},
          {wf::EnableFedCm, raw_ref(features::kFedCm), kSetOnlyIfOverridden},
          {wf::EnableFedCmButtonMode, raw_ref(features::kFedCmButtonMode),
           kSetOnlyIfOverridden},
          {wf::EnableFedCmAuthz, raw_ref(features::kFedCmAuthz), kDefault},
          {wf::EnableFedCmIdPRegistration,
           raw_ref(features::kFedCmIdPRegistration), kDefault},
          {wf::EnableFedCmIdpSigninStatus,
           raw_ref(features::kFedCmIdpSigninStatusEnabled),
           kSetOnlyIfOverridden},
          {wf::EnableGamepadMultitouch,
           raw_ref(features::kEnableGamepadMultitouch)},
          {wf::EnableSharedStorageAPI,
           raw_ref(features::kPrivacySandboxAdsAPIsOverride),
           kSetOnlyIfOverridden},
          {wf::EnableSharedStorageAPI,
           raw_ref(features::kPrivacySandboxAdsAPIsM1Override)},
          {wf::EnableSharedStorageAPIM118,
           raw_ref(blink::features::kSharedStorageAPIM118), kDefault},
          {wf::EnableSharedStorageAPIM125,
           raw_ref(blink::features::kSharedStorageAPIM125), kDefault},
          {wf::EnableFedCmMultipleIdentityProviders,
           raw_ref(features::kFedCmMultipleIdentityProviders),
           kSetOnlyIfOverridden},
          {wf::EnableFedCmSelectiveDisclosure,
           raw_ref(features::kFedCmSelectiveDisclosure), kDefault},
          {wf::EnableFencedFrames,
           raw_ref(features::kPrivacySandboxAdsAPIsOverride),
           kSetOnlyIfOverridden},
          {wf::EnableFencedFrames,
           raw_ref(features::kPrivacySandboxAdsAPIsM1Override)},
          {wf::EnableForcedColors, raw_ref(features::kForcedColors)},
          {wf::EnableFractionalScrollOffsets,
           raw_ref(features::kFractionalScrollOffsets)},
          {wf::EnableSensorExtraClasses,
           raw_ref(features::kGenericSensorExtraClasses)},
#if BUILDFLAG(IS_ANDROID)
          {wf::EnableGetDisplayMedia,
           raw_ref(features::kUserMediaScreenCapturing)},
#endif
          {wf::EnableInstalledApp, raw_ref(features::kInstalledApp)},
          {wf::EnableLazyInitializeMediaControls,
           raw_ref(features::kLazyInitializeMediaControls)},
#if BUILDFLAG(IS_CHROMEOS)
          {wf::EnableLockedMode, raw_ref(blink::features::kLockedMode)},
#endif
          {wf::EnableMediaCastOverlayButton,
           raw_ref(media::kMediaCastOverlayButton)},
          {wf::EnableMediaEngagementBypassAutoplayPolicies,
           raw_ref(media::kMediaEngagementBypassAutoplayPolicies)},
          {wf::EnableNotificationContentImage,
           raw_ref(features::kNotificationContentImage), kSetOnlyIfOverridden},
          {wf::EnablePaymentApp, raw_ref(features::kServiceWorkerPaymentApps)},
          {wf::EnablePaymentRequest, raw_ref(features::kWebPayments)},
          {wf::EnablePercentBasedScrolling,
           raw_ref(features::kWindowsScrollingPersonality)},
          {wf::EnablePeriodicBackgroundSync,
           raw_ref(features::kPeriodicBackgroundSync)},
          {wf::EnablePushMessagingSubscriptionChange,
           raw_ref(features::kPushSubscriptionChangeEvent)},
          {wf::EnableRestrictGamepadAccess,
           raw_ref(features::kRestrictGamepadAccess)},
          {wf::EnableSecurePaymentConfirmation,
           raw_ref(features::kSecurePaymentConfirmation)},
          {wf::EnableSecurePaymentConfirmationDebug,
           raw_ref(features::kSecurePaymentConfirmationDebug)},
          {wf::EnableSendBeaconThrowForBlobWithNonSimpleType,
           raw_ref(features::kSendBeaconThrowForBlobWithNonSimpleType)},
          {wf::EnableSharedArrayBuffer, raw_ref(features::kSharedArrayBuffer)},
          {wf::EnableSharedArrayBufferOnDesktop,
           raw_ref(features::kSharedArrayBufferOnDesktop)},
#if BUILDFLAG(IS_ANDROID)
          {wf::EnableSmartZoom, raw_ref(features::kSmartZoom)},
#endif
          {wf::EnableTouchDragAndContextMenu,
           raw_ref(features::kTouchDragAndContextMenu)},
          {wf::EnableUserActivationSameOriginVisibility,
           raw_ref(features::kUserActivationSameOriginVisibility)},
          {wf::EnableWebAuthenticationAmbient,
           raw_ref(device::kWebAuthnAmbientSignin)},
          {wf::EnableWebBluetooth, raw_ref(features::kWebBluetooth),
           kSetOnlyIfOverridden},
          {wf::EnableWebBluetoothGetDevices,
           raw_ref(features::kWebBluetoothNewPermissionsBackend),
           kSetOnlyIfOverridden},
          {wf::EnableWebBluetoothWatchAdvertisements,
           raw_ref(features::kWebBluetoothNewPermissionsBackend),
           kSetOnlyIfOverridden},
#if BUILDFLAG(IS_ANDROID)
          {wf::EnableWebNFC, raw_ref(features::kWebNfc), kSetOnlyIfOverridden},
#endif
          {wf::EnableWebIdentityDigitalCredentials,
           raw_ref(features::kWebIdentityDigitalCredentials),
           kSetOnlyIfOverridden},
          {wf::EnableWebOTP, raw_ref(features::kWebOTP), kSetOnlyIfOverridden},
          {wf::EnableWebOTPAssertionFeaturePolicy,
           raw_ref(features::kWebOTPAssertionFeaturePolicy),
           kSetOnlyIfOverridden},
          {wf::EnableWebUSB, raw_ref(features::kWebUsb)},
          {wf::EnableWebXR, raw_ref(features::kWebXr)},
#if BUILDFLAG(ENABLE_VR)
          {wf::EnableWebXRFrontFacing,
           raw_ref(device::features::kWebXrIncubations)},
          {wf::EnableWebXRFrameRate,
           raw_ref(device::features::kWebXrIncubations)},
          {wf::EnableWebXRHandInput,
           raw_ref(device::features::kWebXrHandInput)},
          {wf::EnableWebXRImageTracking,
           raw_ref(device::features::kWebXrIncubations)},
          {wf::EnableWebXRLayers, raw_ref(device::features::kWebXrLayers)},
          {wf::EnableWebXRPlaneDetection,
           raw_ref(device::features::kWebXrIncubations)},
          {wf::EnableWebXRPoseMotionData,
           raw_ref(device::features::kWebXrIncubations)},
          {wf::EnableWebXRSpecParity,
           raw_ref(device::features::kWebXrIncubations)},
#endif
          {wf::EnableServiceWorkerStaticRouter,
           raw_ref(features::kServiceWorkerStaticRouter)},
          {wf::EnablePermissions, raw_ref(features::kWebPermissionsApi),
           kSetOnlyIfOverridden},
      };
  for (const auto& mapping : blinkFeatureToBaseFeatureMapping) {
    SetRuntimeFeatureFromChromiumFeature(
        *mapping.chromium_feature, mapping.option, mapping.feature_enabler);
  }

  // TODO(crbug.com/40571563): Cleanup the inconsistency between custom WRF
  // enabler function and using feature string name with
  // EnableFeatureFromString.
  const RuntimeFeatureToChromiumFeatureMap<const char*>
      runtimeFeatureNameToChromiumFeatureMapping[] = {
          {"AllowContentInitiatedDataUrlNavigations",
           raw_ref(features::kAllowContentInitiatedDataUrlNavigations)},
          {"AllowURNsInIframes", raw_ref(blink::features::kAllowURNsInIframes)},
          {"AllowURNsInIframes",
           raw_ref(features::kPrivacySandboxAdsAPIsOverride),
           kSetOnlyIfOverridden},
          {"AllowURNsInIframes",
           raw_ref(features::kPrivacySandboxAdsAPIsM1Override)},
          {"AttributionReporting",
           raw_ref(features::kPrivacySandboxAdsAPIsOverride),
           kSetOnlyIfOverridden},
          {"AttributionReporting",
           raw_ref(features::kPrivacySandboxAdsAPIsM1Override)},
          {"AttributionReportingCrossAppWeb",
           raw_ref(features::kPrivacySandboxAdsAPIsOverride),
           kSetOnlyIfOverridden},
          {"AndroidDownloadableFontsMatching",
           raw_ref(features::kAndroidDownloadableFontsMatching)},
#if BUILDFLAG(IS_ANDROID)
          {"CCTNewRFMPushBehavior",
           raw_ref(blink::features::kCCTNewRFMPushBehavior)},
#endif
          {"CompressionDictionaryTransport",
           raw_ref(network::features::kCompressionDictionaryTransport)},
          {"CompressionDictionaryTransportBackend",
           raw_ref(network::features::kCompressionDictionaryTransportBackend)},
          {"CookieDeprecationFacilitatedTesting",
           raw_ref(features::kCookieDeprecationFacilitatedTesting)},
          {"Database", raw_ref(blink::features::kWebSQLAccess),
           kSetOnlyIfOverridden},
          {"DocumentPolicyIncludeJSCallStacksInCrashReports",
           raw_ref(blink::features::
                       kDocumentPolicyIncludeJSCallStacksInCrashReports),
           kSetOnlyIfOverridden},
          {"FencedFramesLocalUnpartitionedDataAccess",
           raw_ref(blink::features::kFencedFramesLocalUnpartitionedDataAccess)},
          {"Fledge", raw_ref(blink::features::kFledge)},
          {"Fledge", raw_ref(features::kPrivacySandboxAdsAPIsOverride),
           kSetOnlyIfOverridden},
          {"Fledge", raw_ref(features::kPrivacySandboxAdsAPIsM1Override),
           kSetOnlyIfOverridden},
          {"FledgeBiddingAndAuctionServerAPI",
           raw_ref(blink::features::kFledgeBiddingAndAuctionServer), kDefault},
          {"FontationsFontBackend",
           raw_ref(blink::features::kFontationsFontBackend)},
          {"FontSrcLocalMatching", raw_ref(features::kFontSrcLocalMatching)},
          {"LegacyWindowsDWriteFontFallback",
           raw_ref(features::kLegacyWindowsDWriteFontFallback)},
          {"MachineLearningNeuralNetwork",
           raw_ref(webnn::mojom::features::kWebMachineLearningNeuralNetwork),
           kSetOnlyIfOverridden},
          {"OriginIsolationHeader", raw_ref(features::kOriginIsolationHeader)},
          {"ReduceAcceptLanguage",
           raw_ref(network::features::kReduceAcceptLanguage)},
          {"SerialPortConnected", raw_ref(features::kSerialPortConnected)},
          {"TopicsAPI", raw_ref(features::kPrivacySandboxAdsAPIsOverride),
           kSetOnlyIfOverridden},
          {"TopicsAPI", raw_ref(features::kPrivacySandboxAdsAPIsM1Override)},
          {"TopicsDocumentAPI",
           raw_ref(features::kPrivacySandboxAdsAPIsOverride),
           kSetOnlyIfOverridden},
          {"TopicsDocumentAPI",
           raw_ref(features::kPrivacySandboxAdsAPIsM1Override)},
          {"TouchTextEditingRedesign",
           raw_ref(features::kTouchTextEditingRedesign)},
          {"TrustedTypesFromLiteral",
           raw_ref(features::kTrustedTypesFromLiteral)},
          {"WebSerialBluetooth",
           raw_ref(features::kEnableBluetoothSerialPortProfileInSerialApi)},
          {"MediaStreamTrackTransfer",
           raw_ref(features::kMediaStreamTrackTransfer)},
          {"PrivateNetworkAccessPermissionPrompt",
           raw_ref(network::features::kPrivateNetworkAccessPermissionPrompt),
           kSetOnlyIfOverridden},
          {"ExperimentalMachineLearningNeuralNetwork",
           raw_ref(webnn::mojom::features::
                       kExperimentalWebMachineLearningNeuralNetwork),
           kSetOnlyIfOverridden}};
  for (const auto& mapping : runtimeFeatureNameToChromiumFeatureMapping) {
    SetRuntimeFeatureFromChromiumFeature(
        *mapping.chromium_feature, mapping.option, [&mapping](bool enabled) {
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
      {wrf::EnableNetInfoDownlinkMax,
       switches::kEnableNetworkInformationDownlinkMax, true},
      {wrf::EnableNotifications, switches::kDisableNotifications, false},
      {wrf::EnablePreciseMemoryInfo, switches::kEnablePreciseMemoryInfo, true},
      // Chrome's Push Messaging implementation relies on Web Notifications.
      {wrf::EnablePushMessaging, switches::kDisableNotifications, false},
      {wrf::EnableScriptedSpeechRecognition, switches::kDisableSpeechAPI,
       false},
      {wrf::EnableScriptedSpeechSynthesis, switches::kDisableSpeechAPI, false},
      {wrf::EnableScriptedSpeechSynthesis, switches::kDisableSpeechSynthesisAPI,
       false},
      {wrf::EnableSharedWorker, switches::kDisableSharedWorkers, false},
      {wrf::EnableMutationEvents, blink::switches::kMutationEventsEnabled,
       true},
      {wrf::EnableKeyboardFocusableScrollers,
       blink::switches::kKeyboardFocusableScrollersEnabled, true},
      {wrf::EnableKeyboardFocusableScrollers,
       blink::switches::kKeyboardFocusableScrollersOptOut, false},
      {wrf::EnableStandardizedBrowserZoom,
       blink::switches::kDisableStandardizedBrowserZoom, false},
      {wrf::EnableCSSCustomStateDeprecatedSyntax,
       blink::switches::kCSSCustomStateDeprecatedSyntaxEnabled, true},
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
      {wrf::EnableWebGPUExperimentalFeatures, switches::kEnableUnsafeWebGPU,
       true},
  };

  for (const auto& mapping : switchToFeatureMapping) {
    if (command_line.HasSwitch(mapping.switch_name)) {
      mapping.feature_enabler(mapping.target_enabled_state);
    }
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

  // Enable or disable BeforeunloadEventCancelByPreventDefault for Enterprise
  // Policy. This overrides any existing settings via base::Feature.
  if (command_line.HasSwitch(
          blink::switches::kBeforeunloadEventCancelByPreventDefaultPolicy)) {
    const std::string value = command_line.GetSwitchValueASCII(
        blink::switches::kBeforeunloadEventCancelByPreventDefaultPolicy);
    if (value ==
        blink::switches::
            kBeforeunloadEventCancelByPreventDefaultPolicy_ForceEnable) {
      WebRuntimeFeatures::EnableBeforeunloadEventCancelByPreventDefault(true);
    }
    if (value ==
        blink::switches::
            kBeforeunloadEventCancelByPreventDefaultPolicy_ForceDisable) {
      WebRuntimeFeatures::EnableBeforeunloadEventCancelByPreventDefault(false);
    }
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

  // These checks are custom wrappers around base::FeatureList::IsEnabled
  // They're moved here to distinguish them from actual base checks
  WebRuntimeFeatures::EnableOverlayScrollbars(ui::IsOverlayScrollbarEnabled());
  WebRuntimeFeatures::EnableFluentScrollbars(ui::IsFluentScrollbarEnabled());
  WebRuntimeFeatures::EnableFluentOverlayScrollbars(
      ui::IsFluentOverlayScrollbarEnabled());

  // TODO(rodneyding): This is a rare case for a stable feature
  // Need to investigate more to determine whether to refactor it.
  if (command_line.HasSwitch(switches::kDisableV8IdleTasks)) {
    WebRuntimeFeatures::EnableV8IdleTasks(false);
  } else {
    WebRuntimeFeatures::EnableV8IdleTasks(true);
  }

  WebRuntimeFeatures::EnableBackForwardCache(
      content::IsBackForwardCacheEnabled());

  if (base::FeatureList::IsEnabled(network::features::kPrivateStateTokens)) {
    WebRuntimeFeatures::EnablePrivateStateTokens(true);
    WebRuntimeFeatures::EnablePrivateStateTokensAlwaysAllowIssuance(true);
  } else if (base::FeatureList::IsEnabled(network::features::kFledgePst)) {
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
}

// Ensures that the various ways of enabling/disabling features do not produce
// an invalid configuration.
void ResolveInvalidConfigurations() {
  // Fenced frames cannot be enabled without the support of the
  // browser process.
  if ((base::FeatureList::IsEnabled(features::kPrivacySandboxAdsAPIsOverride) ||
       base::FeatureList::IsEnabled(
           features::kPrivacySandboxAdsAPIsM1Override)) &&
      !base::FeatureList::IsEnabled(blink::features::kFencedFrames)) {
    LOG_IF(WARNING, WebRuntimeFeatures::IsFencedFramesEnabled())
        << "Fenced frames cannot be enabled in this configuration. Use --"
        << switches::kEnableFeatures << "="
        << blink::features::kFencedFrames.name << " instead.";
    WebRuntimeFeatures::EnableFencedFrames(false);
  }

  if (!base::FeatureList::IsEnabled(blink::features::kFencedFrames) &&
      base::FeatureList::IsEnabled(
          blink::features::kFencedFramesLocalUnpartitionedDataAccess)) {
    LOG_IF(
        WARNING,
        WebRuntimeFeatures::IsFencedFramesLocalUnpartitionedDataAccessEnabled())
        << "Fenced frames must be enabled in order to enable local "
           "unpartitioned "
        << "data access. Use --" << switches::kEnableFeatures << "="
        << blink::features::kFencedFrames.name << " in addition.";
    WebRuntimeFeatures::EnableFeatureFromString(
        "FencedFramesLocalUnpartitionedDataAccess", false);
  }

  // Topics API cannot be enabled without the support of the browser process.
  // The Document API should be additionally gated by the
  // `kBrowsingTopicsDocumentAPI` feature.
  if (!base::FeatureList::IsEnabled(blink::features::kBrowsingTopics)) {
    LOG_IF(WARNING, WebRuntimeFeatures::IsTopicsAPIEnabled())
        << "Topics cannot be enabled in this configuration. Use --"
        << switches::kEnableFeatures << "="
        << blink::features::kBrowsingTopics.name << " in addition.";
    WebRuntimeFeatures::EnableTopicsAPI(false);
    WebRuntimeFeatures::EnableTopicsDocumentAPI(false);
  } else {
    if (!base::FeatureList::IsEnabled(
            blink::features::kBrowsingTopicsDocumentAPI)) {
      LOG_IF(WARNING, WebRuntimeFeatures::IsTopicsDocumentAPIEnabled())
          << "Topics Document API cannot be enabled in this configuration. Use "
             "--"
          << switches::kEnableFeatures << "="
          << blink::features::kBrowsingTopicsDocumentAPI.name
          << " in addition.";
      WebRuntimeFeatures::EnableTopicsDocumentAPI(false);
    }
  }

  if (!base::FeatureList::IsEnabled(blink::features::kSharedStorageAPI)) {
    LOG_IF(WARNING, WebRuntimeFeatures::IsSharedStorageAPIEnabled())
        << "SharedStorage cannot be enabled in this "
           "configuration. Use --"
        << switches::kEnableFeatures << "="
        << blink::features::kSharedStorageAPI.name << " in addition.";
    WebRuntimeFeatures::EnableSharedStorageAPI(false);
  }

  if (!base::FeatureList::IsEnabled(blink::features::kSharedStorageAPIM118) ||
      !base::FeatureList::IsEnabled(blink::features::kSharedStorageAPI)) {
    LOG_IF(WARNING, WebRuntimeFeatures::IsSharedStorageAPIM118Enabled())
        << "SharedStorage for M118+ cannot be enabled in this "
           "configuration. Use --"
        << switches::kEnableFeatures << "="
        << blink::features::kSharedStorageAPI.name << ","
        << blink::features::kSharedStorageAPIM118.name << " in addition.";
    WebRuntimeFeatures::EnableSharedStorageAPIM118(false);
  }

  if (!base::FeatureList::IsEnabled(blink::features::kSharedStorageAPIM125) ||
      !base::FeatureList::IsEnabled(blink::features::kSharedStorageAPI)) {
    LOG_IF(WARNING, WebRuntimeFeatures::IsSharedStorageAPIM125Enabled())
        << "SharedStorage for M125+ cannot be enabled in this "
           "configuration. Use --"
        << switches::kEnableFeatures << "="
        << blink::features::kSharedStorageAPI.name << ","
        << blink::features::kSharedStorageAPIM125.name << " in addition.";
    WebRuntimeFeatures::EnableSharedStorageAPIM125(false);
  }

  if (!base::FeatureList::IsEnabled(
          attribution_reporting::features::kConversionMeasurement)) {
    LOG_IF(WARNING, WebRuntimeFeatures::IsAttributionReportingEnabled())
        << "AttributionReporting cannot be enabled in this "
           "configuration. Use --"
        << switches::kEnableFeatures << "="
        << attribution_reporting::features::kConversionMeasurement.name
        << " in addition.";
    WebRuntimeFeatures::EnableAttributionReporting(false);
  }

  if (!base::FeatureList::IsEnabled(
          attribution_reporting::features::kConversionMeasurement) ||
      !base::FeatureList::IsEnabled(
          network::features::kAttributionReportingCrossAppWeb)) {
    LOG_IF(WARNING,
           WebRuntimeFeatures::IsAttributionReportingCrossAppWebEnabled())
        << "AttributionReportingCrossAppWeb cannot be enabled in this "
           "configuration. Use --"
        << switches::kEnableFeatures << "="
        << attribution_reporting::features::kConversionMeasurement.name << ","
        << network::features::kAttributionReportingCrossAppWeb.name
        << " in addition.";
    WebRuntimeFeatures::EnableAttributionReportingCrossAppWeb(false);
  }

  if (!base::FeatureList::IsEnabled(blink::features::kInterestGroupStorage)) {
    LOG_IF(WARNING, WebRuntimeFeatures::IsAdInterestGroupAPIEnabled())
        << "AdInterestGroupAPI cannot be enabled in this "
           "configuration. Use --"
        << switches::kEnableFeatures << "="
        << blink::features::kInterestGroupStorage.name << " in addition.";
    WebRuntimeFeatures::EnableAdInterestGroupAPI(false);
    WebRuntimeFeatures::EnableFledge(false);
  }

  if (base::FeatureList::IsEnabled(
          features::kCookieDeprecationFacilitatedTesting)) {
    WebRuntimeFeatures::EnableFledgeMultiBid(false);
    WebRuntimeFeatures::EnableFledgeRealTimeReporting(false);

    if (!base::FeatureList::IsEnabled(
            blink::features::
                kAlwaysAllowFledgeDeprecatedRenderURLReplacements)) {
      WebRuntimeFeatures::EnableFledgeDeprecatedRenderURLReplacements(false);
    }
  }

  // PermissionElement cannot be enabled without the support of the
  // browser process.
  if (!base::FeatureList::IsEnabled(blink::features::kPermissionElement)) {
    LOG_IF(WARNING, WebRuntimeFeatures::IsPermissionElementEnabled())
        << "PermissionElement cannot be enabled in this configuration. Use --"
        << switches::kEnableFeatures << "="
        << blink::features::kPermissionElement.name << " instead.";
    WebRuntimeFeatures::EnablePermissionElement(false);
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

  if (enable_experimental_web_platform_features) {
    WebRuntimeFeatures::EnableExperimentalFeatures(true);
  }

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
