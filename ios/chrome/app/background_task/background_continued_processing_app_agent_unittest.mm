// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_task/background_continued_processing_app_agent.h"

#import <BackgroundTasks/BackgroundTasks.h>
#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/background_mode_buildflags.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_configuration.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_context.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_provider.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_request.h"
#import "ios/chrome/app/background_task/features.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

NSString* const kTestTaskId = @"test.task.FooBar";
NSString* const kTestTaskTitle = @"Foo";
NSString* const kTestTaskSubtitle = @"Bar";

// Task identifiers returned by fake providers.
NSString* const kFirstTaskId = @"test.task.First";
NSString* const kSecondTaskId = @"test.task.Second";
NSString* const kFailingTaskId = @"test.task.Failing";

}  // namespace

// Helper object to dynamically configure the mock app state's init stage,
// active and foreground scenes.
@interface DynamicMockAppStateHelper : NSObject
@property(nonatomic, assign) AppInitStage initStage;
@property(nonatomic, strong) id foregroundActiveScene;
@property(nonatomic, copy) NSArray<SceneState*>* foregroundScenes;
@end

@implementation DynamicMockAppStateHelper
@end

// Fake provider that records every call into a shared `log`, prefixed by
// `name`.
@interface FakeContinuedProcessingTaskProvider
    : NSObject <BackgroundContinuedProcessingTaskProvider>
// Requests returned when the provider is asked for tasks.
@property(nonatomic, copy)
    NSArray<BackgroundContinuedProcessingTaskRequest*>* requests;
// Invoked when the provider is asked for tasks, before returning `requests`.
@property(nonatomic, copy) ProceduralBlock onRequestsAsked;
// Contexts received by the `startedHandler` of the requests created with
// `requestWithIdentifier:configuration:`, in order.
@property(nonatomic, readonly)
    NSMutableArray<BackgroundContinuedProcessingTaskContext*>* startedContexts;
- (instancetype)initWithName:(NSString*)name
                         log:(NSMutableArray<NSString*>*)log;
// Creates a request whose `startedHandler` logs "<name>.started:<identifier>"
// and appends the received context to `startedContexts`.
- (BackgroundContinuedProcessingTaskRequest*)
    requestWithIdentifier:(NSString*)identifier
            configuration:
                (BackgroundContinuedProcessingTaskConfiguration*)configuration;
@end

@implementation FakeContinuedProcessingTaskProvider {
  NSString* _name;
  NSMutableArray<NSString*>* _log;
}

- (instancetype)initWithName:(NSString*)name
                         log:(NSMutableArray<NSString*>*)log {
  if ((self = [super init])) {
    _name = [name copy];
    _log = log;
    _startedContexts = [NSMutableArray array];
  }
  return self;
}

- (BackgroundContinuedProcessingTaskRequest*)
    requestWithIdentifier:(NSString*)identifier
            configuration:
                (BackgroundContinuedProcessingTaskConfiguration*)configuration {
  // Captured weakly since the provider retains its requests.
  __weak __typeof(self) weakSelf = self;
  return [[BackgroundContinuedProcessingTaskRequest alloc]
      initWithIdentifier:identifier
           configuration:configuration
          startedHandler:^(BackgroundContinuedProcessingTaskContext* context) {
            [weakSelf didStartTaskWithIdentifier:identifier context:context];
          }];
}

- (NSArray<BackgroundContinuedProcessingTaskRequest*>*)
    continuedProcessingTaskRequests {
  [_log addObject:[_name stringByAppendingString:@".requests"]];
  if (self.onRequestsAsked) {
    self.onRequestsAsked();
  }
  return self.requests;
}

- (void)backgroundProcessingBecameAvailable {
  [_log addObject:[_name stringByAppendingString:@".background"]];
}

- (void)backgroundProcessingBecameUnnecessary {
  [_log addObject:[_name stringByAppendingString:@".foreground"]];
}

#pragma mark - Private

// Records the `context` received for the request with `identifier`.
- (void)didStartTaskWithIdentifier:(NSString*)identifier
                           context:(BackgroundContinuedProcessingTaskContext*)
                                       context {
  [_log addObject:[NSString
                      stringWithFormat:@"%@.started:%@", _name, identifier]];
  [_startedContexts addObject:context];
}

@end

// Fake provider implementing only the required protocol methods.
@interface MinimalContinuedProcessingTaskProvider
    : NSObject <BackgroundContinuedProcessingTaskProvider>
@property(nonatomic, assign) int requestsAskedCount;
@end

@implementation MinimalContinuedProcessingTaskProvider

- (NSArray<BackgroundContinuedProcessingTaskRequest*>*)
    continuedProcessingTaskRequests {
  self.requestsAskedCount++;
  return nil;
}

@end

#pragma mark - BackgroundContinuedProcessingAppAgentTest

