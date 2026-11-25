// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/ui_bundled/browser_modal_host.h"

#import <PassKit/PassKit.h>
#import <UIKit/UIKit.h>

#import "base/functional/bind.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/send_tab_to_self/metrics_util.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/test/mock_sync_service.h"
#import "components/test/ios/test_utils.h"
#import "components/trusted_vault/trusted_vault_client.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"
#import "ios/chrome/browser/authentication/signin/non_modal_promo/coordinator/non_modal_signin_promo_coordinator.h"
#import "ios/chrome/browser/authentication/signin/non_modal_promo/coordinator/non_modal_signin_promo_types.h"
#import "ios/chrome/browser/authentication/trusted_vault_reauthentication/coordinator/trusted_vault_reauthentication_coordinator.h"
#import "ios/chrome/browser/authentication/trusted_vault_reauthentication/coordinator/trusted_vault_reauthentication_coordinator_delegate.h"
#import "ios/chrome/browser/default_browser/promo/generic/coordinator/default_browser_generic_promo_coordinator.h"
#import "ios/chrome/browser/default_browser/promo/generic/public/default_browser_generic_promo_commands.h"
#import "ios/chrome/browser/download/coordinator/download_list_coordinator.h"
#import "ios/chrome/browser/download/coordinator/pass_kit_coordinator.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actor_overlay_coordinator.h"
#import "ios/chrome/browser/picture_in_picture/coordinator/picture_in_picture_coordinator.h"
#import "ios/chrome/browser/picture_in_picture/public/picture_in_picture_configuration.h"
#import "ios/chrome/browser/promos_manager/coordinator/promos_manager_coordinator.h"
#import "ios/chrome/browser/save_to_photos/ui_bundled/save_to_photos_coordinator.h"
#import "ios/chrome/browser/send_tab_to_self/coordinator/send_tab_to_self_coordinator.h"
#import "ios/chrome/browser/settings/clear_browsing_data/coordinator/quick_delete_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_ui_provider.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/actor_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_iph_commands.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/download_list_commands.h"
#import "ios/chrome/browser/shared/public/commands/non_modal_signin_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/picture_in_picture_commands.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_image_to_photos_command.h"
#import "ios/chrome/browser/shared/public/commands/save_to_photos_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/send_tab_to_self_commands.h"
#import "ios/chrome/browser/shared/public/commands/sync_presenter_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/web_content_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_coordinator.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_params.h"
#import "ios/chrome/browser/store_kit/model/store_kit_coordinator.h"
#import "ios/chrome/browser/store_kit/model/store_kit_coordinator_delegate.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/tab_picker/coordinator/tab_picker_coordinator.h"
#import "ios/chrome/browser/tab_picker/public/tab_picker_snackbar_presenter.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "testing/platform_test.h"
#import "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"
#import "url/origin.h"

// View controller test double exposing a controllable `transitionCoordinator`
// to simulate an active UIKit presentation or dismissal transition.
@interface FakeTransitioningViewController : UIViewController

// Active transition coordinator returned by `-transitionCoordinator`.
@property(nonatomic) id<UIViewControllerTransitionCoordinator>
    fakeTransitionCoordinator;

// Number of transition completion blocks registered on the active transition.
@property(nonatomic, readonly) NSUInteger registeredCompletionCount;

// Installs a mock `transitionCoordinator` that records completion blocks
// registered via `-animateAlongsideTransition:completion:`.
- (void)beginFakeTransition;

// Clears `fakeTransitionCoordinator` and invokes all recorded completion
// blocks.
- (void)completeFakeTransition;

@end

@implementation FakeTransitioningViewController {
  NSMutableArray<void (^)(id<UIViewControllerTransitionCoordinatorContext>)>*
      _transitionCompletions;
}

- (NSUInteger)registeredCompletionCount {
  return _transitionCompletions.count;
}

- (void)beginFakeTransition {
  _transitionCompletions = [[NSMutableArray alloc] init];
  NSMutableArray<void (^)(id<UIViewControllerTransitionCoordinatorContext>)>*
      completions = _transitionCompletions;
  id mockTransitionCoordinator =
      OCMProtocolMock(@protocol(UIViewControllerTransitionCoordinator));
  OCMStub(
      [mockTransitionCoordinator
          animateAlongsideTransition:nil
                          completion:[OCMArg checkWithBlock:^BOOL(id block) {
                            if (block) {
                              [completions addObject:[block copy]];
                            }
                            return YES;
                          }]])
      .andReturn(YES);
  self.fakeTransitionCoordinator = mockTransitionCoordinator;
}

