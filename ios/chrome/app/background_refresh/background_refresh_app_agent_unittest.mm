// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/background_refresh/background_refresh_app_agent.h"

#import <BackgroundTasks/BackgroundTasks.h>

#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/background_refresh/app_refresh_provider.h"
#import "ios/chrome/app/background_refresh/background_refresh_app_agent_audience.h"
#import "ios/chrome/app/background_refresh_constants.h"
#import "ios/chrome/browser/profile/model/ios_chrome_io_thread.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

typedef void (^TaskHandlerBlock)(BGTask*);
typedef void (^TaskExpirationBlock)();

// Refresh provider task stub whose status can be verified.
@interface VerifiableTask : NSObject <AppRefreshProviderTask>
@property(nonatomic, readonly) BOOL executed;
@end
@implementation VerifiableTask {
  // This ivar may be read and written on different threads, so all access
  // should be synchronized.
  BOOL _executed;
}
- (BOOL)executed {
  @synchronized(self) {
    return _executed;
  }
}
- (void)execute {
  @synchronized(self) {
    _executed = YES;
  }
}
@end

// Refresh provider task that delays execution long enough to be canceled.
@interface ProlongedTask : VerifiableTask
@end
@implementation ProlongedTask
- (void)execute {
  sleep(1);
  [super execute];
}
@end

// Refresh provider for use in tests that allows a task to be injected, and is
// always due.
@interface TestRefreshProvider : AppRefreshProvider
@property(nonatomic, strong) VerifiableTask* injectedTask;
@end
@implementation TestRefreshProvider
- (instancetype)init {
  if ((self = [super init])) {
    _injectedTask = [[VerifiableTask alloc] init];
  }
  return self;
}
- (NSString*)identifier {
  return NSStringFromClass([self class]);
}
- (BOOL)isDue {
  return YES;
}
- (id<AppRefreshProviderTask>)task {
  return self.injectedTask;
}
@end

// Test refresh provider that runs on the UI thread.
@interface TestUIThreadRefreshProvider : TestRefreshProvider
@end
@implementation TestUIThreadRefreshProvider
- (scoped_refptr<base::SingleThreadTaskRunner>)taskThread {
  return web::GetUIThreadTaskRunner({});
}
@end

// Test refresh provider that runs on an arbitrary thread.
@interface TestOtherThreadRefreshProvider : TestRefreshProvider
@end
@implementation TestOtherThreadRefreshProvider {
  scoped_refptr<base::SingleThreadTaskRunner> _thread;
}
- (instancetype)init {
  if ((self = [super init])) {
    _thread = base::ThreadPool::CreateSingleThreadTaskRunner({});
  }
  return self;
}
- (scoped_refptr<base::SingleThreadTaskRunner>)taskThread {
  return _thread;
}
@end

// Test refresh provider that is never due.
@interface TestNotDueRefreshProvider : TestRefreshProvider
@end
@implementation TestNotDueRefreshProvider
- (BOOL)isDue {
  return NO;
}
@end

// Test audience for the background refresh agent.
// It (a) records that the start and end callbacks are called, and
//    (b) quits the injected runloop when the end callback is made.
@interface TestRefreshAudience : NSObject <BackgroundRefreshAudience>
@property(nonatomic) base::RunLoop* runLoop;
@property(nonatomic) BOOL started;
@property(nonatomic) BOOL ended;
@end
@implementation TestRefreshAudience
- (void)backgroundRefreshDidStart {
  _started = YES;
}
- (void)backgroundRefreshDidEnd {
  _ended = YES;
  _runLoop->Quit();
}
@end

