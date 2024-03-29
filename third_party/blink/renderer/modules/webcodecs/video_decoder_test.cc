// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_decoder.h"

#include "base/run_loop.h"
#include "build/build_config.h"
#include "media/base/mock_filters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_support.h"
#include "third_party/blink/renderer/core/testing/mock_function_scope.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_manager.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_manager_provider.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

// For FakeVideoDecoder.
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/filters/h264_to_annex_b_bitstream_converter.h"  // nogncheck
#include "media/formats/mp4/box_definitions.h"                  // nogncheck
#endif

namespace blink {

namespace {

using testing::_;
using testing::Unused;

class FakeVideoDecoder : public VideoDecoder {
 public:
  FakeVideoDecoder(ScriptState* script_state,
                   const VideoDecoderInit* init,
                   ExceptionState& exception_state)
      : VideoDecoder(script_state, init, exception_state) {}
  ~FakeVideoDecoder() override = default;

  void SetupMockHardwareDecoder(base::RepeatingClosure quit_closure) {
    mock_decoder_ = std::make_unique<media::MockVideoDecoder>(
        /*is_platform_decoder=*/true, /*supports_decription=*/false,
        /*decoder_id=*/1);
    SetupExpectations(std::move(quit_closure));
  }

  void SetupMockSoftwareDecoder(base::RepeatingClosure quit_closure) {
    mock_decoder_ = std::make_unique<media::MockVideoDecoder>(
        /*is_platform_decoder=*/false, /*supports_decription=*/false,
        /*decoder_id=*/2);
    SetupExpectations(std::move(quit_closure));
  }

 private:
  void SetHardwarePreference(HardwarePreference preference) override {}

  void SetupExpectations(base::RepeatingClosure quit_closure) {
    EXPECT_CALL(*mock_decoder_, GetMaxDecodeRequests())
        .WillRepeatedly(testing::Return(4));

    EXPECT_CALL(*mock_decoder_, Decode_(_, _))
        .WillOnce([](Unused, media::VideoDecoder::DecodeCB& decode_cb) {
          scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
              FROM_HERE,
              WTF::BindOnce(std::move(decode_cb), media::OkStatus()));
        });

    EXPECT_CALL(*mock_decoder_, Initialize_(_, _, _, _, _, _))
        .WillOnce([quit_closure](Unused, Unused, Unused,
                                 media::VideoDecoder::InitCB& init_cb, Unused,
                                 Unused) {
          scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
              FROM_HERE, WTF::BindOnce(std::move(init_cb), media::OkStatus()));
          scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
              FROM_HERE, std::move(quit_closure));
        });
  }

  MediaDecoderType* decoder() override { return mock_decoder_.get(); }

  std::unique_ptr<media::MockVideoDecoder> mock_decoder_;
};

class VideoDecoderTest : public testing::Test {
 public:
  VideoDecoderTest() = default;
  ~VideoDecoderTest() override = default;

  FakeVideoDecoder* CreateFakeDecoder(ScriptState* script_state,
                                      VideoDecoderInit* init,
                                      ExceptionState& exception_state) {
    return MakeGarbageCollected<FakeVideoDecoder>(script_state, init,
                                                  exception_state);
  }

  VideoDecoderInit* CreateVideoDecoderInit(MockFunctionScope& mock_functions) {
    auto* init = MakeGarbageCollected<VideoDecoderInit>();
    init->setOutput(V8VideoFrameOutputCallback::Create(
        mock_functions.ExpectNoCall()->V8Function()));
    init->setError(V8WebCodecsErrorCallback::Create(
        mock_functions.ExpectNoCall()->V8Function()));
    return init;
  }

  VideoDecoderConfig* CreateVideoConfig() {
    auto* config = MakeGarbageCollected<VideoDecoderConfig>();
    config->setCodec("vp09.00.10.08");
    return config;
  }

  VideoDecoderSupport* ToVideoDecoderSupport(V8TestingScope* v8_scope,
                                             ScriptValue value) {
    return NativeValueTraits<VideoDecoderSupport>::NativeValue(
        v8_scope->GetIsolate(), value.V8Value(), v8_scope->GetExceptionState());
  }
  test::TaskEnvironment task_environment_;
};

