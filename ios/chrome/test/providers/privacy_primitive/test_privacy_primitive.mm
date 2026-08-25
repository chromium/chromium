// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/privacy_primitive/privacy_primitive_api.h"

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
  return [[FakePrivacyPrimitiveService alloc] init];
}

}  // namespace ios::provider