// Notes on the structure of these tests:
// - Mocks are used for the iOS classes BGTaskScheduler and BGTask.
// - Both of these classes are configured with callbacks (blocks) by the
//   code under test, and these callbacks are executed on non-main threads
//   determined by the OS. (It is possible to customise the thread that is used,
//   but the current implementation doesn't do that).
// - In order to properly test these callbacks, the mock implementations of the
//   system BG tasks capture the blocks when they are configured. The test class
//   provides helpers to execute the blocks on a dedicated task runner to
//   emulate the OS's calling beahvior.
// - The expected behavior of refresh providers is to run tasks on non-main
//   threads.
// - The tests themselves run on the main thread and use a base::RunLoop to
//   let the configured callbacks and providers execute on other threads.
// - The test refresh audience injected into the app agent under test is
//   responsible for quitting the runloop used by the tests.
class BackgroundRefreshAppAgentTest : public PlatformTest {
 protected:
  BackgroundRefreshAppAgentTest() {
    TestingApplicationContext* application_context =
        TestingApplicationContext::GetGlobal();

    // Configure IO thread -- from profile manager test.
    chrome_io_ = std::make_unique<IOSChromeIOThread>(
        application_context->GetLocalState(), application_context->GetNetLog());
    application_context->SetIOSChromeIOThread(chrome_io_.get());
    web_task_environment_.StartIOThread();
    web::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&IOSChromeIOThread::InitOnIO,
                                  base::Unretained(chrome_io_.get())));
    std::ignore = chrome_io_->system_url_request_context_getter();

    // Thread to simulate how the iOS scheduler calls handlers.
    scheduler_callback_runner_ =
        base::ThreadPool::CreateSequencedTaskRunner({});

    BuildMockTaskScheduler();
    BuildMockTask();

    agent_ = [[BackgroundRefreshAppAgent alloc] init];
    audience_ = [[TestRefreshAudience alloc] init];
    agent_.audience = audience_;
    audience_.runLoop = &run_loop_;
  }

  ~BackgroundRefreshAppAgentTest() override {
    TestingApplicationContext* application_context =
        TestingApplicationContext::GetGlobal();

    application_context->GetIOSChromeIOThread()->NetworkTearDown();
    application_context->SetIOSChromeIOThread(nullptr);
  }

  void SetUp() override {}

  void BuildMockTaskScheduler() {
    task_scheduler_mock_ = OCMStrictClassMock([BGTaskScheduler class]);
    // Stub the class `sharedScheduler` method to return the mock instance.
    OCMStub([task_scheduler_mock_ sharedScheduler])
        .andReturn(task_scheduler_mock_);
    // Stub the registration method to capture the configured block.
    OCMStub(
        [task_scheduler_mock_
            registerForTaskWithIdentifier:kAppBackgroundRefreshTaskIdentifier
                               usingQueue:[OCMArg isNil]
                            launchHandler:OCMOCK_ANY])
        .andDo(^(NSInvocation* invocation) {
          __weak TaskHandlerBlock handler;
          [invocation getArgument:&handler atIndex:4];
          task_handler_ = [handler copy];
        });
  }

  void BuildMockTask() {
    task_mock_ = OCMClassMock([BGTask class]);
    // Stub the task property setter to capture the configured block.
    // This is called on the scheduler callback runner, not the main thread, so
    // tests that need this captured block (specifically tests that call
    // `ExpireTask()` need to wait until this call completes before accessing
    // the expiration handler. See the ExpireTask test for an example.
    OCMStub([task_mock_ setExpirationHandler:OCMOCK_ANY])
        .andDo(^(NSInvocation* invocation) {
          __weak TaskExpirationBlock handler;
          [invocation getArgument:&handler atIndex:2];
          // This write to a member variable is done on a non-main sequence;
          // see the notes above on correctly ensuring that it completes before
          // the member is accessed by `ExpireTask()`.
          task_expiration_handler_ = [handler copy];
        });
  }

  // Simulate the app entering the background by calling the relevant
  // `SceneObservingAppAgent` observer method on the agent under test.
  // Since background refresh is scheduled when the app is backgrounded, this
  // should trigger most of the useful behavior for the agent.
  void SimulateAppBackgrounding() { [agent_ appDidEnterBackground]; }

  // Call the handler passed as the `launchHandler` into `BGTaskScheduler`.
  // Post it to the callback runner thread.
  void InvokeTaskHandler() {
    base::OnceCallback callback = base::BindOnce(task_handler_, task_mock_);
    scheduler_callback_runner_->PostTask(FROM_HERE, std::move(callback));
  }

  // Call the handler passed as the `launchHandler` into `BGTaskScheduler` on
  // the callback runner thread,  and then call `thenCallback` on the main
  // thread.
  void InvokeTaskHandlerThen(base::OnceClosure thenCallback) {
    base::OnceCallback callback = base::BindOnce(task_handler_, task_mock_);
    scheduler_callback_runner_->PostTask(
        FROM_HERE, std::move(callback).Then(std::move(thenCallback)));
  }

  // Call the handler passed as the `expirationHandler` into `BGTask`.
  // Post it to the callback runner thread. It's an error to call this without
  // allowing the handler to be set on the callback runner thread.
  void ExpireTask() {
    ASSERT_TRUE(task_expiration_handler_);
    base::OnceCallback callback = base::BindOnce(task_expiration_handler_);
    scheduler_callback_runner_->PostTask(FROM_HERE, std::move(callback));
  }

  // Wrapper around OCMock's `verify` that catches exceptions without halting
  // the test run. This is the same as the EXPECT_OCMOCK_VERIFY macro, but
  // that macro doesn't allow for specific mocked method call expectations.
  void VerifyTaskCompleted(BOOL completed) {
    @try {
      [[task_mock_ verify] setTaskCompletedWithSuccess:completed];
    } @catch (NSException* e) {
      ADD_FAILURE() << "OCMock validation failed: "
                    << base::SysNSStringToUTF8([e description]);
    }
  }

  // By default, enable background refresh for all tests.
  base::test::ScopedFeatureList scoped_feature_list_{
      kEnableAppBackgroundRefresh};
  // Local state for test application context.
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  // Threads.
  std::unique_ptr<IOSChromeIOThread> chrome_io_;
  web::WebTaskEnvironment web_task_environment_{
      web::WebTaskEnvironment::IOThreadType::REAL_THREAD_DELAYED};
  scoped_refptr<base::SequencedTaskRunner> scheduler_callback_runner_;
  // Common run loop for tests.
  base::RunLoop run_loop_;

  // Mocks and the handlers they are configured with.
  id task_scheduler_mock_;
  TaskHandlerBlock task_handler_;
  id task_mock_;
  TaskExpirationBlock task_expiration_handler_;

  // Object under test and its audience.
  BackgroundRefreshAppAgent* agent_;
  TestRefreshAudience* audience_;
};

