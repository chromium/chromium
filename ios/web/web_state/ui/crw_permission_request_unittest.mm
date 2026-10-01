// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_permission_request.h"

#import "base/run_loop.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/web/public/test/fakes/fake_web_state_delegate.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/web_state/web_state_impl.h"
#import "url/gurl.h"

@interface FakeCRWPermissionPresenter : NSObject <CRWPermissionPresenter>
@property(nonatomic, assign) web::WebStateImpl* presentingWebState;
@end

@implementation FakeCRWPermissionPresenter
@end

namespace {

const char kTestOrigin[] = "https://example.com";

class CRWPermissionRequestTest : public web::WebTestWithWebState {
 protected:
  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    web_state()->SetDelegate(&delegate_);
  }

  CRWPermissionRequest* CreateRequest(
      id<CRWPermissionPresenter> presenter,
      void (^decision_handler)(WKPermissionDecision)) {
    return [[CRWPermissionRequest alloc]
        initWithPresenter:presenter
          decisionHandler:decision_handler
             onTaskRunner:base::SequencedTaskRunner::GetCurrentDefault()];
  }

  web::FakeWebStateDelegate delegate_;
};

// Tests that deallocation before an explicit decision defaults to Deny.
TEST_F(CRWPermissionRequestTest, DenyOnDeallocWithoutDecision) {
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  __block WKPermissionDecision decision = WKPermissionDecisionDeny;
  @autoreleasepool {
    CreateRequest(/*presenter=*/nil, ^(WKPermissionDecision wk_decision) {
      decision = wk_decision;
      quit_closure.Run();
    });
  }
  run_loop.Run();
  EXPECT_EQ(decision, WKPermissionDecisionDeny);
}

// Tests that requesting media capture permission without a valid WebState
// denies.
TEST_F(CRWPermissionRequestTest, MediaCaptureDenyWhenWebStateMissing) {
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  __block WKPermissionDecision decision = WKPermissionDecisionDeny;
  FakeCRWPermissionPresenter* presenter =
      [[FakeCRWPermissionPresenter alloc] init];
  CRWPermissionRequest* request =
      CreateRequest(presenter, ^(WKPermissionDecision wk_decision) {
        decision = wk_decision;
        quit_closure.Run();
      });
  [request displayPromptForMediaCaptureType:WKMediaCaptureTypeCamera
                                     origin:GURL(kTestOrigin)];
  run_loop.Run();
  EXPECT_EQ(decision, WKPermissionDecisionDeny);
}

// Tests that multiple decision attempts for media capture only invoke the
// callback once.
TEST_F(CRWPermissionRequestTest, MediaCaptureSingleDecisionInvoked) {
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  __block int call_count = 0;
  __block WKPermissionDecision decision = WKPermissionDecisionDeny;
  @autoreleasepool {
    CRWPermissionRequest* request =
        CreateRequest(/*presenter=*/nil, ^(WKPermissionDecision wk_decision) {
          ++call_count;
          decision = wk_decision;
          quit_closure.Run();
        });
    [request displayPromptForMediaCaptureType:WKMediaCaptureTypeCamera
                                       origin:GURL(kTestOrigin)];
    [request displayPromptForMediaCaptureType:WKMediaCaptureTypeCamera
                                       origin:GURL(kTestOrigin)];
  }
  run_loop.Run();
  EXPECT_EQ(call_count, 1);
  EXPECT_EQ(decision, WKPermissionDecisionDeny);
}

// Tests that when WebState grants permission, the decision handler is invoked
// with WKPermissionDecisionGrant.
TEST_F(CRWPermissionRequestTest,
       MediaCaptureGrantedWhenWebStateGrantsPermission) {
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  __block WKPermissionDecision decision = WKPermissionDecisionDeny;

  FakeCRWPermissionPresenter* presenter =
      [[FakeCRWPermissionPresenter alloc] init];
  presenter.presentingWebState = static_cast<web::WebStateImpl*>(web_state());

  CRWPermissionRequest* request =
      CreateRequest(presenter, ^(WKPermissionDecision wk_decision) {
        decision = wk_decision;
        quit_closure.Run();
      });

  delegate_.SetPermissionDecision(web::PermissionDecisionGrant);

  [request displayPromptForMediaCaptureType:WKMediaCaptureTypeCamera
                                     origin:GURL(kTestOrigin)];

  run_loop.Run();
  EXPECT_EQ(decision, WKPermissionDecisionGrant);
}

// Tests that when requesting geolocation permission without an available
// WebState, the decision handler is invoked with WKPermissionDecisionDeny.
TEST_F(CRWPermissionRequestTest, GeolocationDenyWhenWebStateUnavailable) {
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  __block WKPermissionDecision decision = WKPermissionDecisionDeny;

  FakeCRWPermissionPresenter* presenter =
      [[FakeCRWPermissionPresenter alloc] init];

  CRWPermissionRequest* request =
      CreateRequest(presenter, ^(WKPermissionDecision wk_decision) {
        decision = wk_decision;
        quit_closure.Run();
      });

  delegate_.SetPermissionDecision(web::PermissionDecisionGrant);

  [request displayPromptForGeolocationOrigin:GURL(kTestOrigin)];

  run_loop.Run();
  EXPECT_EQ(decision, WKPermissionDecisionDeny);
}

// Tests that when requesting geolocation permission on a destroyed WebState,
// the decision handler is invoked with WKPermissionDecisionDeny.
TEST_F(CRWPermissionRequestTest, GeolocationDenyWhenWebStateDestroyed) {
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  __block WKPermissionDecision decision = WKPermissionDecisionGrant;

  FakeCRWPermissionPresenter* presenter =
      [[FakeCRWPermissionPresenter alloc] init];
  DestroyWebState();
  presenter.presentingWebState = static_cast<web::WebStateImpl*>(web_state());

  CRWPermissionRequest* request =
      CreateRequest(presenter, ^(WKPermissionDecision wk_decision) {
        decision = wk_decision;
        quit_closure.Run();
      });

  [request displayPromptForGeolocationOrigin:GURL(kTestOrigin)];

  run_loop.Run();
  EXPECT_EQ(decision, WKPermissionDecisionDeny);
}

// Tests that when WebState grants geolocation permission, the decision handler
// is invoked with WKPermissionDecisionGrant.
TEST_F(CRWPermissionRequestTest,
       GeolocationGrantedWhenWebStateGrantsPermission) {
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  __block WKPermissionDecision decision = WKPermissionDecisionDeny;

  FakeCRWPermissionPresenter* presenter =
      [[FakeCRWPermissionPresenter alloc] init];
  presenter.presentingWebState = static_cast<web::WebStateImpl*>(web_state());

  CRWPermissionRequest* request =
      CreateRequest(presenter, ^(WKPermissionDecision wk_decision) {
        decision = wk_decision;
        quit_closure.Run();
      });

  delegate_.SetPermissionDecision(web::PermissionDecisionGrant);

  [request displayPromptForGeolocationOrigin:GURL(kTestOrigin)];

  run_loop.Run();
  EXPECT_EQ(decision, WKPermissionDecisionGrant);
}
}  // namespace
