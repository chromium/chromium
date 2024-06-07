// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/infobar_modal_completion_notifier.h"

#import "base/scoped_observation.h"
#import "ios/chrome/browser/infobars/model/test/fake_infobar_ios.h"
#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_request_cancel_handler.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

namespace {
// Mock InfobarModalCompletionNotifier::Observer for tests.
class MockInfobarModalCompletionNotifierObserver
    : public InfobarModalCompletionNotifier::Observer {
 public:
  MockInfobarModalCompletionNotifierObserver() = default;
  ~MockInfobarModalCompletionNotifierObserver() override = default;

  MOCK_METHOD2(InfobarModalsCompleted,
               void(InfobarModalCompletionNotifier* notifier,
                    InfoBarIOS* infobar));
};
}  // namespace

// Test fixture for InfobarModalCompletionNotifier.
class InfobarModalCompletionNotifierTest : public PlatformTest {
 public:
  InfobarModalCompletionNotifierTest() {
    OverlayRequestQueue::CreateForWebState(&web_state_);
    notifier_ = std::make_unique<InfobarModalCompletionNotifier>(&web_state_);
    scoped_observation_.Observe(notifier_.get());
  }

  OverlayRequestQueue* queue() {
    return OverlayRequestQueue::FromWebState(&web_state_,
                                             OverlayModality::kInfobarModal);
  }

 protected:
  web::FakeWebState web_state_;
  FakeInfobarIOS infobar_;
  std::unique_ptr<InfobarModalCompletionNotifier> notifier_;
  MockInfobarModalCompletionNotifierObserver observer_;
  base::ScopedObservation<InfobarModalCompletionNotifier,
                          InfobarModalCompletionNotifier::Observer>
      scoped_observation_{&observer_};
};

// Tests that the observer is notified when all modal requests for `infobar_`
// have been removed.
TEST_F(InfobarModalCompletionNotifierTest, ModalCompletion) {
  // Add a modal request for `infobar_`.
  std::unique_ptr<OverlayRequest> modal_request =
      OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(
          &infobar_, InfobarOverlayType::kModal, false);
  std::unique_ptr<FakeOverlayRequestCancelHandler> passed_modal_cancel_handler =
      std::make_unique<FakeOverlayRequestCancelHandler>(modal_request.get(),
                                                        queue());
  FakeOverlayRequestCancelHandler* modal_cancel_handler =
      passed_modal_cancel_handler.get();
  queue()->AddRequest(std::move(modal_request),
                      std::move(passed_modal_cancel_handler));

  // Cancel the modal request, expecting that InfobarModalsCompleted() is
  // called.
  EXPECT_CALL(observer_, InfobarModalsCompleted(notifier_.get(), &infobar_));
  modal_cancel_handler->TriggerCancellation();
}
