// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/ui_bundled/browser_modal_host.h"

#import <PassKit/PassKit.h>
#import <UIKit/UIKit.h>

#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/send_tab_to_self/metrics_util.h"
#import "ios/chrome/browser/authentication/signin/non_modal_promo/coordinator/non_modal_signin_promo_coordinator.h"
#import "ios/chrome/browser/authentication/signin/non_modal_promo/coordinator/non_modal_signin_promo_types.h"
#import "ios/chrome/browser/download/coordinator/download_list_coordinator.h"
#import "ios/chrome/browser/download/coordinator/pass_kit_coordinator.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/save_to_photos/ui_bundled/save_to_photos_coordinator.h"
#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_ui_provider.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_iph_commands.h"
#import "ios/chrome/browser/shared/public/commands/download_list_commands.h"
#import "ios/chrome/browser/shared/public/commands/non_modal_signin_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_image_to_photos_command.h"
#import "ios/chrome/browser/shared/public/commands/save_to_photos_commands.h"
#import "ios/chrome/browser/shared/public/commands/send_tab_to_self_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/web_content_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_coordinator.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_params.h"
#import "ios/chrome/browser/store_kit/model/store_kit_coordinator.h"
#import "ios/chrome/browser/store_kit/model/store_kit_coordinator_delegate.h"
#import "ios/chrome/browser/tab_picker/coordinator/tab_picker_coordinator.h"
#import "ios/chrome/browser/tab_picker/public/tab_picker_snackbar_presenter.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"
#import "url/origin.h"

// Test fixture for BrowserModalHost testing.
class BrowserModalHostTest : public PlatformTest {
 protected:
  BrowserModalHostTest() {
    profile_ = TestProfileIOS::Builder().Build();
    scene_state_ = [[SceneState alloc] init];
    LayoutGuideSceneAgent* layout_guide_scene_agent =
        [[LayoutGuideSceneAgent alloc] init];
    [scene_state_ addAgent:layout_guide_scene_agent];
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    base_view_controller_ = [[UIViewController alloc] init];
    modal_host_ = [[BrowserModalHost alloc] initWithBrowser:browser_.get()];
    [modal_host_ setBaseViewControllerForModals:base_view_controller_];
    [modal_host_ startHostingCommandProtocols];
  }

  ~BrowserModalHostTest() override {
    [modal_host_ stopHostingCommandProtocols];
  }

  void InsertWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
  }

  web::WebState* GetActiveWebState() {
    return browser_->GetWebStateList()->GetActiveWebState();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  SceneState* scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
  BrowserModalHost* modal_host_;
};

// Tests that BrowserModalHost starts and stops the SaveToPhotosCoordinator
// properly when SaveToPhotosCommands are issued.
TEST_F(BrowserModalHostTest, StartsAndStopsSaveToPhotosCoordinator) {
  // Mock the SaveToPhotosCoordinator class.
  id mockSaveToPhotosCoordinator =
      OCMStrictClassMock([SaveToPhotosCoordinator class]);

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<SaveToPhotosCommands> handler =
      HandlerForProtocol(dispatcher, SaveToPhotosCommands);

  // Insert a web state into the Browser.
  InsertWebState();

  GURL fakeImageURL("http://www.example.com/image.jpg");
  web::Referrer fakeImageReferrer;
  web::WebState* webState = GetActiveWebState();
  SaveImageToPhotosCommand* command = [[SaveImageToPhotosCommand alloc]
      initWithImageURL:fakeImageURL
              referrer:fakeImageReferrer
              webState:webState
               frameID:"fake_frame_id"
           frameOrigin:url::Origin::Create(GURL("http://chromium.test/"))];

  // Tests that -[BrowserModalHost saveImageToPhotos:] starts the
  // SaveToPhotosCoordinator.
  OCMExpect([mockSaveToPhotosCoordinator alloc])
      .andReturn(mockSaveToPhotosCoordinator);
  OCMExpect([[mockSaveToPhotosCoordinator ignoringNonObjectArgs]
                initWithBaseViewController:base_view_controller_
                                   browser:browser_.get()
                                  imageURL:command.imageURL
                                  referrer:command.referrer
                                  webState:command.webState.get()
                                   frameID:command.frameID
                               frameOrigin:command.frameOrigin])
      .andReturn(mockSaveToPhotosCoordinator);
  OCMExpect([(SaveToPhotosCoordinator*)mockSaveToPhotosCoordinator start]);
  [handler saveImageToPhotos:command];
  EXPECT_OCMOCK_VERIFY(mockSaveToPhotosCoordinator);

  // Tests that -[BrowserModalHost stopSaveToPhotos:] stops the
  // SaveToPhotosCoordinator.
  OCMExpect([mockSaveToPhotosCoordinator stop]);
  [handler stopSaveToPhotos];
  EXPECT_OCMOCK_VERIFY(mockSaveToPhotosCoordinator);
}