class BackgroundContinuedProcessingAppAgentTest : public PlatformTest {
 public:
  BackgroundContinuedProcessingAppAgentTest() {
    mock_app_state_ = OCMClassMock([AppState class]);
    mock_scene_state_ = OCMClassMock([SceneState class]);
    app_state_helper_ = [[DynamicMockAppStateHelper alloc] init];
    app_state_helper_.initStage = AppInitStage::kFinal;
    app_state_helper_.foregroundActiveScene = mock_scene_state_;
    app_state_helper_.foregroundScenes = @[ mock_scene_state_ ];

    DynamicMockAppStateHelper* helper = app_state_helper_;
    OCMStub([mock_app_state_ initStage]).andDo(^(NSInvocation* inv) {
      AppInitStage stage = helper.initStage;
      [inv setReturnValue:&stage];
    });
    OCMStub([mock_app_state_ foregroundActiveScene])
        .andDo(^(NSInvocation* inv) {
          id scene = helper.foregroundActiveScene;
          [inv setReturnValue:&scene];
        });
    OCMStub([mock_app_state_ foregroundScenes]).andDo(^(NSInvocation* inv) {
      NSArray<SceneState*>* scenes = helper.foregroundScenes;
      [inv setReturnValue:&scenes];
    });
    OCMStub([mock_app_state_ addObserver:[OCMArg any]]);
    OCMStub([mock_app_state_ removeObserver:[OCMArg any]]);

    mock_scheduler_ = OCMStrictClassMock([BGTaskScheduler class]);
    OCMStub([mock_scheduler_ sharedScheduler]).andReturn(mock_scheduler_);

    // The app starts in the foreground.
    agent_ = [[BackgroundContinuedProcessingAppAgent alloc] init];
    agent_.appState = mock_app_state_;
    log_ = [NSMutableArray array];
  }

  ~BackgroundContinuedProcessingAppAgentTest() override {
    [mock_app_state_ stopMocking];
    [mock_scene_state_ stopMocking];
    [mock_scheduler_ stopMocking];
  }

  // Creates a test task configuration with default properties.
  BackgroundContinuedProcessingTaskConfiguration* CreateTestConfiguration(
      ProceduralBlock expiration_handler = ^{
      }) {
    DCHECK(expiration_handler);
    BackgroundContinuedProcessingTaskConfiguration* config =
        [[BackgroundContinuedProcessingTaskConfiguration alloc]
                initWithTitle:kTestTaskTitle
                     subtitle:kTestTaskSubtitle
            expirationHandler:expiration_handler];
    return config;
  }

  // Creates a task request with `identifier`, a test configuration and no
  // `startedHandler`.
  BackgroundContinuedProcessingTaskRequest* CreateRequest(
      NSString* identifier) {
    return [[BackgroundContinuedProcessingTaskRequest alloc]
        initWithIdentifier:identifier
             configuration:CreateTestConfiguration()
            startedHandler:nil];
  }

  // Creates a task request with `identifier` and a test configuration, whose
  // `startedHandler` logs into `provider` and records the context there.
  BackgroundContinuedProcessingTaskRequest* CreateTrackedRequest(
      FakeContinuedProcessingTaskProvider* provider,
      NSString* identifier) {
    return [provider requestWithIdentifier:identifier
                             configuration:CreateTestConfiguration()];
  }

  // Creates a fake provider named `name` logging into `log_`, registers it
  // with `agent_` and keeps it alive for the test's duration.
  FakeContinuedProcessingTaskProvider* AddFakeProvider(NSString* name) {
    FakeContinuedProcessingTaskProvider* provider =
        [[FakeContinuedProcessingTaskProvider alloc] initWithName:name
                                                              log:log_];
    [agent_ addTaskProvider:provider];
    if (!providers_) {
      providers_ = [NSMutableArray array];
    }
    [providers_ addObject:provider];
    return provider;
  }

  // Simulates `scene` transitioning to `level`, leaving `foreground_scenes` in
  // the foreground.
  void TransitionScene(id scene,
                       SceneActivationLevel level,
                       NSArray<SceneState*>* foreground_scenes) {
    app_state_helper_.foregroundScenes = foreground_scenes;
    app_state_helper_.foregroundActiveScene =
        level == SceneActivationLevelForegroundActive ? scene : nil;
    [agent_ sceneState:scene transitionedToActivationLevel:level];
  }

  // Simulates the only scene moving to the background.
  void EnterBackground() {
    TransitionScene(mock_scene_state_, SceneActivationLevelBackground, @[]);
  }

  // Simulates the only scene moving back to the foreground.
  void EnterForeground() {
    TransitionScene(mock_scene_state_, SceneActivationLevelForegroundActive,
                    @[ mock_scene_state_ ]);
  }

  // Stubs task registration and submission in `BGTaskScheduler`. If
  // `out_launch_handler` is provided, captures the registered launch handler.
  void StubScheduler(void (^__strong* out_launch_handler)(BGTask*) = nullptr,
                     BOOL registration_success = YES,
                     BOOL submission_success = YES,
                     NSError* submission_error = nil) {
    if (out_launch_handler) {
      OCMStub([mock_scheduler_
                  registerForTaskWithIdentifier:[OCMArg any]
                                     usingQueue:dispatch_get_main_queue()
                                  launchHandler:[OCMArg checkWithBlock:^BOOL(
                                                            id value) {
                                    DCHECK(value);
                                    *out_launch_handler = [value copy];
                                    return YES;
                                  }]])
          .andReturn(registration_success);
    } else {
      OCMStub([mock_scheduler_
                  registerForTaskWithIdentifier:[OCMArg any]
                                     usingQueue:dispatch_get_main_queue()
                                  launchHandler:[OCMArg any]])
          .andReturn(registration_success);
    }

    OCMStub([mock_scheduler_ submitTaskRequest:[OCMArg any]
                                         error:[OCMArg setTo:submission_error]])
        .andReturn(submission_success);
  }

