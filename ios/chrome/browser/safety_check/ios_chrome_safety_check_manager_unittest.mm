// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager.h"

#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/testing_pref_service.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class IOSChromeSafetyCheckManagerTest : public PlatformTest {
 public:
  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    PrefRegistrySimple* registry = pref_service_->registry();

    registry->RegisterBooleanPref(prefs::kSafeBrowsingEnabled, false);
    registry->RegisterBooleanPref(prefs::kSafeBrowsingEnhanced, false);

    safety_check_manager_ =
        std::make_unique<IOSChromeSafetyCheckManager>(pref_service_.get());
  }

  void TearDown() override { safety_check_manager_->Shutdown(); }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<IOSChromeSafetyCheckManager> safety_check_manager_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

}  // namespace

// Tests the Safe Browsing Check state is `kSafe` when Safe Browsing is enabled,
// but not managed.
TEST_F(IOSChromeSafetyCheckManagerTest, SafeBrowsingEnabledReturnsSafeState) {
  pref_service_->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  EXPECT_EQ(safety_check_manager_->GetSafeBrowsingCheckState(),
            SafeBrowsingSafetyCheckState::kSafe);
}

// Tests the Safe Browsing Check state is `kUnsafe` when Safe Browsing is
// disabled, and not managed.
TEST_F(IOSChromeSafetyCheckManagerTest,
       SafeBrowsingDisabledReturnsUnsafeState) {
  pref_service_->SetBoolean(prefs::kSafeBrowsingEnabled, false);

  EXPECT_EQ(safety_check_manager_->GetSafeBrowsingCheckState(),
            SafeBrowsingSafetyCheckState::kUnsafe);
}

// Tests the Safe Browsing Check state is `kManaged` when Safe Browsing is
// enabled, and managed.
TEST_F(IOSChromeSafetyCheckManagerTest,
       SafeBrowsingManagedAndEnabledReturnsManagedState) {
  pref_service_->SetManagedPref(prefs::kSafeBrowsingEnabled, base::Value(true));

  EXPECT_EQ(safety_check_manager_->GetSafeBrowsingCheckState(),
            SafeBrowsingSafetyCheckState::kManaged);
}

// Tests the Safe Browsing Check state is `kManaged` when Safe Browsing is
// disabled, and managed.
TEST_F(IOSChromeSafetyCheckManagerTest,
       SafeBrowsingManagedAndDisabledReturnsManagedState) {
  pref_service_->SetManagedPref(prefs::kSafeBrowsingEnabled,
                                base::Value(false));

  EXPECT_EQ(safety_check_manager_->GetSafeBrowsingCheckState(),
            SafeBrowsingSafetyCheckState::kManaged);
}
