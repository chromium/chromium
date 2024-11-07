// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/credential_provider_browser_agent.h"

#import "base/time/time.h"
#import "components/sync/base/features.h"
#import "components/webauthn/core/browser/test_passkey_model.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "url/gurl.h"

class CredentialProviderBrowserAgentTest : public PlatformTest {
 public:
  CredentialProviderBrowserAgentTest() {
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        IOSPasskeyModelFactory::GetInstance(),
        base::BindRepeating(
            [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<webauthn::TestPasskeyModel>();
            }));

    profile_ = std::move(test_profile_builder).Build();
  }

  void SetUpBrowserAgent(bool incognito) {
    browser_ = std::make_unique<TestBrowser>(
        incognito ? profile_->GetOffTheRecordProfile() : profile_.get());
    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    mock_settings_commands_handler_ =
        OCMStrictProtocolMock(@protocol(SettingsCommands));
    [dispatcher startDispatchingToTarget:mock_settings_commands_handler_
                             forProtocol:@protocol(SettingsCommands)];
    CredentialProviderBrowserAgent::CreateForBrowser(browser_.get());
    agent_ = CredentialProviderBrowserAgent::FromBrowser(browser_.get());
    agent_->SetInfobarAllowed(true);
    model_ = static_cast<webauthn::TestPasskeyModel*>(
        IOSPasskeyModelFactory::GetForProfile(
            browser_->GetProfile()->GetOriginalProfile()));
  }

  web::FakeWebState* AppendNewWebState(const GURL& url,
                                       bool activate = true,
                                       bool is_visible = true) {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state->SetCurrentURL(url);
    // Create a navigation item to match the URL and give it a title.
    std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
    item->SetURL(url);
    item->SetTitle(u"Page title");
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->SetLastCommittedItem(item.get());
    // Test nav manager doesn't own its items, so move `item` into the storage
    // vector to define its lifetime.
    navigation_items_.push_back(std::move(item));
    fake_web_state->SetNavigationManager(std::move(navigation_manager));

    // Capture a pointer to the created web state to return.
    web::FakeWebState* inserted_web_state = fake_web_state.get();
    InfoBarManagerImpl::CreateForWebState(inserted_web_state);
    browser_->GetWebStateList()->InsertWebState(
        std::move(fake_web_state),
        WebStateList::InsertionParams::Automatic().Activate(activate));

    if (is_visible) {
      inserted_web_state->WasShown();
    }

    return inserted_web_state;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  id<SettingsCommands> mock_settings_commands_handler_;
  raw_ptr<CredentialProviderBrowserAgent> agent_;
  raw_ptr<webauthn::TestPasskeyModel> model_;
  // Storage vector for navigation items created for test cases.
  std::vector<std::unique_ptr<web::NavigationItem>> navigation_items_;
};

TEST_F(CredentialProviderBrowserAgentTest, TestAddPasskey) {
  if (!syncer::IsWebauthnCredentialSyncEnabled()) {
    GTEST_SKIP() << "This build configuration does not support passkeys.";
  }

  SetUpBrowserAgent(/*incognito=*/false);

  web::WebState* web_state = AppendNewWebState(GURL("http://www.blank.com"));
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  EXPECT_EQ(0U, infobar_manager->infobars().size());

  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_creation_time(base::Time::Now().InMillisecondsSinceUnixEpoch());
  model_->AddNewPasskeyForTesting(passkey);

  // An infobar for the entry should have been added.
  EXPECT_EQ(1U, infobar_manager->infobars().size());
}

TEST_F(CredentialProviderBrowserAgentTest, TestAddPasskeyIncognito) {
  if (!syncer::IsWebauthnCredentialSyncEnabled()) {
    GTEST_SKIP() << "This build configuration does not support passkeys.";
  }

  SetUpBrowserAgent(/*incognito=*/true);

  web::WebState* web_state = AppendNewWebState(GURL("http://www.blank.com"));
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  EXPECT_EQ(0U, infobar_manager->infobars().size());

  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_creation_time(base::Time::Now().InMillisecondsSinceUnixEpoch());
  model_->AddNewPasskeyForTesting(passkey);

  // An infobar for the entry should have been added.
  EXPECT_EQ(1U, infobar_manager->infobars().size());
}