TEST_F(VideoDecoderTest, HardwareDecodersApplyPressure) {
  V8TestingScope v8_scope;
  MockFunctionScope mock_functions(v8_scope.GetScriptState());

  auto& pressure_manager_provider =
      CodecPressureManagerProvider::From(*v8_scope.GetExecutionContext());

  auto* decoder_pressure_manager =
      pressure_manager_provider.GetDecoderPressureManager();
  auto* encoder_pressure_manager =
      pressure_manager_provider.GetEncoderPressureManager();

  auto* fake_decoder = CreateFakeDecoder(v8_scope.GetScriptState(),
                                         CreateVideoDecoderInit(mock_functions),
                                         v8_scope.GetExceptionState());

  ASSERT_TRUE(fake_decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  ASSERT_FALSE(fake_decoder->is_applying_codec_pressure());
  ASSERT_EQ(0u, decoder_pressure_manager->pressure_for_testing());
  ASSERT_EQ(0u, encoder_pressure_manager->pressure_for_testing());

  {
    base::RunLoop run_loop;
    fake_decoder->SetupMockHardwareDecoder(run_loop.QuitClosure());

    fake_decoder->configure(CreateVideoConfig(), v8_scope.GetExceptionState());
    ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
    run_loop.Run();
  }

  // Make sure VideoDecoders apply pressure when configured with a HW decoder.
  ASSERT_TRUE(fake_decoder->is_applying_codec_pressure());
  ASSERT_EQ(1u, decoder_pressure_manager->pressure_for_testing());
  ASSERT_EQ(0u, encoder_pressure_manager->pressure_for_testing());

  {
    base::RunLoop run_loop;
    fake_decoder->SetupMockSoftwareDecoder(run_loop.QuitClosure());

    fake_decoder->configure(CreateVideoConfig(), v8_scope.GetExceptionState());
    ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
    run_loop.Run();
  }

  // Make sure the pressure is released when reconfigured with a SW decoder.
  ASSERT_FALSE(fake_decoder->is_applying_codec_pressure());
  ASSERT_EQ(0u, decoder_pressure_manager->pressure_for_testing());
  ASSERT_EQ(0u, encoder_pressure_manager->pressure_for_testing());
}

TEST_F(VideoDecoderTest, ResetReleasesPressure) {
  V8TestingScope v8_scope;
  MockFunctionScope mock_functions(v8_scope.GetScriptState());

  auto* fake_decoder = CreateFakeDecoder(v8_scope.GetScriptState(),
                                         CreateVideoDecoderInit(mock_functions),
                                         v8_scope.GetExceptionState());

  ASSERT_TRUE(fake_decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  // Create a HW decoder.
  {
    base::RunLoop run_loop;
    fake_decoder->SetupMockHardwareDecoder(run_loop.QuitClosure());

    fake_decoder->configure(CreateVideoConfig(), v8_scope.GetExceptionState());
    ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
    run_loop.Run();
  }

  // Make sure VideoDecoders apply pressure when configured with a HW decoder.
  ASSERT_TRUE(fake_decoder->is_applying_codec_pressure());

  // Satisfy reclamation preconditions.
  fake_decoder->SimulateLifecycleStateForTesting(
      scheduler::SchedulingLifecycleState::kHidden);
  fake_decoder->SetGlobalPressureExceededFlag(true);

  // The reclamation timer should be running.
  EXPECT_TRUE(fake_decoder->IsReclamationTimerActiveForTesting());

  // Reset the codec.
  fake_decoder->reset(v8_scope.GetExceptionState());

  // The underlying codec might not be internally released.
  ASSERT_TRUE(fake_decoder->is_applying_codec_pressure());
  EXPECT_TRUE(fake_decoder->IsReclamationTimerActiveForTesting());

  // Reclaiming the codec after a period of inactivity should release pressure.
  fake_decoder->SimulateCodecReclaimedForTesting();
  ASSERT_FALSE(fake_decoder->is_applying_codec_pressure());
  EXPECT_FALSE(fake_decoder->IsReclamationTimerActiveForTesting());
}

TEST_F(VideoDecoderTest, isConfigureSupportedWithInvalidSWConfig) {
  V8TestingScope v8_scope;

  auto* config = MakeGarbageCollected<VideoDecoderConfig>();
  config->setCodec("invalid video codec");
  config->setHardwareAcceleration(V8HardwarePreference::Enum::kPreferSoftware);
  auto promise = VideoDecoder::isConfigSupported(
      v8_scope.GetScriptState(), config, v8_scope.GetExceptionState());
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());
  auto* result = ToVideoDecoderSupport(&v8_scope, tester.Value());
  EXPECT_FALSE(result->supported());
}

TEST_F(VideoDecoderTest, isConfigureSupportedWithInvalidHWConfig) {
  V8TestingScope v8_scope;

  auto* config = MakeGarbageCollected<VideoDecoderConfig>();
  config->setCodec("invalid video codec");
  config->setHardwareAcceleration(V8HardwarePreference::Enum::kPreferHardware);
  auto promise = VideoDecoder::isConfigSupported(
      v8_scope.GetScriptState(), config, v8_scope.GetExceptionState());
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());
  auto* result = ToVideoDecoderSupport(&v8_scope, tester.Value());
  EXPECT_FALSE(result->supported());
}

}  // namespace

}  // namespace blink
