// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/presentation/presentation_service_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "content/browser/presentation/presentation_test_utils.h"
#include "content/public/browser/presentation_request.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

using blink::mojom::PresentationConnectionResult;
using blink::mojom::PresentationInfo;

namespace content {

namespace {

const char kPresentationId[] = "presentationId";
const char kPresentationUrl1[] = "http://foo.com/index.html";
const char kPresentationUrl2[] = "http://example.com/index.html";
const char kPresentationUrl3[] = "http://example.net/index.html";

}  // namespace

class PresentationServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  PresentationServiceImplTest()
      : presentation_url1_(GURL(kPresentationUrl1)),
        presentation_url2_(GURL(kPresentationUrl2)),
        presentation_url3_(GURL(kPresentationUrl3)) {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    // This needed to keep the WebContentsObserverConsistencyChecker checks
    // happy for when AppendChild is called.
    NavigateAndCommit(GURL("about:blank"));

    EXPECT_CALL(mock_delegate_, AddObserver(_, _, _)).Times(1);
    TestRenderFrameHost* render_frame_host = contents()->GetPrimaryMainFrame();
    render_frame_host->InitializeRenderFrameIfNeeded();
    service_impl_.reset(new PresentationServiceImpl(
        render_frame_host, contents(), &mock_delegate_, nullptr));

    mojo::PendingRemote<PresentationController> presentation_controller_remote;
    controller_receiver_.emplace(
        &mock_controller_,
        presentation_controller_remote.InitWithNewPipeAndPassReceiver());
    service_impl_->SetController(std::move(presentation_controller_remote));

    presentation_urls_.push_back(presentation_url1_);
    presentation_urls_.push_back(presentation_url2_);

    expect_presentation_success_cb_ =
        base::BindOnce(&PresentationServiceImplTest::ExpectPresentationSuccess,
                       base::Unretained(this));
    expect_presentation_error_cb_ =
        base::BindOnce(&PresentationServiceImplTest::ExpectPresentationError,
                       base::Unretained(this));
  }

  void TearDown() override {
    if (service_impl_.get()) {
      ExpectDelegateReset();
      EXPECT_CALL(mock_delegate_, RemoveObserver(_, _)).Times(1);
      service_impl_.reset();
    }
    RenderViewHostImplTestHarness::TearDown();
  }

  void Navigate(bool main_frame) {
    RenderFrameHost* rfh = main_rfh();
    RenderFrameHostTester* rfh_tester = RenderFrameHostTester::For(rfh);
    if (!main_frame)
      rfh = rfh_tester->AppendChild("subframe");
    MockNavigationHandle handle(GURL(), rfh);
    handle.set_has_committed(true);
    service_impl_->DidFinishNavigation(&handle);
  }

  void ListenForScreenAvailabilityAndWait(const GURL& url,
                                          bool delegate_success) {
    EXPECT_CALL(mock_delegate_, AddScreenAvailabilityListener())
        .WillOnce(Return(delegate_success));
    service_impl_->ListenForScreenAvailability(url);

    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_delegate_));
  }

  void SimulateScreenAvailabilityChangeAndWait(
      const GURL& url,
      ScreenAvailability availability) {
    auto listener_it = service_impl_->screen_availability_listeners_.find(url);
    ASSERT_TRUE(listener_it->second);

    EXPECT_CALL(mock_controller_,
                OnScreenAvailabilityUpdated(url, availability));
    listener_it->second->OnScreenAvailabilityChanged(availability);
    base::RunLoop().RunUntilIdle();
  }

  void ExpectDelegateReset() {
    EXPECT_CALL(mock_delegate_, Reset(_, _)).Times(1);
  }

  void ExpectCleanState() {
    EXPECT_TRUE(service_impl_->default_presentation_urls_.empty());
    EXPECT_EQ(
        service_impl_->screen_availability_listeners_.find(presentation_url1_),
        service_impl_->screen_availability_listeners_.end());
  }

  void ExpectPresentationSuccess(PresentationConnectionResultPtr result,
                                 PresentationErrorPtr error) {
    EXPECT_TRUE(result);
    EXPECT_FALSE(error);
    presentation_cb_was_run_ = true;
  }

  void ExpectPresentationError(PresentationConnectionResultPtr result,
                               PresentationErrorPtr error) {
    EXPECT_FALSE(result);
    EXPECT_TRUE(error);
    presentation_cb_was_run_ = true;
  }

  void ExpectPresentationCallbackWasRun() const {
    EXPECT_TRUE(presentation_cb_was_run_)
        << "ExpectPresentationSuccess or ExpectPresentationError was called";
  }

  MockPresentationServiceDelegate mock_delegate_;
  MockReceiverPresentationServiceDelegate mock_receiver_delegate_;

  std::unique_ptr<PresentationServiceImpl> service_impl_;

  MockPresentationController mock_controller_;
  std::optional<mojo::Receiver<PresentationController>> controller_receiver_;

  GURL presentation_url1_;
  GURL presentation_url2_;
  GURL presentation_url3_;
  std::vector<GURL> presentation_urls_;

  NewPresentationCallback expect_presentation_success_cb_;
  NewPresentationCallback expect_presentation_error_cb_;
  bool presentation_cb_was_run_ = false;
};

