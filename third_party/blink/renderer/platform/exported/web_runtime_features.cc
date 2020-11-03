/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/platform/web_runtime_features.h"

#include "third_party/blink/renderer/platform/graphics/scrollbar_theme_settings.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

void WebRuntimeFeatures::EnableBlockingFocusWithoutUserActivation(bool enable) {
  RuntimeEnabledFeatures::SetBlockingFocusWithoutUserActivationEnabled(enable);
}

void WebRuntimeFeatures::EnableBrowserVerifiedUserActivationKeyboard(
    bool enable) {
  RuntimeEnabledFeatures::SetBrowserVerifiedUserActivationKeyboardEnabled(
      enable);
}

void WebRuntimeFeatures::EnableBrowserVerifiedUserActivationMouse(bool enable) {
  RuntimeEnabledFeatures::SetBrowserVerifiedUserActivationMouseEnabled(enable);
}

void WebRuntimeFeatures::EnableClickPointerEvent(bool enable) {
  RuntimeEnabledFeatures::SetClickPointerEventEnabled(enable);
}

void WebRuntimeFeatures::EnableExperimentalFeatures(bool enable) {
  RuntimeEnabledFeatures::SetExperimentalFeaturesEnabled(enable);
}

void WebRuntimeFeatures::EnableWebBluetooth(bool enable) {
  RuntimeEnabledFeatures::SetWebBluetoothEnabled(enable);
}

void WebRuntimeFeatures::EnableWebBluetoothRemoteCharacteristicNewWriteValue(
    bool enable) {
  RuntimeEnabledFeatures::
      SetWebBluetoothRemoteCharacteristicNewWriteValueEnabled(enable);
}

void WebRuntimeFeatures::EnableWebBluetoothScanning(bool enable) {
  RuntimeEnabledFeatures::SetWebBluetoothScanningEnabled(enable);
}

void WebRuntimeFeatures::EnableWebNfc(bool enable) {
  RuntimeEnabledFeatures::SetWebNFCEnabled(enable);
}

void WebRuntimeFeatures::EnableWebUsb(bool enable) {
  RuntimeEnabledFeatures::SetWebUSBEnabled(enable);
}

void WebRuntimeFeatures::EnableFeatureFromString(const std::string& name,
                                                 bool enable) {
  RuntimeEnabledFeatures::SetFeatureEnabledFromString(name, enable);
}

void WebRuntimeFeatures::EnableForcedColors(bool enable) {
  RuntimeEnabledFeatures::SetForcedColorsEnabled(enable);
}

bool WebRuntimeFeatures::IsForcedColorsEnabled() {
  return RuntimeEnabledFeatures::ForcedColorsEnabled();
}

void WebRuntimeFeatures::EnableFractionalScrollOffsets(bool enable) {
  RuntimeEnabledFeatures::SetFractionalScrollOffsetsEnabled(enable);
}

bool WebRuntimeFeatures::IsFractionalScrollOffsetsEnabled() {
  return RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled();
}

void WebRuntimeFeatures::EnableTestOnlyFeatures(bool enable) {
  RuntimeEnabledFeatures::SetTestFeaturesEnabled(enable);
}

void WebRuntimeFeatures::EnableOriginTrialControlledFeatures(bool enable) {
  RuntimeEnabledFeatures::SetOriginTrialControlledFeaturesEnabled(enable);
}

void WebRuntimeFeatures::EnableAccelerated2dCanvas(bool enable) {
  RuntimeEnabledFeatures::SetAccelerated2dCanvasEnabled(enable);
}

void WebRuntimeFeatures::EnableAccessibilityExposeDisplayNone(bool enable) {
  RuntimeEnabledFeatures::SetAccessibilityExposeDisplayNoneEnabled(enable);
}

void WebRuntimeFeatures::EnableAccessibilityExposeHTMLElement(bool enable) {
  RuntimeEnabledFeatures::SetAccessibilityExposeHTMLElementEnabled(enable);
}

void WebRuntimeFeatures::EnableAccessibilityExposeIgnoredNodes(bool enable) {
  RuntimeEnabledFeatures::SetAccessibilityExposeIgnoredNodesEnabled(enable);
}

