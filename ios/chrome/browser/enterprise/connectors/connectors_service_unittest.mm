// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/connectors_service.h"

#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace enterprise_connectors {

namespace {

class ConnectorsServiceTest : public PlatformTest {
 public:
  PrefService* prefs() { return &pref_service_; }

 private:
  TestingPrefServiceSimple pref_service_;
};

}  // namespace

TEST_F(ConnectorsServiceTest, GetPrefs) {
  ConnectorsService connectors_service{prefs()};
  const ConnectorsService const_connectors_service{prefs()};

  PrefService* prefs = connectors_service.GetPrefs();
  const PrefService* const_prefs = const_connectors_service.GetPrefs();

  ASSERT_TRUE(prefs);
  ASSERT_TRUE(const_prefs);
  ASSERT_EQ(prefs, const_prefs);
}

}  // namespace enterprise_connectors
