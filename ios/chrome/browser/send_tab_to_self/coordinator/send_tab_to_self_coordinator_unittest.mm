// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#import "components/send_tab_to_self/features.h"
#import "components/send_tab_to_self/send_tab_to_self_model.h"
#import "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_coordinator_delegate.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface SendTabToSelfCoordinator (Testing)
@property(nonatomic, assign, readonly) BOOL stopped;
@end

namespace {

constexpr char kTestURL[] = "https://www.example.com/";
constexpr char kTestTitle[] = "Example Page";
constexpr char kTargetDeviceGUID[] = "target_device_guid";
constexpr char kTargetDeviceName[] = "My Target Device";

class SendTabToSelfCoordinatorTest : public PlatformTest {
 protected:
  SendTabToSelfCoordinatorTest() {
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    test_profile_builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    test_profile_builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(&CreateTestSyncService));
    test_profile_builder.AddTestingFactory(
        SendTabToSelfSyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  send_tab_to_self::StubSendTabToSelfSyncService>();
            }));

    profile_ = std::move(test_profile_builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    SendTabToSelfBrowserAgent::CreateForBrowser(browser_.get());
    view_controller_ = [[UIViewController alloc] init];

    mock_delegate_ =
        OCMStrictProtocolMock(@protocol(SendTabToSelfCoordinatorDelegate));
    mock_snackbar_handler_ = OCMStrictProtocolMock(@protocol(SnackbarCommands));
    mock_browser_coordinator_commands_ =
        OCMStrictProtocolMock(@protocol(BrowserCoordinatorCommands));

    // Register mock handlers on dispatcher.
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_snackbar_handler_
                     forProtocol:@protocol(SnackbarCommands)];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_browser_coordinator_commands_
                     forProtocol:@protocol(BrowserCoordinatorCommands)];

    // Set up a fake web state with a committed URL and title.
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    fake_web_state->SetBrowserState(profile_.get());
    fake_web_state->SetCurrentURL(GURL(kTestURL));

    std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
    item->SetURL(GURL(kTestURL));
    item->SetTitle(base::SysNSStringToUTF16(@(kTestTitle)));
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->SetLastCommittedItem(item.get());
    navigation_items_.push_back(std::move(item));
    fake_web_state->SetNavigationManager(std::move(navigation_manager));

    browser_->GetWebStateList()->InsertWebState(
        std::move(fake_web_state),
        WebStateList::InsertionParams::AtIndex(0).Activate());

    model_ = static_cast<send_tab_to_self::FakeSendTabToSelfModel*>(
        SendTabToSelfSyncServiceFactory::GetForProfile(profile_.get())
            ->GetSendTabToSelfModel());
  }

  void TearDown() override {
    if (coordinator_ && !coordinator_.stopped) {
      [coordinator_ stop];
    }
    PlatformTest::TearDown();
  }

  SendTabToSelfCoordinator* CreateCoordinator() {
    coordinator_ = [[SendTabToSelfCoordinator alloc]
        initWithBaseViewController:view_controller_
                           browser:browser_.get()
                   signinPresenter:nil
                               url:GURL(kTestURL)
                             title:@(kTestTitle)
                        entryPoint:send_tab_to_self::ShareEntryPoint::
                                       kShareSheet];
    coordinator_.delegate = mock_delegate_;
    return coordinator_;
  }

  SendTabToSelfCoordinator* CreateDirectSendCoordinator(NSString* cacheGUID) {
    coordinator_ = [[SendTabToSelfCoordinator alloc]
        initWithBaseViewController:view_controller_
                           browser:browser_.get()
                   signinPresenter:nil
                               url:GURL(kTestURL)
                             title:@(kTestTitle)
             targetDeviceCacheGUID:cacheGUID
                  targetDeviceName:@(kTargetDeviceName)
                        entryPoint:send_tab_to_self::ShareEntryPoint::
                                       kShareSheet];
    coordinator_.delegate = mock_delegate_;
    return coordinator_;
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* view_controller_;
  id mock_delegate_;
  id mock_snackbar_handler_;
  id mock_browser_coordinator_commands_;
  std::vector<std::unique_ptr<web::NavigationItem>> navigation_items_;
  raw_ptr<send_tab_to_self::FakeSendTabToSelfModel> model_;
  SendTabToSelfCoordinator* coordinator_;
};