void WebRuntimeFeatures::EnableAccessibilityObjectModel(bool enable) {
  RuntimeEnabledFeatures::SetAccessibilityObjectModelEnabled(enable);
}

void WebRuntimeFeatures::EnableAccessibilityUseAXPositionForDocumentMarkers(
    bool enable) {
  RuntimeEnabledFeatures::
      SetAccessibilityUseAXPositionForDocumentMarkersEnabled(enable);
}

void WebRuntimeFeatures::EnableAdTagging(bool enable) {
  RuntimeEnabledFeatures::SetAdTaggingEnabled(enable);
}

void WebRuntimeFeatures::EnableAllowActivationDelegationAttr(bool enable) {
  RuntimeEnabledFeatures::SetAllowActivationDelegationAttrEnabled(enable);
}

void WebRuntimeFeatures::EnableAudioOutputDevices(bool enable) {
  RuntimeEnabledFeatures::SetAudioOutputDevicesEnabled(enable);
}

void WebRuntimeFeatures::EnableAutomaticLazyFrameLoading(bool enable) {
  RuntimeEnabledFeatures::SetAutomaticLazyFrameLoadingEnabled(enable);
}

void WebRuntimeFeatures::EnableAutomaticLazyImageLoading(bool enable) {
  RuntimeEnabledFeatures::SetAutomaticLazyImageLoadingEnabled(enable);
}

void WebRuntimeFeatures::EnableCacheInlineScriptCode(bool enable) {
  RuntimeEnabledFeatures::SetCacheInlineScriptCodeEnabled(enable);
}

void WebRuntimeFeatures::EnableCookieDeprecationMessages(bool enable) {
  RuntimeEnabledFeatures::SetCookieDeprecationMessagesEnabled(enable);
}

void WebRuntimeFeatures::EnableCookiesWithoutSameSiteMustBeSecure(bool enable) {
  RuntimeEnabledFeatures::SetCookiesWithoutSameSiteMustBeSecureEnabled(enable);
}

void WebRuntimeFeatures::EnableCanvas2dImageChromium(bool enable) {
  RuntimeEnabledFeatures::SetCanvas2dImageChromiumEnabled(enable);
}

void WebRuntimeFeatures::EnableCompositedSelectionUpdate(bool enable) {
  RuntimeEnabledFeatures::SetCompositedSelectionUpdateEnabled(enable);
}

bool WebRuntimeFeatures::IsCompositedSelectionUpdateEnabled() {
  return RuntimeEnabledFeatures::CompositedSelectionUpdateEnabled();
}

void WebRuntimeFeatures::EnableCSSHexAlphaColor(bool enable) {
  RuntimeEnabledFeatures::SetCSSHexAlphaColorEnabled(enable);
}

void WebRuntimeFeatures::EnableSameSiteByDefaultCookies(bool enable) {
  RuntimeEnabledFeatures::SetSameSiteByDefaultCookiesEnabled(enable);
}

void WebRuntimeFeatures::EnableScrollTopLeftInterop(bool enable) {
  RuntimeEnabledFeatures::SetScrollTopLeftInteropEnabled(enable);
}

void WebRuntimeFeatures::EnableKeyboardFocusableScrollers(bool enable) {
  RuntimeEnabledFeatures::SetKeyboardFocusableScrollersEnabled(enable);
}

void WebRuntimeFeatures::EnableDatabase(bool enable) {
  RuntimeEnabledFeatures::SetDatabaseEnabled(enable);
}

void WebRuntimeFeatures::EnableDecodeJpeg420ImagesToYUV(bool enable) {
  RuntimeEnabledFeatures::SetDecodeJpeg420ImagesToYUVEnabled(enable);
}

void WebRuntimeFeatures::EnableDecodeLossyWebPImagesToYUV(bool enable) {
  RuntimeEnabledFeatures::SetDecodeLossyWebPImagesToYUVEnabled(enable);
}

