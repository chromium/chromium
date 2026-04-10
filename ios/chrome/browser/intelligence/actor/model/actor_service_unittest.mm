// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"

#import <set>

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

class ActorServiceTest : public PlatformTest {
 public:
  ActorServiceTest() {
    ActorServiceFactory::GetInstance();
    profile_ = TestProfileIOS::Builder().Build();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

TEST_F(ActorServiceTest, ServiceCreationWithFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  EXPECT_NE(nullptr, service);
}

TEST_F(ActorServiceTest, ServiceCreationWithFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kActorTools);

  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  EXPECT_EQ(nullptr, service);
}

TEST_F(ActorServiceTest, CreateTaskGeneratesUniqueIds) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  std::set<ActorTaskId> task_ids;
  for (int i = 0; i < 100; ++i) {
    ActorTaskId task_id =
        service->CreateTask("Test Task",
                            /*allow_incognito_web_states=*/false);
    EXPECT_FALSE(task_id.is_null());
    EXPECT_TRUE(task_ids.insert(task_id).second);
  }
}

}  // namespace actor