// Tests that `-showShareSheet` is leaving fullscreen and starting the share
// coordinator.
TEST_F(BrowserModalHostTest, ShowShareSheet) {
  TestFullscreenController::CreateForBrowser(browser_.get());
  TestFullscreenController* controller =
      TestFullscreenController::FromBrowser(browser_.get());

  controller->EnterFullscreen();
  ASSERT_EQ(0.0, controller->GetProgress());

  UIView* source = [[UIView alloc] init];

  id classMock = OCMClassMock([SharingCoordinator class]);
  SharingCoordinator* mockSharingCoordinator = classMock;
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                initWithBaseViewController:[OCMArg any]
                                   browser:browser_.get()
                                    params:[OCMArg any]
                                sourceItem:source])
      .andReturn(mockSharingCoordinator);
  OCMExpect([mockSharingCoordinator start]);

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<ActivityServiceCommands> handler =
      HandlerForProtocol(dispatcher, ActivityServiceCommands);

  [handler showShareSheetFromShareButton:source];

  // Check that fullscreen is exited.
  EXPECT_EQ(1.0, controller->GetProgress());

  // Check that -start has been called.
  EXPECT_OCMOCK_VERIFY(classMock);
}

// Tests that `-showShareSheetForChromeApp` is instantiating the
// SharingCoordinator with SharingParams where scenario is ShareChrome, leaving
// fullscreen and starting the share coordinator.
TEST_F(BrowserModalHostTest, ShowShareSheetForChromeApp) {
  TestFullscreenController::CreateForBrowser(browser_.get());
  TestFullscreenController* controller =
      TestFullscreenController::FromBrowser(browser_.get());

  controller->EnterFullscreen();
  ASSERT_EQ(0.0, controller->GetProgress());

  id expectShareChromeScenarioArg =
      [OCMArg checkWithBlock:^BOOL(SharingParams* params) {
        return params.scenario == SharingScenario::ShareChrome;
      }];

  id classMock = OCMClassMock([SharingCoordinator class]);
  SharingCoordinator* mockSharingCoordinator = classMock;
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                initWithBaseViewController:[OCMArg any]
                                   browser:browser_.get()
                                    params:expectShareChromeScenarioArg
                                sourceItem:[OCMArg any]])
      .andReturn(mockSharingCoordinator);
  OCMExpect([mockSharingCoordinator start]);

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<ActivityServiceCommands> handler =
      HandlerForProtocol(dispatcher, ActivityServiceCommands);

  [handler showShareSheetForChromeApp];

  // Check that fullscreen is exited.
  EXPECT_EQ(1.0, controller->GetProgress());

  // Check that -start has been called.
  EXPECT_OCMOCK_VERIFY(classMock);
}