TEST_F(PresentationServiceImplTest, ListenForScreenAvailability) {
  ListenForScreenAvailabilityAndWait(presentation_url1_, true);

  SimulateScreenAvailabilityChangeAndWait(presentation_url1_,
                                          ScreenAvailability::AVAILABLE);
  SimulateScreenAvailabilityChangeAndWait(presentation_url1_,
                                          ScreenAvailability::UNAVAILABLE);
  SimulateScreenAvailabilityChangeAndWait(presentation_url1_,
                                          ScreenAvailability::AVAILABLE);
}

TEST_F(PresentationServiceImplTest, ScreenAvailabilityNotSupported) {
  mock_delegate_.set_screen_availability_listening_supported(false);
  EXPECT_CALL(mock_controller_,
              OnScreenAvailabilityUpdated(presentation_url1_,
                                          ScreenAvailability::DISABLED));
  ListenForScreenAvailabilityAndWait(presentation_url1_, false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PresentationServiceImplTest, OnDelegateDestroyed) {
  ListenForScreenAvailabilityAndWait(presentation_url1_, true);

  service_impl_->OnDelegateDestroyed();

  // TearDown() expects |mock_delegate_| to have been notified when
  // |service_impl_| is destroyed; this does not apply here since the delegate
  // is destroyed first.
  service_impl_.reset();
}

TEST_F(PresentationServiceImplTest, DidNavigateThisFrame) {
  ListenForScreenAvailabilityAndWait(presentation_url1_, true);

  ExpectDelegateReset();
  Navigate(true);
  ExpectCleanState();
}

TEST_F(PresentationServiceImplTest, DidNavigateOtherFrame) {
  ListenForScreenAvailabilityAndWait(presentation_url1_, true);

  Navigate(false);

  // Availability is reported and callback is invoked since it was not
  // removed.
  SimulateScreenAvailabilityChangeAndWait(presentation_url1_,
                                          ScreenAvailability::AVAILABLE);
}

TEST_F(PresentationServiceImplTest, DelegateFails) {
  ListenForScreenAvailabilityAndWait(presentation_url1_, false);
  ASSERT_EQ(
      service_impl_->screen_availability_listeners_.end(),
      service_impl_->screen_availability_listeners_.find(presentation_url1_));
}

TEST_F(PresentationServiceImplTest, SetDefaultPresentationUrls) {
  EXPECT_CALL(mock_delegate_, SetDefaultPresentationUrls(
                                  PresentationUrlsAre(presentation_urls_), _))
      .Times(1);

  service_impl_->SetDefaultPresentationUrls(presentation_urls_);

  // Sets different DPUs.
  std::vector<GURL> more_urls = presentation_urls_;
  more_urls.push_back(presentation_url3_);

  PresentationConnectionCallback callback;
  EXPECT_CALL(mock_delegate_,
              SetDefaultPresentationUrls(PresentationUrlsAre(more_urls), _))
      .WillOnce(SaveArgByMove<1>(&callback));
  service_impl_->SetDefaultPresentationUrls(more_urls);

  PresentationInfo presentation_info(presentation_url2_, kPresentationId);

  EXPECT_CALL(mock_controller_, OnDefaultPresentationStarted(_))
      .WillOnce([&presentation_info](PresentationConnectionResultPtr result) {
        EXPECT_THAT(*result->presentation_info, InfoEquals(presentation_info));
      });
  EXPECT_CALL(mock_delegate_, ListenForConnectionStateChange(_, _, _, _));

  mojo::PendingRemote<PresentationConnection> presentation_connection_remote;
  mojo::Remote<PresentationConnection> controller_remote;
  std::ignore = presentation_connection_remote.InitWithNewPipeAndPassReceiver();
  std::move(callback).Run(PresentationConnectionResult::New(
      PresentationInfo::New(presentation_url2_, kPresentationId),
      std::move(presentation_connection_remote),
      controller_remote.BindNewPipeAndPassReceiver()));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PresentationServiceImplTest,
       SetDefaultPresentationUrlsNoopsOnNonMainFrame) {
  RenderFrameHost* rfh = main_rfh();
  RenderFrameHostTester* rfh_tester = RenderFrameHostTester::For(rfh);
  rfh = rfh_tester->AppendChild("subframe");

  EXPECT_CALL(mock_delegate_, RemoveObserver(_, _)).Times(1);
  EXPECT_CALL(mock_delegate_, AddObserver(_, _, _)).Times(1);
  service_impl_.reset(
      new PresentationServiceImpl(rfh, contents(), &mock_delegate_, nullptr));

  EXPECT_CALL(mock_delegate_, SetDefaultPresentationUrls(_, _)).Times(0);
  service_impl_->SetDefaultPresentationUrls(presentation_urls_);
}

TEST_F(PresentationServiceImplTest, ListenForConnectionStateChange) {
  PresentationInfo connection(presentation_url1_, kPresentationId);
  PresentationConnectionStateChangedCallback state_changed_cb;
  // Trigger state change. It should be propagated back up to
  // |mock_controller_|.
  PresentationInfo presentation_connection(presentation_url1_, kPresentationId);

  EXPECT_CALL(mock_delegate_, ListenForConnectionStateChange(_, _, _, _))
      .WillOnce(SaveArg<3>(&state_changed_cb));
  service_impl_->ListenForConnectionStateChange(connection);

  EXPECT_CALL(mock_controller_, OnConnectionStateChanged(
                                    InfoPtrEquals(presentation_connection),
                                    PresentationConnectionState::TERMINATED));
  state_changed_cb.Run(PresentationConnectionStateChangeInfo(
      PresentationConnectionState::TERMINATED));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PresentationServiceImplTest, ListenForConnectionClose) {
  PresentationInfo connection(presentation_url1_, kPresentationId);
  PresentationConnectionStateChangedCallback state_changed_cb;
  EXPECT_CALL(mock_delegate_, ListenForConnectionStateChange(_, _, _, _))
      .WillOnce(SaveArg<3>(&state_changed_cb));
  service_impl_->ListenForConnectionStateChange(connection);

  // Trigger connection close. It should be propagated back up to
  // |mock_controller_|.
  PresentationInfo presentation_connection(presentation_url1_, kPresentationId);
  PresentationConnectionStateChangeInfo closed_info(
      PresentationConnectionState::CLOSED);
  closed_info.close_reason = PresentationConnectionCloseReason::WENT_AWAY;
  closed_info.message = "Foo";

  EXPECT_CALL(
      mock_controller_,
      OnConnectionClosed(InfoPtrEquals(presentation_connection),
                         PresentationConnectionCloseReason::WENT_AWAY, "Foo"));
  state_changed_cb.Run(closed_info);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PresentationServiceImplTest, SetSameDefaultPresentationUrls) {
  EXPECT_CALL(mock_delegate_, SetDefaultPresentationUrls(_, _)).Times(1);
  service_impl_->SetDefaultPresentationUrls(presentation_urls_);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_delegate_));

  // Same URLs as before; no-ops.
  service_impl_->SetDefaultPresentationUrls(presentation_urls_);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_delegate_));
}

