// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"

#include "base/logging.h"

namespace blink {

bool SchedulingPolicy::IsFeatureSticky(SchedulingPolicy::Feature feature) {
  switch (feature) {
    case Feature::kWebSocket:
    case Feature::kWebRTC:
    case Feature::kDedicatedWorkerOrWorklet:
    case Feature::kOutstandingNetworkRequest:
    case Feature::kOutstandingIndexedDBTransaction:
    case Feature::kHasScriptableFramesInMultipleTabs:
    case Feature::kBroadcastChannel:
    case Feature::kIndexedDBConnection:
    case Feature::kWebGL:
    case Feature::kWebVR:
    case Feature::kWebXR:
    case Feature::kSharedWorker:
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
    case Feature::kServiceWorkerControlledPage:
    case Feature::kRequestedGeolocationPermission:
    case Feature::kRequestedNotificationsPermission:
    case Feature::kRequestedMIDIPermission:
    case Feature::kRequestedAudioCapturePermission:
    case Feature::kRequestedVideoCapturePermission:
    case Feature::kRequestedSensorsPermission:
    case Feature::kRequestedBackgroundWorkPermission:
    case Feature::kWebLocks:
      return true;
  }
}

}  // namespace blink
