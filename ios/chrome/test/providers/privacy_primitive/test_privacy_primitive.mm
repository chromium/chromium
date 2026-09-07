// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/privacy_primitive/test_privacy_primitive.h"

#import "ios/public/provider/chrome/browser/privacy_primitive/privacy_primitive_api.h"

namespace {
id<PrivacyPrimitiveServiceFactory> g_privacy_primitive_service_factory;
}

@interface FakePrivacyPrimitiveService : NSObject <PrivacyPrimitiveService>
@end

@implementation FakePrivacyPrimitiveService

- (void)showFlowWithPresentingViewController:(UIViewController*)viewController
                           completionHandler:
                               (void (^)(BOOL success))completionHandler {
  if (completionHandler) {
    completionHandler(NO);
  }
}

@end

namespace ios::provider {

id<PrivacyPrimitiveService> CreatePrivacyPrimitiveService(
    PrivacyPrimitiveConfiguration* configuration) {
  if (g_privacy_primitive_service_factory) {
    return [g_privacy_primitive_service_factory
        createPrivacyPrimitiveService:configuration];
  }
  return [[FakePrivacyPrimitiveService alloc] init];
}

namespace test {

void SetPrivacyPrimitiveServiceFactory(
    id<PrivacyPrimitiveServiceFactory> factory) {
  g_privacy_primitive_service_factory = factory;
}

}  // namespace test

}  // namespace ios::provider
