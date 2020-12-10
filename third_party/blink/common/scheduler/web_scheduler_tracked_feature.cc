// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace blink {
namespace scheduler {

const char* FeatureToString(WebSchedulerTrackedFeature feature) {
  switch (feature) {
    case WebSchedulerTrackedFeature::kWebSocket:
      return "WebSocket";
    case WebSchedulerTrackedFeature::kWebRTC:
      return "WebRTC";
    case WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoCache:
      return "main resource has Cache-Control: No-Cache";
    case WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore:
      return "main resource has Cache-Control: No-Store";
    case WebSchedulerTrackedFeature::kSubresourceHasCacheControlNoCache:
      return "subresource has Cache-Control: No-Cache";
    case WebSchedulerTrackedFeature::kSubresourceHasCacheControlNoStore:
      return "subresource has Cache-Control: No-Store";
    case WebSchedulerTrackedFeature::kPageShowEventListener:
      return "onpageshow() event listener";
    case WebSchedulerTrackedFeature::kPageHideEventListener:
      return "onpagehide() event listener";
    case WebSchedulerTrackedFeature::kBeforeUnloadEventListener:
      return "onbeforeunload() event listener";
    case WebSchedulerTrackedFeature::kUnloadEventListener:
      return "onunload() event listener";
    case WebSchedulerTrackedFeature::kFreezeEventListener:
      return "onfreeze() event listener";
    case WebSchedulerTrackedFeature::kResumeEventListener:
      return "onresume() event listener";
    case WebSchedulerTrackedFeature::kContainsPlugins:
      return "page contains plugins";
    case WebSchedulerTrackedFeature::kDocumentLoaded:
      return "document loaded";
    case WebSchedulerTrackedFeature::kDedicatedWorkerOrWorklet:
      return "Dedicated worker or worklet present";
    case WebSchedulerTrackedFeature::kSharedWorker:
      return "Shared worker present";
    case WebSchedulerTrackedFeature::kOutstandingNetworkRequestFetch:
      return "outstanding network request (fetch)";
    case WebSchedulerTrackedFeature::kOutstandingNetworkRequestXHR:
      return "outstanding network request (XHR)";
    case WebSchedulerTrackedFeature::kOutstandingNetworkRequestOthers:
      return "outstanding network request (others)";
    case WebSchedulerTrackedFeature::kOutstandingIndexedDBTransaction:
      return "outstanding IndexedDB transaction";
    case WebSchedulerTrackedFeature::kRequestedGeolocationPermission:
      return "requested geolocation permission";
    case WebSchedulerTrackedFeature::kRequestedNotificationsPermission:
      return "requested notifications permission";
    case WebSchedulerTrackedFeature::kRequestedMIDIPermission:
      return "requested midi permission";
    case WebSchedulerTrackedFeature::kRequestedAudioCapturePermission:
      return "requested audio capture permission";
    case WebSchedulerTrackedFeature::kRequestedVideoCapturePermission:
      return "requested video capture permission";
    case WebSchedulerTrackedFeature::kRequestedBackForwardCacheBlockedSensors:
      return "requested sensors permission";
    case WebSchedulerTrackedFeature::kRequestedBackgroundWorkPermission:
      return "requested background work permission";
    case WebSchedulerTrackedFeature::kBroadcastChannel:
      return "requested broadcast channel permission";
    case WebSchedulerTrackedFeature::kIndexedDBConnection:
      return "IndexedDB connection present";
    case WebSchedulerTrackedFeature::kWebVR:
      return "WebVR";
    case WebSchedulerTrackedFeature::kWebXR:
      return "WebXR";
    case WebSchedulerTrackedFeature::kWebLocks:
      return "WebLocks";
    case WebSchedulerTrackedFeature::kWebHID:
      return "WebHID";
    case WebSchedulerTrackedFeature::kWakeLock:
      return "WakeLock";
    case WebSchedulerTrackedFeature::kWebShare:
      return "WebShare";
    case WebSchedulerTrackedFeature::kRequestedStorageAccessGrant:
      return "requested storage access permission";
    case WebSchedulerTrackedFeature::kWebNfc:
      return "WebNfc";
    case WebSchedulerTrackedFeature::kWebFileSystem:
      return "WebFileSystem";
    case WebSchedulerTrackedFeature::kAppBanner:
      return "AppBanner";
    case WebSchedulerTrackedFeature::kPrinting:
      return "Printing";
    case WebSchedulerTrackedFeature::kWebDatabase:
      return "WebDatabase";
    case WebSchedulerTrackedFeature::kPictureInPicture:
      return "PictureInPicture";
    case WebSchedulerTrackedFeature::kPortal:
      return "Portal";
    case WebSchedulerTrackedFeature::kSpeechRecognizer:
      return "SpeechRecognizer";
    case WebSchedulerTrackedFeature::kIdleManager:
      return "IdleManager";
    case WebSchedulerTrackedFeature::kPaymentManager:
      return "PaymentManager";
    case WebSchedulerTrackedFeature::kSpeechSynthesis:
      return "SpeechSynthesis";
    case WebSchedulerTrackedFeature::kKeyboardLock:
      return "KeyboardLock";
    case WebSchedulerTrackedFeature::kWebOTPService:
      return "SMSService";
    case WebSchedulerTrackedFeature::kOutstandingNetworkRequestDirectSocket:
      return "outstanding network request (direct socket)";
  }
}

bool IsFeatureSticky(WebSchedulerTrackedFeature feature) {
  return (FeatureToBit(feature) & StickyFeaturesBitmask()) > 0;
}

uint64_t StickyFeaturesBitmask() {
  return FeatureToBit(
             WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore) |
         FeatureToBit(
             WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoCache) |
         FeatureToBit(
             WebSchedulerTrackedFeature::kSubresourceHasCacheControlNoStore) |
         FeatureToBit(
             WebSchedulerTrackedFeature::kSubresourceHasCacheControlNoCache) |
         FeatureToBit(WebSchedulerTrackedFeature::kPageShowEventListener) |
         FeatureToBit(WebSchedulerTrackedFeature::kPageHideEventListener) |
         FeatureToBit(WebSchedulerTrackedFeature::kBeforeUnloadEventListener) |
         FeatureToBit(WebSchedulerTrackedFeature::kUnloadEventListener) |
         FeatureToBit(WebSchedulerTrackedFeature::kFreezeEventListener) |
         FeatureToBit(WebSchedulerTrackedFeature::kResumeEventListener) |
         FeatureToBit(WebSchedulerTrackedFeature::kContainsPlugins) |
         FeatureToBit(WebSchedulerTrackedFeature::kDocumentLoaded) |
         FeatureToBit(
             WebSchedulerTrackedFeature::kRequestedGeolocationPermission) |
         FeatureToBit(
             WebSchedulerTrackedFeature::kRequestedNotificationsPermission) |
         FeatureToBit(WebSchedulerTrackedFeature::kRequestedMIDIPermission) |
         FeatureToBit(
             WebSchedulerTrackedFeature::kRequestedAudioCapturePermission) |
         FeatureToBit(
             WebSchedulerTrackedFeature::kRequestedVideoCapturePermission) |
         FeatureToBit(WebSchedulerTrackedFeature::
                          kRequestedBackForwardCacheBlockedSensors) |
         FeatureToBit(
             WebSchedulerTrackedFeature::kRequestedBackgroundWorkPermission) |
         FeatureToBit(WebSchedulerTrackedFeature::kWebLocks) |
         FeatureToBit(
             WebSchedulerTrackedFeature::kRequestedStorageAccessGrant) |
         FeatureToBit(WebSchedulerTrackedFeature::kWebNfc) |
         FeatureToBit(WebSchedulerTrackedFeature::kWebFileSystem) |
         FeatureToBit(WebSchedulerTrackedFeature::kAppBanner) |
         FeatureToBit(WebSchedulerTrackedFeature::kPrinting) |
         FeatureToBit(WebSchedulerTrackedFeature::kPictureInPicture) |
         FeatureToBit(WebSchedulerTrackedFeature::kIdleManager) |
         FeatureToBit(WebSchedulerTrackedFeature::kPaymentManager) |
         FeatureToBit(WebSchedulerTrackedFeature::kKeyboardLock) |
         FeatureToBit(WebSchedulerTrackedFeature::kWebOTPService);
}

}  // namespace scheduler
}  // namespace blink
