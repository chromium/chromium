// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/overlay_presenter_impl.h"

#import "ios/chrome/browser/overlays/model/overlay_request_queue_impl.h"
#import "ios/chrome/browser/overlays/model/public/overlay_modality.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter_observer.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_presentation_context.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_request_cancel_handler.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_user_data.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

namespace {
// Mock queue observer.
class MockOverlayPresenterObserver : public OverlayPresenterObserver {
 public:
  MockOverlayPresenterObserver() {}
  ~MockOverlayPresenterObserver() override {}

  MOCK_METHOD3(WillShowOverlay, void(OverlayPresenter*, OverlayRequest*, bool));
  MOCK_METHOD2(DidHideOverlay, void(OverlayPresenter*, OverlayRequest*));

  // OverlayPresenter's ObserverList checks that it is empty upon deallocation,
  // so a custom verification implemetation must be created.
  void OverlayPresenterDestroyed(OverlayPresenter* presenter) override {
    presenter_destroyed_ = true;
    presenter->RemoveObserver(this);
  }
  bool presenter_destroyed() const { return presenter_destroyed_; }

 private:
  bool presenter_destroyed_ = false;
};
}  // namespace

// Test fixture for OverlayPresenterImpl.
class OverlayPresenterImplTest : public PlatformTest {
 public:
  OverlayPresenterImplTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    OverlayPresenterImpl::Container::CreateForUserData(browser_.get(),
                                                       browser_.get());
    presenter().AddObserver(&observer_);
  }
  ~OverlayPresenterImplTest() override {
    if (browser_)
      presenter().RemoveObserver(&observer_);
  }
  WebStateList* web_state_list() { return browser_->GetWebStateList(); }
  web::WebState* active_web_state() {
    return web_state_list()->GetActiveWebState();
  }
  OverlayPresenterImpl& presenter() {
    return *OverlayPresenterImpl::Container::FromUserData(browser_.get())
                ->PresenterForModality(OverlayModality::kWebContentArea);
  }
  FakeOverlayPresentationContext& presentation_context() {
    return presentation_context_;
  }
  MockOverlayPresenterObserver& observer() { return observer_; }

  OverlayRequestQueueImpl* GetQueueForWebState(web::WebState* web_state) {
    if (!web_state)
      return nullptr;
    OverlayRequestQueueImpl::Container::CreateForWebState(web_state);
    return OverlayRequestQueueImpl::Container::FromWebState(web_state)
        ->QueueForModality(OverlayModality::kWebContentArea);
  }

  OverlayRequest* InsertRequest(web::WebState* web_state,
                                size_t index,
                                bool expect_presentation = true) {
    OverlayRequestQueueImpl* queue = GetQueueForWebState(web_state);
    if (!queue)
      return nullptr;
    std::unique_ptr<OverlayRequest> inserted_request =
        OverlayRequest::CreateWithConfig<FakeOverlayUserData>();
    OverlayRequest* request = inserted_request.get();
    if (expect_presentation)
      EXPECT_CALL(observer(), WillShowOverlay(&presenter(), request,
                                              /*initial_presentation=*/true));
    GetQueueForWebState(web_state)->InsertRequest(index,
                                                  std::move(inserted_request));
    return request;
  }

  OverlayRequest* AddRequest(web::WebState* web_state,
                             bool expect_presentation = true) {
    return InsertRequest(web_state, GetQueueForWebState(web_state)->size(),
                         expect_presentation);
  }

  void DeleteBrowser() { browser_ = nullptr; }

 private:
  web::WebTaskEnvironment task_environment_;
  FakeOverlayPresentationContext presentation_context_;
  MockOverlayPresenterObserver observer_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
};

// Tests that setting the presentation context will present overlays requested
// before the delegate is provided.
TEST_F(OverlayPresenterImplTest, PresentAfterSettingPresentationContext) {
  // Add a WebState to the list and add a request to that WebState's queue.
  web_state_list()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::Automatic().Activate());
  OverlayRequest* request = AddRequest(active_web_state());
  ASSERT_EQ(FakeOverlayPresentationContext::PresentationState::kNotPresented,
            presentation_context().GetPresentationState(request));

  // Set the UI delegate and verify that the request has been presented.
  presenter().SetPresentationContext(&presentation_context());
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(request));
  EXPECT_TRUE(presenter().IsShowingOverlayUI());
}

// Tests that requested overlays are presented when added to the active queue
// after the presentation context has been provided.
TEST_F(OverlayPresenterImplTest, PresentAfterRequestAddedToActiveQueue) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presenter().SetPresentationContext(&presentation_context());
  web_state_list()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::Automatic().Activate());
  OverlayRequest* request = AddRequest(active_web_state());
  // Verify that the requested overlay has been presented.
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(request));
  EXPECT_TRUE(presenter().IsShowingOverlayUI());
}

