// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_permission_request.h"

#import <tuple>
#import <utility>

#import "base/functional/callback_helpers.h"
#import "base/memory/ref_counted.h"
#import "base/run_loop.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/test_future.h"
#import "ios/web/public/test/fakes/fake_web_state_delegate.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/web_state/web_state_impl.h"
#import "url/gurl.h"

@interface FakeCRWPermissionPresenter : NSObject <CRWPermissionPresenter>
@property(nonatomic, assign) web::WebStateImpl* presentingWebState;
@end

@implementation FakeCRWPermissionPresenter {
  base::WeakPtr<web::WebState> _webState;
}

- (void)setPresentingWebState:(web::WebStateImpl*)presentingWebState {
  if (!presentingWebState) {
    _webState.reset();
  } else {
    _webState = presentingWebState->GetWeakPtr();
  }
}

- (web::WebStateImpl*)presentingWebState {
  if (web::WebState* webState = _webState.get()) {
    return static_cast<web::WebStateImpl*>(webState);
  }

  return nullptr;
}

@end

namespace {

const char kTestOrigin[] = "https://example.com";

// A test double SequencedTaskRunner that rejects all posted tasks.
class RejectingSequencedTaskRunner final : public base::SequencedTaskRunner {
 public:
  // TaskRunner:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) final {
    return false;
  }

  // SequencedTaskRunner:
  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) final {
    return false;
  }

  bool RunsTasksInCurrentSequence() const final { return true; }

 protected:
  ~RejectingSequencedTaskRunner() override = default;
};

class CRWPermissionRequestTest : public web::WebTestWithWebState {
 protected:
  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    web_state()->SetDelegate(&delegate_);
    delegate_.SetPermissionDecision(web::PermissionDecisionGrant);
    presenter_ = [[FakeCRWPermissionPresenter alloc] init];
    presenter_.presentingWebState =
        static_cast<web::WebStateImpl*>(web_state());
  }

  void TearDown() override {
    presenter_.presentingWebState = nullptr;
    web::WebTestWithWebState::TearDown();
  }

  FakeCRWPermissionPresenter* presenter() { return presenter_; }

  web::FakeWebStateDelegate& delegate() { return delegate_; }

 private:
  web::FakeWebStateDelegate delegate_;
  FakeCRWPermissionPresenter* presenter_;
};

// Tests that deallocation before an explicit decision defaults to Deny.
TEST_F(CRWPermissionRequestTest, DenyOnDeallocWithoutDecision) {
  base::test::TestFuture<WKPermissionDecision> future;

  @autoreleasepool {
    CRWPermissionRequest* request = [[CRWPermissionRequest alloc]
        initWithPresenter:presenter()
          decisionHandler:base::CallbackToBlock(future.GetRepeatingCallback())
             onTaskRunner:base::SequencedTaskRunner::GetCurrentDefault()];
    EXPECT_FALSE(future.IsReady());

    std::ignore = request;
  }

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(WKPermissionDecisionDeny, future.Take());
}

// Tests that requesting media capture permission without a valid WebState
// denies.
TEST_F(CRWPermissionRequestTest, MediaCaptureDenyWhenWebStateMissing) {
  base::test::TestFuture<WKPermissionDecision> future;

  CRWPermissionRequest* request = [[CRWPermissionRequest alloc]
      initWithPresenter:presenter()
        decisionHandler:base::CallbackToBlock(future.GetRepeatingCallback())
           onTaskRunner:base::SequencedTaskRunner::GetCurrentDefault()];
  EXPECT_FALSE(future.IsReady());

  presenter().presentingWebState = nullptr;
  [request displayPromptForMediaCaptureType:WKMediaCaptureTypeCamera
                                     origin:GURL(kTestOrigin)];

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Take(), WKPermissionDecisionDeny);
}

// Tests that requesting media capture permission is denied if the presenter
// is deallocated before the request is executed.
TEST_F(CRWPermissionRequestTest, MediaCaptureDenyWhenPresenterDeallocated) {
  base::test::TestFuture<WKPermissionDecision> future;

  CRWPermissionRequest* request = [[CRWPermissionRequest alloc]
      initWithPresenter:[[FakeCRWPermissionPresenter alloc] init]
        decisionHandler:base::CallbackToBlock(future.GetRepeatingCallback())
           onTaskRunner:base::SequencedTaskRunner::GetCurrentDefault()];
  EXPECT_FALSE(future.IsReady());

  [request displayPromptForMediaCaptureType:WKMediaCaptureTypeCamera
                                     origin:GURL(kTestOrigin)];

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Take(), WKPermissionDecisionDeny);
}

