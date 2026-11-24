// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_coordinator.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_mediator.h"
#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_block_data_types.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_custom_ui_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_gateway_manager.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_session_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_snackbar_data_types.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/fake_page_context_wrapper.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/fake_page_context_wrapper_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/signin_test_util.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

namespace {

const char kTestURL[] = "https://gemini.google.com/quiz/123";

}  // namespace

@interface GeminiContainerCoordinator (Testing)
- (void)dismissGeminiFromOtherWindowsWithCompletion:
    (base::OnceClosure)completion;
@end

class GeminiContainerCoordinatorTest : public PlatformTest {
 public:
  GeminiContainerCoordinatorTest() {
    feature_list_.InitWithFeatures(
        {kPageActionMenu, kAssistantContainer, kIOSGeminiBottomSheetMigration},
        {});

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindOnce(&CreateTestSyncService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    personal_profile_ =
        profile_manager_.AddProfileWithBuilder(std::move(builder));

    browser_ = std::make_unique<TestBrowser>(personal_profile_);
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(personal_profile_);
    browser_list->AddBrowser(browser_.get());
    GeminiBrowserAgent::CreateForBrowser(browser_.get());

    mock_fullscreen_handler_ = OCMProtocolMock(@protocol(FullscreenCommands));
    mock_gemini_handler_ = OCMProtocolMock(@protocol(GeminiCommands));
    mock_settings_handler_ = OCMProtocolMock(@protocol(SettingsCommands));
    mock_snackbar_handler_ = OCMProtocolMock(@protocol(SnackbarCommands));
    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mock_fullscreen_handler_
                             forProtocol:@protocol(FullscreenCommands)];
    [dispatcher startDispatchingToTarget:mock_gemini_handler_
                             forProtocol:@protocol(GeminiCommands)];
    [dispatcher startDispatchingToTarget:mock_settings_handler_
                             forProtocol:@protocol(SettingsCommands)];
    [dispatcher startDispatchingToTarget:mock_snackbar_handler_
                             forProtocol:@protocol(SnackbarCommands)];

    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
    startup_state_ = [[GeminiStartupState alloc] init];

    coordinator_ = [[GeminiContainerCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
    coordinator_.startupState = startup_state_;
  }

  ~GeminiContainerCoordinatorTest() override {
    [coordinator_ stop];
    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher stopDispatchingForProtocol:@protocol(FullscreenCommands)];
    [dispatcher stopDispatchingForProtocol:@protocol(GeminiCommands)];
    [dispatcher stopDispatchingForProtocol:@protocol(SettingsCommands)];
    [dispatcher stopDispatchingForProtocol:@protocol(SnackbarCommands)];
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(personal_profile_);
    browser_list->RemoveBrowser(browser_.get());
  }

 protected:
  // Returns whether the web modal went away. Dismissal is animated, so it is
  // not observable synchronously.
  bool WaitForWebModalDismissal() {
    return base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForUIElementTimeout, ^{
          return base_view_controller_.presentedViewController == nil;
        });
  }

  // Returns a block node carrying a start-task confirmation card.
  FakeGeminiBlockNode* MakeConfirmationCardBlock() {
    FakeGeminiBlockNode* block = [[FakeGeminiBlockNode alloc] init];
    block.browserActuationConfirmationCard =
        [[FakeGeminiBrowserActuationConfirmationCard alloc] init];
    return block;
  }