void WebRuntimeFeatures::EnableFeaturePolicyForSandbox(bool enable) {
  RuntimeEnabledFeatures::SetFeaturePolicyForSandboxEnabled(enable);
}

void WebRuntimeFeatures::EnableFileSystem(bool enable) {
  RuntimeEnabledFeatures::SetFileSystemEnabled(enable);
}

void WebRuntimeFeatures::EnableForceSynchronousHTMLParsing(bool enable) {
  RuntimeEnabledFeatures::SetForceSynchronousHTMLParsingEnabled(enable);
}

void WebRuntimeFeatures::EnableForceTallerSelectPopup(bool enable) {
  RuntimeEnabledFeatures::SetForceTallerSelectPopupEnabled(enable);
}

void WebRuntimeFeatures::EnableGenericSensorExtraClasses(bool enable) {
  RuntimeEnabledFeatures::SetSensorExtraClassesEnabled(enable);
}

void WebRuntimeFeatures::EnableImplicitRootScroller(bool enable) {
  RuntimeEnabledFeatures::SetImplicitRootScrollerEnabled(enable);
}

void WebRuntimeFeatures::EnableInputMultipleFieldsUI(bool enable) {
  RuntimeEnabledFeatures::SetInputMultipleFieldsUIEnabled(enable);
}

void WebRuntimeFeatures::EnableLayoutNG(bool enable) {
  RuntimeEnabledFeatures::SetLayoutNGEnabled(enable);
}

void WebRuntimeFeatures::EnableLazyFrameLoading(bool enable) {
  RuntimeEnabledFeatures::SetLazyFrameLoadingEnabled(enable);
}

void WebRuntimeFeatures::EnableLazyFrameVisibleLoadTimeMetrics(bool enable) {
  RuntimeEnabledFeatures::SetLazyFrameVisibleLoadTimeMetricsEnabled(enable);
}

void WebRuntimeFeatures::EnableLazyImageLoading(bool enable) {
  RuntimeEnabledFeatures::SetLazyImageLoadingEnabled(enable);
}

void WebRuntimeFeatures::EnableLazyImageVisibleLoadTimeMetrics(bool enable) {
  RuntimeEnabledFeatures::SetLazyImageVisibleLoadTimeMetricsEnabled(enable);
}

void WebRuntimeFeatures::EnableMediaCapture(bool enable) {
  RuntimeEnabledFeatures::SetMediaCaptureEnabled(enable);
}

void WebRuntimeFeatures::EnableMediaFeeds(bool enable) {
  RuntimeEnabledFeatures::SetMediaFeedsEnabled(enable);
}

void WebRuntimeFeatures::EnableMediaSession(bool enable) {
  RuntimeEnabledFeatures::SetMediaSessionEnabled(enable);
}

void WebRuntimeFeatures::EnableNotificationConstructor(bool enable) {
  RuntimeEnabledFeatures::SetNotificationConstructorEnabled(enable);
}

void WebRuntimeFeatures::EnableNotificationContentImage(bool enable) {
  RuntimeEnabledFeatures::SetNotificationContentImageEnabled(enable);
}

void WebRuntimeFeatures::EnableNotifications(bool enable) {
  RuntimeEnabledFeatures::SetNotificationsEnabled(enable);
}

void WebRuntimeFeatures::EnableNavigatorContentUtils(bool enable) {
  RuntimeEnabledFeatures::SetNavigatorContentUtilsEnabled(enable);
}

void WebRuntimeFeatures::EnableNetInfoDownlinkMax(bool enable) {
  RuntimeEnabledFeatures::SetNetInfoDownlinkMaxEnabled(enable);
}

void WebRuntimeFeatures::EnableNeverSlowMode(bool enable) {
  RuntimeEnabledFeatures::SetNeverSlowModeEnabled(enable);
}

void WebRuntimeFeatures::EnableNewCanvas2DAPI(bool enable) {
  RuntimeEnabledFeatures::SetNewCanvas2DAPIEnabled(enable);
}

void WebRuntimeFeatures::EnableOnDeviceChange(bool enable) {
  RuntimeEnabledFeatures::SetOnDeviceChangeEnabled(enable);
}

