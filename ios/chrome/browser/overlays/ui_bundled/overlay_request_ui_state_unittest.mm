// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_ui_state.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_user_data.h"
#import "ios/chrome/browser/overlays/ui_bundled/test/fake_overlay_request_coordinator.h"
#import "ios/chrome/browser/overlays/ui_bundled/test/fake_overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/platform_test.h"

// Test fixture for OverlayRequestUIState.
class OverlayRequestUIStateTest : public PlatformTest {
 public:
  OverlayRequestUIStateTest()
      : PlatformTest(),
        profile_(TestProfileIOS::Builder().Build()),
        browser_(std::make_unique<TestBrowser>(profile_.get())),
        request_(OverlayRequest::CreateWithConfig<FakeOverlayUserData>()),
        state_(request_.get()) {}

  OverlayRequest* request() { return request_.get(); }
  OverlayRequestUIState& state() { return state_; }

 private:
  base::test::TaskEnvironment task_environment_;

 protected:
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<OverlayRequest> request_;
  OverlayRequestUIState state_;
};

// Tests that OverlayPresentionRequested() passes the callback to the state and
// resets the default dismissal reason to kUserInteraction.
TEST_F(OverlayRequestUIStateTest, OverlayPresentionRequested) {
  ASSERT_FALSE(state().has_callback());
  state().OverlayPresentionRequested(base::BindOnce(^{
                                     }),
                                     base::BindOnce(^(OverlayDismissalReason){
                                     }));
  EXPECT_TRUE(state().has_callback());
  EXPECT_EQ(OverlayDismissalReason::kUserInteraction,
            state().dismissal_reason());
}

// Tests that OverlayUIWillBePresented() stores the coordinator in the state.
TEST_F(OverlayRequestUIStateTest, OverlayUIWillBePresented) {
  FakeOverlayRequestCoordinatorDelegate delegate;
  FakeOverlayRequestCoordinator* coordinator =
      [[FakeOverlayRequestCoordinator alloc]
          initWithBaseViewController:nil
                             browser:browser_.get()
                             request:request()
                            delegate:&delegate];
  state().OverlayUIWillBePresented(coordinator);
  EXPECT_EQ(coordinator, state().coordinator());
}

// Tests that OverlayUIWasPresented() correctly updates has_ui_been_presented().
TEST_F(OverlayRequestUIStateTest, OverlayUIWasPresented) {
  ASSERT_FALSE(state().has_ui_been_presented());
  __block bool presentation_callback_executed = false;
  state().OverlayPresentionRequested(base::BindOnce(^{
                                       presentation_callback_executed = true;
                                     }),
                                     base::BindOnce(^(OverlayDismissalReason){
                                     }));
  state().OverlayUIWasPresented();
  EXPECT_TRUE(state().has_ui_been_presented());
  EXPECT_TRUE(presentation_callback_executed);
}

// Tests that OverlayUIWasDismissed() executes the dismissal callback.
TEST_F(OverlayRequestUIStateTest, OverlayUIWasDismissed) {
  __block bool dismissal_callback_executed = false;
  state().OverlayPresentionRequested(base::BindOnce(^{
                                     }),
                                     base::BindOnce(^(OverlayDismissalReason) {
                                       dismissal_callback_executed = true;
                                     }));
  FakeOverlayRequestCoordinatorDelegate delegate;
  FakeOverlayRequestCoordinator* coordinator =
      [[FakeOverlayRequestCoordinator alloc]
          initWithBaseViewController:nil
                             browser:browser_.get()
                             request:request()
                            delegate:&delegate];
  state().OverlayUIWillBePresented(coordinator);
  state().OverlayUIWasPresented();
  state().OverlayUIWasDismissed();
  EXPECT_TRUE(dismissal_callback_executed);
}
