// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sender.h"

#include <memory>
#include <optional>

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoding_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_encoding_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_send_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_set_parameter_options.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_rtc_peer_connection_handler_platform.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_audio_stream_transformer.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_video_stream_transformer.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_sender_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_void_request.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/webrtc/api/rtc_error.h"
#include "third_party/webrtc/api/rtp_parameters.h"

namespace blink {

namespace {

class FakeRTCRtpSenderPlatform : public RTCRtpSenderPlatform {
 public:
  FakeRTCRtpSenderPlatform()
      : audio_transformer_(std::make_unique<RTCEncodedAudioStreamTransformer>(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting())),
        video_transformer_(std::make_unique<RTCEncodedVideoStreamTransformer>(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
            /*metronome=*/nullptr)) {}
  ~FakeRTCRtpSenderPlatform() override = default;

  std::unique_ptr<RTCRtpSenderPlatform> ShallowCopy() const override {
    return nullptr;
  }

  uintptr_t Id() const override { return 1; }
  webrtc::scoped_refptr<webrtc::DtlsTransportInterface> DtlsTransport()
      override {
    return nullptr;
  }
  webrtc::DtlsTransportInformation DtlsTransportInformation() override {
    return webrtc::DtlsTransportInformation(webrtc::DtlsTransportState::kNew);
  }
  MediaStreamComponent* Track() const override { return nullptr; }
  Vector<String> StreamIds() const override { return {}; }
  void ReplaceTrack(MediaStreamComponent*, RTCVoidRequest*) override {}
  std::unique_ptr<RtcDtmfSenderHandler> GetDtmfSender() const override {
    return nullptr;
  }

  std::unique_ptr<webrtc::RtpParameters> GetParameters() const override {
    auto parameters = std::make_unique<webrtc::RtpParameters>();
    parameters->transaction_id = "1234";
    return parameters;
  }

  void SetParameters(Vector<webrtc::RtpEncodingParameters>,
                     std::optional<webrtc::DegradationPreference>,
                     RTCVoidRequest* request) override {
    set_parameters_called_ = true;
    request->RequestSucceeded();
  }

  void GetStats(RTCStatsReportCallback) override {}
  void SetStreams(const Vector<String>& stream_ids) override {}
  RTCEncodedAudioStreamTransformer* GetEncodedAudioStreamTransformer()
      const override {
    return audio_transformer_.get();
  }
  RTCEncodedVideoStreamTransformer* GetEncodedVideoStreamTransformer()
      const override {
    return video_transformer_.get();
  }

  bool set_parameters_called_ = false;

 private:
  std::unique_ptr<RTCEncodedAudioStreamTransformer> audio_transformer_;
  std::unique_ptr<RTCEncodedVideoStreamTransformer> video_transformer_;
};

// Simulates a native layer that successfully returns an active encoding.
// It also tracks the encodings passed to SetParameters to verify mutations.
class EncodingsFakeRTCRtpSenderPlatform : public FakeRTCRtpSenderPlatform {
 public:
  std::unique_ptr<webrtc::RtpParameters> GetParameters() const override {
    auto parameters = std::make_unique<webrtc::RtpParameters>();
    parameters->transaction_id = "1234";
    // Add one encoding to test size matching and key_frame mapping.
    parameters->encodings.emplace_back();
    return parameters;
  }

  void SetParameters(
      Vector<webrtc::RtpEncodingParameters> encodings,
      std::optional<webrtc::DegradationPreference> degradation_preference,
      RTCVoidRequest* request) override {
    set_parameters_called_ = true;
    last_encodings_ = encodings;
    request->RequestSucceeded();
  }