// Tests that `-showDownloadList` starts the DownloadListCoordinator.
TEST_F(BrowserModalHostTest, ShowDownloadList) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kDownloadList);

  id classMock = OCMClassMock([DownloadListCoordinator class]);
  DownloadListCoordinator* mockCoordinator = classMock;
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                initWithBaseViewController:base_view_controller_
                                   browser:browser_.get()])
      .andReturn(mockCoordinator);
  OCMExpect([mockCoordinator start]);

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<DownloadListCommands> handler =
      HandlerForProtocol(dispatcher, DownloadListCommands);

  [handler showDownloadList];

  EXPECT_OCMOCK_VERIFY(classMock);
}

// Tests that `-sendTabToSelfToDeviceWithURL:...` starts the
// SendTabToSelfCoordinator using the default base view controller when no scene
// UI provider is available.
TEST_F(BrowserModalHostTest,
       SendTabToSelfStartsCoordinatorWithDefaultBaseViewController) {
  id classMock = OCMClassMock([SendTabToSelfCoordinator class]);
  SendTabToSelfCoordinator* mockCoordinator = classMock;
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                initWithBaseViewController:base_view_controller_
                                   browser:browser_.get()
                                       url:GURL("https://www.example.com")
                                     title:@"Example Title"
                     targetDeviceCacheGUID:@"target_guid"
                          targetDeviceName:@"Target Device"
                                entryPoint:send_tab_to_self::ShareEntryPoint::
                                               kShareSheet])
      .andReturn(mockCoordinator);
  __block base::test::TestFuture<void> start_future;
  OCMExpect([mockCoordinator start]).andDo(^(NSInvocation* invocation) {
    start_future.SetValue();
  });

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<SendTabToSelfCommands> handler =
      HandlerForProtocol(dispatcher, SendTabToSelfCommands);

  [handler sendTabToSelfToDeviceWithURL:GURL("https://www.example.com")
                                  title:@"Example Title"
                               deviceID:@"target_guid"
                             deviceName:@"Target Device"
                             entryPoint:send_tab_to_self::ShareEntryPoint::
                                            kShareSheet];

  EXPECT_TRUE(start_future.Wait());
  EXPECT_OCMOCK_VERIFY(classMock);
}

// Tests that `-sendTabToSelfToDeviceWithURL:...` starts the
// SendTabToSelfCoordinator using the active view controller from the scene UI
// provider when available.
TEST_F(
    BrowserModalHostTest,
    SendTabToSelfStartsCoordinatorWithActiveViewControllerFromSceneUIProvider) {
  UIViewController* activeViewController = [[UIViewController alloc] init];
  id mockSceneUIProvider = OCMProtocolMock(@protocol(SceneUIProvider));
  OCMStub([mockSceneUIProvider activeViewController])
      .andReturn(activeViewController);
  scene_state_.controller = (SceneController*)mockSceneUIProvider;

  id classMock = OCMClassMock([SendTabToSelfCoordinator class]);
  SendTabToSelfCoordinator* mockCoordinator = classMock;
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                initWithBaseViewController:activeViewController
                                   browser:browser_.get()
                                       url:GURL("https://www.example.com")
                                     title:@"Example Title"
                     targetDeviceCacheGUID:@"target_guid"
                          targetDeviceName:@"Target Device"
                                entryPoint:send_tab_to_self::ShareEntryPoint::
                                               kShareSheet])
      .andReturn(mockCoordinator);
  __block base::test::TestFuture<void> start_future;
  OCMExpect([mockCoordinator start]).andDo(^(NSInvocation* invocation) {
    start_future.SetValue();
  });

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<SendTabToSelfCommands> handler =
      HandlerForProtocol(dispatcher, SendTabToSelfCommands);

  [handler sendTabToSelfToDeviceWithURL:GURL("https://www.example.com")
                                  title:@"Example Title"
                               deviceID:@"target_guid"
                             deviceName:@"Target Device"
                             entryPoint:send_tab_to_self::ShareEntryPoint::
                                            kShareSheet];

  EXPECT_TRUE(start_future.Wait());
  EXPECT_OCMOCK_VERIFY(classMock);
}

