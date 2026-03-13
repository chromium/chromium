// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/coordinator/app_bar_coordinator.h"

#import <memory>

#import "components/open_from_clipboard/fake_clipboard_recent_content.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_container_view_controller.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class AppBarCoordinatorTest : public PlatformTest {
 protected:
  AppBarCoordinatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(&IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(BwgServiceFactory::GetInstance(),
                              BwgServiceFactory::GetDefaultFactory());
    regular_profile_ = std::move(builder).Build();
    incognito_profile_ = TestProfileIOS::Builder().Build();
    regular_browser_ = std::make_unique<TestBrowser>(regular_profile_.get());
    incognito_browser_ =
        std::make_unique<TestBrowser>(incognito_profile_.get());

    TestFullscreenController::CreateForBrowser(regular_browser_.get());
    TestFullscreenController::CreateForBrowser(incognito_browser_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(regular_browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(regular_browser_.get());

    coordinator_ = [[AppBarCoordinator alloc]
        initWithRegularBrowser:regular_browser_.get()
              incognitoBrowser:incognito_browser_.get()];

    ClipboardRecentContent::SetInstance(
        std::make_unique<FakeClipboardRecentContent>());

    scene_handler_ = OCMProtocolMock(@protocol(SceneCommands));
    [regular_browser_->GetCommandDispatcher()
        startDispatchingToTarget:scene_handler_
                     forProtocol:@protocol(SceneCommands)];
    [incognito_browser_->GetCommandDispatcher()
        startDispatchingToTarget:scene_handler_
                     forProtocol:@protocol(SceneCommands)];

    tab_grid_handler_ = OCMProtocolMock(@protocol(TabGridCommands));
    [regular_browser_->GetCommandDispatcher()
        startDispatchingToTarget:tab_grid_handler_
                     forProtocol:@protocol(TabGridCommands)];

    tab_group_handler_ = OCMProtocolMock(@protocol(TabGroupsCommands));
    [regular_browser_->GetCommandDispatcher()
        startDispatchingToTarget:tab_group_handler_
                     forProtocol:@protocol(TabGroupsCommands)];

    bwg_handler_ = OCMProtocolMock(@protocol(BWGCommands));
    [regular_browser_->GetCommandDispatcher()
        startDispatchingToTarget:bwg_handler_
                     forProtocol:@protocol(BWGCommands)];

    settings_handler_ = OCMProtocolMock(@protocol(SettingsCommands));
    [regular_browser_->GetCommandDispatcher()
        startDispatchingToTarget:settings_handler_
                     forProtocol:@protocol(SettingsCommands)];

    browser_coordinator_handler_ =
        OCMProtocolMock(@protocol(BrowserCoordinatorCommands));
    [regular_browser_->GetCommandDispatcher()
        startDispatchingToTarget:browser_coordinator_handler_
                     forProtocol:@protocol(BrowserCoordinatorCommands)];
    [incognito_browser_->GetCommandDispatcher()
        startDispatchingToTarget:browser_coordinator_handler_
                     forProtocol:@protocol(BrowserCoordinatorCommands)];

    qr_scanner_handler_ = OCMProtocolMock(@protocol(QRScannerCommands));
    [regular_browser_->GetCommandDispatcher()
        startDispatchingToTarget:qr_scanner_handler_
                     forProtocol:@protocol(QRScannerCommands)];
    [incognito_browser_->GetCommandDispatcher()
        startDispatchingToTarget:qr_scanner_handler_
                     forProtocol:@protocol(QRScannerCommands)];

    lens_handler_ = OCMProtocolMock(@protocol(LensCommands));
    [regular_browser_->GetCommandDispatcher()
        startDispatchingToTarget:lens_handler_
                     forProtocol:@protocol(LensCommands)];
    [incognito_browser_->GetCommandDispatcher()
        startDispatchingToTarget:lens_handler_
                     forProtocol:@protocol(LensCommands)];
  }

  ~AppBarCoordinatorTest() override { [coordinator_ stop]; }

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> regular_profile_;
  std::unique_ptr<TestProfileIOS> incognito_profile_;
  std::unique_ptr<TestBrowser> regular_browser_;
  std::unique_ptr<TestBrowser> incognito_browser_;
  AppBarCoordinator* coordinator_;
  id scene_handler_;
  id tab_grid_handler_;
  id tab_group_handler_;
  id bwg_handler_;
  id settings_handler_;
  id browser_coordinator_handler_;
  id qr_scanner_handler_;
  id lens_handler_;
};

// Tests that the coordinator creates a view controller when started.
TEST_F(AppBarCoordinatorTest, TestStart) {
  [coordinator_ start];
  EXPECT_TRUE(coordinator_.viewController);
  EXPECT_TRUE([coordinator_.viewController
      isKindOfClass:[AppBarContainerViewController class]]);
}