  // Stubs `BGTaskScheduler` so that submission fails only for requests whose
  // identifier contains `failing_identifier`. Identifiers of successfully
  // submitted requests are appended to `submitted_identifiers_`.
  void StubSchedulerForProviders(NSString* failing_identifier = nil) {
    OCMStub([mock_scheduler_
                registerForTaskWithIdentifier:[OCMArg any]
                                   usingQueue:dispatch_get_main_queue()
                                launchHandler:[OCMArg any]])
        .andReturn(YES);
    NSMutableArray<NSString*>* submitted_identifiers = submitted_identifiers_;
    OCMStub([mock_scheduler_ submitTaskRequest:[OCMArg any]
                                         error:[OCMArg anyObjectRef]])
        .andDo(^(NSInvocation* inv) {
          __unsafe_unretained BGTaskRequest* request = nil;
          [inv getArgument:&request atIndex:2];
          BOOL success =
              !failing_identifier ||
              ![request.identifier containsString:failing_identifier];
          if (success) {
            [submitted_identifiers addObject:request.identifier];
          }
          [inv setReturnValue:&success];
        });
    OCMStub([mock_scheduler_ cancelTaskRequestWithIdentifier:[OCMArg any]]);
  }

  // Completes every context handed to `provider` so none outlives the test.
  void CompleteStartedTasks(FakeContinuedProcessingTaskProvider* provider) {
    for (BackgroundContinuedProcessingTaskContext* context in provider
             .startedContexts) {
      [context setTaskCompletedWithSuccess:YES];
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  id mock_app_state_;
  id mock_scene_state_;
  DynamicMockAppStateHelper* app_state_helper_;
  id mock_scheduler_;
  BackgroundContinuedProcessingAppAgent* agent_;
  // Shared call log of the fake providers.
  NSMutableArray<NSString*>* log_;
  // Strong references to the fake providers added with `AddFakeProvider`.
  NSMutableArray<FakeContinuedProcessingTaskProvider*>* providers_;
  // Identifiers submitted to `BGTaskScheduler` by `StubSchedulerForProviders`.
  NSMutableArray<NSString*>* submitted_identifiers_ = [NSMutableArray array];
};

// Tests that requesting a task returns nil when the feature flag is disabled.
TEST_F(BackgroundContinuedProcessingAppAgentTest,
       TestRequestTaskFailsWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kEnableBackgroundContinuedProcessing);

  EXPECT_EQ([agent_ requestTaskWithIdentifier:kTestTaskId
                                configuration:CreateTestConfiguration()],
            nil);
}

// Tests that providers are neither notified nor asked for tasks when the
// feature flag is disabled.
TEST_F(BackgroundContinuedProcessingAppAgentTest,
       TestProvidersIgnoredWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kEnableBackgroundContinuedProcessing);
  AddFakeProvider(@"A");

  EnterBackground();
  EnterForeground();

  EXPECT_EQ(log_.count, 0u);
}

// Tests that requesting a task returns nil without touching `BGTaskScheduler`
// when background continued processing is unavailable in the current
// configuration (i.e. the `ios_enable_background_continued_processing` GN arg
// is false, or the OS is older than iOS 26), even with the killswitch enabled.
TEST_F(BackgroundContinuedProcessingAppAgentTest,
       TestRequestTaskFailsWhenUnavailable) {
  if (IsBackgroundContinuedProcessingEnabled()) {
    GTEST_SKIP() << "Background continued processing is available.";
  }

  // `mock_scheduler_` is a strict mock, so any scheduler call would fail.
  EXPECT_EQ([agent_ requestTaskWithIdentifier:kTestTaskId
                                configuration:CreateTestConfiguration()],
            nil);
}

#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
#pragma mark - BackgroundContinuedProcessingAppAgentAvailableTest

// Fixture for tests that require background continued processing to be
// available. Tests are skipped when the
// `ios_enable_background_continued_processing` GN arg is false or the OS is
// older than iOS 26.
class BackgroundContinuedProcessingAppAgentAvailableTest
    : public BackgroundContinuedProcessingAppAgentTest {
 protected:
  void SetUp() override {
    BackgroundContinuedProcessingAppAgentTest::SetUp();
    if (!IsBackgroundContinuedProcessingEnabled()) {
      GTEST_SKIP() << "Background continued processing is unavailable.";
    }
  }
};

// Tests that requesting a task succeeds when a scene is in
// `SceneActivationLevelForegroundInactive` (`foregroundActiveScene` is nil,
// while `foregroundScenes` still contains the resigning scene).
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestRequestTaskSucceedsWhenSceneForegroundInactive) {
  if (@available(iOS 26.0, *)) {
    app_state_helper_.foregroundActiveScene = nil;
    app_state_helper_.foregroundScenes = @[ mock_scene_state_ ];
    OCMStub([mock_scheduler_ cancelTaskRequestWithIdentifier:[OCMArg any]]);
    StubScheduler();

    BackgroundContinuedProcessingTaskContext* context =
        [agent_ requestTaskWithIdentifier:kTestTaskId
                            configuration:CreateTestConfiguration()];
    ASSERT_NE(context, nil);
    [context setTaskCompletedWithSuccess:YES];
  }
}

