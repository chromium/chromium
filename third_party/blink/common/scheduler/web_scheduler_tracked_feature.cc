// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

#include <atomic>
#include <map>
#include <vector>

namespace blink {
namespace scheduler {

namespace {

std::atomic_bool disable_align_wake_ups{false};

struct FeatureNames {
  std::string short_name;
  std::string human_readable;
};

FeatureNames FeatureToNames(WebSchedulerTrackedFeature feature) {
  switch (feature) {
    case WebSchedulerTrackedFeature::kWebSocket:
      return {"websocket", "WebSocket live connection"};
    case WebSchedulerTrackedFeature::kWebSocketSticky:
      return {"websocket", "WebSocket used"};
    case WebSchedulerTrackedFeature::kWebTransport:
      return {"webtransport", "WebTransport live connection"};
    case WebSchedulerTrackedFeature::kWebTransportSticky:
      return {"webtransport", "WebTransport used"};
    case WebSchedulerTrackedFeature::kWebRTC:
      return {"rtc", "WebRTC live connection"};
    case WebSchedulerTrackedFeature::kWebRTCSticky:
      return {"rtc", "WebRTC used"};
    case WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoCache:
      return {"response-cache-control-no-cache",
              "main resource has Cache-Control: No-Cache"};
    case WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore:
      return {"response-cache-control-no-store",
              "main resource has Cache-Control: No-Store"};
    case WebSchedulerTrackedFeature::kSubresourceHasCacheControlNoCache:
      return {"response-cache-control-no-cache",
              "subresource has Cache-Control: No-Cache"};
    case WebSchedulerTrackedFeature::kSubresourceHasCacheControlNoStore:
      return {"response-cache-control-no-store",
              "subresource has Cache-Control: No-Store"};
    case WebSchedulerTrackedFeature::kContainsPlugins:
      return {"plugins", "page contains plugins"};
    case WebSchedulerTrackedFeature::kDocumentLoaded:
      return {"document-loaded", "document loaded"};
    case WebSchedulerTrackedFeature::kSharedWorker:
      return {"sharedworker", "Shared worker present"};
    case WebSchedulerTrackedFeature::kOutstandingNetworkRequestFetch:
      return {"fetch", "outstanding network request (fetch)"};
    case WebSchedulerTrackedFeature::kOutstandingNetworkRequestXHR:
      return {"outstanding-network-request",
              "outstanding network request (XHR)"};
    case WebSchedulerTrackedFeature::kOutstandingNetworkRequestOthers:
      return {"outstanding-network-request",
              "outstanding network request (others)"};
    case WebSchedulerTrackedFeature::kRequestedMIDIPermission:
      return {"midi", "requested midi permission"};
    case WebSchedulerTrackedFeature::kRequestedAudioCapturePermission:
      return {"audio-capture", "requested audio capture permission"};
    case WebSchedulerTrackedFeature::kRequestedVideoCapturePermission:
      return {"video-capture", "requested video capture permission"};
    case WebSchedulerTrackedFeature::kRequestedBackForwardCacheBlockedSensors:
      return {"sensors", "requested sensors permission"};
    case WebSchedulerTrackedFeature::kRequestedBackgroundWorkPermission:
      return {"background-work", "requested background work permission"};
    case WebSchedulerTrackedFeature::kBroadcastChannel:
      return {"broadcastchannel", "requested broadcast channel permission"};
    case WebSchedulerTrackedFeature::kWebXR:
      return {"webxrdevice", "WebXR"};
    case WebSchedulerTrackedFeature::kWebLocks:
      return {"lock", "WebLocks"};
    case WebSchedulerTrackedFeature::kWebHID:
      return {"webhid", "WebHID"};
    case WebSchedulerTrackedFeature::kWebShare:
      return {"webshare", "WebShare"};
    case WebSchedulerTrackedFeature::kRequestedStorageAccessGrant:
      return {"storageaccess", "requested storage access permission"};
    case WebSchedulerTrackedFeature::kWebNfc:
      return {"webnfc", "WebNfc"};
    case WebSchedulerTrackedFeature::kPrinting:
      return {"printing", "Printing"};
    case WebSchedulerTrackedFeature::kWebDatabase:
      return {"web-database", "WebDatabase"};
    case WebSchedulerTrackedFeature::kPictureInPicture:
      return {"pictureinpicturewindow", "PictureInPicture"};
    case WebSchedulerTrackedFeature::kSpeechRecognizer:
      return {"speechrecognition", "SpeechRecognizer"};
    case WebSchedulerTrackedFeature::kIdleManager:
      return {"idledetector", "IdleManager"};
    case WebSchedulerTrackedFeature::kPaymentManager:
      return {"paymentrequest", "PaymentManager"};
    case WebSchedulerTrackedFeature::kKeyboardLock:
      return {"keyboardlock", "KeyboardLock"};
    case WebSchedulerTrackedFeature::kWebOTPService:
      return {"otpcredential", "SMSService"};
    case WebSchedulerTrackedFeature::kOutstandingNetworkRequestDirectSocket:
      return {"outstanding-network-request",
              "outstanding network request (direct socket)"};
    case WebSchedulerTrackedFeature::kInjectedJavascript:
      return {"injected-javascript", "External javascript injected"};
    case WebSchedulerTrackedFeature::kInjectedStyleSheet:
      return {"injected-stylesheet", "External systesheet injected"};
    case WebSchedulerTrackedFeature::kKeepaliveRequest:
      return {"response-keep-alive", "requests with keepalive set"};
    case WebSchedulerTrackedFeature::kDummy:
      return {"Dummy", "Dummy for testing"};
    case WebSchedulerTrackedFeature::
        kJsNetworkRequestReceivedCacheControlNoStoreResource:
      return {"response-cache-control-no-store",
              "JavaScript network request received Cache-Control: no-store "
              "resource"};
    case WebSchedulerTrackedFeature::kIndexedDBEvent:
      return {"idbversionchangeevent", "IndexedDB event is pending"};
    case WebSchedulerTrackedFeature::kWebSerial:
      return {"webserial", "Serial port open"};
    case WebSchedulerTrackedFeature::kSmartCard:
      return {"smartcardconnection", "SmartCardContext used"};
    case WebSchedulerTrackedFeature::kLiveMediaStreamTrack:
      return {"mediastream", "page has live MediaStreamTrack"};
    case WebSchedulerTrackedFeature::kUnloadHandler:
      return {"unload-handler", "page contains unload handler"};
    case WebSchedulerTrackedFeature::kParserAborted:
      return {"parser-aborted", "parser was aborted"};
  }
  return {};
}

std::map<std::string, WebSchedulerTrackedFeature> MakeShortNameToFeature() {
  std::map<std::string, WebSchedulerTrackedFeature> short_name_to_feature;
  for (int i = 0; i <= static_cast<int>(WebSchedulerTrackedFeature::kMaxValue);
       i++) {
    WebSchedulerTrackedFeature feature =
        static_cast<WebSchedulerTrackedFeature>(i);
    FeatureNames strs = FeatureToNames(feature);
    if (strs.short_name.size())
      short_name_to_feature[strs.short_name] = feature;
  }
  return short_name_to_feature;
}

const std::map<std::string, WebSchedulerTrackedFeature>&
ShortStringToFeatureMap() {
  static const std::map<std::string, WebSchedulerTrackedFeature>
      short_name_to_feature = MakeShortNameToFeature();
  return short_name_to_feature;
}

}  // namespace

std::string FeatureToHumanReadableString(WebSchedulerTrackedFeature feature) {
  return FeatureToNames(feature).human_readable;
}

std::string FeatureToShortString(WebSchedulerTrackedFeature feature) {
  return FeatureToNames(feature).short_name;
}

std::optional<WebSchedulerTrackedFeature> StringToFeature(
    const std::string& str) {
  auto map = ShortStringToFeatureMap();
  auto it = map.find(str);
  if (it == map.end()) {
    return std::nullopt;
  }
  return it->second;
}

bool IsRemovedFeature(const std::string& feature) {
  // This is an incomplete list. It only contains features that were
  // BFCache-enabled via finch. It does not contain all those that were removed.
  // This function is simple, not efficient because it is called once during
  // finch param parsing.
  const char* removed_features[] = {"MediaSessionImplOnServiceCreated"};
  for (const char* removed_feature : removed_features) {
    if (feature == removed_feature) {
      return true;
    }
  }
  return false;
}

bool IsFeatureSticky(WebSchedulerTrackedFeature feature) {
  return StickyFeatures().Has(feature);
}

WebSchedulerTrackedFeatures StickyFeatures() {
  return {WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore,
          WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoCache,
          WebSchedulerTrackedFeature::kSubresourceHasCacheControlNoStore,
          WebSchedulerTrackedFeature::kSubresourceHasCacheControlNoCache,
          WebSchedulerTrackedFeature::kContainsPlugins,
          WebSchedulerTrackedFeature::kDocumentLoaded,
          WebSchedulerTrackedFeature::kRequestedMIDIPermission,
          WebSchedulerTrackedFeature::kRequestedAudioCapturePermission,
          WebSchedulerTrackedFeature::kRequestedVideoCapturePermission,
          WebSchedulerTrackedFeature::kRequestedBackForwardCacheBlockedSensors,
          WebSchedulerTrackedFeature::kRequestedBackgroundWorkPermission,
          WebSchedulerTrackedFeature::kWebLocks,
          WebSchedulerTrackedFeature::kRequestedStorageAccessGrant,
          WebSchedulerTrackedFeature::kWebNfc,
          WebSchedulerTrackedFeature::kPrinting,
          WebSchedulerTrackedFeature::kPictureInPicture,
          WebSchedulerTrackedFeature::kIdleManager,
          WebSchedulerTrackedFeature::kPaymentManager,
          WebSchedulerTrackedFeature::kWebOTPService,
          WebSchedulerTrackedFeature::kInjectedJavascript,
          WebSchedulerTrackedFeature::kInjectedStyleSheet,
          WebSchedulerTrackedFeature::kKeepaliveRequest,
          WebSchedulerTrackedFeature::kDummy,
          WebSchedulerTrackedFeature::
              kJsNetworkRequestReceivedCacheControlNoStoreResource,
          WebSchedulerTrackedFeature::kWebRTCSticky,
          WebSchedulerTrackedFeature::kWebSocketSticky,
          WebSchedulerTrackedFeature::kWebTransportSticky,
          WebSchedulerTrackedFeature::kParserAborted};
}

// static
void DisableAlignWakeUpsForProcess() {
  disable_align_wake_ups.store(true, std::memory_order_relaxed);
}

// static
bool IsAlignWakeUpsDisabledForProcess() {
  return disable_align_wake_ups.load(std::memory_order_relaxed);
}

}  // namespace scheduler
}  // namespace blink