// Tests that `-showTabPickerWithParams:...` and `-hideTabPicker` correctly
// start and stop the TabPickerCoordinator.
TEST_F(BrowserModalHostTest, StartsAndStopsTabPickerCoordinator) {
  id classMock = OCMClassMock([TabPickerCoordinator class]);
  TabPickerCoordinator* mockCoordinator = classMock;
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                initWithBaseViewController:base_view_controller_
                                   browser:browser_.get()])
      .andReturn(mockCoordinator);
  __block base::test::TestFuture<void> start_future;
  OCMExpect([mockCoordinator start]).andDo(^(NSInvocation* invocation) {
    start_future.SetValue();
  });
  __block base::test::TestFuture<void> stop_future;
  OCMExpect([mockCoordinator stop]).andDo(^(NSInvocation* invocation) {
    stop_future.SetValue();
  });

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<TabPickerCommands> handler =
      HandlerForProtocol(dispatcher, TabPickerCommands);

  id mockPresenter = OCMProtocolMock(@protocol(TabPickerSnackbarPresenter));
  TabPickerParams* params =
      [[TabPickerParams alloc] initWithSnackbarPresenter:mockPresenter];

  [handler showTabPickerWithParams:params completion:nil];

  EXPECT_TRUE(start_future.Wait());

  [handler hideTabPicker];

  EXPECT_TRUE(stop_future.Wait());
  EXPECT_OCMOCK_VERIFY(classMock);
}

// Tests that `-showNonModalSignInPromoWithType:` starts the
// NonModalSignInPromoCoordinator and delegate dismisses it.
TEST_F(BrowserModalHostTest, StartsAndDismissesNonModalSignInPromo) {
  id classMock = OCMClassMock([NonModalSignInPromoCoordinator class]);
  NonModalSignInPromoCoordinator* mockCoordinator = classMock;
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                initWithBaseViewController:base_view_controller_
                                   browser:browser_.get()
                                 promoType:NonModalSignInPromoType::kBookmark])
      .andReturn(mockCoordinator);
  OCMExpect([(NonModalSignInPromoCoordinator*)mockCoordinator
      setDelegate:[OCMArg any]]);
  OCMExpect([mockCoordinator start]);

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<NonModalSignInPromoCommands> handler =
      HandlerForProtocol(dispatcher, NonModalSignInPromoCommands);

  [handler showNonModalSignInPromoWithType:NonModalSignInPromoType::kBookmark];

  EXPECT_OCMOCK_VERIFY(classMock);

  id<NonModalSignInPromoCoordinatorDelegate> delegate =
      (id<NonModalSignInPromoCoordinatorDelegate>)modal_host_;
  OCMExpect([mockCoordinator stop]);
  OCMExpect([(NonModalSignInPromoCoordinator*)mockCoordinator setDelegate:nil]);
  [delegate dismissNonModalSignInPromo:mockCoordinator];

  EXPECT_OCMOCK_VERIFY(classMock);
}

