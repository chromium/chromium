// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/prefs/pref_backed_string.h"

#import <memory>
#import <string>
#import <utility>

#import "base/values.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/model/utils/fake_observable_string.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

const char kTestSwitchPref[] = "test-pref";
const char kTestValue1[] = "value1";
const char kTestValue2[] = "value2";

class PrefBackedStringTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    pref_service_.registry()->RegisterStringPref(kTestSwitchPref, "");
    observable_string_ =
        [[PrefBackedString alloc] initWithPrefService:&pref_service_
                                             prefName:kTestSwitchPref];
  }

  void TearDown() override {
    [observable_string_ stop];
    observable_string_ = nil;
    PlatformTest::TearDown();
  }

 protected:
  const std::string& GetPref() {
    return pref_service_.GetString(kTestSwitchPref);
  }

  void SetPref(const std::string& value) {
    pref_service_.SetString(kTestSwitchPref, value);
  }

  PrefBackedString* GetObservableString() { return observable_string_; }

  web::WebTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  PrefBackedString* observable_string_;
};

TEST_F(PrefBackedStringTest, ReadFromPrefs) {
  SetPref(kTestValue1);
  EXPECT_NSEQ(GetObservableString().value, @(kTestValue1));

  SetPref(kTestValue2);
  EXPECT_NSEQ(GetObservableString().value, @(kTestValue2));
}

TEST_F(PrefBackedStringTest, WriteToPrefs) {
  GetObservableString().value = @(kTestValue1);
  EXPECT_EQ(GetPref(), kTestValue1);

  GetObservableString().value = @(kTestValue2);
  EXPECT_EQ(GetPref(), kTestValue2);
}

TEST_F(PrefBackedStringTest, ObserverUpdates) {
  SetPref(kTestValue1);
  TestStringObserver* observer = [[TestStringObserver alloc] init];
  GetObservableString().observer = observer;
  EXPECT_EQ(0, observer.updateCount);

  SetPref(kTestValue2);
  EXPECT_EQ(1, observer.updateCount) << "Changing value should update observer";

  SetPref(kTestValue2);
  EXPECT_EQ(1, observer.updateCount)
      << "Setting the same value should not update observer";
}

}  // namespace
