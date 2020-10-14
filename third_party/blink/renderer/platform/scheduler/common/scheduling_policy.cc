// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"

namespace blink {

bool SchedulingPolicy::IsFeatureSticky(SchedulingPolicy::Feature feature) {
  switch (feature) {
    case Feature::kWebSocket:
    case Feature::kWebRTC:
    case Feature::kDedicatedWorkerOrWorklet:
    case Feature::kOutstandingIndexedDBTransaction:
    case Feature::kOutstandingNetworkRequestDirectSocket:
    case Feature::kOutstandingNetworkRequestFetch:
    case Feature::kOutstandingNetworkRequestOthers:
    case Feature::kOutstandingNetworkRequestXHR:
    case Feature::kBroadcastChannel:
    case Feature::kIndexedDBConnection:
    case Feature::kWebGL:
    case Feature::kWebVR:
    case Feature::kWebXR:
    case Feature::kSharedWorker:
    case Feature::kWebHID:
    case Feature::kWebShare:
    case Feature::kWebDatabase:
    case Feature::kPortal:
    case Feature::kSpeechRecognizer:
    case Feature::kSpeechSynthesis:
      return false;
    case Feature::kMainResourceHasCacheControlNoStore:
    case Feature::kMainResourceHasCacheControlNoCache:
    case Feature::kSubresourceHasCacheControlNoStore:
    case Feature::kSubresourceHasCacheControlNoCache:
    case Feature::kPageShowEventListener:
    case Feature::kPageHideEventListener:
    case Feature::kBeforeUnloadEventListener:
    case Feature::kUnloadEventListener:
    case Feature::kFreezeEventListener:
    case Feature::kResumeEventListener:
    case Feature::kContainsPlugins:
    case Feature::kDocumentLoaded:
    case Feature::kRequestedGeolocationPermission:
    case Feature::kRequestedNotificationsPermission:
    case Feature::kRequestedMIDIPermission:
    case Feature::kRequestedAudioCapturePermission:
    case Feature::kRequestedVideoCapturePermission:
    case Feature::kRequestedBackForwardCacheBlockedSensors:
    case Feature::kRequestedBackgroundWorkPermission:
    case Feature::kWebLocks:
    case Feature::kWakeLock:
    case Feature::kRequestedStorageAccessGrant:
    case Feature::kWebNfc:
    case Feature::kWebFileSystem:
    case Feature::kAppBanner:
    case Feature::kPrinting:
    case Feature::kPictureInPicture:
    case Feature::kIdleManager:
    case Feature::kPaymentManager:
    case Feature::kKeyboardLock:
    case Feature::kWebOTPService:
      return true;
  }
}

}  // namespace blink