// Tests that requesting a task succeeds when no scene is in the foreground.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestRequestTaskSucceedsWithNoForegroundScene) {
  if (@available(iOS 26.0, *)) {
    app_state_helper_.foregroundActiveScene = nil;
    app_state_helper_.foregroundScenes = @[];
    OCMStub([mock_scheduler_ cancelTaskRequestWithIdentifier:[OCMArg any]]);
    StubScheduler();

    BackgroundContinuedProcessingTaskContext* context =
        [agent_ requestTaskWithIdentifier:kTestTaskId
                            configuration:CreateTestConfiguration()];
    ASSERT_NE(context, nil);
    [context setTaskCompletedWithSuccess:YES];
  }
}

// Tests that requesting a task returns nil if system registration fails.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestRequestTaskFailsWhenRegistrationFails) {
  if (@available(iOS 26.0, *)) {
    StubScheduler(nullptr, /*registration_success=*/NO);

    EXPECT_EQ([agent_ requestTaskWithIdentifier:kTestTaskId
                                  configuration:CreateTestConfiguration()],
              nil);
  }
}

// Tests that requesting a task returns nil if system submission fails.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestRequestTaskFailsWhenSubmissionFails) {
  if (@available(iOS 26.0, *)) {
    NSError* error = [NSError errorWithDomain:@"org.chromium.test"
                                         code:1
                                     userInfo:nil];
    StubScheduler(nullptr, /*registration_success=*/YES,
                  /*submission_success=*/NO, error);

    EXPECT_EQ([agent_ requestTaskWithIdentifier:kTestTaskId
                                  configuration:CreateTestConfiguration()],
              nil);
  }
}

// Tests that task registration and submission succeed with expected parameters,
// auto-incremented identifiers, and complete successfully.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestRequestTaskRegistersAndSubmitsWithSystem) {
  if (@available(iOS 26.0, *)) {
    __block void (^captured_launch_handler)(BGTask*) = nil;
    __block BGContinuedProcessingTaskRequest* captured_request = nil;
    OCMStub([mock_scheduler_
                registerForTaskWithIdentifier:[OCMArg any]
                                   usingQueue:dispatch_get_main_queue()
                                launchHandler:[OCMArg checkWithBlock:^BOOL(
                                                          id value) {
                                  captured_launch_handler = [value copy];
                                  return YES;
                                }]])
        .andReturn(YES);
    OCMStub([mock_scheduler_
                submitTaskRequest:[OCMArg checkWithBlock:^BOOL(id value) {
                  captured_request = value;
                  return [value
                      isKindOfClass:[BGContinuedProcessingTaskRequest class]];
                }]
                            error:[OCMArg setTo:nil]])
        .andReturn(YES);

    BackgroundContinuedProcessingTaskContext* context =
        [agent_ requestTaskWithIdentifier:kTestTaskId
                            configuration:CreateTestConfiguration()];

    ASSERT_NE(context, nil);
    ASSERT_NE(captured_request, nil);
    EXPECT_NSEQ(captured_request.title, kTestTaskTitle);
    EXPECT_NSEQ(captured_request.subtitle, kTestTaskSubtitle);
    // The default configuration strategy maps to the SDK's queue strategy.
    EXPECT_EQ(BGContinuedProcessingTaskRequestSubmissionStrategyQueue,
              captured_request.strategy);
    ASSERT_NE(captured_launch_handler, nil);

    NSString* expected_segment = [NSString
        stringWithFormat:@".continuedProcessingTask.%@.", kTestTaskId];
    EXPECT_TRUE([context.taskIdentifier containsString:expected_segment]);

    // Simulate system delivering the task on the main queue.
    id mock_task = OCMClassMock([BGContinuedProcessingTask class]);
    OCMStub([mock_task identifier]).andReturn(context.taskIdentifier);
    OCMStub([(BGContinuedProcessingTask*)mock_task progress])
        .andReturn([NSProgress progressWithTotalUnitCount:100]);
    OCMExpect([mock_task setTaskCompletedWithSuccess:YES]);

    captured_launch_handler(mock_task);
    [context setTaskCompletedWithSuccess:YES];

    EXPECT_OCMOCK_VERIFY(mock_task);
  }
}

// Tests that a configuration whose submission strategy is `kFail` is submitted
// to the system with the SDK's fail strategy.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestRequestTaskSubmitsFailStrategy) {
  if (@available(iOS 26.0, *)) {
    __block BGContinuedProcessingTaskRequest* captured_request = nil;
    OCMStub([mock_scheduler_
                registerForTaskWithIdentifier:[OCMArg any]
                                   usingQueue:dispatch_get_main_queue()
                                launchHandler:[OCMArg any]])
        .andReturn(YES);
    OCMStub([mock_scheduler_
                submitTaskRequest:[OCMArg checkWithBlock:^BOOL(id value) {
                  captured_request = value;
                  return [value
                      isKindOfClass:[BGContinuedProcessingTaskRequest class]];
                }]
                            error:[OCMArg setTo:nil]])
        .andReturn(YES);
    OCMStub([mock_scheduler_ cancelTaskRequestWithIdentifier:[OCMArg any]]);
    BackgroundContinuedProcessingTaskConfiguration* config =
        CreateTestConfiguration();
    config.submissionStrategy =
        BackgroundContinuedProcessingSubmissionStrategy::kFail;

    BackgroundContinuedProcessingTaskContext* context =
        [agent_ requestTaskWithIdentifier:kTestTaskId configuration:config];

    ASSERT_NE(context, nil);
    ASSERT_NE(captured_request, nil);
    EXPECT_EQ(BGContinuedProcessingTaskRequestSubmissionStrategyFail,
              captured_request.strategy);
    [context setTaskCompletedWithSuccess:YES];
  }
}