TEST_F(BackgroundRefreshAppAgentTest, ScheduleOnBackground) {
  // Test that moving to a background state schedules a refresh.
  // Test that when no providers are registered, background refresh is still
  // marked as successful.
  OCMStub([task_scheduler_mock_ submitTaskRequest:OCMOCK_ANY
                                            error:[OCMArg setTo:nil]]);

  SimulateAppBackgrounding();
  InvokeTaskHandler();

  run_loop_.Run();

  EXPECT_TRUE(audience_.started);
  VerifyTaskCompleted(YES);
  EXPECT_TRUE(audience_.ended);
}

TEST_F(BackgroundRefreshAppAgentTest, ExecuteSingleTask) {
  // Test that when a single provider is registered, that task executes and the
  // refresh is marked successful.
  TestRefreshProvider* provider = [[TestRefreshProvider alloc] init];
  [agent_ addAppRefreshProvider:provider];

  OCMStub([task_scheduler_mock_ submitTaskRequest:OCMOCK_ANY
                                            error:[OCMArg setTo:nil]]);
  OCMStub([task_mock_ setTaskCompletedWithSuccess:YES]);

  SimulateAppBackgrounding();
  InvokeTaskHandler();

  run_loop_.Run();

  EXPECT_TRUE(audience_.started);
  VerifyTaskCompleted(YES);
  EXPECT_TRUE(audience_.ended);
  EXPECT_TRUE([provider injectedTask].executed);
}

TEST_F(BackgroundRefreshAppAgentTest, NotExecuteNotDueTask) {
  // Test that when the only provider is not due, that provider is not executed,
  // and that the BGTask is marked successful.
  TestNotDueRefreshProvider* notDueProvider =
      [[TestNotDueRefreshProvider alloc] init];
  [agent_ addAppRefreshProvider:notDueProvider];

  OCMStub([task_scheduler_mock_ submitTaskRequest:OCMOCK_ANY
                                            error:[OCMArg setTo:nil]]);
  OCMStub([task_mock_ setTaskCompletedWithSuccess:YES]);

  SimulateAppBackgrounding();
  InvokeTaskHandler();

  run_loop_.Run();

  EXPECT_TRUE(audience_.started);
  VerifyTaskCompleted(YES);
  EXPECT_TRUE(audience_.ended);
  EXPECT_FALSE([notDueProvider injectedTask].executed);
}

TEST_F(BackgroundRefreshAppAgentTest, ExecuteTwoTasks) {
  // Test that when two providers are configured, both of them are executed and
  // the task is marked successful.
  TestRefreshProvider* provider1 = [[TestRefreshProvider alloc] init];
  [agent_ addAppRefreshProvider:provider1];
  TestRefreshProvider* provider2 = [[TestRefreshProvider alloc] init];
  [agent_ addAppRefreshProvider:provider2];

  OCMStub([task_scheduler_mock_ submitTaskRequest:OCMOCK_ANY
                                            error:[OCMArg setTo:nil]]);
  OCMStub([task_mock_ setTaskCompletedWithSuccess:YES]);

  SimulateAppBackgrounding();
  InvokeTaskHandler();

  run_loop_.Run();

  EXPECT_TRUE(audience_.started);
  VerifyTaskCompleted(YES);
  EXPECT_TRUE(audience_.ended);
  EXPECT_TRUE([provider1 injectedTask].executed);
  EXPECT_TRUE([provider2 injectedTask].executed);
}

