// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/push_notification/push_notification_api.h"

#import "base/task/sequenced_task_runner.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace provider {
namespace {

// Domain for Chromium push_notification error API.
NSString* const kChromiumPushNotificationErrorDomain =
    @"chromium_push_notification_error_domain";

class ChromiumPushNotificationService final : public PushNotificationService {
 public:
  // PushNotificationService implementation.
  void RegisterDevice(PushNotificationConfiguration* config,
                      void (^completion_handler)(NSError* error)) final;
  void UnregisterDevice(void (^completion_handler)(NSError* error)) final;
  bool DeviceTokenIsSet() const final;

 protected:
  // PushNotificationService implementation.
  void SetAccountsToDevice(NSArray<NSString*>* account_ids,
                           CompletionHandler completion_handler) final;
};

void ChromiumPushNotificationService::RegisterDevice(
    PushNotificationConfiguration* config,
    void (^completion_handler)(NSError* error)) {
  // Chromium does not initialize the device's connection to the push
  // notification server. As a result, the `completion_handler` is called with
  // a NSFeatureUnsupportedError.

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        NSError* error =
            [NSError errorWithDomain:kChromiumPushNotificationErrorDomain
                                code:NSFeatureUnsupportedError
                            userInfo:nil];
        completion_handler(error);
      }));
}

void ChromiumPushNotificationService::UnregisterDevice(
    void (^completion_handler)(NSError* error)) {
  // Chromium does not unregister the device on the push notification server. As
  // a result, the `completion_handler` is called with a
  // NSFeatureUnsupportedError.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        NSError* error =
            [NSError errorWithDomain:kChromiumPushNotificationErrorDomain
                                code:NSFeatureUnsupportedError
                            userInfo:nil];
        completion_handler(error);
      }));
}

bool ChromiumPushNotificationService::DeviceTokenIsSet() const {
  return false;
}

void ChromiumPushNotificationService::SetAccountsToDevice(
    NSArray<NSString*>* account_ids,
    void (^completion_handler)(NSError* error)) {
  // Chromium does not initialize the device's connection to the push
  // notification server. As a result, the `completion_handler` is called with
  // a NSFeatureUnsupportedError.

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        NSError* error =
            [NSError errorWithDomain:kChromiumPushNotificationErrorDomain
                                code:NSFeatureUnsupportedError
                            userInfo:nil];
        completion_handler(error);
      }));
}

}  // namespace

std::unique_ptr<PushNotificationService> CreatePushNotificationService() {
  return std::make_unique<ChromiumPushNotificationService>();
}

}  // namespace provider
}  // namespace ios
