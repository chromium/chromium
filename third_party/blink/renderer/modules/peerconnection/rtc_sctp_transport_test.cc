// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_sctp_transport.h"

#include "base/memory/raw_ptr.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/webrtc/api/sctp_transport_interface.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace blink {

using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::NiceMock;
using testing::Return;

class MockEventListener final : public NativeEventListener {
 public:
  MOCK_METHOD2(Invoke, void(ExecutionContext*, Event*));
};

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
  raw_ptr<webrtc::SctpTransportObserverInterface, DanglingUntriaged> observer_ =
      nullptr;
};

class RTCSctpTransportTest : public testing::Test {
 public:
  RTCSctpTransportTest();
  ~RTCSctpTransportTest() override;

  // Run the main thread and worker thread until both are idle.
  void RunUntilIdle();

 protected:
  test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestSimpleTaskRunner> main_thread_;
  scoped_refptr<base::TestSimpleTaskRunner> worker_thread_;
  Vector<Persistent<MockEventListener>> mock_event_listeners_;
};

RTCSctpTransportTest::RTCSctpTransportTest()
    : main_thread_(new base::TestSimpleTaskRunner()),
      worker_thread_(new base::TestSimpleTaskRunner()) {}

RTCSctpTransportTest::~RTCSctpTransportTest() {
  RunUntilIdle();

  // Explicitly verify expectations of garbage collected mock objects.
  for (auto mock : mock_event_listeners_) {
    Mock::VerifyAndClear(mock);
  }
}

void RTCSctpTransportTest::RunUntilIdle() {
  while (worker_thread_->HasPendingTask() || main_thread_->HasPendingTask()) {
    worker_thread_->RunPendingTasks();
    main_thread_->RunPendingTasks();
  }
}

TEST_F(RTCSctpTransportTest, CreateFromMocks) {
  V8TestingScope scope;

  ExecutionContext* context = scope.GetExecutionContext();
  rtc::scoped_refptr<webrtc::SctpTransportInterface> mock_native_transport(
      new rtc::RefCountedObject<NiceMock<MockSctpTransport>>());
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
