// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/explain_with_gemini/coordinator/explain_with_gemini_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/web_selection/model/web_selection_response.h"
#import "ios/chrome/browser/web_selection/model/web_selection_tab_helper.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

@interface WebSelectionResponse (Testing)
- (instancetype)initWithSelectedText:(NSString*)selectedText
                          sourceView:(UIView*)sourceView
                          sourceRect:(CGRect)sourceRect
                               valid:(BOOL)valid;
@end

@interface ExplainWithGeminiMediator (Testing)
- (void)triggerExplainWithGeminiForText:(NSString*)text
                               webState:(web::WebState*)webState;
- (BOOL)canPerformExplainWithGeminiInWebState:(web::WebState*)webState;
- (void)addItemWithResponse:(WebSelectionResponse*)response
                 completion:(void (^)(NSArray*))completion
                   webState:(web::WebState*)webState;
@end

class ExplainWithGeminiMediatorTest : public PlatformTest {
 protected:
  ExplainWithGeminiMediatorTest()
      : task_environment_(web::WebTaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindOnce(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::TestSyncService>();
            }));
    builder.AddTestingFactory(
        GeminiServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<FakeGeminiService>();
            }));

    browser_state_ = std::move(builder).Build();
    auth_service_ =
        AuthenticationServiceFactory::GetForProfile(browser_state_.get());
    identity_manager_ =
        IdentityManagerFactory::GetForProfile(browser_state_.get());
    fake_gemini_service_ = static_cast<FakeGeminiService*>(
        GeminiServiceFactory::GetForProfile(browser_state_.get()));

    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(browser_state_.get());
    web_state_->SetVisibleURL(GURL("https://example.com"));

    GeminiTabHelper::CreateForWebState(web_state_.get());
    WebSelectionTabHelper::CreateForWebState(web_state_.get());

    mediator_ = [[ExplainWithGeminiMediator alloc]
        initWithIdentityManager:identity_manager_
                    authService:auth_service_];
  }

  void TearDown() override {
    mediator_ = nil;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> browser_state_;
  raw_ptr<AuthenticationService> auth_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  ExplainWithGeminiMediator* mediator_;
  std::unique_ptr<web::FakeWebState> web_state_;
  raw_ptr<FakeGeminiService> fake_gemini_service_;
};

// Tests that the mediator can be instantiated.
TEST_F(ExplainWithGeminiMediatorTest, Initialisation) {
  EXPECT_NE(mediator_, nil);
}

// Tests that triggering the action calls the BWGHandler.
TEST_F(ExplainWithGeminiMediatorTest, TriggerAction_StartsFlow) {
  // Wrapped in @autoreleasepool to ensure that the partial mock is deallocated
  // before the test fixture destroys the profile and its services, avoiding
  // dangling pointer crashes.
  @autoreleasepool {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{kPageActionMenu, {}},
         {kExplainGeminiEditMenu, {{"PositionForExplainGeminiEditMenu", "1"}}}},
        {});

    id mockBwgHandler = OCMProtocolMock(@protocol(BWGCommands));

    id partialMock = OCMPartialMock(mediator_);
    OCMStub(
        [partialMock canPerformExplainWithGeminiInWebState:web_state_.get()])
        .andReturn(YES);
    [partialMock setBWGHandler:mockBwgHandler];

    NSString* testText = @"Hello World";

    OCMExpect([mockBwgHandler
        startGeminiFlowWithStartupState:[OCMArg checkWithBlock:^BOOL(id value) {
          GeminiStartupState* startupState = (GeminiStartupState*)value;
          return [startupState.prepopulatedPrompt containsString:testText];
        }]]);

    [partialMock triggerExplainWithGeminiForText:testText
                                        webState:web_state_.get()];

    EXPECT_OCMOCK_VERIFY(mockBwgHandler);
  }
}

// Tests that the action is added when Gemini is available and selection is
// valid.
TEST_F(ExplainWithGeminiMediatorTest, AddItem_GeminiAvailable) {
  // Wrapped in @autoreleasepool to ensure that the partial mock is deallocated
  // before the test fixture destroys the profile and its services, avoiding
  // dangling pointer crashes.
  @autoreleasepool {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{kPageActionMenu, {}},
         {kExplainGeminiEditMenu, {{"PositionForExplainGeminiEditMenu", "1"}}}},
        {});

    id partialMock = OCMPartialMock(mediator_);
    OCMStub(
        [partialMock canPerformExplainWithGeminiInWebState:web_state_.get()])
        .andReturn(YES);

    WebSelectionResponse* response =
        [[WebSelectionResponse alloc] initWithSelectedText:@"Hello World"
                                                sourceView:nil
                                                sourceRect:CGRectZero
                                                     valid:YES];

    __block BOOL completionCalled = NO;
    __block NSArray* items = nil;

    [partialMock addItemWithResponse:response
                          completion:^(NSArray* result) {
                            completionCalled = YES;
                            items = result;
                          }
                            webState:web_state_.get()];

    EXPECT_TRUE(completionCalled);
    EXPECT_EQ(items.count, 1u);
    UIAction* action = items.firstObject;
    EXPECT_NE(action, nil);
  }
}

