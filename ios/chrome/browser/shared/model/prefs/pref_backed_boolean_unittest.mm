// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"

#import <memory>
#import <utility>

#import "base/values.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/model/utils/fake_observable_boolean.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

const char kTestSwitchPref[] = "test-pref";

class PrefBackedBooleanTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    pref_service_.registry()->RegisterBooleanPref(kTestSwitchPref, false);
    observable_boolean_ =
        [[PrefBackedBoolean alloc] initWithPrefService:&pref_service_
                                              prefName:kTestSwitchPref];
  }

  void TearDown() override {
    [observable_boolean_ stop];
    observable_boolean_ = nil;
    PlatformTest::TearDown();
  }

 protected:
  bool GetPref() { return pref_service_.GetBoolean(kTestSwitchPref); }

  void SetPref(bool value) {
    auto booleanValue = std::make_unique<base::Value>(value);
    pref_service_.SetUserPref(kTestSwitchPref, std::move(booleanValue));
  }

  PrefBackedBoolean* GetObservableBoolean() { return observable_boolean_; }

  web::WebTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  PrefBackedBoolean* observable_boolean_;
};

TEST_F(PrefBackedBooleanTest, ReadFromPrefs) {
  SetPref(false);
  EXPECT_FALSE(GetObservableBoolean().value);

  SetPref(true);
  EXPECT_TRUE(GetObservableBoolean().value);
}

TEST_F(PrefBackedBooleanTest, WriteToPrefs) {
  GetObservableBoolean().value = true;
  EXPECT_TRUE(GetPref());

  GetObservableBoolean().value = false;
  EXPECT_FALSE(GetPref());
}

TEST_F(PrefBackedBooleanTest, ObserverUpdates) {
  SetPref(false);
  TestBooleanObserver* observer = [[TestBooleanObserver alloc] init];
  GetObservableBoolean().observer = observer;
  EXPECT_EQ(0, observer.updateCount);

  SetPref(true);
  EXPECT_EQ(1, observer.updateCount) << "Changing value should update observer";

  SetPref(true);
  EXPECT_EQ(1, observer.updateCount)
      << "Setting the same value should not update observer";
}

}  // namespace
