// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/coordinator/face_pile_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/data_sharing/public/group_data.h"
#import "components/data_sharing/test_support/mock_data_sharing_service.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#import "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#import "ios/chrome/browser/saved_tab_groups/coordinator/face_pile_configuration.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_service.h"
#import "ios/chrome/browser/saved_tab_groups/ui/fake_face_pile_consumer.h"
#import "ios/chrome/browser/saved_tab_groups/ui/fake_face_pile_provider.h"
#import "ios/chrome/browser/share_kit/model/test_share_kit_service.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using data_sharing::GroupData;
using data_sharing::GroupMember;
using data_sharing::MemberRole;
using testing::_;
using ::testing::Return;

namespace {

// Creates a `GroupData` object containing `member_count` members.
// `member_count` should be positive.
GroupData CreateGroupData(int member_count) {
  CHECK_GT(member_count, 0);
  GroupData group_data = GroupData();
  GroupMember group_owner = GroupMember();
  group_owner.gaia_id = [FakeSystemIdentity fakeIdentity1].gaiaId;
  group_owner.role = MemberRole::kOwner;
  group_data.members.push_back(group_owner);
  for (int counter = 1; counter < member_count; counter++) {
    GroupMember group_member = GroupMember();
    group_member.gaia_id = GaiaId("MEMBER_" + base::NumberToString(counter));
    group_member.role = MemberRole::kMember;
    group_data.members.push_back(group_member);
  }
  return group_data;
}

}  // namespace

// Unit tests for the FacePileMediatorTest.
class FacePileMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    tab_group_service_ = std::make_unique<TabGroupService>(
        profile_.get(), &tab_group_sync_service_);
    share_kit_service_ = std::make_unique<TestShareKitService>(
        nullptr, nullptr, nullptr, tab_group_service_.get());
    data_sharing_service_ = std::make_unique<
        ::testing::NiceMock<data_sharing::MockDataSharingService>>();
    identity_manager_ = IdentityManagerFactory::GetForProfile(profile_.get());

    face_pile_config_ = [[FacePileConfiguration alloc] init];
    face_pile_config_.groupID = data_sharing::GroupId("test_group_id");
    face_pile_config_.avatarSize = 40.0;
    face_pile_config_.showsEmptyState = true;

    fake_face_pile_consumer_ = [[FakeFacePileConsumer alloc] init];

    _mediator = [[FacePileMediator alloc]
        initWithConfiguration:face_pile_config_
           dataSharingService:data_sharing_service_.get()
              shareKitService:share_kit_service_.get()
              identityManager:identity_manager_];
    SignIn();
  }

  // Sign in with a fake identity.
  void SignIn() {
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);
    AuthenticationServiceFactory::GetForProfile(profile_.get())
        ->SignIn(identity, signin_metrics::AccessPoint::kUnknown);
  }

  // Sign out.
  void SignOut() {
    AuthenticationServiceFactory::GetForProfile(profile_.get())
        ->SignOut(signin_metrics::ProfileSignout::kTest, ^(){
                  });
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestProfileIOS> profile_;
  ::testing::NiceMock<tab_groups::MockTabGroupSyncService>
      tab_group_sync_service_;
  std::unique_ptr<TabGroupService> tab_group_service_;
  std::unique_ptr<ShareKitService> share_kit_service_;
  std::unique_ptr<data_sharing::MockDataSharingService> data_sharing_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  FakeFacePileConsumer* fake_face_pile_consumer_;
  FacePileConfiguration* face_pile_config_;
  FacePileMediator* _mediator;
};

// Tests updateConsumer with an empty group (0 member).
TEST_F(FacePileMediatorTest, UpdateConsumerEmptyGroup) {
  GroupData group_data = CreateGroupData(1);

  // Stub IsGroupDataModelLoaded and ReadGroup to return an empty group data.
  EXPECT_CALL(*data_sharing_service_, IsGroupDataModelLoaded())
      .WillOnce(Return(true));
  EXPECT_CALL(*data_sharing_service_, ReadGroup(face_pile_config_.groupID))
      .WillOnce(Return(std::nullopt));

  _mediator.consumer = fake_face_pile_consumer_;

  // Expect consumer to be updated for empty state and with no faces.
  ASSERT_EQ(fake_face_pile_consumer_.updateWithViewsCallCount, 0u);
  ASSERT_EQ(fake_face_pile_consumer_.lastFaces.count, 0u);
  ASSERT_EQ(fake_face_pile_consumer_.lastShowsShareButtonWhenEmpty, YES);
}