void WebRuntimeFeatures::EnableOrientationEvent(bool enable) {
  RuntimeEnabledFeatures::SetOrientationEventEnabled(enable);
}

void WebRuntimeFeatures::EnableOverscrollCustomization(bool enable) {
  RuntimeEnabledFeatures::SetOverscrollCustomizationEnabled(enable);
}

void WebRuntimeFeatures::EnablePagePopup(bool enable) {
  RuntimeEnabledFeatures::SetPagePopupEnabled(enable);
}

void WebRuntimeFeatures::EnableMiddleClickAutoscroll(bool enable) {
  RuntimeEnabledFeatures::SetMiddleClickAutoscrollEnabled(enable);
}

void WebRuntimeFeatures::EnablePaymentApp(bool enable) {
  RuntimeEnabledFeatures::SetPaymentAppEnabled(enable);
}

void WebRuntimeFeatures::EnablePaymentHandlerMinimalUI(bool enable) {
  RuntimeEnabledFeatures::SetPaymentHandlerMinimalUIEnabled(enable);
}

void WebRuntimeFeatures::EnablePaymentRequest(bool enable) {
  RuntimeEnabledFeatures::SetPaymentRequestEnabled(enable);
  if (!enable) {
    // Disable features that depend on Payment Request API.
    RuntimeEnabledFeatures::SetPaymentAppEnabled(false);
    RuntimeEnabledFeatures::SetPaymentHandlerMinimalUIEnabled(false);
    RuntimeEnabledFeatures::SetPaymentMethodChangeEventEnabled(false);
  }
}

void WebRuntimeFeatures::EnablePercentBasedScrolling(bool enable) {
  RuntimeEnabledFeatures::SetPercentBasedScrollingEnabled(enable);
}

void WebRuntimeFeatures::EnablePerformanceManagerInstrumentation(bool enable) {
  RuntimeEnabledFeatures::SetPerformanceManagerInstrumentationEnabled(enable);
}

void WebRuntimeFeatures::EnablePermissionsAPI(bool enable) {
  RuntimeEnabledFeatures::SetPermissionsEnabled(enable);
}

void WebRuntimeFeatures::EnablePeriodicBackgroundSync(bool enable) {
  RuntimeEnabledFeatures::SetPeriodicBackgroundSyncEnabled(enable);
}

void WebRuntimeFeatures::EnablePictureInPicture(bool enable) {
  RuntimeEnabledFeatures::SetPictureInPictureEnabled(enable);
}

void WebRuntimeFeatures::EnablePictureInPictureAPI(bool enable) {
  RuntimeEnabledFeatures::SetPictureInPictureAPIEnabled(enable);
}

void WebRuntimeFeatures::EnablePointerLockOptions(bool enable) {
  RuntimeEnabledFeatures::SetPointerLockOptionsEnabled(enable);
}

void WebRuntimeFeatures::EnablePortals(bool enable) {
  RuntimeEnabledFeatures::SetPortalsEnabled(enable);
}

void WebRuntimeFeatures::EnableScriptedSpeechRecognition(bool enable) {
  RuntimeEnabledFeatures::SetScriptedSpeechRecognitionEnabled(enable);
}

void WebRuntimeFeatures::EnableScriptedSpeechSynthesis(bool enable) {
  RuntimeEnabledFeatures::SetScriptedSpeechSynthesisEnabled(enable);
}

void WebRuntimeFeatures::EnableUserActivationSameOriginVisibility(bool enable) {
  RuntimeEnabledFeatures::SetUserActivationSameOriginVisibilityEnabled(enable);
}

void WebRuntimeFeatures::EnableTouchEventFeatureDetection(bool enable) {
  RuntimeEnabledFeatures::SetTouchEventFeatureDetectionEnabled(enable);
}

void WebRuntimeFeatures::EnableScrollUnification(bool enable) {
  RuntimeEnabledFeatures::SetScrollUnificationEnabled(enable);
}

void WebRuntimeFeatures::EnableWebGL2ComputeContext(bool enable) {
  RuntimeEnabledFeatures::SetWebGL2ComputeContextEnabled(enable);
}

