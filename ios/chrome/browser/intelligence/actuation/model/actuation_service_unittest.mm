// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/actuation_service.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_service_factory.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using ActuationResult = ActuationTool::ActuationResult;

class ActuationServiceTest : public PlatformTest {
 public:
  ActuationServiceTest() {
    ActuationServiceFactory::GetInstance();
    profile_ = TestProfileIOS::Builder().Build();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

TEST_F(ActuationServiceTest, ServiceCreationWithFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActuationTools);

  ActuationService* service =
      ActuationServiceFactory::GetForProfile(profile_.get());
  EXPECT_NE(nullptr, service);
}

TEST_F(ActuationServiceTest, ServiceCreationWithFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kActuationTools);

  ActuationService* service =
      ActuationServiceFactory::GetForProfile(profile_.get());
  EXPECT_EQ(nullptr, service);
}