// Tests that the agent retains active task contexts even if the caller drops
// its reference, and removes it from active tasks upon completion.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestAgentRetainsContextAcrossCallerHandleDrop) {
  if (@available(iOS 26.0, *)) {
    __block void (^captured_launch_handler)(BGTask*) = nil;
    StubScheduler(&captured_launch_handler);

    __weak BackgroundContinuedProcessingTaskContext* weak_context = nil;
    NSString* task_identifier = nil;

    @autoreleasepool {
      BackgroundContinuedProcessingTaskContext* context =
          [agent_ requestTaskWithIdentifier:kTestTaskId
                              configuration:CreateTestConfiguration()];
      ASSERT_NE(context, nil);
      weak_context = context;
      task_identifier = context.taskIdentifier;
      // Caller drops context handle.
      context = nil;
    }

    // Context should still be alive because the agent retains it in
    // `_activeTasks`.
    ASSERT_NE(weak_context, nil);
    ASSERT_NE(captured_launch_handler, nil);

    id mock_task = OCMClassMock([BGContinuedProcessingTask class]);
    OCMStub([mock_task identifier]).andReturn(task_identifier);
    OCMStub([(BGContinuedProcessingTask*)mock_task progress])
        .andReturn([NSProgress
            progressWithTotalUnitCount:kDefaultTotalUnitsOfProgress]);
    OCMExpect([mock_task setTaskCompletedWithSuccess:YES]);

    captured_launch_handler(mock_task);

    // Completing the retained task should notify the mock task and clean up
    // from the agent.
    @autoreleasepool {
      [weak_context setTaskCompletedWithSuccess:YES];
    }
    EXPECT_OCMOCK_VERIFY(mock_task);

    // Now that the task has completed, the agent should have released it.
    EXPECT_EQ(weak_context, nil);
  }
}

// Tests that if the system delivers an unrecognized task identifier,
// the OS task is safely completed with failure.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestLaunchDeliveryWithUnknownIdentifierFailsOSTask) {
  if (@available(iOS 26.0, *)) {
    OCMStub([mock_scheduler_ cancelTaskRequestWithIdentifier:[OCMArg any]]);
    __block void (^captured_launch_handler)(BGTask*) = nil;
    StubScheduler(&captured_launch_handler);

    BackgroundContinuedProcessingTaskContext* context =
        [agent_ requestTaskWithIdentifier:kTestTaskId
                            configuration:CreateTestConfiguration()];
    ASSERT_NE(context, nil);
    ASSERT_NE(captured_launch_handler, nil);

    id mock_task = OCMClassMock([BGContinuedProcessingTask class]);
    OCMStub([mock_task identifier]).andReturn(@"unknown.unregistered.task.id");
    OCMExpect([mock_task setTaskCompletedWithSuccess:NO]);

    captured_launch_handler(mock_task);
    EXPECT_OCMOCK_VERIFY(mock_task);

    [context setTaskCompletedWithSuccess:YES];
  }
}

// Tests that if the system delivers a task that is not a
// `BGContinuedProcessingTask`, the OS task is safely completed with failure.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestLaunchDeliveryWithUnexpectedTaskClassFailsOSTask) {
  if (@available(iOS 26.0, *)) {
    OCMStub([mock_scheduler_ cancelTaskRequestWithIdentifier:[OCMArg any]]);
    __block void (^captured_launch_handler)(BGTask*) = nil;
    StubScheduler(&captured_launch_handler);

    BackgroundContinuedProcessingTaskContext* context =
        [agent_ requestTaskWithIdentifier:kTestTaskId
                            configuration:CreateTestConfiguration()];
    ASSERT_NE(context, nil);
    ASSERT_NE(captured_launch_handler, nil);

    id mock_generic_task = OCMClassMock([BGTask class]);
    OCMExpect([mock_generic_task setTaskCompletedWithSuccess:NO]);

    captured_launch_handler(mock_generic_task);
    EXPECT_OCMOCK_VERIFY(mock_generic_task);

    [context setTaskCompletedWithSuccess:YES];
  }
}

// Tests that completing the context before the OS delivers the underlying
// task cancels the scheduled request in `BGTaskScheduler`, and safely marks
// any subsequent system delivery as completed with failure.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestEarlyCompletionSuccessBeforeOSTaskDelivery) {
  if (@available(iOS 26.0, *)) {
    __block void (^captured_launch_handler)(BGTask*) = nil;
    StubScheduler(&captured_launch_handler);

    BackgroundContinuedProcessingTaskContext* context =
        [agent_ requestTaskWithIdentifier:kTestTaskId
                            configuration:CreateTestConfiguration()];
    ASSERT_NE(context, nil);
    ASSERT_NE(captured_launch_handler, nil);

    NSString* task_identifier = context.taskIdentifier;

    OCMExpect(
        [mock_scheduler_ cancelTaskRequestWithIdentifier:task_identifier]);

    [context setTaskCompletedWithSuccess:YES];
    EXPECT_TRUE(context.isCompleted);
    EXPECT_OCMOCK_VERIFY(mock_scheduler_);

    id mock_task = OCMClassMock([BGContinuedProcessingTask class]);
    OCMStub([mock_task identifier]).andReturn(task_identifier);
    OCMExpect([mock_task setTaskCompletedWithSuccess:NO]);

    captured_launch_handler(mock_task);
    EXPECT_OCMOCK_VERIFY(mock_task);
  }
}