- (void)completeFakeTransition {
  NSArray<void (^)(id<UIViewControllerTransitionCoordinatorContext>)>*
      completions = [_transitionCompletions copy];
  _transitionCompletions = nil;
  self.fakeTransitionCoordinator = nil;
  for (void (^completion)(id<UIViewControllerTransitionCoordinatorContext>)
           in completions) {
    completion(nil);
  }
}

#pragma mark - UIViewController

- (id<UIViewControllerTransitionCoordinator>)transitionCoordinator {
  return self.fakeTransitionCoordinator;
}

@end

// Test fixture for BrowserModalHost testing.
class BrowserModalHostTest : public PlatformTest {
 protected:
  BrowserModalHostTest() : BrowserModalHostTest(TestProfileIOS::Builder()) {}

  // Creates the fixture with a profile built from `profile_builder`, so that
  // subclasses can install testing factories.
  explicit BrowserModalHostTest(TestProfileIOS::Builder profile_builder) {
    profile_ = std::move(profile_builder).Build();
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

  // Flushes tasks already queued on the UI thread task runner.
  void FlushUITaskRunner() {
    base::test::TestFuture<void> flush_future;
    web::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                             flush_future.GetCallback());
    EXPECT_TRUE(flush_future.Wait());
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

// Tests that `-showAppStoreWithParameters:` does not clear presented state or
// start a new `StoreKitCoordinator` if one is already started.
TEST_F(BrowserModalHostTest, DoesNotStartStoreKitCoordinatorIfAlreadyStarted) {
  id classMock = OCMClassMock([StoreKitCoordinator class]);
  StoreKitCoordinator* mockCoordinator = classMock;
  NSDictionary* parameters = @{@"id" : @"12345"};
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                initWithBaseViewController:base_view_controller_
                                   browser:browser_.get()])
      .andReturn(mockCoordinator);
  OCMExpect([mockCoordinator setDelegate:[OCMArg any]]);
  OCMExpect([mockCoordinator setITunesProductParameters:parameters]);
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
        __unsafe_unretained ProceduralBlock completion;
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

  // Calling `-showAppStoreWithParameters:` again while `StoreKitCoordinator` is
  // already started should early return without clearing presented state or
  // starting another coordinator.
  OCMReject([mockBrowserCoordinatorCommandsHandler
      clearPresentedStateWithCompletion:[OCMArg any]
                         dismissOmnibox:YES]);
  OCMReject([classMock alloc]);

  [handler showAppStoreWithParameters:parameters];

  EXPECT_OCMOCK_VERIFY(mockBrowserCoordinatorCommandsHandler);
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

// Tests that `-showQuickDeleteAndCanPerformRadialWipeAnimation:` starts
// QuickDeleteCoordinator and `-stopQuickDelete` stops it.
TEST_F(BrowserModalHostTest, StartsAndStopsQuickDeleteCoordinator) {
  id classMock = OCMClassMock([QuickDeleteCoordinator class]);
  QuickDeleteCoordinator* mockCoordinator = classMock;
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                   initWithBaseViewController:[OCMArg any]
                                      browser:browser_.get()
                canPerformRadialWipeAnimation:YES])
      .andReturn(mockCoordinator);
  OCMExpect([mockCoordinator start]);

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<QuickDeleteCommands> handler =
      HandlerForProtocol(dispatcher, QuickDeleteCommands);
  [handler showQuickDeleteAndCanPerformRadialWipeAnimation:YES];
  EXPECT_OCMOCK_VERIFY(classMock);

  OCMExpect([mockCoordinator stop]);
  [handler stopQuickDelete];
  EXPECT_OCMOCK_VERIFY(classMock);
}

