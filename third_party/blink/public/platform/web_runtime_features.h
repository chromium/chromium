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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RUNTIME_FEATURES_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RUNTIME_FEATURES_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"

#include <string>

namespace blink {
// This class is used to enable runtime features of Blink.
// Stable features are enabled by default.
class WebRuntimeFeatures {
 public:
  // Enable or disable features with status=experimental listed in
  // Source/platform/runtime_enabled_features.json5.
  BLINK_PLATFORM_EXPORT static void EnableExperimentalFeatures(bool);

  // Enable or disable features with status=test listed in
  // Source/platform/runtime_enabled_features.json5.
  BLINK_PLATFORM_EXPORT static void EnableTestOnlyFeatures(bool);

  // Enable or disable features with non-empty origin_trial_feature_name in
  // Source/platform/runtime_enabled_features.json5.
  BLINK_PLATFORM_EXPORT static void EnableOriginTrialControlledFeatures(bool);

  // Enables or disables a feature by its string identifier from
  // Source/platform/runtime_enabled_features.json5.
  // Note: We use std::string instead of WebString because this API can
  // be called before blink::Initalize(). We can't create WebString objects
  // before blink::Initialize().
  BLINK_PLATFORM_EXPORT static void EnableFeatureFromString(
      const std::string& name,
      bool enable);

  BLINK_PLATFORM_EXPORT static bool IsBlinkGenPropertyTreesEnabled();

  BLINK_PLATFORM_EXPORT static bool IsFractionalScrollOffsetsEnabled();

  BLINK_PLATFORM_EXPORT static void EnableCompositedSelectionUpdate(bool);
  BLINK_PLATFORM_EXPORT static bool IsCompositedSelectionUpdateEnabled();

  BLINK_PLATFORM_EXPORT static void EnableCompositorTouchAction(bool);

  BLINK_PLATFORM_EXPORT static void EnableOriginTrials(bool);
  BLINK_PLATFORM_EXPORT static bool IsOriginTrialsEnabled();

  BLINK_PLATFORM_EXPORT static bool IsSlimmingPaintV2Enabled();

