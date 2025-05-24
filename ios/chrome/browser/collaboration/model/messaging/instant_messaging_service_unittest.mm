// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/messaging/instant_messaging_service.h"

#import "base/test/mock_callback.h"
#import "base/test/scoped_feature_list.h"
#import "components/collaboration/public/features.h"
#import "components/data_sharing/public/features.h"
#import "ios/chrome/browser/collaboration/model/messaging/instant_messaging_service_factory.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace collaboration::messaging {

// Returns an `InstantMessage`.
InstantMessage CreateInstantMessage(InstantNotificationLevel level) {
  InstantMessage message;
  message.level = level;
  message.type = InstantNotificationType::UNDEFINED;
  message.collaboration_event = CollaborationEvent::TAB_GROUP_REMOVED;
  return message;
}

class InstantMessagingServiceTest : public PlatformTest {
 public:
  InstantMessagingServiceTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            kTabGroupSync,
            data_sharing::features::kDataSharingFeature,
            collaboration::features::kCollaborationMessaging,
        },
        /*disable_features=*/{});

    profile_ = TestProfileIOS::Builder().Build();
    service_ = InstantMessagingServiceFactory::GetForProfile(profile_.get());
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());

    ConfigureWebStateList();
    ConfigureInfobarManager();
  }

  // Configures the `web_state_list_`.
  void ConfigureWebStateList() {
    web_state_list_ = browser_->GetWebStateList();
    WebStateListBuilderFromDescription builder(web_state_list_);
    ASSERT_TRUE(
        builder.BuildWebStateListFromDescription("| [0 a*]", profile_.get()));
    active_web_state_ =
        static_cast<web::FakeWebState*>(web_state_list_->GetActiveWebState());
  }

  // Configures the InfoBarManager.
  void ConfigureInfobarManager() {
    active_web_state_->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(active_web_state_);
  }

  ~InstantMessagingServiceTest() override {
    InfoBarManagerImpl::FromWebState(active_web_state_)->ShutDown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<InstantMessagingService> service_;
  raw_ptr<WebStateList> web_state_list_;
  raw_ptr<web::FakeWebState> active_web_state_;
};

// Tests the DisplayInstantaneousMessage method for an undefined level message.
TEST_F(InstantMessagingServiceTest, DisplayInstantaneousUndefinedMessage) {
  if (!IsTabGroupInGridEnabled()) {
    // Disabled on iPadOS 16.
    return;
  }
  InstantMessage message =
      CreateInstantMessage(InstantNotificationLevel::UNDEFINED);
  base::MockCallback<
      MessagingBackendService::InstantMessageDelegate::SuccessCallback>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(false));
  service_->DisplayInstantaneousMessage(message, mock_callback.Get());
}

// Tests the DisplayInstantaneousMessage method for a browser level message.
TEST_F(InstantMessagingServiceTest, DisplayInstantaneousBrowserMessage) {
  if (!IsTabGroupInGridEnabled()) {
    // Disabled on iPadOS 16.
    return;
  }
  InstantMessage message =
      CreateInstantMessage(InstantNotificationLevel::BROWSER);
  base::MockCallback<
      MessagingBackendService::InstantMessageDelegate::SuccessCallback>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(true));
  service_->DisplayInstantaneousMessage(message, mock_callback.Get());
}

// Tests the DisplayInstantaneousMessage method for a system level message.
TEST_F(InstantMessagingServiceTest, DisplayInstantaneousSystemMessage) {
  if (!IsTabGroupInGridEnabled()) {
    // Disabled on iPadOS 16.
    return;
  }
  InstantMessage message =
      CreateInstantMessage(InstantNotificationLevel::SYSTEM);
  base::MockCallback<
      MessagingBackendService::InstantMessageDelegate::SuccessCallback>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(false));
  service_->DisplayInstantaneousMessage(message, mock_callback.Get());
}

}  // namespace collaboration::messaging