// Tests that the requested overlay is presented when inserted to the front of
// the active queue while already presenting overlay UI for its front request.
TEST_F(OverlayPresenterImplTest,
       PresentAfterRequestInsertedToFrontOfActiveQueue) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presenter().SetPresentationContext(&presentation_context());
  web_state_list()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::Automatic().Activate());
  OverlayRequest* request = AddRequest(active_web_state());
  // Verify that the requested overlay has been presented.
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(request));
  EXPECT_TRUE(presenter().IsShowingOverlayUI());
  // Insert a request in front of the `request` and verify that the inserted
  // request's UI is presented while `request`'s UI is hidden.
  EXPECT_CALL(observer(), DidHideOverlay(&presenter(), request));
  OverlayRequest* inserted_request =
      InsertRequest(active_web_state(), /*index=*/0);
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(inserted_request));
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kHidden,
            presentation_context().GetPresentationState(request));
  EXPECT_TRUE(presenter().IsShowingOverlayUI());
  // Dismiss `inserted_request`'s UI check that the `request`'s UI is presented
  // again.
  EXPECT_CALL(observer(), DidHideOverlay(&presenter(), inserted_request));
  EXPECT_CALL(observer(), WillShowOverlay(&presenter(), request,
                                          /*initial_presentation=*/false));
  presentation_context().SimulateDismissalForRequest(
      inserted_request, OverlayDismissalReason::kUserInteraction);
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kUserDismissed,
            presentation_context().GetPresentationState(inserted_request));
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(request));
  EXPECT_TRUE(presenter().IsShowingOverlayUI());
}

// Tests that requested overlays are presented when the presentation context is
// activated.
TEST_F(OverlayPresenterImplTest, PresentAfterContextActivation) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presentation_context().SetPresentationCapabilities(
      OverlayPresentationContext::UIPresentationCapabilities::kNone);
  presenter().SetPresentationContext(&presentation_context());
  web_state_list()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::Automatic().Activate());
  OverlayRequest* request = AddRequest(active_web_state());
  ASSERT_EQ(FakeOverlayPresentationContext::PresentationState::kNotPresented,
            presentation_context().GetPresentationState(request));

  // Activate the presentation context and verify that the UI is presented.
  presentation_context().SetPresentationCapabilities(
      OverlayPresentationContext::UIPresentationCapabilities::kPresented);
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(request));
  EXPECT_TRUE(presenter().IsShowingOverlayUI());
}

// Tests that presented overlay UI is hidden when the presentation context is
// deactivated.
TEST_F(OverlayPresenterImplTest, HideAfterContextDeactivation) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presenter().SetPresentationContext(&presentation_context());
  web_state_list()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::Automatic().Activate());
  OverlayRequest* request = AddRequest(active_web_state());
  ASSERT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(request));
  ASSERT_TRUE(presenter().IsShowingOverlayUI());

  // Deactivate the presentation context and verify that the UI is hidden.
  presentation_context().SetPresentationCapabilities(
      OverlayPresentationContext::UIPresentationCapabilities::kNone);
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kHidden,
            presentation_context().GetPresentationState(request));
  EXPECT_FALSE(presenter().IsShowingOverlayUI());
}

// Tests resetting the presentation context.  The UI should be cancelled in the
// previous context and presented in the new context.
TEST_F(OverlayPresenterImplTest, ResetPresentationContext) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presenter().SetPresentationContext(&presentation_context());
  web_state_list()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::Automatic().Activate());
  web::WebState* web_state = active_web_state();
  OverlayRequest* request = AddRequest(web_state);
  OverlayRequestQueueImpl* queue = GetQueueForWebState(web_state);

  ASSERT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(request));
  ASSERT_TRUE(presenter().IsShowingOverlayUI());

  // Reset the presentation context and verify that the overlay UI is cancelled
  // in the previous context and presented in the new context.
  FakeOverlayPresentationContext new_presentation_context;
  EXPECT_CALL(observer(), DidHideOverlay(&presenter(), request));
  EXPECT_CALL(observer(), WillShowOverlay(&presenter(), request,
                                          /*initial_presentation=*/true));
  presenter().SetPresentationContext(&new_presentation_context);
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kCancelled,
            presentation_context().GetPresentationState(request));
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            new_presentation_context.GetPresentationState(request));
  EXPECT_EQ(request, queue->front_request());
  EXPECT_TRUE(presenter().IsShowingOverlayUI());

  // Reset the UI delegate to nullptr and verify that the overlay UI is
  // cancelled in `new_presentation_context`'s context.
  EXPECT_CALL(observer(), DidHideOverlay(&presenter(), request));
  presenter().SetPresentationContext(nullptr);
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kCancelled,
            new_presentation_context.GetPresentationState(request));
  EXPECT_EQ(request, queue->front_request());
  EXPECT_FALSE(presenter().IsShowingOverlayUI());
}