// Tests that requesting media capture permission is denied if the TaskRunner
// does not execute the posted task (e.g. PostTask(...) fails because the
// TaskRunner is being destroyed).
TEST_F(CRWPermissionRequestTest, MediaCaptureDenyWhenTaskRunnerFails) {
  base::test::TestFuture<WKPermissionDecision> future;

  CRWPermissionRequest* request = [[CRWPermissionRequest alloc]
      initWithPresenter:presenter()
        decisionHandler:base::CallbackToBlock(future.GetRepeatingCallback())
           onTaskRunner:base::MakeRefCounted<RejectingSequencedTaskRunner>()];
  EXPECT_FALSE(future.IsReady());

  [request displayPromptForMediaCaptureType:WKMediaCaptureTypeCamera
                                     origin:GURL(kTestOrigin)];

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Take(), WKPermissionDecisionDeny);
}

// Tests that multiple decision attempts for media capture only invoke the
// callback once (base::test::TestFuture<T> asserts that it is only called
// once before the value is consumed).
TEST_F(CRWPermissionRequestTest, MediaCaptureSingleDecisionInvoked) {
  base::test::TestFuture<WKPermissionDecision> future;

  CRWPermissionRequest* request = [[CRWPermissionRequest alloc]
      initWithPresenter:presenter()
        decisionHandler:base::CallbackToBlock(future.GetRepeatingCallback())
           onTaskRunner:base::SequencedTaskRunner::GetCurrentDefault()];
  EXPECT_FALSE(future.IsReady());

  [request displayPromptForMediaCaptureType:WKMediaCaptureTypeCamera
                                     origin:GURL(kTestOrigin)];

  [request displayPromptForMediaCaptureType:WKMediaCaptureTypeCamera
                                     origin:GURL(kTestOrigin)];

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Take(), WKPermissionDecisionGrant);

  // Verify that there are no other tasks in flight by posting one task,
  // and waiting for it to run. Since the task runner is sequenced, if
  // the second call to -displayPromptForMediaCaptureType:origin: had
  // scheduled a task, then it should execute before this new one.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(future.IsReady());
}

// Tests that when WebState grants permission, the decision handler is invoked
// with WKPermissionDecisionGrant.
TEST_F(CRWPermissionRequestTest,
       MediaCaptureGrantedWhenWebStateGrantsPermission) {
  base::test::TestFuture<WKPermissionDecision> future;

  CRWPermissionRequest* request = [[CRWPermissionRequest alloc]
      initWithPresenter:presenter()
        decisionHandler:base::CallbackToBlock(future.GetRepeatingCallback())
           onTaskRunner:base::SequencedTaskRunner::GetCurrentDefault()];
  EXPECT_FALSE(future.IsReady());

  [request displayPromptForMediaCaptureType:WKMediaCaptureTypeCamera
                                     origin:GURL(kTestOrigin)];

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Take(), WKPermissionDecisionGrant);
}

// Tests that when requesting geolocation permission without an available
// WebState, the decision handler is invoked with WKPermissionDecisionDeny.
TEST_F(CRWPermissionRequestTest, GeolocationDenyWhenWebStateUnavailable) {
  base::test::TestFuture<WKPermissionDecision> future;

  CRWPermissionRequest* request = [[CRWPermissionRequest alloc]
      initWithPresenter:presenter()
        decisionHandler:base::CallbackToBlock(future.GetRepeatingCallback())
           onTaskRunner:base::SequencedTaskRunner::GetCurrentDefault()];

  presenter().presentingWebState = nullptr;
  [request displayPromptForGeolocationOrigin:GURL(kTestOrigin)];

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Take(), WKPermissionDecisionDeny);
}

// Tests that when requesting geolocation permission on a destroyed WebState,
// the decision handler is invoked with WKPermissionDecisionDeny.
TEST_F(CRWPermissionRequestTest, GeolocationDenyWhenWebStateDestroyed) {
  base::test::TestFuture<WKPermissionDecision> future;

  CRWPermissionRequest* request = [[CRWPermissionRequest alloc]
      initWithPresenter:presenter()
        decisionHandler:base::CallbackToBlock(future.GetRepeatingCallback())
           onTaskRunner:base::SequencedTaskRunner::GetCurrentDefault()];

  DestroyWebState();
  [request displayPromptForGeolocationOrigin:GURL(kTestOrigin)];

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Take(), WKPermissionDecisionDeny);
}

// Tests that when WebState grants geolocation permission, the decision handler
// is invoked with WKPermissionDecisionGrant.
TEST_F(CRWPermissionRequestTest,
       GeolocationGrantedWhenWebStateGrantsPermission) {
  base::test::TestFuture<WKPermissionDecision> future;

  CRWPermissionRequest* request = [[CRWPermissionRequest alloc]
      initWithPresenter:presenter()
        decisionHandler:base::CallbackToBlock(future.GetRepeatingCallback())
           onTaskRunner:base::SequencedTaskRunner::GetCurrentDefault()];

  [request displayPromptForGeolocationOrigin:GURL(kTestOrigin)];

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Take(), WKPermissionDecisionGrant);
}
}  // namespace