// Test that `-stopQuickDeleteForAnimationWithCompletion:` does not crash when
// `BrowserCoordinatorCommands` and `SceneCommands` are no longer dispatched
// during UI shutdown, and invokes them when they are dispatched.
TEST_F(BrowserModalHostTest, StopQuickDeleteForAnimationWithCompletion) {
  id mockBaseViewController = OCMPartialMock(base_view_controller_);
  UIViewController* presentedViewController = [[UIViewController alloc] init];
  OCMStub([mockBaseViewController presentedViewController])
      .andReturn(presentedViewController);
  OCMStub([mockBaseViewController
      dismissViewControllerAnimated:YES
                         completion:[OCMArg invokeBlock]]);

  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<QuickDeleteCommands> handler =
      HandlerForProtocol(dispatcher, QuickDeleteCommands);

  // When `BrowserCoordinatorCommands` and `SceneCommands` are not dispatched
  // (e.g. during shutdown), the completion block should still run without
  // crashing.
  __block base::test::TestFuture<void> first_completion;
  [handler stopQuickDeleteForAnimationWithCompletion:^{
    first_completion.SetValue();
  }];
  EXPECT_TRUE(first_completion.Wait());

  // When `BrowserCoordinatorCommands` and `SceneCommands` are dispatched, both
  // handlers should be invoked.
  id mockBrowserCoordinatorCommands =
      OCMProtocolMock(@protocol(BrowserCoordinatorCommands));
  [dispatcher startDispatchingToTarget:mockBrowserCoordinatorCommands
                           forProtocol:@protocol(BrowserCoordinatorCommands)];
  OCMExpect([mockBrowserCoordinatorCommands
      clearPresentedStateWithCompletion:nil
                         dismissOmnibox:YES]);

  id mockSceneCommands = OCMProtocolMock(@protocol(SceneCommands));
  [dispatcher startDispatchingToTarget:mockSceneCommands
                           forProtocol:@protocol(SceneCommands)];
  OCMExpect([mockSceneCommands dismissModalDialogsWithCompletion:nil]);

  __block base::test::TestFuture<void> second_completion;
  [handler stopQuickDeleteForAnimationWithCompletion:^{
    second_completion.SetValue();
  }];
  EXPECT_TRUE(second_completion.Wait());

  EXPECT_OCMOCK_VERIFY(mockBrowserCoordinatorCommands);
  EXPECT_OCMOCK_VERIFY(mockSceneCommands);
}

// Tests that `-showPictureInPictureWithConfig:` starts
// `PictureInPictureCoordinator` synchronously when no transition is active.
TEST_F(BrowserModalHostTest,
       ShowPictureInPictureStartsImmediatelyWithoutActiveTransition) {
  PictureInPictureConfiguration* config =
      [[PictureInPictureConfiguration alloc] init];
  id class_mock = OCMStrictClassMock([PictureInPictureCoordinator class]);
  PictureInPictureCoordinator* mock_coordinator = class_mock;
  // Detaches strict mocks before fixture teardown, even if an `ASSERT_*` exits
  // the test early.
  absl::Cleanup cleanup = [&] {
    OCMStub([mock_coordinator stop]);
    [modal_host_ clearPresentedState];
    [class_mock stopMocking];
  };

  OCMExpect([class_mock alloc]).andReturn(class_mock);
  OCMExpect([class_mock initWithConfiguration:config
                           baseViewController:base_view_controller_
                                      browser:browser_.get()])
      .andReturn(mock_coordinator);
  OCMExpect([mock_coordinator start]);

  id<PictureInPictureCommands> handler = HandlerForProtocol(
      browser_->GetCommandDispatcher(), PictureInPictureCommands);
  [handler showPictureInPictureWithConfig:config];
  EXPECT_OCMOCK_VERIFY(class_mock);

  OCMExpect([mock_coordinator stop]);
  [handler dismissPictureInPicture];
  EXPECT_OCMOCK_VERIFY(class_mock);
}