TEST_F(PresentationServiceImplTest, StartPresentationSuccess) {
  PresentationConnectionCallback saved_success_cb;
  EXPECT_CALL(mock_delegate_, StartPresentation(_, _, _))
      .WillOnce([&saved_success_cb](const auto& request, auto success_cb,
                                    auto error_cb) {
        saved_success_cb = std::move(success_cb);
      });
  service_impl_->StartPresentation(presentation_urls_,
                                   std::move(expect_presentation_success_cb_));
  EXPECT_FALSE(saved_success_cb.is_null());
  EXPECT_CALL(mock_delegate_, ListenForConnectionStateChange(_, _, _, _))
      .Times(1);
  std::move(saved_success_cb)
      .Run(PresentationConnectionResult::New(
          PresentationInfo::New(presentation_url1_, kPresentationId),
          mojo::NullRemote(), mojo::NullReceiver()));
  ExpectPresentationCallbackWasRun();
}

TEST_F(PresentationServiceImplTest, StartPresentationError) {
  base::OnceCallback<void(const PresentationError&)> saved_error_cb;
  EXPECT_CALL(mock_delegate_, StartPresentation(_, _, _))
      .WillOnce([&](const auto& request, auto success_cb, auto error_cb) {
        saved_error_cb = std::move(error_cb);
      });
  service_impl_->StartPresentation(presentation_urls_,
                                   std::move(expect_presentation_error_cb_));
  EXPECT_FALSE(saved_error_cb.is_null());
  std::move(saved_error_cb)
      .Run(PresentationError(PresentationErrorType::UNKNOWN, "Error message"));
  ExpectPresentationCallbackWasRun();
}