void WebRuntimeFeatures::EnableWebGLDraftExtensions(bool enable) {
  RuntimeEnabledFeatures::SetWebGLDraftExtensionsEnabled(enable);
}

void WebRuntimeFeatures::EnableWebGLImageChromium(bool enable) {
  RuntimeEnabledFeatures::SetWebGLImageChromiumEnabled(enable);
}

void WebRuntimeFeatures::EnableXSLT(bool enable) {
  RuntimeEnabledFeatures::SetXSLTEnabled(enable);
}

void WebRuntimeFeatures::EnableOverlayScrollbars(bool enable) {
  ScrollbarThemeSettings::SetOverlayScrollbarsEnabled(enable);
}

void WebRuntimeFeatures::ForceOverlayFullscreenVideo(bool enable) {
  RuntimeEnabledFeatures::SetForceOverlayFullscreenVideoEnabled(enable);
}

void WebRuntimeFeatures::EnableSharedArrayBuffer(bool enable) {
  RuntimeEnabledFeatures::SetSharedArrayBufferEnabled(enable);
}

void WebRuntimeFeatures::EnableSharedWorker(bool enable) {
  RuntimeEnabledFeatures::SetSharedWorkerEnabled(enable);
}

void WebRuntimeFeatures::EnableTextFragmentAnchor(bool enable) {
  RuntimeEnabledFeatures::SetTextFragmentIdentifiersEnabled(enable);
}

void WebRuntimeFeatures::EnablePreciseMemoryInfo(bool enable) {
  RuntimeEnabledFeatures::SetPreciseMemoryInfoEnabled(enable);
}

void WebRuntimeFeatures::EnableV8IdleTasks(bool enable) {
  RuntimeEnabledFeatures::SetV8IdleTasksEnabled(enable);
}

void WebRuntimeFeatures::EnableDirectSockets(bool enable) {
  RuntimeEnabledFeatures::SetDirectSocketsEnabled(enable);
}

void WebRuntimeFeatures::EnablePushMessaging(bool enable) {
  RuntimeEnabledFeatures::SetPushMessagingEnabled(enable);
}

void WebRuntimeFeatures::EnablePushSubscriptionChangeEvent(bool enable) {
  RuntimeEnabledFeatures::SetPushMessagingSubscriptionChangeEnabled(enable);
}

void WebRuntimeFeatures::EnableWebShare(bool enable) {
  RuntimeEnabledFeatures::SetWebShareEnabled(enable);
}

void WebRuntimeFeatures::EnableWebGPU(bool enable) {
  RuntimeEnabledFeatures::SetWebGPUEnabled(enable);
}

void WebRuntimeFeatures::EnableWebXR(bool enable) {
  RuntimeEnabledFeatures::SetWebXREnabled(enable);
}

void WebRuntimeFeatures::EnableWebXRAnchors(bool enable) {
  RuntimeEnabledFeatures::SetWebXRAnchorsEnabled(enable);
}

void WebRuntimeFeatures::EnableWebXRARModule(bool enable) {
  RuntimeEnabledFeatures::SetWebXRARModuleEnabled(enable);
}

void WebRuntimeFeatures::EnableWebXRCameraAccess(bool enable) {
  RuntimeEnabledFeatures::SetWebXRCameraAccessEnabled(enable);
}

void WebRuntimeFeatures::EnableWebXRDepth(bool enable) {
  RuntimeEnabledFeatures::SetWebXRDepthEnabled(enable);
}

void WebRuntimeFeatures::EnableWebXRHitTest(bool enable) {
  RuntimeEnabledFeatures::SetWebXRHitTestEnabled(enable);
}

void WebRuntimeFeatures::EnableWebXRImageTracking(bool enable) {
  RuntimeEnabledFeatures::SetWebXRImageTrackingEnabled(enable);
}

void WebRuntimeFeatures::EnableWebXRLightEstimation(bool enable) {
  RuntimeEnabledFeatures::SetWebXRLightEstimationEnabled(enable);
}