// Tests that initializing the coordinator in direct-send mode and calling start
// successfully sends the tab to the target device, shows a success snackbar,
// and requests the coordinator to be stopped immediately.
TEST_F(SendTabToSelfCoordinatorTest, SendsTabDirectToDeviceSuccessfully) {
  // Enable the post-send toast feature. The direct-send flow always surfaces a
  // success toast, whereas the picker UI only surfaces a toast in error cases.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      send_tab_to_self::kSendTabToSelfPostSendToast);

  CreateDirectSendCoordinator(@(kTargetDeviceGUID));

  ASSERT_NE(nullptr, browser_.get());
  ASSERT_NE(nullptr, browser_->GetProfile());
  ASSERT_NE(nullptr, coordinator_);
  ASSERT_NE(nullptr, coordinator_.browser);
  ASSERT_NE(nullptr, coordinator_.profile);

  // Configure fake model.
  model_->SetSendResult(send_tab_to_self::SendTabToSelfResult::kSuccess);
  model_->AddTargetDevice(send_tab_to_self::TargetDeviceInfo(
      kTargetDeviceName, kTargetDeviceGUID,
      syncer::DeviceInfo::FormFactor::kPhone, base::Time::Now()));

  __block base::test::TestFuture<void> stop_future;
  __block base::test::TestFuture<void> snackbar_future;

  // Expect the delegate to be notified that the coordinator wants to be
  // stopped.
  OCMExpect(
      [mock_delegate_ sendTabToSelfCoordinatorWantsToBeStopped:coordinator_])
      .andDo(^(NSInvocation* invocation) {
        [coordinator_ stop];
        stop_future.SetValue();
      });

  // Expect the success snackbar to be shown.
  OCMExpect(
      [mock_snackbar_handler_
          showSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                          SnackbarMessage* message) {
            return
                [message.title rangeOfString:@(kTargetDeviceName)].location !=
                NSNotFound;
          }]])
      .andDo(^(NSInvocation* invocation) {
        snackbar_future.SetValue();
      });

  // Trigger direct-send via start.
  [coordinator_ start];

  // Wait for both the stop callback and the snackbar presentation to complete
  // asynchronously.
  EXPECT_TRUE(stop_future.Wait());
  EXPECT_TRUE(snackbar_future.Wait());

  EXPECT_OCMOCK_VERIFY(mock_delegate_);
  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler_);
}

// Tests that initializing the coordinator in direct-send mode and calling
// start when there is an error (e.g., no internet connection) shows a
// failure snackbar and requests the coordinator to be stopped immediately.
TEST_F(SendTabToSelfCoordinatorTest,
       SendsTabDirectToDeviceWithConnectionFailure) {
  // Enable the post-send toast feature. The direct-send flow always surfaces a
  // success toast, whereas the picker UI only surfaces a toast in error cases.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      send_tab_to_self::kSendTabToSelfPostSendToast);

  CreateDirectSendCoordinator(@(kTargetDeviceGUID));

  // Configure fake model with connection failure.
  model_->SetSendResult(
      send_tab_to_self::SendTabToSelfResult::kFailureNoInternetConnection);
  model_->AddTargetDevice(send_tab_to_self::TargetDeviceInfo(
      kTargetDeviceName, kTargetDeviceGUID,
      syncer::DeviceInfo::FormFactor::kPhone, base::Time::Now()));

  __block base::test::TestFuture<void> stop_future;
  __block base::test::TestFuture<void> snackbar_future;

  // Expect the delegate to be notified that the coordinator wants to be
  // stopped.
  OCMExpect(
      [mock_delegate_ sendTabToSelfCoordinatorWantsToBeStopped:coordinator_])
      .andDo(^(NSInvocation* invocation) {
        [coordinator_ stop];
        stop_future.SetValue();
      });

  // Expect the connection failure snackbar to be shown.
  OCMExpect(
      [mock_snackbar_handler_
          showSnackbarMessage:[OCMArg checkWithBlock:^BOOL(
                                          SnackbarMessage* message) {
            return
                [message.title rangeOfString:@(kTargetDeviceName)].location ==
                NSNotFound;
          }]])
      .andDo(^(NSInvocation* invocation) {
        snackbar_future.SetValue();
      });

  // Trigger direct-send via start.
  [coordinator_ start];

  // Wait for both the stop callback and the snackbar presentation to complete
  // asynchronously.
  EXPECT_TRUE(stop_future.Wait());
  EXPECT_TRUE(snackbar_future.Wait());

  EXPECT_OCMOCK_VERIFY(mock_delegate_);
  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler_);
}

}  // namespace