  Vector<webrtc::RtpEncodingParameters> last_encodings_;
};

// Simulates a failure at the lower WebRTC native layer.
class RejectingFakeRTCRtpSenderPlatform : public FakeRTCRtpSenderPlatform {
 public:
  void SetParameters(Vector<webrtc::RtpEncodingParameters>,
                     std::optional<webrtc::DegradationPreference>,
                     RTCVoidRequest* request) override {
    set_parameters_called_ = true;
    request->RequestFailed(webrtc::RTCError(
        webrtc::RTCErrorType::UNSUPPORTED_PARAMETER, "Native layer rejected"));
  }
};

class RTCRtpSenderTest : public testing::Test {
 public:
  RTCRtpSenderTest() = default;

  RTCPeerConnection* CreatePC(V8TestingScope& scope) {
    RTCPeerConnection::SetRtcPeerConnectionHandlerFactoryForTesting(
        base::BindRepeating([]() -> std::unique_ptr<RTCPeerConnectionHandler> {
          return std::make_unique<MockRTCPeerConnectionHandlerPlatform>();
        }));
    return RTCPeerConnection::Create(scope.GetExecutionContext(),
                                     RTCConfiguration::Create(),
                                     scope.GetExceptionState());
  }

  RTCRtpSender* CreateSender(
      RTCPeerConnection* pc,
      std::unique_ptr<RTCRtpSenderPlatform> platform = nullptr) {
    if (!platform) {
      platform = std::make_unique<FakeRTCRtpSenderPlatform>();
    }
    return MakeGarbageCollected<RTCRtpSender>(
        pc, std::move(platform), "video", nullptr, MediaStreamVector(), false,
        blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  }

 protected:
  test::TaskEnvironment task_environment_;
};

}  // namespace

TEST_F(RTCRtpSenderTest, SetParametersSucceeds) {
  V8TestingScope scope;
  RTCPeerConnection* pc = CreatePC(scope);
  ASSERT_TRUE(pc);

  auto platform = std::make_unique<FakeRTCRtpSenderPlatform>();
  auto* platform_ptr = platform.get();
  auto* sender = CreateSender(pc, std::move(platform));

  auto* parameters = sender->getParameters();

  auto* options = RTCSetParameterOptions::Create();
  auto promise = sender->setParameters(scope.GetScriptState(), parameters,
                                       options, scope.GetExceptionState());

  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  // Verify native layer was called.
  EXPECT_TRUE(platform_ptr->set_parameters_called_);
}

TEST_F(RTCRtpSenderTest, SetParametersWithoutGetParametersFails) {
  V8TestingScope scope;
  RTCPeerConnection* pc = CreatePC(scope);
  ASSERT_TRUE(pc);

  auto platform = std::make_unique<FakeRTCRtpSenderPlatform>();
  auto* platform_ptr = platform.get();
  auto* sender = CreateSender(pc, std::move(platform));

  auto* parameters = RTCRtpSendParameters::Create();
  auto* options = RTCSetParameterOptions::Create();
  auto promise = sender->setParameters(scope.GetScriptState(), parameters,
                                       options, scope.GetExceptionState());

  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());

  // Verify native layer was never reached.
  EXPECT_FALSE(platform_ptr->set_parameters_called_);

  DOMException* exception =
      V8DOMException::ToWrappable(scope.GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(exception);
  EXPECT_EQ(exception->name(), "InvalidStateError");
}

TEST_F(RTCRtpSenderTest, SetParametersWithModifiedTransactionIdFails) {
  V8TestingScope scope;
  RTCPeerConnection* pc = CreatePC(scope);
  ASSERT_TRUE(pc);

  auto platform = std::make_unique<FakeRTCRtpSenderPlatform>();
  auto* platform_ptr = platform.get();
  auto* sender = CreateSender(pc, std::move(platform));

  auto* parameters = sender->getParameters();
  // Call getParameters() again to overwrite last_returned_parameters_,
  // so that the modified `parameters` won't be identical to the saved state.
  sender->getParameters();

  parameters->setTransactionId("modified");

  auto* options = RTCSetParameterOptions::Create();
  auto promise = sender->setParameters(scope.GetScriptState(), parameters,
                                       options, scope.GetExceptionState());

  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());