// Tests that `-showPictureInPictureWithConfig:` waits for an active dismissal
// or presentation transition on the base view controller to finish before
// starting `PictureInPictureCoordinator`.
TEST_F(BrowserModalHostTest,
       ShowPictureInPictureWaitsForActiveTransitionToComplete) {
  FakeTransitioningViewController* transitioning_view_controller =
      [[FakeTransitioningViewController alloc] init];
  [modal_host_ setBaseViewControllerForModals:transitioning_view_controller];
  [transitioning_view_controller beginFakeTransition];

  PictureInPictureConfiguration* config =
      [[PictureInPictureConfiguration alloc] init];
  id class_mock = OCMStrictClassMock([PictureInPictureCoordinator class]);
  PictureInPictureCoordinator* mock_coordinator = class_mock;
  // Detaches strict mocks before fixture teardown, even if an `ASSERT_*` exits
  // the test early.
  absl::Cleanup cleanup = [&] {
    OCMStub([mock_coordinator stop]);
    [modal_host_ clearPresentedState];
    [class_mock stopMocking];
  };

  OCMExpect([class_mock alloc]).andReturn(class_mock);
  OCMExpect([class_mock initWithConfiguration:config
                           baseViewController:transitioning_view_controller
                                      browser:browser_.get()])
      .andReturn(mock_coordinator);

  id<PictureInPictureCommands> handler = HandlerForProtocol(
      browser_->GetCommandDispatcher(), PictureInPictureCommands);
  [handler showPictureInPictureWithConfig:config];
  FlushUITaskRunner();

  EXPECT_OCMOCK_VERIFY(class_mock);
  ASSERT_EQ(1u, transitioning_view_controller.registeredCompletionCount);

  // Completing the transition starts `PictureInPictureCoordinator`.
  __block base::test::TestFuture<void> start_future;
  OCMExpect([mock_coordinator start]).andDo(^(NSInvocation* invocation) {
    start_future.SetValue();
  });
  [transitioning_view_controller completeFakeTransition];

  EXPECT_TRUE(start_future.Wait());
  EXPECT_OCMOCK_VERIFY(class_mock);

  OCMExpect([mock_coordinator stop]);
  [handler dismissPictureInPicture];
  EXPECT_OCMOCK_VERIFY(class_mock);
}

// Tests that `-dismissPictureInPicture` while waiting for an active transition
// cancels the deferred `PictureInPictureCoordinator` `-start`.
TEST_F(BrowserModalHostTest,
       DismissPictureInPictureDuringActiveTransitionCancelsDeferredStart) {
  FakeTransitioningViewController* transitioning_view_controller =
      [[FakeTransitioningViewController alloc] init];
  [modal_host_ setBaseViewControllerForModals:transitioning_view_controller];
  [transitioning_view_controller beginFakeTransition];

  PictureInPictureConfiguration* config =
      [[PictureInPictureConfiguration alloc] init];
  id class_mock = OCMStrictClassMock([PictureInPictureCoordinator class]);
  PictureInPictureCoordinator* mock_coordinator = class_mock;
  // Detaches strict mocks before fixture teardown, even if an `ASSERT_*` exits
  // the test early.
  absl::Cleanup cleanup = [&] {
    OCMStub([mock_coordinator stop]);
    [modal_host_ clearPresentedState];
    [class_mock stopMocking];
  };

  OCMExpect([class_mock alloc]).andReturn(class_mock);
  OCMExpect([class_mock initWithConfiguration:config
                           baseViewController:transitioning_view_controller
                                      browser:browser_.get()])
      .andReturn(mock_coordinator);

  id<PictureInPictureCommands> handler = HandlerForProtocol(
      browser_->GetCommandDispatcher(), PictureInPictureCommands);
  [handler showPictureInPictureWithConfig:config];
  ASSERT_EQ(1u, transitioning_view_controller.registeredCompletionCount);

  OCMExpect([mock_coordinator stop]);
  [handler dismissPictureInPicture];

  [transitioning_view_controller completeFakeTransition];
  FlushUITaskRunner();

  EXPECT_OCMOCK_VERIFY(class_mock);
}

