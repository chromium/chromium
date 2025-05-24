// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/messaging/infobar/collaboration_group_infobar_delegate.h"

#import "base/test/scoped_feature_list.h"
#import "components/collaboration/public/collaboration_flow_entry_point.h"
#import "components/collaboration/public/features.h"
#import "components/collaboration/public/messaging/message.h"
#import "components/data_sharing/public/features.h"
#import "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/public/commands/collaboration_group_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using testing::Return;

namespace collaboration::messaging {

namespace {

const char kExampleUrl[] = "https://example.com/";

// Returns a `MessageAttribution` for `CreateInstantMessage:`.
MessageAttribution CreateAttribution(tab_groups::LocalTabGroupID local_group_id,
                                     CollaborationEvent event) {
  MessageAttribution attribution;
  attribution.id = base::Uuid::GenerateRandomV4();
  attribution.collaboration_id = data_sharing::GroupId("shared group");
  // GroupMember has its own conversion utils, so only check a single field.
  attribution.affected_user = data_sharing::GroupMember();
  attribution.affected_user->gaia_id = GaiaId("affected");
  attribution.triggering_user = data_sharing::GroupMember();
  attribution.triggering_user->gaia_id = GaiaId("triggering");

  // TabGroupMessageMetadata.
  attribution.tab_group_metadata = TabGroupMessageMetadata();
  attribution.tab_group_metadata->local_tab_group_id = local_group_id;
  attribution.tab_group_metadata->sync_tab_group_id =
      base::Uuid::GenerateRandomV4();
  attribution.tab_group_metadata->last_known_title = "previous title";
  attribution.tab_group_metadata->last_known_color =
      tab_groups::TabGroupColorId::kOrange;

  // TabMessageMetadata.
  attribution.tab_metadata = TabMessageMetadata();
  attribution.tab_metadata->local_tab_id = std::make_optional(499897179);
  attribution.tab_metadata->sync_tab_id = base::Uuid::GenerateRandomV4();
  attribution.tab_metadata->last_known_url = kExampleUrl;
  attribution.tab_metadata->previous_url = kExampleUrl;
  attribution.tab_metadata->last_known_title = "last known tab title";

  return attribution;
}

// Returns an `InstantMessage`.
InstantMessage CreateInstantMessage(tab_groups::LocalTabGroupID local_group_id,
                                    CollaborationEvent event,
                                    bool multiple_attributions = false) {
  InstantMessage message;
  message.level = InstantNotificationLevel::BROWSER;
  message.type = InstantNotificationType::UNDEFINED;
  message.collaboration_event = event;

  message.attributions.emplace_back(CreateAttribution(local_group_id, event));
  if (multiple_attributions) {
    // Add a second `MessageAttribution` for testing purpose.
    message.attributions.emplace_back(CreateAttribution(local_group_id, event));
  }
  return message;
}

// CollaborationGroupInfoBarDelegate test suite.
class CollaborationGroupInfoBarDelegateTest : public PlatformTest {
 public:
  CollaborationGroupInfoBarDelegateTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            kTabGroupSync,
            data_sharing::features::kDataSharingFeature,
            collaboration::features::kCollaborationMessaging,
        },
        /*disable_features=*/{});

    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        tab_groups::TabGroupSyncServiceFactory::GetInstance(),
        tab_groups::TabGroupSyncServiceFactory::GetDefaultFactory());
    profile_ = std::move(test_profile_builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());
    url_loader_ = FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
        UrlLoadingBrowserAgent::FromBrowser(browser_.get()));

    ConfigureWebStateList();
    ConfigureInfobarManager();
    ConfigureMockHandler();
  }

  ~CollaborationGroupInfoBarDelegateTest() override {
    InfoBarManagerImpl::FromWebState(active_web_state_)->ShutDown();
    [browser_->GetCommandDispatcher() stopDispatchingToTarget:mock_handler_];
  }

  // Configures the `web_state_list_` and `active_web_state_`.
  void ConfigureWebStateList() {
    web_state_list_ = browser_->GetWebStateList();
    WebStateListBuilderFromDescription builder(web_state_list_);
    ASSERT_TRUE(
        builder.BuildWebStateListFromDescription("| [0 a*]", profile_.get()));
    tab_group_ = web_state_list_->GetGroupOfWebStateAt(0);
    active_web_state_ =
        static_cast<web::FakeWebState*>(web_state_list_->GetActiveWebState());
  }

  // Configures the InfoBarManager.
  void ConfigureInfobarManager() {
    active_web_state_->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(active_web_state_);
  }

  // Configures the `mock_handler_` to dispatch CollaborationGroupCommands.
  void ConfigureMockHandler() {
    mock_handler_ = OCMProtocolMock(@protocol(CollaborationGroupCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_handler_
                     forProtocol:@protocol(CollaborationGroupCommands)];
  }

  // Returns the InfoBarManager attached to the active webstate.
  infobars::InfoBarManager* infobar_manager() {
    return InfoBarManagerImpl::FromWebState(active_web_state_);
  }

  // Returns the current infobar, if available.
  InfoBarIOS* infobar() {
    return infobar_manager()->infobars().empty()
               ? nullptr
               : static_cast<InfoBarIOS*>(infobar_manager()->infobars()[0]);
  }

  // Returns the infobar's delegate.
  CollaborationGroupInfoBarDelegate* infobar_delegate() {
    return infobar() ? static_cast<CollaborationGroupInfoBarDelegate*>(
                           infobar()->delegate())
                     : nullptr;
  }

 protected:
  raw_ptr<const TabGroup> tab_group_;
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  id mock_handler_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<FakeUrlLoadingBrowserAgent> url_loader_;
  raw_ptr<WebStateList> web_state_list_;
  raw_ptr<web::FakeWebState> active_web_state_;
};