  BLINK_PLATFORM_EXPORT static void EnableAccelerated2dCanvas(bool);
  BLINK_PLATFORM_EXPORT static void EnableAccessibilityObjectModel(bool);
  BLINK_PLATFORM_EXPORT static void EnableAdTagging(bool);
  BLINK_PLATFORM_EXPORT static void EnableAllowActivationDelegationAttr(bool);
  BLINK_PLATFORM_EXPORT static void EnableAudioOutputDevices(bool);
  BLINK_PLATFORM_EXPORT static void EnableBackgroundFetch(bool);
  BLINK_PLATFORM_EXPORT static void EnableBackgroundFetchUploads(bool);
  BLINK_PLATFORM_EXPORT static void EnableBlinkHeapIncrementalMarking(bool);
  BLINK_PLATFORM_EXPORT static void EnableBlinkHeapUnifiedGarbageCollection(
      bool);
  BLINK_PLATFORM_EXPORT static void EnableBloatedRendererDetection(bool);
  BLINK_PLATFORM_EXPORT static void EnableCacheInlineScriptCode(bool);
  BLINK_PLATFORM_EXPORT static void EnableIsolatedCodeCache(bool);
  BLINK_PLATFORM_EXPORT static void EnableCanvas2dImageChromium(bool);
  BLINK_PLATFORM_EXPORT static void EnableCSSHexAlphaColor(bool);
  BLINK_PLATFORM_EXPORT static void EnableCSSFragmentIdentifiers(bool);
  BLINK_PLATFORM_EXPORT static void EnableScrollTopLeftInterop(bool);
  BLINK_PLATFORM_EXPORT static void EnableDatabase(bool);
  BLINK_PLATFORM_EXPORT static void EnableDecodeToYUV(bool);
  BLINK_PLATFORM_EXPORT static void EnableDisplayCutoutAPI(bool);
  BLINK_PLATFORM_EXPORT static void EnableFastMobileScrolling(bool);
  BLINK_PLATFORM_EXPORT static void EnableFileSystem(bool);
  BLINK_PLATFORM_EXPORT static void EnableForceTallerSelectPopup(bool);
  BLINK_PLATFORM_EXPORT static void EnableGamepadExtensions(bool);
  BLINK_PLATFORM_EXPORT static void EnableGamepadVibration(bool);
  BLINK_PLATFORM_EXPORT static void EnableGenericSensor(bool);
  BLINK_PLATFORM_EXPORT static void EnableGenericSensorExtraClasses(bool);
  BLINK_PLATFORM_EXPORT static void EnableHeapCompaction(bool);
  BLINK_PLATFORM_EXPORT static void EnableInputMultipleFieldsUI(bool);
  BLINK_PLATFORM_EXPORT static void EnableJankTracking(bool);
  BLINK_PLATFORM_EXPORT static void EnableLayeredAPI(bool);
  BLINK_PLATFORM_EXPORT static void EnableLayoutNG(bool);
  BLINK_PLATFORM_EXPORT static void EnableLazyFrameLoading(bool);
  BLINK_PLATFORM_EXPORT static void EnableLazyFrameVisibleLoadTimeMetrics(bool);
  BLINK_PLATFORM_EXPORT static void EnableLazyImageLoading(bool);
  BLINK_PLATFORM_EXPORT static void EnableLazyImageVisibleLoadTimeMetrics(bool);
  BLINK_PLATFORM_EXPORT static void EnableMediaCapture(bool);
  BLINK_PLATFORM_EXPORT static void EnableMediaSession(bool);
  BLINK_PLATFORM_EXPORT static void EnableMiddleClickAutoscroll(bool);
  BLINK_PLATFORM_EXPORT static void EnableModernMediaControls(bool);
  BLINK_PLATFORM_EXPORT static void EnableNavigatorContentUtils(bool);
  BLINK_PLATFORM_EXPORT static void EnableNestedWorkers(bool);
  BLINK_PLATFORM_EXPORT static void EnableNetInfoDownlinkMax(bool);
  BLINK_PLATFORM_EXPORT static void EnableNetworkService(bool);
  BLINK_PLATFORM_EXPORT static void EnableNoHoverAfterLayoutChange(bool);
  BLINK_PLATFORM_EXPORT static void EnableNoHoverDuringScroll(bool);
  BLINK_PLATFORM_EXPORT static void EnableNotificationConstructor(bool);
  BLINK_PLATFORM_EXPORT static void EnableNotificationContentImage(bool);
  BLINK_PLATFORM_EXPORT static void EnableNotifications(bool);
  BLINK_PLATFORM_EXPORT static void EnableOnDeviceChange(bool);
  BLINK_PLATFORM_EXPORT static void EnableOrientationEvent(bool);
  BLINK_PLATFORM_EXPORT static void EnableOverflowIconsForMediaControls(bool);
  BLINK_PLATFORM_EXPORT static void EnableOverlayScrollbars(bool);
  BLINK_PLATFORM_EXPORT static void EnableOutOfBlinkCORS(bool);
  BLINK_PLATFORM_EXPORT static void EnablePageLifecycle(bool);
  BLINK_PLATFORM_EXPORT static void EnablePagePopup(bool);
  BLINK_PLATFORM_EXPORT static void EnablePassiveDocumentEventListeners(bool);
  BLINK_PLATFORM_EXPORT static void EnablePassiveDocumentWheelEventListeners(
      bool);
  BLINK_PLATFORM_EXPORT static void EnablePaymentApp(bool);
  BLINK_PLATFORM_EXPORT static void EnablePaymentRequest(bool);
  BLINK_PLATFORM_EXPORT static void EnablePermissionsAPI(bool);
  BLINK_PLATFORM_EXPORT static void EnablePictureInPicture(bool);
  BLINK_PLATFORM_EXPORT static void EnablePictureInPictureAPI(bool);
  BLINK_PLATFORM_EXPORT static void EnablePortals(bool);
  BLINK_PLATFORM_EXPORT static void EnablePreciseMemoryInfo(bool);
  BLINK_PLATFORM_EXPORT static void EnablePreloadDefaultIsMetadata(bool);
  BLINK_PLATFORM_EXPORT static void EnablePreloadImageSrcSetEnabled(bool);
  BLINK_PLATFORM_EXPORT static void EnablePrintBrowser(bool);
  BLINK_PLATFORM_EXPORT static void EnablePresentationAPI(bool);
  BLINK_PLATFORM_EXPORT static void EnablePushMessaging(bool);
  BLINK_PLATFORM_EXPORT static void EnableRasterInducingScroll(bool);
  BLINK_PLATFORM_EXPORT static void EnableReducedReferrerGranularity(bool);
  BLINK_PLATFORM_EXPORT static void EnableRemotePlaybackAPI(bool);
  BLINK_PLATFORM_EXPORT static void EnableRenderingPipelineThrottling(bool);
  BLINK_PLATFORM_EXPORT static void EnableRequireCSSExtensionForFile(bool);
  BLINK_PLATFORM_EXPORT static void EnableResourceLoadScheduler(bool);
  BLINK_PLATFORM_EXPORT static void EnableScriptedSpeech(bool);
  BLINK_PLATFORM_EXPORT static void EnableScrollAnchorSerialization(bool);
  BLINK_PLATFORM_EXPORT static void EnableSecMetadata(bool);
  BLINK_PLATFORM_EXPORT static void EnableSharedArrayBuffer(bool);
  BLINK_PLATFORM_EXPORT static void EnableSharedWorker(bool);
  BLINK_PLATFORM_EXPORT static void EnableSignedHTTPExchange(bool);
  BLINK_PLATFORM_EXPORT static void EnableTouchEventFeatureDetection(bool);
  BLINK_PLATFORM_EXPORT static void EnableUserActivationV2(bool);
  BLINK_PLATFORM_EXPORT static void EnableV8IdleTasks(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebAuth(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebAuthGetTransports(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebBluetooth(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebGL2ComputeContext(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebGLDraftExtensions(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebGLImageChromium(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebGPU(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebNfc(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebShare(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebUsb(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebVR(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebXR(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebXRHitTest(bool);
  BLINK_PLATFORM_EXPORT static void EnableWebXRGamepadSupport(bool);
  BLINK_PLATFORM_EXPORT static void EnableXSLT(bool);
  BLINK_PLATFORM_EXPORT static void ForceOverlayFullscreenVideo(bool);
  BLINK_PLATFORM_EXPORT static void EnableTimerThrottlingForBackgroundTabs(
      bool);
  BLINK_PLATFORM_EXPORT static void EnableTimerThrottlingForHiddenFrames(bool);
  BLINK_PLATFORM_EXPORT static void EnableExpensiveBackgroundTimerThrottling(
      bool);
  BLINK_PLATFORM_EXPORT static void EnableCanvas2dDynamicRenderingModeSwitching(
      bool);
  BLINK_PLATFORM_EXPORT static void
  EnableSendBeaconThrowForBlobWithNonSimpleType(bool);
  BLINK_PLATFORM_EXPORT static void EnableBackgroundVideoTrackOptimization(
      bool);
  BLINK_PLATFORM_EXPORT static void EnableNewRemotePlaybackPipeline(bool);
  BLINK_PLATFORM_EXPORT static void EnableVideoFullscreenOrientationLock(bool);
  BLINK_PLATFORM_EXPORT static void EnableVideoRotateToFullscreen(bool);
  BLINK_PLATFORM_EXPORT static void EnableVideoFullscreenDetection(bool);
  BLINK_PLATFORM_EXPORT static void EnableMediaControlsOverlayPlayButton(bool);
  BLINK_PLATFORM_EXPORT static void EnableRemotePlaybackBackend(bool);
  BLINK_PLATFORM_EXPORT static void EnableMediaCastOverlayButton(bool);
  BLINK_PLATFORM_EXPORT static void EnableClientPlaceholdersForServerLoFi(bool);
  BLINK_PLATFORM_EXPORT static void EnableLazyInitializeMediaControls(bool);
  BLINK_PLATFORM_EXPORT static void EnableMediaEngagementBypassAutoplayPolicies(
      bool);
  BLINK_PLATFORM_EXPORT static void EnableV8ContextSnapshot(bool);
  BLINK_PLATFORM_EXPORT static void EnableAutomationControlled(bool);
  BLINK_PLATFORM_EXPORT static void EnableWorkStealingInScriptRunner(bool);
  BLINK_PLATFORM_EXPORT static void EnableScheduledScriptStreaming(bool);
  BLINK_PLATFORM_EXPORT static void EnableExperimentalProductivityFeatures(
      bool);
  BLINK_PLATFORM_EXPORT static void EnableAutoplayIgnoresWebAudio(bool);
  BLINK_PLATFORM_EXPORT static void EnableMediaControlsExpandGesture(bool);
  BLINK_PLATFORM_EXPORT static void EnableHrefTranslate(bool);

 private:
  WebRuntimeFeatures();
};

}  // namespace blink

#endif