  // Returns the custom UI handler of the container mediator the coordinator
  // showed the container with.
  GeminiCustomUIHandler* CustomUIHandlerOf(
      id<AssistantContainerDelegate> delegate) {
    return base::apple::ObjCCastStrict<GeminiContainerMediator>(delegate)
        .gatewayManager.customUIHandler;
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> personal_profile_ = nullptr;
  std::unique_ptr<TestBrowser> browser_;
  id mock_fullscreen_handler_;
  id mock_gemini_handler_;
  id mock_settings_handler_;
  ScopedKeyWindow scoped_key_window_;
  id mock_snackbar_handler_;
  UIViewController* base_view_controller_;
  GeminiStartupState* startup_state_;
  GeminiContainerCoordinator* coordinator_;
};

// Test that `dismissGeminiFromOtherWindowsWithCompletion:` executes completion
// when no other browsers exist.
TEST_F(GeminiContainerCoordinatorTest,
       TestDismissGeminiFromOtherWindowsWhenNoOtherBrowsers) {
  base::test::TestFuture<void> future;
  [coordinator_
      dismissGeminiFromOtherWindowsWithCompletion:future.GetCallback()];
  EXPECT_TRUE(future.Wait());
}

// Test that `dismissGeminiFromOtherWindowsWithCompletion:` dismisses Gemini in
// other browsers.
TEST_F(GeminiContainerCoordinatorTest, TestDismissGeminiFromOtherWindows) {
  TestProfileIOS* managed_profile = AddManagedProfileWithBuilder(
      profile_manager_, [FakeSystemIdentity fakeManagedIdentity]);

  // Emulate opening a new window.
  std::unique_ptr<TestBrowser> second_browser =
      std::make_unique<TestBrowser>(managed_profile);
  BrowserList* second_browser_list =
      BrowserListFactory::GetForProfile(managed_profile);
  second_browser_list->AddBrowser(second_browser.get());

  id mock_second_handler = OCMProtocolMock(@protocol(GeminiCommands));
  [second_browser->GetCommandDispatcher()
      startDispatchingToTarget:mock_second_handler
                   forProtocol:@protocol(GeminiCommands)];

  [[mock_second_handler expect]
      dismissGeminiFlowWithCompletion:[OCMArg checkWithBlock:^BOOL(
                                                  ProceduralBlock block) {
        CHECK(block);
        block();
        return YES;
      }]];

  base::test::TestFuture<void> future;
  [coordinator_
      dismissGeminiFromOtherWindowsWithCompletion:future.GetCallback()];
  EXPECT_TRUE(future.Wait());
  EXPECT_OCMOCK_VERIFY(mock_second_handler);

  [second_browser->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(GeminiCommands)];
  second_browser_list->RemoveBrowser(second_browser.get());
}

// Tests that `start` waits for `dismissGeminiFromOtherWindowsWithCompletion:`
// to finish before presenting the assistant container.
TEST_F(GeminiContainerCoordinatorTest,
       TestStartWaitsForDismissalFromOtherWindowsBeforePresenting) {
  id mock_container_handler =
      OCMProtocolMock(@protocol(AssistantContainerCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_container_handler
                   forProtocol:@protocol(AssistantContainerCommands)];

  TestProfileIOS* managed_profile = AddManagedProfileWithBuilder(
      profile_manager_, [FakeSystemIdentity fakeManagedIdentity]);

  std::unique_ptr<TestBrowser> second_browser =
      std::make_unique<TestBrowser>(managed_profile);
  BrowserList* second_browser_list =
      BrowserListFactory::GetForProfile(managed_profile);
  second_browser_list->AddBrowser(second_browser.get());

  id mock_second_gemini_handler = OCMProtocolMock(@protocol(GeminiCommands));
  [second_browser->GetCommandDispatcher()
      startDispatchingToTarget:mock_second_gemini_handler
                   forProtocol:@protocol(GeminiCommands)];

  __block ProceduralBlock captured_dismiss_completion = nil;
  [[mock_second_gemini_handler expect]
      dismissGeminiFlowWithCompletion:[OCMArg checkWithBlock:^BOOL(
                                                  ProceduralBlock block) {
        captured_dismiss_completion = [block copy];
        return YES;
      }]];

  __block BOOL container_presented = NO;
  OCMStub([mock_container_handler
              showAssistantContainerWithContent:[OCMArg any]
                                       delegate:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        container_presented = YES;
      });

  GeminiBrowserAgent* agent = GeminiBrowserAgent::FromBrowser(browser_.get());
  ASSERT_TRUE(agent);
  EXPECT_FALSE(agent->is_floaty_invoked());

  [coordinator_ start];

  EXPECT_OCMOCK_VERIFY(mock_second_gemini_handler);
  EXPECT_FALSE(container_presented);
  EXPECT_FALSE(agent->is_floaty_invoked());
  ASSERT_TRUE(captured_dismiss_completion != nil);

  captured_dismiss_completion();

  EXPECT_TRUE(container_presented);
  EXPECT_TRUE(agent->is_floaty_invoked());

  [coordinator_ stop];
  EXPECT_FALSE(agent->is_floaty_invoked());

  [second_browser->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(GeminiCommands)];
  [browser_->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(AssistantContainerCommands)];
  second_browser_list->RemoveBrowser(second_browser.get());
}

// Tests that `start` with an auto-submit prompt waits for page context
// generation before dismissing Gemini from other windows and presenting the
// assistant container.
TEST_F(GeminiContainerCoordinatorTest,
       TestStartWithAutoSubmitDefersDismissalFromOtherWindowsAndPresentation) {
  FakePageContextWrapperFactory page_context_wrapper_factory;
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(personal_profile_);
  web_state->SetCurrentURL(GURL("https://example.com"));
  web_state->SetContentsMimeType("text/html");
  web_state->WasShown();
  web_state->SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  GeminiTabHelper::CreateForWebState(web_state.get(),
                                     &page_context_wrapper_factory);
  WebViewProxyTabHelper::CreateForWebState(web_state.get());
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate(true));

  id mock_container_handler =
      OCMProtocolMock(@protocol(AssistantContainerCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_container_handler
                   forProtocol:@protocol(AssistantContainerCommands)];

  TestProfileIOS* managed_profile = AddManagedProfileWithBuilder(
      profile_manager_, [FakeSystemIdentity fakeManagedIdentity]);
  std::unique_ptr<TestBrowser> second_browser =
      std::make_unique<TestBrowser>(managed_profile);
  BrowserList* second_browser_list =
      BrowserListFactory::GetForProfile(managed_profile);
  second_browser_list->AddBrowser(second_browser.get());

  id mock_second_gemini_handler = OCMProtocolMock(@protocol(GeminiCommands));
  [second_browser->GetCommandDispatcher()
      startDispatchingToTarget:mock_second_gemini_handler
                   forProtocol:@protocol(GeminiCommands)];

  __block BOOL other_window_dismissed = NO;
  OCMStub([mock_second_gemini_handler
      dismissGeminiFlowWithCompletion:[OCMArg checkWithBlock:^BOOL(
                                                  ProceduralBlock completion) {
        other_window_dismissed = YES;
        CHECK(completion);
        completion();
        return YES;
      }]]);

  __block BOOL container_presented = NO;
  OCMStub([mock_container_handler
              showAssistantContainerWithContent:[OCMArg any]
                                       delegate:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        container_presented = YES;
      });

  startup_state_.prepopulatedPrompt = @"Explain this page";
  startup_state_.shouldAutoSubmit = YES;

  GeminiBrowserAgent* agent = GeminiBrowserAgent::FromBrowser(browser_.get());
  ASSERT_TRUE(agent);

  [coordinator_ start];

  // Neither multi-window dismissal nor container presentation occurs while
  // waiting for page context generation.
  EXPECT_FALSE(other_window_dismissed);
  EXPECT_FALSE(container_presented);
  EXPECT_FALSE(agent->is_floaty_invoked());

  FakePageContextWrapper* wrapper = page_context_wrapper_factory.last_wrapper();
  ASSERT_TRUE(wrapper);
  [wrapper completeWithEmptyPageContext];

  EXPECT_TRUE(other_window_dismissed);
  EXPECT_TRUE(container_presented);
  EXPECT_TRUE(agent->is_floaty_invoked());

  [coordinator_ stop];
  [second_browser->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(GeminiCommands)];
  [browser_->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(AssistantContainerCommands)];
  second_browser_list->RemoveBrowser(second_browser.get());
}

// Tests that `start` updates the existing container instead of presenting a new
// one when the container is already invoked.
TEST_F(GeminiContainerCoordinatorTest, TestStartUpdatesWhenAlreadyInvoked) {
  id mock_container_handler =
      OCMProtocolMock(@protocol(AssistantContainerCommands));
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_container_handler
                   forProtocol:@protocol(AssistantContainerCommands)];

  OCMExpect([mock_container_handler
      showAssistantContainerWithContent:[OCMArg any]
                               delegate:[OCMArg any]]);

  [coordinator_ start];
  EXPECT_OCMOCK_VERIFY(mock_container_handler);

  GeminiBrowserAgent* agent = GeminiBrowserAgent::FromBrowser(browser_.get());
  ASSERT_TRUE(agent);
  EXPECT_TRUE(agent->is_floaty_invoked());

  // Second call while already invoked should update the container (animating to
  // medium detent) without calling
  // `showAssistantContainerWithContent:delegate:` again.
  [[mock_container_handler reject]
      showAssistantContainerWithContent:[OCMArg any]
                               delegate:[OCMArg any]];
  OCMExpect([mock_container_handler
      animateAssistantContainerToDetent:AssistantContainerDetent::kMedium]);

  GeminiStartupState* second_startup_state =
      [[GeminiStartupState alloc] initWithEntryPoint:gemini::EntryPoint::AIHub];
  coordinator_.startupState = second_startup_state;
  [coordinator_ start];
  EXPECT_OCMOCK_VERIFY(mock_container_handler);

  [browser_->GetCommandDispatcher()
      stopDispatchingForProtocol:@protocol(AssistantContainerCommands)];
}

// Tests that `start` wires `FullscreenCommands` when `kFullscreenRefactoring`
// is enabled.
TEST_F(GeminiContainerCoordinatorTest,
       TestStartWiresFullscreenHandlerWhenRefactoringEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kFullscreenRefactoring);