void WebRuntimeFeatures::EnableWebXRPlaneDetection(bool enable) {
  RuntimeEnabledFeatures::SetWebXRPlaneDetectionEnabled(enable);
}

void WebRuntimeFeatures::EnableWebXRReflectionEstimation(bool enable) {
  RuntimeEnabledFeatures::SetWebXRReflectionEstimationEnabled(enable);
}

void WebRuntimeFeatures::EnableWebXRViewportScale(bool enable) {
  RuntimeEnabledFeatures::SetWebXRViewportScaleEnabled(enable);
}

void WebRuntimeFeatures::EnablePresentationAPI(bool enable) {
  RuntimeEnabledFeatures::SetPresentationEnabled(enable);
}

void WebRuntimeFeatures::EnableRestrictAutomaticLazyFrameLoadingToDataSaver(
    bool enable) {
  RuntimeEnabledFeatures::
      SetRestrictAutomaticLazyFrameLoadingToDataSaverEnabled(enable);
}

void WebRuntimeFeatures::EnableRestrictAutomaticLazyImageLoadingToDataSaver(
    bool enable) {
  RuntimeEnabledFeatures::
      SetRestrictAutomaticLazyImageLoadingToDataSaverEnabled(enable);
}

void WebRuntimeFeatures::EnableSecurePaymentConfirmationDebug(bool enable) {
  RuntimeEnabledFeatures::SetSecurePaymentConfirmationDebugEnabled(enable);
}

void WebRuntimeFeatures::EnableAutoLazyLoadOnReloads(bool enable) {
  RuntimeEnabledFeatures::SetAutoLazyLoadOnReloadsEnabled(enable);
}

void WebRuntimeFeatures::EnableExpensiveBackgroundTimerThrottling(bool enable) {
  RuntimeEnabledFeatures::SetExpensiveBackgroundTimerThrottlingEnabled(enable);
}

void WebRuntimeFeatures::EnableTimerThrottlingForBackgroundTabs(bool enable) {
  RuntimeEnabledFeatures::SetTimerThrottlingForBackgroundTabsEnabled(enable);
}

void WebRuntimeFeatures::EnableTimerThrottlingForHiddenFrames(bool enable) {
  RuntimeEnabledFeatures::SetTimerThrottlingForHiddenFramesEnabled(enable);
}

void WebRuntimeFeatures::EnableSendBeaconThrowForBlobWithNonSimpleType(
    bool enable) {
  RuntimeEnabledFeatures::SetSendBeaconThrowForBlobWithNonSimpleTypeEnabled(
      enable);
}

void WebRuntimeFeatures::EnableBackgroundVideoTrackOptimization(bool enable) {
  RuntimeEnabledFeatures::SetBackgroundVideoTrackOptimizationEnabled(enable);
}

void WebRuntimeFeatures::EnableRemotePlaybackAPI(bool enable) {
  RuntimeEnabledFeatures::SetRemotePlaybackEnabled(enable);
}

void WebRuntimeFeatures::EnableVideoFullscreenOrientationLock(bool enable) {
  RuntimeEnabledFeatures::SetVideoFullscreenOrientationLockEnabled(enable);
}

void WebRuntimeFeatures::EnableVideoRotateToFullscreen(bool enable) {
  RuntimeEnabledFeatures::SetVideoRotateToFullscreenEnabled(enable);
}

void WebRuntimeFeatures::EnableVideoPlaybackQuality(bool enable) {
  RuntimeEnabledFeatures::SetVideoPlaybackQualityEnabled(enable);
}

void WebRuntimeFeatures::EnableMediaControlsOverlayPlayButton(bool enable) {
  RuntimeEnabledFeatures::SetMediaControlsOverlayPlayButtonEnabled(enable);
}

void WebRuntimeFeatures::EnableRemotePlaybackBackend(bool enable) {
  RuntimeEnabledFeatures::SetRemotePlaybackBackendEnabled(enable);
}

void WebRuntimeFeatures::EnableMediaCastOverlayButton(bool enable) {
  RuntimeEnabledFeatures::SetMediaCastOverlayButtonEnabled(enable);
}

