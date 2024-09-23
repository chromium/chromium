// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_transport.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_test.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/webrtc/api/transport/network_control.h"

namespace blink {

using testing::_;
using testing::Invoke;

class MockFeedbackProvider : public FeedbackProvider {
 public:
  MOCK_METHOD(void,
              SetProcessor,
              (CrossThreadWeakHandle<RTCRtpTransportProcessor>
                   rtp_transport_processor_handle,
               scoped_refptr<base::SequencedTaskRunner> task_runner),
              (override));

  MOCK_METHOD(void,
              SetCustomMaxBitrateBps,
              (uint64_t custom_max_bitrate_bps),
              (override));
};

class RTCRtpTransportTest : public DedicatedWorkerTest {};

TEST_F(RTCRtpTransportTest, RegisterFeedbackProviderAfterCreateProcessor) {
  V8TestingScope scope_;
  RTCRtpTransport* transport =
      MakeGarbageCollected<RTCRtpTransport>(scope_.GetExecutionContext());

  base::RunLoop loop;
  const String source_code = R"JS(
    onrtcrtptransportprocessor = () => {};
  )JS";
  StartWorker();
  EvaluateClassicScript(source_code);
  WaitUntilWorkerIsRunning();

  transport->createProcessor(scope_.GetScriptState(), WorkerObject(),
                             scope_.GetExceptionState());

  auto mock_feedback_provider = base::MakeRefCounted<MockFeedbackProvider>();

  EXPECT_CALL(*mock_feedback_provider, SetProcessor(_, _))
      .WillOnce(Invoke([&]() { loop.Quit(); }));
  transport->RegisterFeedbackProvider(mock_feedback_provider);
  loop.Run();
}

TEST_F(RTCRtpTransportTest, RegisterFeedbackProviderBeforeCreateProcessor) {
  V8TestingScope scope_;
  RTCRtpTransport* transport =
      MakeGarbageCollected<RTCRtpTransport>(scope_.GetExecutionContext());
  auto mock_feedback_provider = base::MakeRefCounted<MockFeedbackProvider>();

  EXPECT_CALL(*mock_feedback_provider, SetProcessor(_, _)).Times(0);
  transport->RegisterFeedbackProvider(mock_feedback_provider);
  transport->createProcessor(scope_.GetScriptState(), WorkerObject(),
                             scope_.GetExceptionState());

  base::RunLoop loop;
  EXPECT_CALL(*mock_feedback_provider, SetProcessor(_, _))
      .WillOnce(Invoke([&]() { loop.Quit(); }));
  const String source_code = R"JS(
    onrtcrtptransportprocessor = () => {};
  )JS";
  StartWorker();
  EvaluateClassicScript(source_code);
  WaitUntilWorkerIsRunning();

  loop.Run();
}

}  // namespace blink