  id mock_container_handler =
      OCMProtocolMock(@protocol(AssistantContainerCommands));
  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  [dispatcher startDispatchingToTarget:mock_container_handler
                           forProtocol:@protocol(AssistantContainerCommands)];

  __block id<AssistantContainerDelegate> captured_delegate = nil;
  OCMExpect([mock_container_handler
      showAssistantContainerWithContent:[OCMArg any]
                               delegate:[OCMArg
                                            checkWithBlock:^BOOL(
                                                id<AssistantContainerDelegate>
                                                    delegate) {
                                              captured_delegate = delegate;
                                              return YES;
                                            }]]);

  [coordinator_ start];
  EXPECT_OCMOCK_VERIFY(mock_container_handler);

  GeminiContainerMediator* mediator =
      base::apple::ObjCCastStrict<GeminiContainerMediator>(captured_delegate);
  ASSERT_NE(mediator.fullscreenHandler, nil);
  OCMExpect([mock_fullscreen_handler_ exitForceFullscreen]);
  [mediator.fullscreenHandler exitForceFullscreen];
  EXPECT_OCMOCK_VERIFY(mock_fullscreen_handler_);

  [coordinator_ stop];
  [dispatcher stopDispatchingForProtocol:@protocol(AssistantContainerCommands)];
}

