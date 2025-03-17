// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_GOOGLE_ONE_TEST_GOOGLE_ONE_H_
#define IOS_CHROME_TEST_PROVIDERS_GOOGLE_ONE_TEST_GOOGLE_ONE_H_

#import "ios/public/provider/chrome/browser/google_one/google_one_api.h"

// A protocol to replace the Google One providers in tests.
@protocol GoogleOneControllerFactory

// Create a GoogleOneController.
- (id<GoogleOneController>)createControllerWithConfiguration:
    (GoogleOneConfiguration*)configuration;

@end

namespace ios {
namespace provider {
namespace test {

// Sets the global factory for the tests.
// Resets it if `factory` is nil.
void SetGoogleOneControllerFactory(id<GoogleOneControllerFactory> factory);

}  // namespace test
}  // namespace provider
}  // namespace ios

#endif  // IOS_CHROME_TEST_PROVIDERS_GOOGLE_ONE_TEST_GOOGLE_ONE_H_
