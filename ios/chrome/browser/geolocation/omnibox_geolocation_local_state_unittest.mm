// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/geolocation/omnibox_geolocation_local_state.h"

#include <memory>
#include <string>

#include "components/prefs/testing_pref_service.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/chrome/test/testing_application_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class OmniboxGeolocationLocalStateTest : public PlatformTest {
 protected:
  OmniboxGeolocationLocalStateTest() {
    local_state_ = [[OmniboxGeolocationLocalState alloc] init];
  }

  IOSChromeScopedTestingLocalState scoped_local_state_;
  OmniboxGeolocationLocalState* local_state_;
};

TEST_F(OmniboxGeolocationLocalStateTest, LastAuthorizationAlertVersion) {
  EXPECT_TRUE([local_state_ lastAuthorizationAlertVersion].empty());

  std::string expectedVersion("fakeVersion");
  [local_state_ setLastAuthorizationAlertVersion:expectedVersion];
  EXPECT_EQ(expectedVersion, [local_state_ lastAuthorizationAlertVersion]);
}

}  // namespace