// Tests that `start` wires `SnackbarCommands` to the session handler so SDK
// snackbar requests are forwarded in the bottom sheet flow.
TEST_F(GeminiContainerCoordinatorTest, TestStartWiresSessionSnackbarHandler) {
  id mock_container_handler =
      OCMProtocolMock(@protocol(AssistantContainerCommands));
  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  [dispatcher startDispatchingToTarget:mock_container_handler
                           forProtocol:@protocol(AssistantContainerCommands)];

  __block id<AssistantContainerDelegate> captured_delegate = nil;
  OCMExpect([mock_container_handler
      showAssistantContainerWithContent:[OCMArg any]
                               delegate:[OCMArg
                                            checkWithBlock:^BOOL(
                                                id<AssistantContainerDelegate>
                                                    delegate) {
                                              captured_delegate = delegate;
                                              return YES;
                                            }]]);

  [coordinator_ start];
  EXPECT_OCMOCK_VERIFY(mock_container_handler);

  GeminiContainerMediator* mediator =
      base::apple::ObjCCastStrict<GeminiContainerMediator>(captured_delegate);
  GeminiSessionHandler* session_handler =
      mediator.gatewayManager.sessionHandler;
  ASSERT_NE(session_handler.snackbarHandler, nil);

  OCMExpect([mock_snackbar_handler_
      showSnackbarMessage:[OCMArg checkWithBlock:^BOOL(SnackbarMessage* msg) {
        return [msg.title isEqualToString:@"Response copied"];
      }]]);
  GeminiSnackbar* snackbar =
      [[GeminiSnackbar alloc] initWithType:GeminiSnackbarType::kResponseCopied
                                   message:@"Response copied"];
  [session_handler showSnackbar:snackbar];
  EXPECT_OCMOCK_VERIFY(mock_snackbar_handler_);

  [coordinator_ stop];
  [dispatcher stopDispatchingForProtocol:@protocol(AssistantContainerCommands)];
}