void WebRuntimeFeatures::EnableWebAuth(bool enable) {
  RuntimeEnabledFeatures::SetWebAuthEnabled(enable);
}

void WebRuntimeFeatures::EnableWebAuthenticationGetAssertionFeaturePolicy(
    bool enable) {
  RuntimeEnabledFeatures::SetWebAuthenticationGetAssertionFeaturePolicyEnabled(
      enable);
}

void WebRuntimeFeatures::EnableLazyInitializeMediaControls(bool enable) {
  RuntimeEnabledFeatures::SetLazyInitializeMediaControlsEnabled(enable);
}

void WebRuntimeFeatures::EnableMediaEngagementBypassAutoplayPolicies(
    bool enable) {
  RuntimeEnabledFeatures::SetMediaEngagementBypassAutoplayPoliciesEnabled(
      enable);
}

void WebRuntimeFeatures::EnableAutomationControlled(bool enable) {
  RuntimeEnabledFeatures::SetAutomationControlledEnabled(enable);
}

void WebRuntimeFeatures::EnableExperimentalProductivityFeatures(bool enable) {
  RuntimeEnabledFeatures::SetExperimentalProductivityFeaturesEnabled(enable);
}

void WebRuntimeFeatures::EnableDisallowDocumentAccess(bool enable) {
  RuntimeEnabledFeatures::SetDisallowDocumentAccessEnabled(enable);
}

void WebRuntimeFeatures::EnableDisplayCutoutAPI(bool enable) {
  RuntimeEnabledFeatures::SetDisplayCutoutAPIEnabled(enable);
}

void WebRuntimeFeatures::EnableDocumentPolicy(bool enable) {
  RuntimeEnabledFeatures::SetDocumentPolicyEnabled(enable);
}

void WebRuntimeFeatures::EnableDocumentPolicyNegotiation(bool enable) {
  RuntimeEnabledFeatures::SetDocumentPolicyNegotiationEnabled(enable);
}

void WebRuntimeFeatures::EnablePermissionsPolicyHeader(bool enable) {
  RuntimeEnabledFeatures::SetPermissionsPolicyHeaderEnabled(enable);
}

void WebRuntimeFeatures::EnableAutoplayIgnoresWebAudio(bool enable) {
  RuntimeEnabledFeatures::SetAutoplayIgnoresWebAudioEnabled(enable);
}

void WebRuntimeFeatures::EnableMediaControlsExpandGesture(bool enable) {
  RuntimeEnabledFeatures::SetMediaControlsExpandGestureEnabled(enable);
}

void WebRuntimeFeatures::EnableTranslateService(bool enable) {
  RuntimeEnabledFeatures::SetTranslateServiceEnabled(enable);
}

void WebRuntimeFeatures::EnableBackgroundFetch(bool enable) {
  RuntimeEnabledFeatures::SetBackgroundFetchEnabled(enable);
}

void WebRuntimeFeatures::EnableGetDisplayMedia(bool enable) {
  RuntimeEnabledFeatures::SetGetDisplayMediaEnabled(enable);
}

void WebRuntimeFeatures::EnableGetCurrentBrowsingContextMedia(bool enable) {
  RuntimeEnabledFeatures::SetGetCurrentBrowsingContextMediaEnabled(enable);
}

void WebRuntimeFeatures::EnableAllowSyncXHRInPageDismissal(bool enable) {
  RuntimeEnabledFeatures::SetAllowSyncXHRInPageDismissalEnabled(enable);
}

void WebRuntimeFeatures::EnableShadowDOMV0(bool enable) {
  RuntimeEnabledFeatures::SetShadowDOMV0Enabled(enable);
}

void WebRuntimeFeatures::EnableCustomElementsV0(bool enable) {
  RuntimeEnabledFeatures::SetCustomElementsV0Enabled(enable);
}

void WebRuntimeFeatures::EnableHTMLImports(bool enable) {
  RuntimeEnabledFeatures::SetHTMLImportsEnabled(enable);
}