// Tests updateConsumer with an empty group (1 members).
TEST_F(FacePileMediatorTest, UpdateConsumerEmptyGroupWithOwner) {
  GroupData group_data = CreateGroupData(1);

  // Stub IsGroupDataModelLoaded and ReadGroup to return an group data.
  EXPECT_CALL(*data_sharing_service_, IsGroupDataModelLoaded())
      .WillOnce(Return(true));
  EXPECT_CALL(*data_sharing_service_, ReadGroup(face_pile_config_.groupID))
      .WillOnce(Return(group_data));

  _mediator.consumer = fake_face_pile_consumer_;

  // Expect consumer to be updated for empty state and with one face.
  ASSERT_EQ(fake_face_pile_consumer_.lastAvatarSize, 40u);
  ASSERT_EQ(fake_face_pile_consumer_.updateWithViewsCallCount, 1u);
  ASSERT_EQ(fake_face_pile_consumer_.lastFaces.count, 1u);
  ASSERT_EQ(fake_face_pile_consumer_.lastTotalNumber, 1u);
  ASSERT_EQ(fake_face_pile_consumer_.lastShowsShareButtonWhenEmpty, YES);
}

// Tests updateConsumer with an medium group (3 members).
TEST_F(FacePileMediatorTest, UpdateConsumerMediumGroup) {
  GroupData group_data = CreateGroupData(3);

  // Stub IsGroupDataModelLoaded and ReadGroup to return an group data.
  EXPECT_CALL(*data_sharing_service_, IsGroupDataModelLoaded())
      .WillOnce(Return(true));
  EXPECT_CALL(*data_sharing_service_, ReadGroup(face_pile_config_.groupID))
      .WillOnce(Return(group_data));

  _mediator.consumer = fake_face_pile_consumer_;

  // Expect consumer to be updated for with 3 faces.
  ASSERT_EQ(fake_face_pile_consumer_.lastAvatarSize, 40u);
  ASSERT_EQ(fake_face_pile_consumer_.updateWithViewsCallCount, 1u);
  ASSERT_EQ(fake_face_pile_consumer_.lastFaces.count, 3u);
  ASSERT_EQ(fake_face_pile_consumer_.lastTotalNumber, 3u);
  ASSERT_EQ(fake_face_pile_consumer_.lastShowsShareButtonWhenEmpty, YES);
}

// Tests updateConsumer with an big group (8 members).
TEST_F(FacePileMediatorTest, UpdateConsumerBigGroup) {
  GroupData group_data = CreateGroupData(8);

  // Stub IsGroupDataModelLoaded and ReadGroup to return an group data.
  EXPECT_CALL(*data_sharing_service_, IsGroupDataModelLoaded())
      .WillOnce(Return(true));
  EXPECT_CALL(*data_sharing_service_, ReadGroup(face_pile_config_.groupID))
      .WillOnce(Return(group_data));

  _mediator.consumer = fake_face_pile_consumer_;

  // Expect consumer to be updated for with 2 faces.
  ASSERT_EQ(fake_face_pile_consumer_.lastAvatarSize, 40u);
  ASSERT_EQ(fake_face_pile_consumer_.updateWithViewsCallCount, 1u);
  ASSERT_EQ(fake_face_pile_consumer_.lastFaces.count, 2u);
  ASSERT_EQ(fake_face_pile_consumer_.lastTotalNumber, 8u);
  ASSERT_EQ(fake_face_pile_consumer_.lastShowsShareButtonWhenEmpty, YES);
}

// Tests updateConsumer when the user is signed out.
TEST_F(FacePileMediatorTest, UpdateConsumerSignedOut) {
  SignOut();

  // Stub IsGroupDataModelLoaded and ReadGroup should not be called.
  EXPECT_CALL(*data_sharing_service_, IsGroupDataModelLoaded())
      .WillOnce(Return(true));
  EXPECT_CALL(*data_sharing_service_, ReadGroup(face_pile_config_.groupID))
      .Times(0);

  _mediator.consumer = fake_face_pile_consumer_;

  // Expect consumer to be updated to show the share button.
  ASSERT_EQ(fake_face_pile_consumer_.lastShowsShareButtonWhenEmpty, YES);
}