// Tests that calling `-showPictureInPictureWithConfig:` a second time during an
// active transition stops the superseded coordinator and starts only the latest
// coordinator when the transition completes.
TEST_F(BrowserModalHostTest,
       ShowPictureInPictureDuringActiveTransitionSupersedesPendingCoordinator) {
  FakeTransitioningViewController* transitioning_view_controller =
      [[FakeTransitioningViewController alloc] init];
  [modal_host_ setBaseViewControllerForModals:transitioning_view_controller];
  [transitioning_view_controller beginFakeTransition];

  PictureInPictureConfiguration* first_config =
      [[PictureInPictureConfiguration alloc] init];
  PictureInPictureConfiguration* second_config =
      [[PictureInPictureConfiguration alloc] init];
  id first_mock = OCMStrictClassMock([PictureInPictureCoordinator class]);
  id class_mock = OCMStrictClassMock([PictureInPictureCoordinator class]);
  PictureInPictureCoordinator* first_coordinator = first_mock;
  PictureInPictureCoordinator* second_coordinator = class_mock;
  // Detaches strict mocks before fixture teardown, even if an `ASSERT_*` exits
  // the test early.
  absl::Cleanup cleanup = [&] {
    OCMStub([first_coordinator stop]);
    OCMStub([second_coordinator stop]);
    [modal_host_ clearPresentedState];
    [first_mock stopMocking];
    [class_mock stopMocking];
  };

  OCMExpect([class_mock alloc]).andReturn(first_mock);
  OCMExpect([first_mock initWithConfiguration:first_config
                           baseViewController:transitioning_view_controller
                                      browser:browser_.get()])
      .andReturn(first_coordinator);

  id<PictureInPictureCommands> handler = HandlerForProtocol(
      browser_->GetCommandDispatcher(), PictureInPictureCommands);
  [handler showPictureInPictureWithConfig:first_config];
  ASSERT_EQ(1u, transitioning_view_controller.registeredCompletionCount);

  // Second request stops `first_coordinator` and replaces it with
  // `second_coordinator`.
  OCMExpect([first_coordinator stop]);
  OCMExpect([class_mock alloc]).andReturn(class_mock);
  OCMExpect([class_mock initWithConfiguration:second_config
                           baseViewController:transitioning_view_controller
                                      browser:browser_.get()])
      .andReturn(second_coordinator);
  [handler showPictureInPictureWithConfig:second_config];
  ASSERT_EQ(2u, transitioning_view_controller.registeredCompletionCount);
  EXPECT_OCMOCK_VERIFY(first_mock);
  EXPECT_OCMOCK_VERIFY(class_mock);

  // Completing the transition starts only `second_coordinator`.
  __block base::test::TestFuture<void> start_future;
  OCMExpect([second_coordinator start]).andDo(^(NSInvocation* invocation) {
    start_future.SetValue();
  });
  [transitioning_view_controller completeFakeTransition];
  EXPECT_TRUE(start_future.Wait());
  FlushUITaskRunner();

  EXPECT_OCMOCK_VERIFY(first_mock);
  EXPECT_OCMOCK_VERIFY(class_mock);

  OCMExpect([second_coordinator stop]);
  [handler dismissPictureInPicture];
  EXPECT_OCMOCK_VERIFY(class_mock);
}

// Tests that `-dismissCurrentPromo` stops PromosManagerCoordinator.
TEST_F(BrowserModalHostTest, DismissesCurrentPromo) {
  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id mockSceneCommands = OCMProtocolMock(@protocol(SceneCommands));
  [dispatcher startDispatchingToTarget:mockSceneCommands
                           forProtocol:@protocol(SceneCommands)];
  id mockCredentialProviderPromoCommands =
      OCMProtocolMock(@protocol(CredentialProviderPromoCommands));
  [dispatcher
      startDispatchingToTarget:mockCredentialProviderPromoCommands
                   forProtocol:@protocol(CredentialProviderPromoCommands)];

  id classMock = OCMClassMock([PromosManagerCoordinator class]);
  PromosManagerCoordinator* mockCoordinator = classMock;
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                initWithBaseViewController:base_view_controller_
                                   browser:browser_.get()
                              sceneHandler:[OCMArg any]])
      .andReturn(mockCoordinator);
  OCMExpect([mockCoordinator start]);

  id<PromosManagerCommands> handler =
      HandlerForProtocol(dispatcher, PromosManagerCommands);
  [handler showPromo];
  EXPECT_OCMOCK_VERIFY(classMock);

  OCMExpect([mockCoordinator stop]);
  [handler dismissCurrentPromo];
  EXPECT_OCMOCK_VERIFY(classMock);
}