// Tests that failing the context before the OS delivers the underlying task
// cancels the scheduled request in `BGTaskScheduler`, and safely marks
// any subsequent system delivery as completed with failure.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestEarlyCompletionFailureBeforeOSTaskDelivery) {
  if (@available(iOS 26.0, *)) {
    __block void (^captured_launch_handler)(BGTask*) = nil;
    StubScheduler(&captured_launch_handler);

    BackgroundContinuedProcessingTaskContext* context =
        [agent_ requestTaskWithIdentifier:kTestTaskId
                            configuration:CreateTestConfiguration()];
    ASSERT_NE(context, nil);
    ASSERT_NE(captured_launch_handler, nil);

    NSString* task_identifier = context.taskIdentifier;

    OCMExpect(
        [mock_scheduler_ cancelTaskRequestWithIdentifier:task_identifier]);

    [context setTaskCompletedWithSuccess:NO];
    EXPECT_TRUE(context.isCompleted);
    EXPECT_OCMOCK_VERIFY(mock_scheduler_);

    id mock_task = OCMClassMock([BGContinuedProcessingTask class]);
    OCMStub([mock_task identifier]).andReturn(task_identifier);
    OCMExpect([mock_task setTaskCompletedWithSuccess:NO]);

    captured_launch_handler(mock_task);
    EXPECT_OCMOCK_VERIFY(mock_task);
  }
}

// Tests that deallocating the agent completes all active tasks with failure.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestAgentDeallocCompletesActiveTasks) {
  if (@available(iOS 26.0, *)) {
    OCMStub([mock_scheduler_ cancelTaskRequestWithIdentifier:[OCMArg any]]);
    StubScheduler();

    BackgroundContinuedProcessingTaskContext* context = nil;

    @autoreleasepool {
      id local_app_state = OCMClassMock([AppState class]);
      OCMStub([local_app_state foregroundActiveScene])
          .andReturn(mock_scene_state_);
      OCMStub([local_app_state foregroundScenes]).andReturn(@[
        mock_scene_state_
      ]);

      auto* local_agent = [[BackgroundContinuedProcessingAppAgent alloc] init];
      local_agent.appState = local_app_state;
      context =
          [local_agent requestTaskWithIdentifier:kTestTaskId
                                   configuration:CreateTestConfiguration()];
      ASSERT_NE(context, nil);
      EXPECT_FALSE(context.isCompleted);
      local_agent = nil;
      [local_app_state stopMocking];
    }

    EXPECT_TRUE(context.isCompleted);
  }
}

// Tests that if the agent is deallocated before the OS invokes `launchHandler`,
// the task is safely marked completed with failure.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestLaunchHandlerWhenAgentDeallocatedFailsTask) {
  if (@available(iOS 26.0, *)) {
    OCMStub([mock_scheduler_ cancelTaskRequestWithIdentifier:[OCMArg any]]);
    __block void (^captured_launch_handler)(BGTask*) = nil;
    StubScheduler(&captured_launch_handler);

    @autoreleasepool {
      id local_app_state = OCMClassMock([AppState class]);
      OCMStub([local_app_state foregroundActiveScene])
          .andReturn(mock_scene_state_);
      OCMStub([local_app_state foregroundScenes]).andReturn(@[
        mock_scene_state_
      ]);

      auto* local_agent = [[BackgroundContinuedProcessingAppAgent alloc] init];
      local_agent.appState = local_app_state;
      BackgroundContinuedProcessingTaskContext* context =
          [local_agent requestTaskWithIdentifier:kTestTaskId
                                   configuration:CreateTestConfiguration()];
      ASSERT_NE(context, nil);
      ASSERT_NE(captured_launch_handler, nil);
      local_agent = nil;
      [local_app_state stopMocking];
    }

    id mock_task = OCMClassMock([BGContinuedProcessingTask class]);
    OCMExpect([mock_task setTaskCompletedWithSuccess:NO]);

    captured_launch_handler(mock_task);
    EXPECT_OCMOCK_VERIFY(mock_task);
  }
}

#pragma mark - Task providers

// Tests that entering the background notifies every provider before asking any
// of them for tasks.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestBackgroundNotifiesProvidersThenRequestsTasks) {
  AddFakeProvider(@"A");
  AddFakeProvider(@"B");

  EnterBackground();

  EXPECT_NSEQ(
      (@[ @"A.background", @"B.background", @"A.requests", @"B.requests" ]),
      log_);
}

