// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/push_notification/push_notification_api.h"

#import "base/task/sequenced_task_runner.h"

namespace ios {
namespace provider {
namespace {

// Domain for test push_notification error API.
NSString* const kTestPushNotificationErrorDomain =
    @"test_push_notification_error_domain";

// Helper method that asynchronously invoke `completion_handler`
// with an `NSFeatureUnsupportedError` on the current sequence.
void FailWithUnsupportedFeatureError(
    PushNotificationService::CompletionHandler completion_handler) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        NSError* error =
            [NSError errorWithDomain:kTestPushNotificationErrorDomain
                                code:NSFeatureUnsupportedError
                            userInfo:nil];
        completion_handler(error);
      }));
}

}  // namespace

// Implementation of the PushNotificationService for use by unit tests
class TestPushNotificationService final : public PushNotificationService {
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
  void SetPreferences(NSString* account_id,
                      PreferenceMap preference_map,
                      CompletionHandler completion_handler) final;
};

void TestPushNotificationService::RegisterDevice(
    PushNotificationConfiguration* config,
    void (^completion_handler)(NSError* error)) {
  // Test implementation does nothing. As a result, the `completion_handler` is
  // called with a NSFeatureUnsupportedError.
  FailWithUnsupportedFeatureError(completion_handler);
}

void TestPushNotificationService::UnregisterDevice(
    void (^completion_handler)(NSError* error)) {
  // Test implementation does nothing. As a result, the `completion_handler` is
  // called with a NSFeatureUnsupportedError.
  FailWithUnsupportedFeatureError(completion_handler);
}

bool TestPushNotificationService::DeviceTokenIsSet() const {
  return false;
}

void TestPushNotificationService::SetAccountsToDevice(
    NSArray<NSString*>* account_ids,
    void (^completion_handler)(NSError* error)) {
  // Test implementation does nothing. As a result, the `completion_handler` is
  // called with a NSFeatureUnsupportedError.
  FailWithUnsupportedFeatureError(completion_handler);
}

void TestPushNotificationService::SetPreferences(
    NSString* account_id,
    PreferenceMap preference_map,
    CompletionHandler completion_handler) {
  // Test implementation does nothing. As a result, the `completion_handler` is
  // called with a NSFeatureUnsupportedError.
  FailWithUnsupportedFeatureError(completion_handler);
}

std::unique_ptr<PushNotificationService> CreatePushNotificationService() {
  return std::make_unique<TestPushNotificationService>();
}
}  // namespace provider
}  // namespace ios