void WebRuntimeFeatures::EnableSignedExchangePrefetchCacheForNavigations(
    bool enable) {
  RuntimeEnabledFeatures::SetSignedExchangePrefetchCacheForNavigationsEnabled(
      enable);
}

void WebRuntimeFeatures::EnableSignedExchangeSubresourcePrefetch(bool enable) {
  RuntimeEnabledFeatures::SetSignedExchangeSubresourcePrefetchEnabled(enable);
}

void WebRuntimeFeatures::EnableSubresourceWebBundles(bool enable) {
  RuntimeEnabledFeatures::SetSubresourceWebBundlesEnabled(enable);
}

void WebRuntimeFeatures::EnableIdleDetection(bool enable) {
  RuntimeEnabledFeatures::SetIdleDetectionEnabled(enable);
}

void WebRuntimeFeatures::EnableSkipTouchEventFilter(bool enable) {
  RuntimeEnabledFeatures::SetSkipTouchEventFilterEnabled(enable);
}

void WebRuntimeFeatures::EnableWebOTP(bool enable) {
  RuntimeEnabledFeatures::SetWebOTPEnabled(enable);
}

void WebRuntimeFeatures::EnableConsolidatedMovementXY(bool enable) {
  RuntimeEnabledFeatures::SetConsolidatedMovementXYEnabled(enable);
}

void WebRuntimeFeatures::EnableCooperativeScheduling(bool enable) {
  RuntimeEnabledFeatures::SetCooperativeSchedulingEnabled(enable);
}

void WebRuntimeFeatures::EnableMouseSubframeNoImplicitCapture(bool enable) {
  RuntimeEnabledFeatures::SetMouseSubframeNoImplicitCaptureEnabled(enable);
}

void WebRuntimeFeatures::EnableBackForwardCache(bool enable) {
  RuntimeEnabledFeatures::SetBackForwardCacheEnabled(enable);
}

void WebRuntimeFeatures::EnableSurfaceEmbeddingFeatures(bool enable) {
  RuntimeEnabledFeatures::SetSurfaceEmbeddingFeaturesEnabled(enable);
}

void WebRuntimeFeatures::EnableAcceleratedSmallCanvases(bool enable) {
  RuntimeEnabledFeatures::SetAcceleratedSmallCanvasesEnabled(enable);
}

void WebRuntimeFeatures::EnableTrustTokens(bool enable) {
  RuntimeEnabledFeatures::SetTrustTokensEnabled(enable);
}

void WebRuntimeFeatures::EnableTrustTokensAlwaysAllowIssuance(bool enable) {
  RuntimeEnabledFeatures::SetTrustTokensAlwaysAllowIssuanceEnabled(enable);
}

void WebRuntimeFeatures::EnableInstalledApp(bool enable) {
  RuntimeEnabledFeatures::SetInstalledAppEnabled(enable);
}

void WebRuntimeFeatures::EnableTransformInterop(bool enable) {
  RuntimeEnabledFeatures::SetTransformInteropEnabled(enable);
}

void WebRuntimeFeatures::EnableVideoWakeLockOptimisationHiddenMuted(
    bool enable) {
  RuntimeEnabledFeatures::SetVideoWakeLockOptimisationHiddenMutedEnabled(
      enable);
}

void WebRuntimeFeatures::EnableContentIndex(bool enable) {
  RuntimeEnabledFeatures::SetContentIndexEnabled(enable);
}

void WebRuntimeFeatures::EnableRestrictGamepadAccess(bool enable) {
  RuntimeEnabledFeatures::SetRestrictGamepadAccessEnabled(enable);
}

void WebRuntimeFeatures::EnableCompositingOptimizations(bool enable) {
  RuntimeEnabledFeatures::SetCompositingOptimizationsEnabled(enable);
}

void WebRuntimeFeatures::EnableConversionMeasurementInfraSupport(bool enable) {
  RuntimeEnabledFeatures::SetConversionMeasurementInfraSupportEnabled(enable);
}

void WebRuntimeFeatures::EnableParseUrlProtocolHandler(bool enable) {
  RuntimeEnabledFeatures::SetParseUrlProtocolHandlerEnabled(enable);
}

}  // namespace blink