// Tests that `-hidePromo` stops DefaultBrowserGenericPromoCoordinator.
TEST_F(BrowserModalHostTest, HidesDefaultBrowserPromo) {
  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  id<PromosManagerCommands> promosHandler =
      HandlerForProtocol(dispatcher, PromosManagerCommands);
  id<DefaultBrowserGenericPromoCommands> defaultBrowserHandler =
      HandlerForProtocol(dispatcher, DefaultBrowserGenericPromoCommands);

  id classMock = OCMClassMock([DefaultBrowserGenericPromoCoordinator class]);
  DefaultBrowserGenericPromoCoordinator* mockCoordinator = classMock;
  OCMExpect([classMock alloc]).andReturn(classMock);
  OCMExpect([[classMock ignoringNonObjectArgs]
                initWithBaseViewController:base_view_controller_
                                   browser:browser_.get()])
      .andReturn(mockCoordinator);
  OCMExpect([mockCoordinator setPromosUIHandler:[OCMArg any]]);
  OCMExpect([mockCoordinator
      setHandler:static_cast<id<DefaultBrowserGenericPromoCommands>>(
                     modal_host_)]);
  OCMExpect([mockCoordinator start]);

  [promosHandler showDefaultBrowserPromo];
  EXPECT_OCMOCK_VERIFY(classMock);

  OCMExpect([mockCoordinator stop]);
  [defaultBrowserHandler hidePromo];
  EXPECT_OCMOCK_VERIFY(classMock);
}

// Tests that `ActorOverlayCommands` starts `ActorOverlayCoordinator` with the
// initial `scrimVisible` value, updates `scrimVisible` in place without
// restarting the coordinator on subsequent show calls, and stops it on hide.
TEST_F(BrowserModalHostTest, StartsUpdatesAndStopsActorOverlayCoordinator) {
  id class_mock = OCMStrictClassMock([ActorOverlayCoordinator class]);
  [class_mock setExpectationOrderMatters:YES];
  ActorOverlayCoordinator* mock_coordinator = class_mock;
  OCMExpect([class_mock alloc]).andReturn(class_mock);
  OCMExpect([mock_coordinator initWithBaseViewController:base_view_controller_
                                                 browser:browser_.get()])
      .andReturn(mock_coordinator);
  OCMExpect([mock_coordinator setScrimVisible:NO]);
  OCMExpect([mock_coordinator start]);

  id<ActorOverlayCommands> handler = HandlerForProtocol(
      browser_->GetCommandDispatcher(), ActorOverlayCommands);
  [handler showActorOverlayWithoutScrim];
  EXPECT_OCMOCK_VERIFY(class_mock);

  // Calling `-showActorOverlayWithScrim` while already started updates
  // `scrimVisible` in place without re-allocating or re-starting.
  OCMExpect([mock_coordinator setScrimVisible:YES]);
  [handler showActorOverlayWithScrim];
  EXPECT_OCMOCK_VERIFY(class_mock);

  OCMExpect([mock_coordinator stop]);
  [handler hideActorOverlay];
  EXPECT_OCMOCK_VERIFY(class_mock);
}

namespace {

// Returns a profile builder with a `MockSyncService`.
TestProfileIOS::Builder ProfileBuilderWithMockSyncService() {
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(
      SyncServiceFactory::GetInstance(),
      base::BindOnce(&std::make_unique<syncer::MockSyncService>));
  return builder;
}

}  // namespace