// Tests that the agent submits each requested task and calls `startedHandler`
// only for successfully submitted requests, in request order.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestStartedHandlerCalledOnlyForSubmittedRequests) {
  if (@available(iOS 26.0, *)) {
    StubSchedulerForProviders(kFailingTaskId);
    FakeContinuedProcessingTaskProvider* provider = AddFakeProvider(@"A");
    provider.requests = @[
      CreateTrackedRequest(provider, kFirstTaskId),
      CreateTrackedRequest(provider, kFailingTaskId),
      CreateTrackedRequest(provider, kSecondTaskId),
    ];

    EnterBackground();

    ASSERT_EQ(provider.startedContexts.count, 2u);
    EXPECT_TRUE([provider.startedContexts[0].taskIdentifier
        containsString:kFirstTaskId]);
    EXPECT_TRUE([provider.startedContexts[1].taskIdentifier
        containsString:kSecondTaskId]);
    EXPECT_NSEQ((@[
                  @"A.background", @"A.requests", @"A.started:test.task.First",
                  @"A.started:test.task.Second"
                ]),
                log_);

    CompleteStartedTasks(provider);
  }
}

// Tests that `startedHandler` receives the live context of the task submitted
// to `BGTaskScheduler`, i.e. the one `requestTaskWithIdentifier:configuration:`
// created, which the agent retains until completion.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestStartedHandlerReceivesSubmittedTaskContext) {
  if (@available(iOS 26.0, *)) {
    StubSchedulerForProviders();
    FakeContinuedProcessingTaskProvider* provider = AddFakeProvider(@"A");
    provider.requests = @[ CreateTrackedRequest(provider, kFirstTaskId) ];

    EnterBackground();

    __weak BackgroundContinuedProcessingTaskContext* weak_context = nil;
    @autoreleasepool {
      ASSERT_EQ(provider.startedContexts.count, 1u);
      ASSERT_EQ(submitted_identifiers_.count, 1u);
      BackgroundContinuedProcessingTaskContext* context =
          provider.startedContexts[0];
      EXPECT_NSEQ(context.taskIdentifier, submitted_identifiers_[0]);
      EXPECT_FALSE(context.isCompleted);
      weak_context = context;
      [provider.startedContexts removeAllObjects];
    }

    // The agent retains the context while the task is active, and releases it
    // once completed.
    ASSERT_NE(weak_context, nil);
    @autoreleasepool {
      [weak_context setTaskCompletedWithSuccess:YES];
    }
    EXPECT_EQ(weak_context, nil);
  }
}

// Tests that requests without a `startedHandler` are submitted normally.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestNilStartedHandlerIsSupported) {
  if (@available(iOS 26.0, *)) {
    StubSchedulerForProviders();
    FakeContinuedProcessingTaskProvider* provider = AddFakeProvider(@"A");
    provider.requests = @[ CreateRequest(kFirstTaskId) ];

    EnterBackground();

    ASSERT_EQ(submitted_identifiers_.count, 1u);
    EXPECT_TRUE([submitted_identifiers_[0] containsString:kFirstTaskId]);
    EXPECT_EQ(provider.startedContexts.count, 0u);
    EXPECT_NSEQ((@[ @"A.background", @"A.requests" ]), log_);
  }
}

// Tests that the `startedHandler` of each request is called with the context
// of its own task when several providers request tasks.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestStartedHandlersAcrossMultipleProviders) {
  if (@available(iOS 26.0, *)) {
    StubSchedulerForProviders(kFailingTaskId);
    FakeContinuedProcessingTaskProvider* first = AddFakeProvider(@"A");
    FakeContinuedProcessingTaskProvider* second = AddFakeProvider(@"B");
    first.requests = @[
      CreateTrackedRequest(first, kFirstTaskId),
      CreateTrackedRequest(first, kFailingTaskId),
    ];
    second.requests = @[
      CreateTrackedRequest(second, kSecondTaskId),
    ];

    EnterBackground();

    ASSERT_EQ(first.startedContexts.count, 1u);
    EXPECT_TRUE(
        [first.startedContexts[0].taskIdentifier containsString:kFirstTaskId]);
    ASSERT_EQ(second.startedContexts.count, 1u);
    EXPECT_TRUE([second.startedContexts[0].taskIdentifier
        containsString:kSecondTaskId]);
    EXPECT_NSEQ((@[
                  @"A.background", @"B.background", @"A.requests",
                  @"A.started:test.task.First", @"B.requests",
                  @"B.started:test.task.Second"
                ]),
                log_);

    CompleteStartedTasks(first);
    CompleteStartedTasks(second);
  }
}

// Tests that providers returning nil or an empty array are handled without
// touching `BGTaskScheduler`.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestNilAndEmptyRequestsHandled) {
  FakeContinuedProcessingTaskProvider* nil_provider = AddFakeProvider(@"A");
  FakeContinuedProcessingTaskProvider* empty_provider = AddFakeProvider(@"B");
  nil_provider.requests = nil;
  empty_provider.requests = @[];

  // `mock_scheduler_` is a strict mock, so any scheduler call would fail.
  EnterBackground();

  EXPECT_NSEQ(
      (@[ @"A.background", @"B.background", @"A.requests", @"B.requests" ]),
      log_);
}

// Tests that entering the foreground notifies providers without asking them
// for tasks.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestForegroundNotifiesProviders) {
  AddFakeProvider(@"A");
  EnterBackground();
  [log_ removeAllObjects];

  EnterForeground();

  EXPECT_NSEQ(@[ @"A.foreground" ], log_);
}