// Tests that `-showAppStoreWithParameters:` clears presented state, starts the
// StoreKitCoordinator, and delegate stops it.
TEST_F(BrowserModalHostTest, StartsAndStopsStoreKitCoordinator) {
  id classMock = OCMClassMock([StoreKitCoordinator class]);
  StoreKitCoordinator* mockCoordinator = classMock;
  NSDictionary* parameters = @{@"id" : @"12345"};
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                initWithBaseViewController:base_view_controller_
                                   browser:browser_.get()])
      .andReturn(mockCoordinator);
  OCMExpect([(StoreKitCoordinator*)mockCoordinator setDelegate:[OCMArg any]]);
  OCMExpect([(StoreKitCoordinator*)mockCoordinator
      setITunesProductParameters:parameters]);
  OCMExpect([mockCoordinator start]);

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id mockBrowserCoordinatorCommandsHandler =
      OCMProtocolMock(@protocol(BrowserCoordinatorCommands));
  [dispatcher startDispatchingToTarget:mockBrowserCoordinatorCommandsHandler
                           forProtocol:@protocol(BrowserCoordinatorCommands)];
  OCMExpect([mockBrowserCoordinatorCommandsHandler
                clearPresentedStateWithCompletion:[OCMArg any]
                                   dismissOmnibox:YES])
      .andDo(^(NSInvocation* invocation) {
        ProceduralBlock completion;
        [invocation getArgument:&completion atIndex:2];
        if (completion) {
          completion();
        }
      });

  id<WebContentCommands> handler =
      HandlerForProtocol(dispatcher, WebContentCommands);

  [handler showAppStoreWithParameters:parameters];

  EXPECT_OCMOCK_VERIFY(mockBrowserCoordinatorCommandsHandler);
  EXPECT_OCMOCK_VERIFY(classMock);

  id<StoreKitCoordinatorDelegate> delegate =
      (id<StoreKitCoordinatorDelegate>)modal_host_;
  OCMExpect([mockCoordinator stop]);
  OCMExpect([(StoreKitCoordinator*)mockCoordinator setDelegate:nil]);
  [delegate storeKitCoordinatorWantsToStop:mockCoordinator];

  EXPECT_OCMOCK_VERIFY(classMock);
}

// Tests that `-showDialogForPassKitPasses:` starts the PassKitCoordinator.
TEST_F(BrowserModalHostTest, StartsPassKitCoordinator) {
  id classMock = OCMClassMock([PassKitCoordinator class]);
  PassKitCoordinator* mockCoordinator = classMock;
  NSArray<PKPass*>* passes = @[];
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                initWithBaseViewController:base_view_controller_
                                   browser:browser_.get()])
      .andReturn(mockCoordinator);
  OCMExpect([(PassKitCoordinator*)mockCoordinator setPasses:passes]);
  OCMExpect([mockCoordinator start]);

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<WebContentCommands> handler =
      HandlerForProtocol(dispatcher, WebContentCommands);

  [handler showDialogForPassKitPasses:passes];

  EXPECT_OCMOCK_VERIFY(classMock);
}

// Tests that `-showDialogForPassKitPasses:` does not start a new coordinator
// if one is already displaying passes.
TEST_F(BrowserModalHostTest,
       DoesNotStartPassKitCoordinatorIfPassesAlreadyDisplayed) {
  id classMock = OCMClassMock([PassKitCoordinator class]);
  PassKitCoordinator* mockCoordinator = classMock;
  NSArray<PKPass*>* passes = @[];
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                initWithBaseViewController:base_view_controller_
                                   browser:browser_.get()])
      .andReturn(mockCoordinator);
  OCMExpect([(PassKitCoordinator*)mockCoordinator setPasses:passes]);
  OCMExpect([mockCoordinator start]);

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<WebContentCommands> handler =
      HandlerForProtocol(dispatcher, WebContentCommands);

  [handler showDialogForPassKitPasses:passes];

  EXPECT_OCMOCK_VERIFY(classMock);

  // When passes are active on the existing coordinator, attempting to show
  // passes again should early return and not start another coordinator.
  OCMStub([mockCoordinator passes]).andReturn(passes);
  [handler showDialogForPassKitPasses:passes];

  EXPECT_OCMOCK_VERIFY(classMock);
}

// Tests that `-dismissContextualPanelEntrypointIPH:` can be invoked without
// crashing.
TEST_F(BrowserModalHostTest, DismissesContextualPanelEntrypointIPH) {
  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<ContextualPanelEntrypointIPHCommands> handler =
      HandlerForProtocol(dispatcher, ContextualPanelEntrypointIPHCommands);
  [handler dismissContextualPanelEntrypointIPH:NO];
}