TEST_F(PresentationServiceImplTest, StartPresentationInProgress) {
  EXPECT_CALL(mock_delegate_, StartPresentation(_, _, _)).Times(1);
  // Uninvoked callbacks must outlive |service_impl_| since they get invoked
  // at |service_impl_|'s destruction.
  service_impl_->StartPresentation(presentation_urls_, base::DoNothing());

  // This request should fail immediately, since there is already a
  // StartPresentation in progress.
  service_impl_->StartPresentation(presentation_urls_,
                                   std::move(expect_presentation_error_cb_));
  ExpectPresentationCallbackWasRun();
}

TEST_F(PresentationServiceImplTest, ReconnectPresentationSuccess) {
  PresentationConnectionCallback saved_success_cb;
  EXPECT_CALL(mock_delegate_, ReconnectPresentation(_, kPresentationId, _, _))
      .WillOnce([&saved_success_cb](const auto& request, const auto& id,
                                    auto success_cb, auto error_cb) {
        saved_success_cb = std::move(success_cb);
      });
  service_impl_->ReconnectPresentation(
      presentation_urls_, kPresentationId,
      std::move(expect_presentation_success_cb_));
  EXPECT_FALSE(saved_success_cb.is_null());
  EXPECT_CALL(mock_delegate_, ListenForConnectionStateChange(_, _, _, _))
      .Times(1);
  std::move(saved_success_cb)
      .Run(PresentationConnectionResult::New(
          PresentationInfo::New(presentation_url1_, kPresentationId),
          mojo::NullRemote(), mojo::NullReceiver()));
  ExpectPresentationCallbackWasRun();
}

TEST_F(PresentationServiceImplTest, ReconnectPresentationError) {
  base::OnceCallback<void(const PresentationError&)> saved_error_cb;
  EXPECT_CALL(mock_delegate_, ReconnectPresentation(_, kPresentationId, _, _))
      .WillOnce([&](const auto& request, const std::string& id, auto success_cb,
                    auto error_cb) { saved_error_cb = std::move(error_cb); });
  service_impl_->ReconnectPresentation(
      presentation_urls_, kPresentationId,
      std::move(expect_presentation_error_cb_));
  EXPECT_FALSE(saved_error_cb.is_null());
  std::move(saved_error_cb)
      .Run(PresentationError(PresentationErrorType::UNKNOWN, "Error message"));
  ExpectPresentationCallbackWasRun();
}