// Tests changing the active WebState while no overlays are presented over the
// current active WebState.
TEST_F(OverlayPresenterImplTest, ChangeActiveWebStateWhileNotPresenting) {
  // Add a WebState to the list and activate it.
  presenter().SetPresentationContext(&presentation_context());
  web_state_list()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::Automatic().Activate());

  // Create a new WebState with a queued request and add it as the new active
  // WebState.
  auto passed_web_state = std::make_unique<web::FakeWebState>();
  web::WebState* second_web_state = passed_web_state.get();
  OverlayRequest* request = AddRequest(second_web_state);
  web_state_list()->InsertWebState(
      std::move(passed_web_state),
      WebStateList::InsertionParams::Automatic().Activate());

  // Verify that the new active WebState's overlay is being presented.
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(request));
  EXPECT_TRUE(presenter().IsShowingOverlayUI());
}

// Tests changing the active WebState while is it presenting an overlay.
TEST_F(OverlayPresenterImplTest, ChangeActiveWebStateWhilePresenting) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presenter().SetPresentationContext(&presentation_context());
  web_state_list()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::Automatic().Activate());
  web::WebState* first_web_state = active_web_state();
  OverlayRequest* first_request = AddRequest(first_web_state);
  ASSERT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(first_request));
  ASSERT_TRUE(presenter().IsShowingOverlayUI());

  // Create a new WebState with a queued request and add it as the new active
  // WebState.
  auto passed_web_state = std::make_unique<web::FakeWebState>();
  web::WebState* second_web_state = passed_web_state.get();
  OverlayRequest* second_request = AddRequest(second_web_state);
  EXPECT_CALL(observer(), DidHideOverlay(&presenter(), first_request));
  web_state_list()->InsertWebState(
      std::move(passed_web_state),
      WebStateList::InsertionParams::Automatic().Activate());

  // Verify that the previously shown overlay is hidden and that the overlay for
  // the new active WebState is presented.
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kHidden,
            presentation_context().GetPresentationState(first_request));
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(second_request));
  EXPECT_TRUE(presenter().IsShowingOverlayUI());

  // Reactivate the first WebState and verify that its overlay is presented
  // while the second WebState's overlay is hidden.
  EXPECT_CALL(observer(), DidHideOverlay(&presenter(), second_request));
  EXPECT_CALL(observer(), WillShowOverlay(&presenter(), first_request,
                                          /*initial_presentation=*/false));
  web_state_list()->ActivateWebStateAt(0);
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(first_request));
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kHidden,
            presentation_context().GetPresentationState(second_request));
  EXPECT_TRUE(presenter().IsShowingOverlayUI());
}

// Tests replacing the active WebState while it is presenting an overlay.
TEST_F(OverlayPresenterImplTest, ReplaceActiveWebState) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presenter().SetPresentationContext(&presentation_context());
  web_state_list()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::Automatic().Activate());
  web::WebState* first_web_state = active_web_state();
  OverlayRequest* first_request = AddRequest(first_web_state);
  ASSERT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(first_request));
  ASSERT_TRUE(presenter().IsShowingOverlayUI());

  // Replace `first_web_state` with a new active WebState with a queued request.
  auto passed_web_state = std::make_unique<web::FakeWebState>();
  web::WebState* replacement_web_state = passed_web_state.get();
  OverlayRequest* replacement_request = AddRequest(replacement_web_state);
  EXPECT_CALL(observer(), DidHideOverlay(&presenter(), first_request));
  web_state_list()->ReplaceWebStateAt(/*index=*/0, std::move(passed_web_state));

  // Verify that the previously shown overlay is canceled and that the overlay
  // for the replacement WebState is presented.
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kCancelled,
            presentation_context().GetPresentationState(first_request));
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(replacement_request));
  EXPECT_TRUE(presenter().IsShowingOverlayUI());
}