TEST_F(BackgroundRefreshAppAgentTest, ExecuteThreeTasksOnDifferentThreads) {
  // Test that when three providers are configured, each of them using different
  // execution threads, all of them are executed and the task is marked
  // successful.
  TestRefreshProvider* provider1 = [[TestRefreshProvider alloc] init];
  [agent_ addAppRefreshProvider:provider1];
  TestUIThreadRefreshProvider* provider2 =
      [[TestUIThreadRefreshProvider alloc] init];
  [agent_ addAppRefreshProvider:provider2];
  TestOtherThreadRefreshProvider* provider3 =
      [[TestOtherThreadRefreshProvider alloc] init];
  [agent_ addAppRefreshProvider:provider3];

  OCMStub([task_scheduler_mock_ submitTaskRequest:OCMOCK_ANY
                                            error:[OCMArg setTo:nil]]);
  OCMStub([task_mock_ setTaskCompletedWithSuccess:YES]);

  SimulateAppBackgrounding();
  InvokeTaskHandler();

  run_loop_.Run();

  EXPECT_TRUE(audience_.started);
  VerifyTaskCompleted(YES);
  EXPECT_TRUE(audience_.ended);
  EXPECT_TRUE([provider1 injectedTask].executed);
  EXPECT_TRUE([provider2 injectedTask].executed);
  EXPECT_TRUE([provider3 injectedTask].executed);
}

TEST_F(BackgroundRefreshAppAgentTest, HandleNotDueTask) {
  // Test that when both as due and non-due provider are configured, only the
  // due provider is executed.
  TestRefreshProvider* provider = [[TestRefreshProvider alloc] init];
  [agent_ addAppRefreshProvider:provider];
  TestNotDueRefreshProvider* notDueProvider =
      [[TestNotDueRefreshProvider alloc] init];
  [agent_ addAppRefreshProvider:notDueProvider];

  OCMStub([task_scheduler_mock_ submitTaskRequest:OCMOCK_ANY
                                            error:[OCMArg setTo:nil]]);
  OCMStub([task_mock_ setTaskCompletedWithSuccess:YES]);

  SimulateAppBackgrounding();
  InvokeTaskHandler();

  run_loop_.Run();

  EXPECT_TRUE(audience_.started);
  VerifyTaskCompleted(YES);
  EXPECT_TRUE(audience_.ended);
  EXPECT_TRUE([provider injectedTask].executed);
  EXPECT_FALSE([notDueProvider injectedTask].executed);
}

// TODO(crbug.com/377624966): Flaky-failing on ios-simulator
TEST_F(BackgroundRefreshAppAgentTest, DISABLED_ExpireTask) {
  // Test that when a task is running and the OS expiration method is called,
  // the task is terminated, and the task is *not* marked successful.
  // Note: The current implementation doesn't terminate the prolonged task;
  // it just stops listening for the task to finish.
  TestRefreshProvider* long_provider = [[TestRefreshProvider alloc] init];
  long_provider.injectedTask = [[ProlongedTask alloc] init];
  [agent_ addAppRefreshProvider:long_provider];

  OCMStub([task_scheduler_mock_ submitTaskRequest:OCMOCK_ANY
                                            error:[OCMArg setTo:nil]]);
  OCMStub([task_mock_ setTaskCompletedWithSuccess:YES]);

  SimulateAppBackgrounding();
  InvokeTaskHandlerThen(run_loop_.QuitClosure());
  run_loop_.Run();

  ExpireTask();
  base::RunLoop second_run_loop;
  audience_.runLoop = &second_run_loop;
  second_run_loop.Run();

  EXPECT_TRUE(audience_.started);
  VerifyTaskCompleted(NO);
  EXPECT_TRUE(audience_.ended);
  EXPECT_FALSE([long_provider injectedTask].executed);
}

TEST_F(BackgroundRefreshAppAgentTest, NoSchedulingWhenNotEnabled) {
  // Test that nothing is scheduled or executed when the flag isn't enabled.
  scoped_feature_list_.Reset();
  // Force-disable the feature regardless of its default state.
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kEnableAppBackgroundRefresh});

  TestRefreshProvider* provider = [[TestRefreshProvider alloc] init];
  [agent_ addAppRefreshProvider:provider];

  SimulateAppBackgrounding();

  @try {
    [[[task_scheduler_mock_ verify] withQuantifier:[OCMQuantifier never]]
        submitTaskRequest:OCMOCK_ANY
                    error:[OCMArg setTo:nil]];
    [[[task_mock_ verify] withQuantifier:[OCMQuantifier never]]
        setTaskCompletedWithSuccess:OCMOCK_ANY];
  } @catch (NSException* e) {
    ADD_FAILURE() << "OCMock validation failed: "
                  << base::SysNSStringToUTF8([e description]);
  }

  EXPECT_FALSE(audience_.started);
  EXPECT_FALSE(audience_.ended);
  EXPECT_FALSE([provider injectedTask].executed);
}
