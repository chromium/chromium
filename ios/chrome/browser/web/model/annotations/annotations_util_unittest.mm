// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/annotations/annotations_util.h"

#import "base/values.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class AnnotationUtilTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterDictionaryPref(prefs::kWebAnnotationsPolicy);
  }

  std::unique_ptr<TestingPrefServiceSimple> prefs_;
};

TEST_F(AnnotationUtilTest, TestPolicyDefaultEnabled) {
  EXPECT_EQ(WebAnnotationPolicyValue::kEnabled,
            GetPolicyForType(prefs_.get(), WebAnnotationType::kAddresses));
  EXPECT_EQ(WebAnnotationPolicyValue::kEnabled,
            GetPolicyForType(prefs_.get(), WebAnnotationType::kPhoneNumbers));
}

TEST_F(AnnotationUtilTest, TestGetPolicyForType) {
  base::Value::Dict dict;
  dict.Set("default", "enabled");
  dict.Set("calendar", "longpressonly");
  dict.Set("email", "disabled");

  prefs_->SetDict(prefs::kWebAnnotationsPolicy, std::move(dict));
  EXPECT_EQ(WebAnnotationPolicyValue::kLongPressOnly,
            GetPolicyForType(prefs_.get(), WebAnnotationType::kCalendar));
  EXPECT_EQ(WebAnnotationPolicyValue::kEnabled,
            GetPolicyForType(prefs_.get(), WebAnnotationType::kPhoneNumbers));
  EXPECT_EQ(WebAnnotationPolicyValue::kDisabled,
            GetPolicyForType(prefs_.get(), WebAnnotationType::kEMailAddresses));
}