// Tests that, with the actor enabled, `start` registers a provider for the
// start-task confirmation card before the container is shown, and that the
// card is no longer claimed after `stop`.
TEST_F(GeminiContainerCoordinatorTest,
       TestStartRegistersConfirmationCardProviderWhenActorEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kActorTools, kGeminiActor}, {});

  id mock_container_handler =
      OCMProtocolMock(@protocol(AssistantContainerCommands));
  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  [dispatcher startDispatchingToTarget:mock_container_handler
                           forProtocol:@protocol(AssistantContainerCommands)];

  FakeGeminiBlockNode* card_block = MakeConfirmationCardBlock();
  __block GeminiCustomUIHandler* handler = nil;
  __block BOOL card_claimed_when_shown = NO;
  OCMExpect([mock_container_handler
      showAssistantContainerWithContent:[OCMArg any]
                               delegate:
                                   [OCMArg checkWithBlock:^BOOL(
                                               id<AssistantContainerDelegate>
                                                   delegate) {
                                     handler = CustomUIHandlerOf(delegate);
                                     card_claimed_when_shown = [handler
                                         providesCustomContentViewForBlockNode:
                                             card_block];
                                     return YES;
                                   }]]);

  [coordinator_ start];
  EXPECT_OCMOCK_VERIFY(mock_container_handler);
  ASSERT_TRUE(handler);
  EXPECT_TRUE(card_claimed_when_shown);
  EXPECT_TRUE([handler providesCustomContentViewForBlockNode:card_block]);

  [coordinator_ stop];
  EXPECT_FALSE([handler providesCustomContentViewForBlockNode:card_block]);
  [dispatcher stopDispatchingForProtocol:@protocol(AssistantContainerCommands)];
}

// Tests that, with the actor disabled, the start-task confirmation card is
// not claimed.
TEST_F(GeminiContainerCoordinatorTest,
       TestStartDoesNotRegisterConfirmationCardProviderWhenActorDisabled) {
  id mock_container_handler =
      OCMProtocolMock(@protocol(AssistantContainerCommands));
  CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
  [dispatcher startDispatchingToTarget:mock_container_handler
                           forProtocol:@protocol(AssistantContainerCommands)];

  __block GeminiCustomUIHandler* handler = nil;
  OCMExpect([mock_container_handler
      showAssistantContainerWithContent:[OCMArg any]
                               delegate:[OCMArg
                                            checkWithBlock:^BOOL(
                                                id<AssistantContainerDelegate>
                                                    delegate) {
                                              handler =
                                                  CustomUIHandlerOf(delegate);
                                              return YES;
                                            }]]);

  [coordinator_ start];
  EXPECT_OCMOCK_VERIFY(mock_container_handler);
  ASSERT_TRUE(handler);
  EXPECT_FALSE([handler
      providesCustomContentViewForBlockNode:MakeConfirmationCardBlock()]);

  [coordinator_ stop];
  [dispatcher stopDispatchingForProtocol:@protocol(AssistantContainerCommands)];
}

// Tests that calling `showWebModalForURL:` while a web modal is already shown
// keeps the existing modal instead of presenting a second one.
TEST_F(GeminiContainerCoordinatorTest,
       TestShowWebModalTwiceReusesExistingModal) {
  ASSERT_EQ(nil, base_view_controller_.presentedViewController);

  [coordinator_ showWebModalForURL:GURL(kTestURL)];
  UIViewController* first_modal = base_view_controller_.presentedViewController;
  ASSERT_TRUE(first_modal);

  [coordinator_ showWebModalForURL:GURL(kTestURL)];
  EXPECT_EQ(first_modal, base_view_controller_.presentedViewController);
}

// Tests that minimizing the container dismisses the web modal, since the modal
// is presented over the container and would otherwise outlive it on screen.
TEST_F(GeminiContainerCoordinatorTest, TestMinimizeDismissesWebModal) {
  [coordinator_ showWebModalForURL:GURL(kTestURL)];
  ASSERT_TRUE(base_view_controller_.presentedViewController);

  [coordinator_ minimize];

  EXPECT_TRUE(WaitForWebModalDismissal());
}

// Tests that once the user dismisses the web modal, the container releases it
// so that a later `showWebModalForURL:` presents a fresh modal.
TEST_F(GeminiContainerCoordinatorTest, TestWebModalCanBeReshownAfterDismiss) {
  [coordinator_ showWebModalForURL:GURL(kTestURL)];
  UIViewController* first_modal = base_view_controller_.presentedViewController;
  ASSERT_TRUE(first_modal);

  UIPresentationController* presentation_controller =
      first_modal.presentationController;
  [presentation_controller.delegate
      presentationControllerDidDismiss:presentation_controller];
  ASSERT_TRUE(WaitForWebModalDismissal());

  [coordinator_ showWebModalForURL:GURL(kTestURL)];
  UIViewController* second_modal =
      base_view_controller_.presentedViewController;
  EXPECT_TRUE(second_modal);
  EXPECT_NE(first_modal, second_modal);
}