// Tests removing the active WebState while it is presenting an overlay.
TEST_F(OverlayPresenterImplTest, RemoveActiveWebState) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presenter().SetPresentationContext(&presentation_context());
  web_state_list()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::Automatic().Activate());
  web::WebState* web_state = active_web_state();
  OverlayRequest* request = AddRequest(web_state);
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(request));
  EXPECT_TRUE(presenter().IsShowingOverlayUI());

  // Remove the WebState and verify that its overlay was cancelled.
  EXPECT_CALL(observer(), DidHideOverlay(&presenter(), request));
  web_state_list()->CloseWebStateAt(/*index=*/0, /* close_flags= */ 0);
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kCancelled,
            presentation_context().GetPresentationState(request));
}

// Tests detaching the active WebState while it is presenting an overlay and
// removing the presenter as the queue's delegate.
TEST_F(OverlayPresenterImplTest, DetachWebStateRemoveDelegate) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presenter().SetPresentationContext(&presentation_context());
  web_state_list()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::Automatic().Activate());
  web::WebState* web_state = active_web_state();
  OverlayRequest* request = AddRequest(web_state);
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(request));
  EXPECT_TRUE(presenter().IsShowingOverlayUI());

  presentation_context().SetDismissalCallbacksEnabled(false);
  // Remove the WebState and verify that its overlay was cancelled.
  std::unique_ptr<web::WebState> detached_web_state =
      web_state_list()->DetachWebStateAt(/*index=*/0);
  EXPECT_CALL(observer(), DidHideOverlay(&presenter(), request));
  GetQueueForWebState(detached_web_state.get())->SetDelegate(nullptr);
  presentation_context().SetDismissalCallbacksEnabled(true);
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kCancelled,
            presentation_context().GetPresentationState(request));
}

// Tests detaching the active WebState while it is presenting an overlay and
// cancelling all requests before the dismissal callback for the presenting
// overlay executes.
TEST_F(OverlayPresenterImplTest,
       DetachWebStateCancelRequestBeforeDismissalCallback) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presenter().SetPresentationContext(&presentation_context());
  web_state_list()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::Automatic().Activate());
  web::WebState* web_state = active_web_state();
  OverlayRequest* request = AddRequest(web_state);
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(request));
  EXPECT_TRUE(presenter().IsShowingOverlayUI());

  presentation_context().SetDismissalCallbacksEnabled(false);
  // Remove the WebState and verify that its overlay was cancelled.
  std::unique_ptr<web::WebState> detached_web_state =
      web_state_list()->DetachWebStateAt(/*index=*/0);
  GetQueueForWebState(detached_web_state.get())->CancelAllRequests();
  EXPECT_CALL(observer(), DidHideOverlay(&presenter(), request));
  presentation_context().SetDismissalCallbacksEnabled(true);
}

// Tests detaching the active WebState while it is presenting an overlay and
// executing the dismissal callback after detachment.
TEST_F(OverlayPresenterImplTest,
       DetachWebStateDismissalCallbackCallsDidHideOverlay) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presenter().SetPresentationContext(&presentation_context());
  web_state_list()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::Automatic().Activate());
  web::WebState* web_state = active_web_state();
  OverlayRequest* request = AddRequest(web_state);
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(request));
  EXPECT_TRUE(presenter().IsShowingOverlayUI());

  presentation_context().SetDismissalCallbacksEnabled(false);
  // Remove the WebState and verify that its overlay was cancelled.
  std::unique_ptr<web::WebState> detached_web_state =
      web_state_list()->DetachWebStateAt(/*index=*/0);
  EXPECT_CALL(observer(), DidHideOverlay(&presenter(), request));
  presentation_context().SetDismissalCallbacksEnabled(true);
}

// Tests dismissing an overlay for user interaction.
TEST_F(OverlayPresenterImplTest, DismissForUserInteraction) {
  // Add a WebState to the list and add two request to that WebState's queue.
  presenter().SetPresentationContext(&presentation_context());
  web_state_list()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::Automatic().Activate());
  web::WebState* web_state = active_web_state();
  OverlayRequestQueueImpl* queue = GetQueueForWebState(web_state);
  OverlayRequest* first_request = AddRequest(web_state);
  OverlayRequest* second_request = AddRequest(web_state);

  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(first_request));
  EXPECT_TRUE(presenter().IsShowingOverlayUI());
  EXPECT_EQ(first_request, queue->front_request());
  EXPECT_EQ(2U, queue->size());

  // Dismiss the overlay and check that the second request's overlay is
  // presented.
  EXPECT_CALL(observer(), DidHideOverlay(&presenter(), first_request));
  presentation_context().SimulateDismissalForRequest(
      first_request, OverlayDismissalReason::kUserInteraction);

  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kUserDismissed,
            presentation_context().GetPresentationState(first_request));
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(second_request));
  EXPECT_EQ(second_request, queue->front_request());
  EXPECT_EQ(1U, queue->size());
  EXPECT_TRUE(presenter().IsShowingOverlayUI());
}

