// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/signin/test_device_identifier.h"

#import <utility>

#import "base/no_destructor.h"
#import "ios/public/provider/chrome/browser/signin/device_identifier_api.h"

namespace ios::provider {

namespace {

std::string& GetTestDeviceIdentifierStorage() {
  static base::NoDestructor<std::string> storage("test-device-identifier");
  return *storage;
}

}  // namespace

namespace test {

void SetDeviceIdentifier(std::string identifier) {
  GetTestDeviceIdentifierStorage() = std::move(identifier);
}

}  // namespace test

std::string GetDeviceIdentifier() {
  return GetTestDeviceIdentifierStorage();
}

}  // namespace ios::provider