  // Verify native layer was never reached.
  EXPECT_FALSE(platform_ptr->set_parameters_called_);

  DOMException* exception =
      V8DOMException::ToWrappable(scope.GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(exception);
  EXPECT_EQ(exception->name(), "InvalidModificationError");
}

TEST_F(RTCRtpSenderTest, SetParametersWithMismatchedEncodingOptionsFails) {
  V8TestingScope scope;
  RTCPeerConnection* pc = CreatePC(scope);
  ASSERT_TRUE(pc);

  auto platform = std::make_unique<EncodingsFakeRTCRtpSenderPlatform>();
  auto* platform_ptr = platform.get();
  auto* sender = CreateSender(pc, std::move(platform));

  auto* parameters = sender->getParameters();

  // Provide 2 encoding options to force a mismatch with the 1 encoding.
  auto* options = RTCSetParameterOptions::Create();
  HeapVector<Member<RTCEncodingOptions>> encoding_options;
  encoding_options.push_back(RTCEncodingOptions::Create());
  encoding_options.push_back(RTCEncodingOptions::Create());
  options->setEncodingOptions(encoding_options);

  auto promise = sender->setParameters(scope.GetScriptState(), parameters,
                                       options, scope.GetExceptionState());

  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());

  // Verify native layer was never reached.
  EXPECT_FALSE(platform_ptr->set_parameters_called_);

  DOMException* exception =
      V8DOMException::ToWrappable(scope.GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(exception);
  EXPECT_EQ(exception->name(), "InvalidModificationError");
}

TEST_F(RTCRtpSenderTest, SetParametersAppliesEncodingOptionsKeyFrame) {
  V8TestingScope scope;
  RTCPeerConnection* pc = CreatePC(scope);
  ASSERT_TRUE(pc);

  auto platform = std::make_unique<EncodingsFakeRTCRtpSenderPlatform>();
  auto* platform_ptr = platform.get();
  auto* sender = CreateSender(pc, std::move(platform));

  auto* parameters = sender->getParameters();

  // Provide 1 encoding option to match the 1 encoding, setting keyFrame to
  // true.
  auto* options = RTCSetParameterOptions::Create();
  HeapVector<Member<RTCEncodingOptions>> encoding_options;
  auto* encoding_option = RTCEncodingOptions::Create();
  encoding_option->setKeyFrame(true);
  encoding_options.push_back(encoding_option);
  options->setEncodingOptions(encoding_options);

  auto promise = sender->setParameters(scope.GetScriptState(), parameters,
                                       options, scope.GetExceptionState());

  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  // Verify the translation made it to the platform fake layer.
  EXPECT_TRUE(platform_ptr->set_parameters_called_);
  ASSERT_EQ(platform_ptr->last_encodings_.size(), 1u);
  EXPECT_TRUE(platform_ptr->last_encodings_[0].request_key_frame);
}

TEST_F(RTCRtpSenderTest, SetParametersFailsOnNativeRejection) {
  V8TestingScope scope;
  RTCPeerConnection* pc = CreatePC(scope);
  ASSERT_TRUE(pc);

  auto platform = std::make_unique<RejectingFakeRTCRtpSenderPlatform>();
  auto* platform_ptr = platform.get();
  auto* sender = CreateSender(pc, std::move(platform));

  auto* parameters = sender->getParameters();
  auto* options = RTCSetParameterOptions::Create();

  auto promise = sender->setParameters(scope.GetScriptState(), parameters,
                                       options, scope.GetExceptionState());

  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());

  // We DO expect it to be called here, but rejected by the native callback.
  EXPECT_TRUE(platform_ptr->set_parameters_called_);

  // Blink bridges webrtc::RTCErrorType::UNSUPPORTED_PARAMETER to
  // OperationError.
  DOMException* exception =
      V8DOMException::ToWrappable(scope.GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(exception);
  EXPECT_EQ(exception->name(), "OperationError");
}

}  // namespace blink
