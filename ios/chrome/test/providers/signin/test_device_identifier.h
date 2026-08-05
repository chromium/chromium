// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_SIGNIN_TEST_DEVICE_IDENTIFIER_H_
#define IOS_CHROME_TEST_PROVIDERS_SIGNIN_TEST_DEVICE_IDENTIFIER_H_

#import <string>

namespace ios::provider::test {

// Sets the device identifier for testing.
void SetDeviceIdentifier(std::string identifier);

}  // namespace ios::provider::test

#endif  // IOS_CHROME_TEST_PROVIDERS_SIGNIN_TEST_DEVICE_IDENTIFIER_H_
