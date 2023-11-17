// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/optimization_guide_push_notification_client.h"

#import "base/base64.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "components/optimization_guide/core/hints_manager.h"
#import "components/optimization_guide/proto/push_notification.pb.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"

namespace {

// Opaque payload key from Chime
NSString* kSerializedPayloadKey = @"op";

}  // namespace

OptimizationGuidePushNotificationClient::
    OptimizationGuidePushNotificationClient(
        const PushNotificationClientId& push_notification_client_id)
    : PushNotificationClient(push_notification_client_id) {}

OptimizationGuidePushNotificationClient::
    ~OptimizationGuidePushNotificationClient() = default;

UIBackgroundFetchResult
OptimizationGuidePushNotificationClient::HandleNotificationReception(
    NSDictionary<NSString*, id>* notification) {
  OptimizationGuideService* optimization_guide_service =
      OptimizationGuideServiceFactory::GetForBrowserState(
          GetLastUsedBrowserState());
  std::unique_ptr<optimization_guide::proto::HintNotificationPayload>
      hint_notification_payload = ParseHintNotificationPayload(
          [notification objectForKey:kSerializedPayloadKey]);
  if (hint_notification_payload) {
    optimization_guide::PushNotificationManager* push_notification_manager =
        optimization_guide_service->GetHintsManager()
            ->push_notification_manager();
    push_notification_manager->OnNewPushNotification(
        *hint_notification_payload);
  }
  return UIBackgroundFetchResultNoData;
}

// static
std::unique_ptr<optimization_guide::proto::HintNotificationPayload>
OptimizationGuidePushNotificationClient::ParseHintNotificationPayload(
    NSString* serialized_payload_escaped) {
  std::string serialized_payload_unescaped;
  if (!base::Base64Decode(base::SysNSStringToUTF8(serialized_payload_escaped),
                          &serialized_payload_unescaped)) {
    return nullptr;
  }
  optimization_guide::proto::Any any;
  if (!any.ParseFromString(serialized_payload_unescaped) || !any.has_value()) {
    return nullptr;
  }
  std::unique_ptr<optimization_guide::proto::HintNotificationPayload>
      hint_notification_payload = std::make_unique<
          optimization_guide::proto::HintNotificationPayload>();
  if (!hint_notification_payload->ParseFromString(any.value())) {
    return nullptr;
  }
  return hint_notification_payload;
}

ChromeBrowserState*
OptimizationGuidePushNotificationClient::GetLastUsedBrowserState() {
  if (last_used_browser_state_for_testing_) {
    return last_used_browser_state_for_testing_;
  }
  return GetApplicationContext()
      ->GetChromeBrowserStateManager()
      ->GetLastUsedBrowserState();
}
