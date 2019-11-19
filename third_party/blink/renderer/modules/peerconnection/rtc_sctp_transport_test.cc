// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_sctp_transport.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_transport_test.h"
#include "third_party/webrtc/api/sctp_transport_interface.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace blink {

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

class MockSctpTransport : public webrtc::SctpTransportInterface {
 public:
  MockSctpTransport() {
    ON_CALL(*this, Information()).WillByDefault(Return(info_));
    ON_CALL(*this, RegisterObserver(_))
        .WillByDefault(Invoke(this, &MockSctpTransport::SetObserver));
  }
  MOCK_CONST_METHOD0(dtls_transport,
                     rtc::scoped_refptr<webrtc::DtlsTransportInterface>());
  MOCK_CONST_METHOD0(Information, webrtc::SctpTransportInformation());
  MOCK_METHOD1(RegisterObserver, void(webrtc::SctpTransportObserverInterface*));
  MOCK_METHOD0(UnregisterObserver, void());

  void SetObserver(webrtc::SctpTransportObserverInterface* observer) {
    observer_ = observer;
  }

  void SendClose() {
    if (observer_) {
      observer_->OnStateChange(webrtc::SctpTransportInformation(
          webrtc::SctpTransportState::kClosed));
    }
  }

 private:
  webrtc::SctpTransportInformation info_ =
      webrtc::SctpTransportInformation(webrtc::SctpTransportState::kNew);
  webrtc::SctpTransportObserverInterface* observer_ = nullptr;
};

class RTCSctpTransportTest : public RTCIceTransportTest {};

TEST_F(RTCSctpTransportTest, CreateFromMocks) {
  V8TestingScope scope;

  ExecutionContext* context = scope.GetExecutionContext();
  rtc::scoped_refptr<webrtc::SctpTransportInterface> mock_native_transport =
      new rtc::RefCountedObject<NiceMock<MockSctpTransport>>();
  RTCSctpTransport* transport = MakeGarbageCollected<RTCSctpTransport>(
      context, mock_native_transport, main_thread_, worker_thread_);
  WeakPersistent<RTCSctpTransport> garbage_collection_observer = transport;
  RunUntilIdle();
  transport = nullptr;
  // An unclosed transport should not be garbage collected, since events
  // might still trickle up.
  ASSERT_TRUE(garbage_collection_observer);
  // A closed transport should be garbage collected.
  static_cast<MockSctpTransport*>(mock_native_transport.get())->SendClose();
  RunUntilIdle();
  WebHeap::CollectAllGarbageForTesting();
  EXPECT_FALSE(garbage_collection_observer);
}

}  // namespace blink