// Tests cancelling the requests.
TEST_F(OverlayPresenterImplTest, CancelRequests) {
  // Add a WebState to the list and a request to that WebState's queue.
  presenter().SetPresentationContext(&presentation_context());
  web_state_list()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::Automatic().Activate());
  OverlayRequestQueueImpl* queue = GetQueueForWebState(active_web_state());
  OverlayRequest* active_request = AddRequest(active_web_state());
  OverlayRequest* queued_request =
      AddRequest(active_web_state(), /*expect_presentation=*/false);

  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kPresented,
            presentation_context().GetPresentationState(active_request));
  ASSERT_TRUE(presenter().IsShowingOverlayUI());

  // Cancel the queue's requests and verify that the UI is also cancelled.
  EXPECT_CALL(observer(), DidHideOverlay(&presenter(), active_request));
  queue->CancelAllRequests();
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kCancelled,
            presentation_context().GetPresentationState(active_request));
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kCancelled,
            presentation_context().GetPresentationState(queued_request));
}

// Tests that the presented UI is correctly hidden if the presentation
// capabilities change to kNone during the dismissal of overlay UI whose request
// is owned by an inactive WebState.
TEST_F(OverlayPresenterImplTest,
       ChangePresentationCapabilitiesDuringDismissalForInactiveWebState) {
  // Insert an activated WebState to the list and add a request.
  presenter().SetPresentationContext(&presentation_context());
  web_state_list()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::Automatic().Activate());
  OverlayRequest* request = AddRequest(active_web_state());

  // Disable dismissal callbacks in the fake context and activate a new
  // WebState.
  presentation_context().SetDismissalCallbacksEnabled(false);
  web_state_list()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::Automatic().Activate());

  // Reset the presentation capabilities to kNone.  Since the context can no
  // longer support presenting `request`, it will be hidden.
  presentation_context().SetPresentationCapabilities(
      OverlayPresentationContext::UIPresentationCapabilities::kNone);

  // Re-enable dismissal callbacks to dimulate the dismissal finishing after the
  // presentation capability reset.
  EXPECT_CALL(observer(), DidHideOverlay(&presenter(), request));
  presentation_context().SetDismissalCallbacksEnabled(true);
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kHidden,
            presentation_context().GetPresentationState(request));
}

// Tests that the presented UI is correctly hidden if the presentation
// capabilities change to kNone during the dismissal of overlay UI whose request
// that was cancelled.
TEST_F(OverlayPresenterImplTest,
       ChangePresentationCapabilitiesDuringDismissalForCancelledRequest) {
  // Insert an activated WebState to the list.
  presenter().SetPresentationContext(&presentation_context());
  web_state_list()->InsertWebState(
      std::make_unique<web::FakeWebState>(),
      WebStateList::InsertionParams::Automatic().Activate());
  OverlayRequestQueueImpl* queue = GetQueueForWebState(active_web_state());

  // Add a request with a fake cancel handler.
  std::unique_ptr<OverlayRequest> inserted_request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>();
  OverlayRequest* request = inserted_request.get();
  std::unique_ptr<FakeOverlayRequestCancelHandler> inserted_cancel_handler =
      std::make_unique<FakeOverlayRequestCancelHandler>(request, queue);
  FakeOverlayRequestCancelHandler* cancel_handler =
      inserted_cancel_handler.get();
  EXPECT_CALL(observer(), WillShowOverlay(&presenter(), request,
                                          /*initial_presentation=*/true));
  queue->AddRequest(std::move(inserted_request));

  // Disable dismissal callbacks in the fake context and cancel the request.
  presentation_context().SetDismissalCallbacksEnabled(false);
  cancel_handler->TriggerCancellation();
  EXPECT_FALSE(queue->front_request());

  // Reset the presentation capabilities to kNone.  Since the context can no
  // longer support presenting `request`, it will be hidden.
  presentation_context().SetPresentationCapabilities(
      OverlayPresentationContext::UIPresentationCapabilities::kNone);

  // Re-enable dismissal callbacks to dimulate the dismissal finishing after the
  // presentation capability reset.
  EXPECT_CALL(observer(), DidHideOverlay(&presenter(), request));
  presentation_context().SetDismissalCallbacksEnabled(true);
  EXPECT_EQ(FakeOverlayPresentationContext::PresentationState::kHidden,
            presentation_context().GetPresentationState(request));
}

// Tests that deleting the presenter
TEST_F(OverlayPresenterImplTest, PresenterWasDestroyed) {
  DeleteBrowser();
  EXPECT_TRUE(observer().presenter_destroyed());
}