// Tests that providers implementing only the required methods are asked for
// tasks and safely skipped for the optional notifications.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestProviderWithoutOptionalMethods) {
  MinimalContinuedProcessingTaskProvider* provider =
      [[MinimalContinuedProcessingTaskProvider alloc] init];
  [agent_ addTaskProvider:provider];

  EnterBackground();
  EnterForeground();

  EXPECT_EQ(provider.requestsAskedCount, 1);
}

// Tests that a cold launch, during which scenes start in the background, does
// not ask providers for tasks.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestNoRequestsAtLaunch) {
  app_state_helper_.foregroundActiveScene = nil;
  app_state_helper_.foregroundScenes = @[];
  BackgroundContinuedProcessingAppAgent* launch_agent =
      [[BackgroundContinuedProcessingAppAgent alloc] init];
  launch_agent.appState = mock_app_state_;
  FakeContinuedProcessingTaskProvider* provider =
      [[FakeContinuedProcessingTaskProvider alloc] initWithName:@"A" log:log_];
  [launch_agent addTaskProvider:provider];

  // The scene connects in the background, then moves to the foreground.
  [launch_agent sceneState:mock_scene_state_
      transitionedToActivationLevel:SceneActivationLevelBackground];
  app_state_helper_.foregroundScenes = @[ mock_scene_state_ ];
  [launch_agent sceneState:mock_scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];

  EXPECT_NSEQ(@[ @"A.foreground" ], log_);
}

// Tests that scene transitions before the final init stage are ignored.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestNoRequestsBeforeFinalInitStage) {
  AddFakeProvider(@"A");
  app_state_helper_.initStage = AppInitStage::kStart;

  EnterBackground();

  EXPECT_EQ(log_.count, 0u);
}

// Tests that providers are not asked for tasks while another scene is still in
// the foreground, and are asked once the last scene leaves it.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestNoRequestsWhileAnotherSceneIsForeground) {
  id other_scene = OCMClassMock([SceneState class]);
  app_state_helper_.foregroundScenes = @[ mock_scene_state_, other_scene ];
  AddFakeProvider(@"A");

  TransitionScene(mock_scene_state_, SceneActivationLevelBackground,
                  @[ other_scene ]);
  EXPECT_EQ(log_.count, 0u);

  TransitionScene(other_scene, SceneActivationLevelBackground, @[]);
  EXPECT_NSEQ((@[ @"A.background", @"A.requests" ]), log_);

  [other_scene stopMocking];
}

// Tests that providers are asked only once per background entry, and asked
// again after the app returns to the foreground.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestRequestsAgainAfterReturningToForeground) {
  if (@available(iOS 26.0, *)) {
    StubSchedulerForProviders();
    FakeContinuedProcessingTaskProvider* provider = AddFakeProvider(@"A");
    provider.requests = @[ CreateTrackedRequest(provider, kFirstTaskId) ];

    EnterBackground();
    // A further background-level transition must not ask again.
    EnterBackground();
    EXPECT_EQ(provider.startedContexts.count, 1u);

    EnterForeground();
    EnterBackground();
    EXPECT_EQ(provider.startedContexts.count, 2u);
    EXPECT_NSEQ((@[
                  @"A.background", @"A.requests", @"A.started:test.task.First",
                  @"A.foreground", @"A.background", @"A.requests",
                  @"A.started:test.task.First"
                ]),
                log_);

    CompleteStartedTasks(provider);
  }
}

// Tests that a removed provider is neither notified nor asked for tasks.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestRemovedProviderIsSkipped) {
  AddFakeProvider(@"A");
  FakeContinuedProcessingTaskProvider* removed = AddFakeProvider(@"B");
  [agent_ removeTaskProvider:removed];

  EnterBackground();

  EXPECT_NSEQ((@[ @"A.background", @"A.requests" ]), log_);
}

// Tests that a deallocated provider is skipped since providers are held weakly.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestDeallocatedProviderIsSkipped) {
  __weak FakeContinuedProcessingTaskProvider* weak_provider = nil;
  @autoreleasepool {
    FakeContinuedProcessingTaskProvider* provider =
        [[FakeContinuedProcessingTaskProvider alloc] initWithName:@"B"
                                                              log:log_];
    [agent_ addTaskProvider:provider];
    weak_provider = provider;
  }
  ASSERT_EQ(weak_provider, nil);
  AddFakeProvider(@"A");

  EnterBackground();

  EXPECT_NSEQ((@[ @"A.background", @"A.requests" ]), log_);
}

// Tests that removing providers while the agent asks them for tasks is safe and
// skips the removed providers.
TEST_F(BackgroundContinuedProcessingAppAgentAvailableTest,
       TestProviderRemovedDuringDispatchIsSafe) {
  FakeContinuedProcessingTaskProvider* first = AddFakeProvider(@"A");
  FakeContinuedProcessingTaskProvider* second = AddFakeProvider(@"B");
  BackgroundContinuedProcessingAppAgent* agent = agent_;
  __weak FakeContinuedProcessingTaskProvider* weak_first = first;
  __weak FakeContinuedProcessingTaskProvider* weak_second = second;
  first.onRequestsAsked = ^{
    [agent removeTaskProvider:weak_first];
    [agent removeTaskProvider:weak_second];
  };

  EnterBackground();

  EXPECT_NSEQ((@[ @"A.background", @"B.background", @"A.requests" ]), log_);
}
#endif  // BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
