// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ICE_TRANSPORT_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ICE_TRANSPORT_TEST_H_

#include "base/test/test_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/test/mock_ice_transport_adapter.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_ice_transport.h"

namespace blink {

class V8TestingScope;

class MockEventListener final : public NativeEventListener {
 public:
  MOCK_METHOD2(Invoke, void(ExecutionContext*, Event*));
};

class RTCIceTransportTest : public testing::Test {
 public:
  static RTCIceParameters* CreateRemoteRTCIceParameters1();

  RTCIceTransportTest();
  ~RTCIceTransportTest() override;

  // Run the main thread and worker thread until both are idle.
  void RunUntilIdle();

  // Construct a new RTCIceTrnasport with a default MockIceTransportAdapter.
  RTCIceTransport* CreateIceTransport(V8TestingScope& scope);

  // Construct a new RTCIceTransport with a mock IceTransportAdapter.
  RTCIceTransport* CreateIceTransport(
      V8TestingScope& scope,
      IceTransportAdapter::Delegate** delegate_out);

  // Construct a new RTCIceTransport with the given mock IceTransportAdapter.
  // |delegate_out|, if non-null, will be populated once the IceTransportAdapter
  // is constructed on the worker thread.
  RTCIceTransport* CreateIceTransport(
      V8TestingScope& scope,
      std::unique_ptr<MockIceTransportAdapter> mock,
      IceTransportAdapter::Delegate** delegate_out = nullptr);

  // Use this method to construct a MockEventListener so that the expectations
  // can be explicitly checked at the end of the test. Normally the expectations
  // would be verified in the mock destructor, but since MockEventListener is
  // garbage collected this may happen after the test has finished, improperly
  // letting it pass.
  MockEventListener* CreateMockEventListener();

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> main_thread_;
  scoped_refptr<base::TestSimpleTaskRunner> worker_thread_;
  Vector<Persistent<MockEventListener>> mock_event_listeners_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ICE_TRANSPORT_TEST_H_