TEST_F(PresentationServiceImplTest, MaxPendingReconnectPresentationRequests) {
  static constexpr char kPresentationUrlTemplate[] = "http://fooUrl%d";
  static constexpr char kPresentationIdTemplate[] = "presentationId%d";
  int num_requests = PresentationServiceImpl::kMaxQueuedRequests;
  int i = 0;
  EXPECT_CALL(mock_delegate_, ReconnectPresentation(_, _, _, _))
      .Times(num_requests);
  for (; i < num_requests; ++i) {
    std::vector<GURL> urls = {
        GURL(base::StringPrintf(kPresentationUrlTemplate, i))};
    // Uninvoked callbacks must outlive |service_impl_| since they get invoked
    // at |service_impl_|'s destruction.
    service_impl_->ReconnectPresentation(
        urls, base::StringPrintf(kPresentationIdTemplate, i),
        base::DoNothing());
  }

  std::vector<GURL> urls = {
      GURL(base::StringPrintf(kPresentationUrlTemplate, i))};
  // Exceeded maximum queue size, should invoke mojo callback with error.
  service_impl_->ReconnectPresentation(
      urls, base::StringPrintf(kPresentationIdTemplate, i),
      std::move(expect_presentation_error_cb_));
  ExpectPresentationCallbackWasRun();
}

TEST_F(PresentationServiceImplTest, CloseConnection) {
  EXPECT_CALL(mock_delegate_, CloseConnection(_, _, Eq(kPresentationId)));
  service_impl_->CloseConnection(presentation_url1_, kPresentationId);
}

TEST_F(PresentationServiceImplTest, Terminate) {
  EXPECT_CALL(mock_delegate_, Terminate(_, _, Eq(kPresentationId)));
  service_impl_->Terminate(presentation_url1_, kPresentationId);
}

TEST_F(PresentationServiceImplTest, ReceiverPresentationServiceDelegate) {
  EXPECT_CALL(mock_receiver_delegate_, AddObserver(_, _, _)).Times(1);

  PresentationServiceImpl service_impl(main_rfh(), contents(), nullptr,
                                       &mock_receiver_delegate_);

  ReceiverConnectionAvailableCallback callback;
  EXPECT_CALL(mock_receiver_delegate_,
              RegisterReceiverConnectionAvailableCallback(_))
      .WillOnce(SaveArg<0>(&callback));

  MockPresentationReceiver mock_receiver;
  mojo::Receiver<blink::mojom::PresentationReceiver>
      presentation_receiver_receiver(&mock_receiver);
  service_impl.SetReceiver(
      presentation_receiver_receiver.BindNewPipeAndPassRemote());
  EXPECT_FALSE(callback.is_null());

  PresentationInfo expected(presentation_url1_, kPresentationId);

  // Client gets notified of receiver connections.
  mojo::PendingRemote<PresentationConnection> controller_connection;
  MockPresentationConnection mock_presentation_connection;
  mojo::Receiver<PresentationConnection> connection_binding(
      &mock_presentation_connection,
      controller_connection.InitWithNewPipeAndPassReceiver());
  mojo::Remote<PresentationConnection> receiver_connection;

  EXPECT_CALL(mock_receiver, OnReceiverConnectionAvailable(_)).Times(1);
  callback.Run(PresentationConnectionResult::New(
      PresentationInfo::New(expected), std::move(controller_connection),
      receiver_connection.BindNewPipeAndPassReceiver()));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_receiver_delegate_, RemoveObserver(_, _)).Times(1);
}

TEST_F(PresentationServiceImplTest, ReceiverDelegateOnSubFrame) {
  EXPECT_CALL(mock_receiver_delegate_, AddObserver(_, _, _)).Times(1);

  PresentationServiceImpl service_impl(main_rfh(), contents(), nullptr,
                                       &mock_receiver_delegate_);
  service_impl.is_outermost_document_ = false;

  ReceiverConnectionAvailableCallback callback;
  EXPECT_CALL(mock_receiver_delegate_,
              RegisterReceiverConnectionAvailableCallback(_))
      .Times(0);

  mojo::PendingRemote<PresentationController> presentation_controller_remote;
  controller_receiver_.emplace(
      &mock_controller_,
      presentation_controller_remote.InitWithNewPipeAndPassReceiver());
  service_impl.controller_delegate_ = nullptr;
  service_impl.SetController(std::move(presentation_controller_remote));

  EXPECT_CALL(mock_receiver_delegate_, Reset(_, _)).Times(0);
  service_impl.Reset();

  EXPECT_CALL(mock_receiver_delegate_, RemoveObserver(_, _)).Times(1);
}

}  // namespace content
