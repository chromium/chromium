// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/push_notification/push_notification_api.h"

#import "base/threading/sequenced_task_runner_handle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace provider {
namespace {

// Domain for test push_notification error API.
NSString* const kTestPushNotificationErrorDomain =
    @"test_push_notification_error_domain";

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
};

void TestPushNotificationService::RegisterDevice(
    PushNotificationConfiguration* config,
    void (^completion_handler)(NSError* error)) {
  // Test implementation does nothing. As a result, the `completion_handler` is
  // called with a NSFeatureUnsupportedError.

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        NSError* error =
            [NSError errorWithDomain:kTestPushNotificationErrorDomain
                                code:NSFeatureUnsupportedError
                            userInfo:nil];
        completion_handler(error);
      }));
}

void TestPushNotificationService::UnregisterDevice(
    void (^completion_handler)(NSError* error)) {
  // Test implementation does nothing. As a result, the `completion_handler` is
  // called with a NSFeatureUnsupportedError.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        NSError* error =
            [NSError errorWithDomain:kTestPushNotificationErrorDomain
                                code:NSFeatureUnsupportedError
                            userInfo:nil];
        completion_handler(error);
      }));
}

bool TestPushNotificationService::DeviceTokenIsSet() const {
  return false;
}

void TestPushNotificationService::SetAccountsToDevice(
    NSArray<NSString*>* account_ids,
    void (^completion_handler)(NSError* error)) {
  // Test implementation does nothing. As a result, the `completion_handler` is
  // called with a NSFeatureUnsupportedError.

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        NSError* error =
            [NSError errorWithDomain:kTestPushNotificationErrorDomain
                                code:NSFeatureUnsupportedError
                            userInfo:nil];
        completion_handler(error);
      }));
}

std::unique_ptr<PushNotificationService> CreatePushNotificationService() {
  return std::make_unique<TestPushNotificationService>();
}
}  // namespace provider
}  // namespace ios
