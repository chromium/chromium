// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_PRIVACY_PRIMITIVE_TEST_PRIVACY_PRIMITIVE_H_
#define IOS_CHROME_TEST_PROVIDERS_PRIVACY_PRIMITIVE_TEST_PRIVACY_PRIMITIVE_H_

#import "ios/public/provider/chrome/browser/privacy_primitive/privacy_primitive_api.h"

// A protocol to replace the Privacy Primitive provider in tests.
@protocol PrivacyPrimitiveServiceFactory

// Creates a PrivacyPrimitiveService with the given configuration.
- (id<PrivacyPrimitiveService>)createPrivacyPrimitiveService:
    (PrivacyPrimitiveConfiguration*)configuration;

@end

namespace ios::provider::test {

// Sets the global factory for the tests.
// Resets it if `factory` is nil.
void SetPrivacyPrimitiveServiceFactory(
    id<PrivacyPrimitiveServiceFactory> factory);

}  // namespace ios::provider::test

#endif  // IOS_CHROME_TEST_PROVIDERS_PRIVACY_PRIMITIVE_TEST_PRIVACY_PRIMITIVE_H_