// Tests the creation of the delegate with a valid message.
TEST_F(CollaborationGroupInfoBarDelegateTest, DelegateCreatedValidMessage) {
  bool infobar_created = CollaborationGroupInfoBarDelegate::Create(
      profile_.get(),
      CreateInstantMessage(tab_group_->tab_group_id(),
                           CollaborationEvent::TAB_GROUP_NAME_UPDATED));
  EXPECT_TRUE(infobar_created);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
}

// Tests the creation of the delegate with an invalid message.
TEST_F(CollaborationGroupInfoBarDelegateTest,
       DelegateCreatedWithInvalidMessage) {
  bool infobar_created = CollaborationGroupInfoBarDelegate::Create(
      profile_.get(),
      CreateInstantMessage(tab_groups::test::GenerateRandomTabGroupID(),
                           CollaborationEvent::TAB_GROUP_NAME_UPDATED));
  EXPECT_FALSE(infobar_created);
  EXPECT_EQ(0U, infobar_manager()->infobars().size());
}

// Tests the delegate for a `TAB_UPDATED` event.
TEST_F(CollaborationGroupInfoBarDelegateTest, DelegateTabUpdated) {
  bool infobar_created = CollaborationGroupInfoBarDelegate::Create(
      profile_.get(), CreateInstantMessage(tab_group_->tab_group_id(),
                                           CollaborationEvent::TAB_UPDATED));
  EXPECT_TRUE(infobar_created);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  infobar_delegate()->Accept();

  EXPECT_EQ(GURL(kExampleUrl), url_loader_->last_params.web_params.url);
  EXPECT_EQ(1, url_loader_->load_current_tab_call_count);
}

// Tests the delegate for a `TAB_REMOVED` event.
TEST_F(CollaborationGroupInfoBarDelegateTest, DelegateTabRemoved) {
  bool infobar_created = CollaborationGroupInfoBarDelegate::Create(
      profile_.get(), CreateInstantMessage(tab_group_->tab_group_id(),
                                           CollaborationEvent::TAB_REMOVED));
  EXPECT_TRUE(infobar_created);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  infobar_delegate()->Accept();

  EXPECT_EQ(GURL(kExampleUrl), url_loader_->last_params.web_params.url);
  EXPECT_TRUE(url_loader_->last_params.load_in_group);
  EXPECT_EQ(tab_group_, url_loader_->last_params.tab_group.get());
  EXPECT_EQ(1, url_loader_->load_new_tab_call_count);
}

// Tests the delegate for a `COLLABORATION_MEMBER_ADDED` event.
TEST_F(CollaborationGroupInfoBarDelegateTest, DelegateMemberAdded) {
  bool infobar_created = CollaborationGroupInfoBarDelegate::Create(
      profile_.get(),
      CreateInstantMessage(tab_group_->tab_group_id(),
                           CollaborationEvent::COLLABORATION_MEMBER_ADDED));
  EXPECT_TRUE(infobar_created);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());

  OCMExpect([mock_handler_
      shareOrManageTabGroup:tab_group_
                 entryPoint:collaboration::
                                CollaborationServiceShareOrManageEntryPoint::
                                    kiOSMessage]);
  infobar_delegate()->Accept();
  EXPECT_OCMOCK_VERIFY(mock_handler_);
}

// Tests the GetAvatarPrimitive delegate method with 1 MessageAttribution.
TEST_F(CollaborationGroupInfoBarDelegateTest, DelegateGetAvatarPrimitive) {
  bool infobar_created = CollaborationGroupInfoBarDelegate::Create(
      profile_.get(), CreateInstantMessage(tab_group_->tab_group_id(),
                                           CollaborationEvent::TAB_REMOVED));
  EXPECT_TRUE(infobar_created);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  id<ShareKitAvatarPrimitive> avatarPrimitive =
      infobar_delegate()->GetAvatarPrimitive();
  EXPECT_TRUE(avatarPrimitive);
}

// Tests the GetAvatarPrimitive delegate method with multiple
// MessageAttribution.
TEST_F(CollaborationGroupInfoBarDelegateTest,
       DelegateGetAvatarPrimitiveMultipleAttributions) {
  // The `TAB_GROUP_COLOR_UPDATED` event message is updated to set multiple
  // `MessageAttribution`.
  bool infobar_created = CollaborationGroupInfoBarDelegate::Create(
      profile_.get(),
      CreateInstantMessage(tab_group_->tab_group_id(),
                           CollaborationEvent::TAB_GROUP_COLOR_UPDATED,
                           /*multiple_attributions=*/true));
  EXPECT_TRUE(infobar_created);
  EXPECT_EQ(1U, infobar_manager()->infobars().size());
  id<ShareKitAvatarPrimitive> avatarPrimitive =
      infobar_delegate()->GetAvatarPrimitive();
  EXPECT_FALSE(avatarPrimitive);
}

}  // namespace

}  // namespace collaboration::messaging