// Test fixture for the `SyncPresenterCommands` handled by BrowserModalHost.
class BrowserModalHostSyncPresenterTest : public BrowserModalHostTest {
 protected:
  BrowserModalHostSyncPresenterTest()
      : BrowserModalHostTest(ProfileBuilderWithMockSyncService()) {
    mock_scene_handler_ = OCMProtocolMock(@protocol(SceneCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_scene_handler_
                     forProtocol:@protocol(SceneCommands)];
  }

  // Returns the `SyncPresenterCommands` handler.
  id<SyncPresenterCommands> GetHandler() {
    return HandlerForProtocol(browser_->GetCommandDispatcher(),
                              SyncPresenterCommands);
  }

  id mock_scene_handler_;
};

// Tests that the completion callback for
// showTrustedVaultReauthForFetchKeysWithTrigger is called correctly.
TEST_F(BrowserModalHostSyncPresenterTest, TestTrustedVaultReauthCompletion) {
  trusted_vault::TrustedVaultUserActionTriggerForUMA trigger =
      trusted_vault::TrustedVaultUserActionTriggerForUMA::kSettings;
  TrustedVaultReauthenticationCoordinator* trusted_vault_mock =
      OCMStrictClassMock([TrustedVaultReauthenticationCoordinator class]);
  OCMExpect([((id)trusted_vault_mock) alloc]).andReturn(trusted_vault_mock);
  OCMExpect(
      [trusted_vault_mock
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                              intent:SigninTrustedVaultDialogIntentFetchKeys
                    securityDomainID:trusted_vault::SecurityDomainId::
                                         kChromeSync
                             trigger:trigger])
      .andReturn(trusted_vault_mock);

  __block id<TrustedVaultReauthenticationCoordinatorDelegate> delegate;
  OCMExpect([trusted_vault_mock setDelegate:AssignValueToVariable(delegate)]);
  OCMExpect([trusted_vault_mock start]);

  __block bool completion_was_called = false;
  [GetHandler() showTrustedVaultReauthForFetchKeysWithTrigger:trigger
                                                   completion:^() {
                                                     completion_was_called =
                                                         true;
                                                   }];
  EXPECT_OCMOCK_VERIFY((id)trusted_vault_mock);

  OCMExpect([trusted_vault_mock setDelegate:nil]);
  OCMExpect([trusted_vault_mock stop]);

  [delegate trustedVaultReauthenticationCoordinatorWantsToBeStopped:
                trusted_vault_mock];
  EXPECT_TRUE(completion_was_called);
  EXPECT_OCMOCK_VERIFY((id)trusted_vault_mock);
}

// Tests that a double tap on the trusted vault reauth errors button doesn't
// trigger two openings of the trusted vault reauth coordinator.
TEST_F(BrowserModalHostSyncPresenterTest, TestDoubleTapTrustedVaultReauth) {
  trusted_vault::TrustedVaultUserActionTriggerForUMA trigger =
      trusted_vault::TrustedVaultUserActionTriggerForUMA::kSettings;
  id<SyncPresenterCommands> handler = GetHandler();
  TrustedVaultReauthenticationCoordinator* trusted_vault_mock =
      OCMStrictClassMock([TrustedVaultReauthenticationCoordinator class]);
  OCMExpect([((id)trusted_vault_mock) alloc]).andReturn(trusted_vault_mock);
  OCMExpect(
      [trusted_vault_mock
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                              intent:SigninTrustedVaultDialogIntentFetchKeys
                    securityDomainID:trusted_vault::SecurityDomainId::
                                         kChromeSync
                             trigger:trigger])
      .andReturn(trusted_vault_mock);
  OCMExpect([trusted_vault_mock setDelegate:[OCMArg any]]);
  OCMExpect([trusted_vault_mock start]);
  [handler showTrustedVaultReauthForFetchKeysWithTrigger:trigger
                                              completion:nil];
  EXPECT_OCMOCK_VERIFY((id)trusted_vault_mock);

  // Checks that the second tap is ignored. No more
  // TrustedVaultReauthenticationCoordinator are allocated.
  OCMStub([((id)trusted_vault_mock) alloc]).andDo(^(NSInvocation* invocation) {
    ADD_FAILURE();
  });
  [handler showTrustedVaultReauthForFetchKeysWithTrigger:trigger
                                              completion:nil];
  [handler showTrustedVaultReauthForDegradedRecoverabilityWithTrigger:trigger
                                                           completion:nil];

  OCMExpect([trusted_vault_mock setDelegate:nil]);
  OCMExpect([trusted_vault_mock stop]);
  [modal_host_ clearPresentedState];

  EXPECT_OCMOCK_VERIFY((id)trusted_vault_mock);
}

// Tests that a double tap on the trusted vault degraded recoverability errors
// button doesn't trigger two openings of the trusted vault reauth coordinator.
TEST_F(BrowserModalHostSyncPresenterTest,
       TestDoubleTapTrustedVaultReauthForDegradedRecoverability) {
  trusted_vault::TrustedVaultUserActionTriggerForUMA trigger =
      trusted_vault::TrustedVaultUserActionTriggerForUMA::kSettings;
  id<SyncPresenterCommands> handler = GetHandler();
  TrustedVaultReauthenticationCoordinator* trusted_vault_mock =
      OCMStrictClassMock([TrustedVaultReauthenticationCoordinator class]);
  OCMExpect([((id)trusted_vault_mock) alloc]).andReturn(trusted_vault_mock);
  SigninTrustedVaultDialogIntent intent =
      SigninTrustedVaultDialogIntentDegradedRecoverability;
  OCMExpect([trusted_vault_mock
                initWithBaseViewController:base_view_controller_
                                   browser:browser_.get()
                                    intent:intent
                          securityDomainID:trusted_vault::SecurityDomainId::
                                               kChromeSync
                                   trigger:trigger])
      .andReturn(trusted_vault_mock);
  OCMExpect([trusted_vault_mock setDelegate:[OCMArg any]]);
  OCMExpect([trusted_vault_mock start]);
  [handler showTrustedVaultReauthForDegradedRecoverabilityWithTrigger:trigger
                                                           completion:nil];
  EXPECT_OCMOCK_VERIFY((id)trusted_vault_mock);

  // Checks that the second tap is ignored. No more
  // TrustedVaultReauthenticationCoordinator are allocated.
  OCMStub([((id)trusted_vault_mock) alloc]).andDo(^(NSInvocation* invocation) {
    ADD_FAILURE();
  });
  [handler showTrustedVaultReauthForFetchKeysWithTrigger:trigger
                                              completion:nil];
  [handler showTrustedVaultReauthForDegradedRecoverabilityWithTrigger:trigger
                                                           completion:nil];

  OCMExpect([trusted_vault_mock setDelegate:nil]);
  OCMExpect([trusted_vault_mock stop]);
  [modal_host_ clearPresentedState];

  EXPECT_OCMOCK_VERIFY((id)trusted_vault_mock);
}

// Tests that showBookmarksLimitExceededHelp acknowledges the error and opens
// the help URL.
TEST_F(BrowserModalHostSyncPresenterTest, ShowBookmarksLimitExceededHelp) {
  syncer::MockSyncService* mock_sync_service =
      static_cast<syncer::MockSyncService*>(
          SyncServiceFactory::GetForProfile(profile_.get()));
  EXPECT_CALL(*mock_sync_service,
              AcknowledgeBookmarksLimitExceededError(
                  syncer::SyncService::BookmarksLimitExceededHelpClickedSource::
                      kSyncErrorMessage));
  OCMExpect([mock_scene_handler_ closePresentedViewsAndOpenURL:[OCMArg any]]);

  [GetHandler() showBookmarksLimitExceededHelp];

  EXPECT_OCMOCK_VERIFY(mock_scene_handler_);
}

// Tests that clearing the presented state stops an active trusted vault reauth
// coordinator and runs its completion.
TEST_F(BrowserModalHostSyncPresenterTest,
       ClearPresentedStateStopsTrustedVaultReauth) {
  trusted_vault::TrustedVaultUserActionTriggerForUMA trigger =
      trusted_vault::TrustedVaultUserActionTriggerForUMA::kSettings;
  TrustedVaultReauthenticationCoordinator* trusted_vault_mock =
      OCMStrictClassMock([TrustedVaultReauthenticationCoordinator class]);
  OCMExpect([((id)trusted_vault_mock) alloc]).andReturn(trusted_vault_mock);
  OCMExpect(
      [trusted_vault_mock
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                              intent:SigninTrustedVaultDialogIntentFetchKeys
                    securityDomainID:trusted_vault::SecurityDomainId::
                                         kChromeSync
                             trigger:trigger])
      .andReturn(trusted_vault_mock);
  OCMExpect([trusted_vault_mock setDelegate:[OCMArg any]]);
  OCMExpect([trusted_vault_mock start]);

  __block bool completion_was_called = false;
  [GetHandler() showTrustedVaultReauthForFetchKeysWithTrigger:trigger
                                                   completion:^() {
                                                     completion_was_called =
                                                         true;
                                                   }];
  EXPECT_OCMOCK_VERIFY((id)trusted_vault_mock);
  EXPECT_FALSE(completion_was_called);

  OCMExpect([trusted_vault_mock stop]);
  OCMExpect([trusted_vault_mock setDelegate:nil]);
  [modal_host_ clearPresentedState];

  EXPECT_TRUE(completion_was_called);
  EXPECT_OCMOCK_VERIFY((id)trusted_vault_mock);
}
