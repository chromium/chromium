// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRESENTATION_PRESENTATION_TEST_UTILS_H_
#define CONTENT_BROWSER_PRESENTATION_PRESENTATION_TEST_UTILS_H_

#include "content/browser/presentation/presentation_service_impl.h"
#include "content/public/browser/presentation_request.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

using blink::mojom::PresentationConnection;
using blink::mojom::PresentationConnectionCloseReason;
using blink::mojom::PresentationConnectionMessagePtr;
using blink::mojom::PresentationConnectionResult;
using blink::mojom::PresentationConnectionResultPtr;
using blink::mojom::PresentationConnectionState;
using blink::mojom::PresentationController;
using blink::mojom::PresentationError;
using blink::mojom::PresentationErrorPtr;
using blink::mojom::PresentationErrorType;
using blink::mojom::PresentationInfo;
using blink::mojom::PresentationInfoPtr;
using blink::mojom::ScreenAvailability;
using ::testing::_;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using NewPresentationCallback =
    content::PresentationServiceImpl::NewPresentationCallback;

namespace content {

namespace {

MATCHER_P(PresentationUrlsAre, expected_urls, "") {
  return arg.presentation_urls == expected_urls;
}

// Matches blink::mojom::PresentationInfo.
MATCHER_P(InfoEquals, expected, "") {
  return expected.url == arg.url && expected.id == arg.id;
}

// Matches blink::mojom::PresentationInfoPtr.
MATCHER_P(InfoPtrEquals, expected, "") {
  return expected.url == arg->url && expected.id == arg->id;
}

ACTION_TEMPLATE(SaveArgByMove,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(pointer)) {
  *pointer = std::move(::testing::get<k>(args));
}

}  // namespace

class MockPresentationServiceDelegate
    : public ControllerPresentationServiceDelegate {
 public:
  MockPresentationServiceDelegate();
  ~MockPresentationServiceDelegate() override;

  MOCK_METHOD3(AddObserver,
               void(int render_process_id,
                    int render_frame_id,
                    PresentationServiceDelegate::Observer* observer));
  MOCK_METHOD2(RemoveObserver,
               void(int render_process_id, int render_frame_id));

  bool AddScreenAvailabilityListener(
      int render_process_id,
      int routing_id,
      PresentationScreenAvailabilityListener* listener) override;

  MOCK_METHOD0(AddScreenAvailabilityListener, bool());

  MOCK_METHOD3(RemoveScreenAvailabilityListener,
               void(int render_process_id,
                    int routing_id,
                    PresentationScreenAvailabilityListener* listener));
  MOCK_METHOD2(Reset, void(int render_process_id, int routing_id));
  MOCK_METHOD2(SetDefaultPresentationUrls,
               void(const PresentationRequest& request,
                    DefaultPresentationConnectionCallback callback));
  MOCK_METHOD3(StartPresentation,
               void(const PresentationRequest& request,
                    PresentationConnectionCallback success_cb,
                    PresentationConnectionErrorCallback error_cb));
  MOCK_METHOD4(ReconnectPresentation,
               void(const PresentationRequest& request,
                    const std::string& presentation_id,
                    PresentationConnectionCallback success_cb,
                    PresentationConnectionErrorCallback error_cb));
  MOCK_METHOD3(CloseConnection,
               void(int render_process_id,
                    int render_frame_id,
                    const std::string& presentation_id));
  MOCK_METHOD3(Terminate,
               void(int render_process_id,
                    int render_frame_id,
                    const std::string& presentation_id));
  MOCK_METHOD3(GetFlingingController,
               std::unique_ptr<media::FlingingController>(
                   int render_process_id,
                   int render_frame_id,
                   const std::string& presentation_id));
  MOCK_METHOD5(SendMessage,
               void(int render_process_id,
                    int render_frame_id,
                    const PresentationInfo& presentation_info,
                    PresentationConnectionMessagePtr message,
                    const SendMessageCallback& send_message_cb));
  MOCK_METHOD4(
      ListenForConnectionStateChange,
      void(int render_process_id,
           int render_frame_id,
           const PresentationInfo& connection,
           const PresentationConnectionStateChangedCallback& state_changed_cb));

  void set_screen_availability_listening_supported(bool value) {
    screen_availability_listening_supported_ = value;
  }

 private:
  bool screen_availability_listening_supported_ = true;
};

class MockPresentationReceiver : public blink::mojom::PresentationReceiver {
 public:
  MockPresentationReceiver();
  ~MockPresentationReceiver() override;

  MOCK_METHOD1(OnReceiverConnectionAvailable,
               void(PresentationConnectionResultPtr result));
};

class MockReceiverPresentationServiceDelegate
    : public ReceiverPresentationServiceDelegate {
 public:
  MockReceiverPresentationServiceDelegate();
  ~MockReceiverPresentationServiceDelegate() override;
  MOCK_METHOD3(AddObserver,
               void(int render_process_id,
                    int render_frame_id,
                    PresentationServiceDelegate::Observer* observer));
  MOCK_METHOD2(RemoveObserver,
               void(int render_process_id, int render_frame_id));
  MOCK_METHOD2(Reset, void(int render_process_id, int routing_id));
  MOCK_METHOD1(RegisterReceiverConnectionAvailableCallback,
               void(const ReceiverConnectionAvailableCallback&));
};

class MockPresentationConnection : public PresentationConnection {
 public:
  MockPresentationConnection();
  ~MockPresentationConnection() override;

  MOCK_METHOD1(OnMessage, void(PresentationConnectionMessagePtr message));
  MOCK_METHOD1(DidChangeState, void(PresentationConnectionState state));
  MOCK_METHOD1(DidClose, void(blink::mojom::PresentationConnectionCloseReason));
};

class MockPresentationController : public blink::mojom::PresentationController {
 public:
  MockPresentationController();
  ~MockPresentationController() override;
  MOCK_METHOD2(OnScreenAvailabilityUpdated,
               void(const GURL& url, ScreenAvailability availability));
  MOCK_METHOD2(OnConnectionStateChanged,
               void(PresentationInfoPtr connection,
                    PresentationConnectionState new_state));
  MOCK_METHOD3(OnConnectionClosed,
               void(PresentationInfoPtr connection,
                    PresentationConnectionCloseReason reason,
                    const std::string& message));
  MOCK_METHOD2(
      OnConnectionMessagesReceived,
      void(const PresentationInfo& presentation_info,
           const std::vector<PresentationConnectionMessagePtr>& messages));
  MOCK_METHOD1(OnDefaultPresentationStarted,
               void(PresentationConnectionResultPtr result));
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENTATION_PRESENTATION_TEST_UTILS_H_