// Tests that triggering the action works when Gemini is available and eligible
// without mocking canPerformExplainWithGeminiInWebState. This verifies that
// the internal eligibility checks (features, service status, web state state)
// are correctly satisfied by the setup.
TEST_F(ExplainWithGeminiMediatorTest, TriggerAction_StartsFlow_NoMock) {
  @autoreleasepool {
    // Enable the features required for the Explain with Gemini feature.
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{kPageActionMenu, {}},
         {kGeminiFloatyAllPages, {}},
         {kExplainGeminiEditMenu, {{"PositionForExplainGeminiEditMenu", "1"}}}},
        {});

    // Simulate that the user is eligible for the BWG (Gemini) service.
    fake_gemini_service_->SetIsEligible(true);

    // Set up a fake web frame manager with a main frame and mark content as
    // HTML to satisfy the conditions in
    // `canPerformExplainWithGeminiInWebState`.
    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    auto main_frame = web::FakeWebFrame::CreateMainWebFrame();
    frames_manager->AddWebFrame(std::move(main_frame));
    web_state_->SetWebFramesManager(std::move(frames_manager));
    web_state_->SetContentIsHTML(true);

    id mockSceneHandler = OCMProtocolMock(@protocol(SceneCommands));
    mediator_.sceneHandler = mockSceneHandler;

    id mockBwgHandler = OCMProtocolMock(@protocol(BWGCommands));
    mediator_.BWGHandler = mockBwgHandler;

    NSString* testText = @"Hello World";

    // Expect that starting the Gemini flow will be called with a prompt
    // containing the test text.
    OCMExpect([mockBwgHandler
        startGeminiFlowWithStartupState:[OCMArg checkWithBlock:^BOOL(id value) {
          GeminiStartupState* startupState = (GeminiStartupState*)value;
          return [startupState.prepopulatedPrompt containsString:testText];
        }]]);

    [mediator_ triggerExplainWithGeminiForText:testText
                                      webState:web_state_.get()];

    EXPECT_OCMOCK_VERIFY(mockBwgHandler);
  }
}

// Tests that no action is added when Gemini is not available.
TEST_F(ExplainWithGeminiMediatorTest, AddItem_GeminiNotAvailable) {
  // Wrapped in @autoreleasepool to ensure that the partial mock is deallocated
  // before the test fixture destroys the profile and its services, avoiding
  // dangling pointer crashes.
  @autoreleasepool {
    id partialMock = OCMPartialMock(mediator_);
    OCMStub(
        [partialMock canPerformExplainWithGeminiInWebState:web_state_.get()])
        .andReturn(NO);

    WebSelectionResponse* response =
        [[WebSelectionResponse alloc] initWithSelectedText:@"Hello World"
                                                sourceView:nil
                                                sourceRect:CGRectZero
                                                     valid:YES];

    __block BOOL completionCalled = NO;
    __block NSArray* items = nil;

    [partialMock addItemWithResponse:response
                          completion:^(NSArray* result) {
                            completionCalled = YES;
                            items = result;
                          }
                            webState:web_state_.get()];

    EXPECT_TRUE(completionCalled);
    EXPECT_EQ(items.count, 0u);
  }
}

// Tests that no action is added when the selection is invalid.
TEST_F(ExplainWithGeminiMediatorTest, AddItem_InvalidSelection) {
  // Wrapped in @autoreleasepool to ensure that the partial mock is deallocated
  // before the test fixture destroys the profile and its services, avoiding
  // dangling pointer crashes.
  @autoreleasepool {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(
        {{kPageActionMenu, {}},
         {kExplainGeminiEditMenu, {{"PositionForExplainGeminiEditMenu", "1"}}}},
        {});

    id partialMock = OCMPartialMock(mediator_);
    OCMStub(
        [partialMock canPerformExplainWithGeminiInWebState:web_state_.get()])
        .andReturn(YES);

    WebSelectionResponse* response =
        [[WebSelectionResponse alloc] initWithSelectedText:@"Hello World"
                                                sourceView:nil
                                                sourceRect:CGRectZero
                                                     valid:NO];

    __block BOOL completionCalled = NO;
    __block NSArray* items = nil;

    [partialMock addItemWithResponse:response
                          completion:^(NSArray* result) {
                            completionCalled = YES;
                            items = result;
                          }
                            webState:web_state_.get()];

    EXPECT_TRUE(completionCalled);
    EXPECT_EQ(items.count, 0u);
  }
}
