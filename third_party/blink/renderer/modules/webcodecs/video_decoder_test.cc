// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_decoder.h"

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_switches.h"
#include "media/base/mock_filters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_support.h"
#include "third_party/blink/renderer/core/testing/mock_function_scope.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_manager.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_manager_provider.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using testing::_;
using testing::Unused;

class MockErrorCallbackFunction : public ScriptFunction {
 public:
  explicit MockErrorCallbackFunction(v8::Isolate* isolate)
      : isolate_(isolate) {}

  MOCK_METHOD1(OnError, void(const String&));

  ScriptValue Call(ScriptState* script_state, ScriptValue args) override {
    OnError(V8DOMException::ToWrappable(isolate_, args.V8Value())->name());
    return ScriptValue();
  }

 private:
  raw_ptr<v8::Isolate> const isolate_;
};

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
    CreateMockSoftwareDecoder();
    SetupExpectations(std::move(quit_closure));
  }

  void CreateMockSoftwareDecoder() {
    mock_decoder_ = std::make_unique<media::MockVideoDecoder>(
        /*is_platform_decoder=*/false, /*supports_decription=*/false,
        /*decoder_id=*/2);
  }

  media::MockVideoDecoder* mock_decoder() { return mock_decoder_.get(); }

 private:
  void SetHardwarePreference(HardwarePreference preference) override {}

  void SetupExpectations(base::RepeatingClosure quit_closure) {
    EXPECT_CALL(*mock_decoder_, GetMaxDecodeRequests())
        .WillRepeatedly(testing::Return(4));

    // Due to how this test overrides decoder(), calls to configure() result in
    // an EOS buffer being sent to the decoder.
    EXPECT_CALL(*mock_decoder_, Decode_(_, _))
        .WillOnce([](Unused, media::VideoDecoder::DecodeCB& decode_cb) {
          scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
              FROM_HERE,
              blink::BindOnce(std::move(decode_cb), media::OkStatus()));
        });

    EXPECT_CALL(*mock_decoder_, Initialize_(_, _, _, _, _, _))
        .WillOnce([quit_closure](Unused, Unused, Unused,
                                 media::VideoDecoder::InitCB& init_cb, Unused,
                                 Unused) {
          scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
              FROM_HERE,
              blink::BindOnce(std::move(init_cb), media::OkStatus()));
          scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
              FROM_HERE, std::move(quit_closure));
        });
  }

  // This is the magic of how this test works. It relaces the VideoDecoderBroker
  // instance created during VideoDecoderTraits::CreateDecoder().
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

  VideoDecoderInit* CreateVideoDecoderInit(ScriptState* script_state,
                                           MockFunctionScope& mock_functions) {
    auto* init = MakeGarbageCollected<VideoDecoderInit>();
    init->setOutput(V8VideoFrameOutputCallback::Create(
        mock_functions.ExpectNoCall()->ToV8Function(script_state)));
    init->setError(V8WebCodecsErrorCallback::Create(
        mock_functions.ExpectNoCall()->ToV8Function(script_state)));
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

  auto* fake_decoder = CreateFakeDecoder(
      v8_scope.GetScriptState(),
      CreateVideoDecoderInit(v8_scope.GetScriptState(), mock_functions),
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

  auto* fake_decoder = CreateFakeDecoder(
      v8_scope.GetScriptState(),
      CreateVideoDecoderInit(v8_scope.GetScriptState(), mock_functions),
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
  EXPECT_CALL(*fake_decoder->mock_decoder(), Reset_(_))
      .WillOnce([](base::OnceClosure& closure) {
        scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
            FROM_HERE, std::move(closure));
      });
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

TEST_F(VideoDecoderTest, ConfigureGeneratesConfigChangeEOS) {
  base::test::ScopedFeatureList feature_list{
      media::kWebCodecsDecoderFlushOptimizations};
  V8TestingScope v8_scope;
  MockFunctionScope mock_functions(v8_scope.GetScriptState());

  auto* fake_decoder = CreateFakeDecoder(
      v8_scope.GetScriptState(),
      CreateVideoDecoderInit(v8_scope.GetScriptState(), mock_functions),
      v8_scope.GetExceptionState());

  fake_decoder->CreateMockSoftwareDecoder();

  base::RunLoop run_loop;
  EXPECT_CALL(*fake_decoder->mock_decoder(), GetMaxDecodeRequests())
      .WillRepeatedly(testing::Return(4));

  // Due to how this test overrides decoder(), calls to configure() result in
  // an EOS buffer being sent to the decoder.
  EXPECT_CALL(*fake_decoder->mock_decoder(), Decode_(_, _))
      .WillOnce([](scoped_refptr<media::DecoderBuffer> buffer,
                   media::VideoDecoder::DecodeCB& decode_cb) {
        EXPECT_TRUE(buffer->end_of_stream());
        EXPECT_TRUE(buffer->next_config().has_value());

        scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
            FROM_HERE,
            blink::BindOnce(std::move(decode_cb), media::OkStatus()));
      });

  EXPECT_CALL(*fake_decoder->mock_decoder(), Initialize_(_, _, _, _, _, _))
      .WillOnce([&](Unused, Unused, Unused,
                    media::VideoDecoder::InitCB& init_cb, Unused, Unused) {
        scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
            FROM_HERE, blink::BindOnce(std::move(init_cb), media::OkStatus()));
        scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
            FROM_HERE, run_loop.QuitClosure());
      });

  fake_decoder->configure(CreateVideoConfig(), v8_scope.GetExceptionState());
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  run_loop.Run();
}

TEST_F(VideoDecoderTest, TooManyDecodersGeneratesQuotaExceededError) {
  V8TestingScope v8_scope;
  MockFunctionScope mock_functions(v8_scope.GetScriptState());

  auto* init = MakeGarbageCollected<VideoDecoderInit>();
  init->setOutput(V8VideoFrameOutputCallback::Create(
      mock_functions.ExpectNoCall()->ToV8Function(v8_scope.GetScriptState())));

  auto* error_callback = MakeGarbageCollected<MockErrorCallbackFunction>(
      v8_scope.GetScriptState()->GetIsolate());
  EXPECT_CALL(*error_callback, OnError(DOMException::GetErrorName(
                                   DOMExceptionCode::kQuotaExceededError)))
      .Times(1);
  init->setError(V8WebCodecsErrorCallback::Create(
      error_callback->ToV8Function(v8_scope.GetScriptState())));

  auto* fake_decoder = CreateFakeDecoder(v8_scope.GetScriptState(), init,
                                         v8_scope.GetExceptionState());

  fake_decoder->CreateMockSoftwareDecoder();

  base::RunLoop run_loop;
  EXPECT_CALL(*fake_decoder->mock_decoder(), GetMaxDecodeRequests())
      .WillRepeatedly(testing::Return(4));

  // Due to how this test overrides decoder(), calls to configure() result in
  // an EOS buffer being sent to the decoder.
  EXPECT_CALL(*fake_decoder->mock_decoder(), Decode_(_, _))
      .WillOnce([](scoped_refptr<media::DecoderBuffer> buffer,
                   media::VideoDecoder::DecodeCB& decode_cb) {
        scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
            FROM_HERE,
            blink::BindOnce(std::move(decode_cb), media::OkStatus()));
      });

  EXPECT_CALL(*fake_decoder->mock_decoder(), Initialize_(_, _, _, _, _, _))
      .WillOnce([&](Unused, Unused, Unused,
                    media::VideoDecoder::InitCB& init_cb, Unused, Unused) {
        scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
            FROM_HERE,
            blink::BindOnce(std::move(init_cb),
                            media::DecoderStatus::Codes::kTooManyDecoders));
        scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
            FROM_HERE, run_loop.QuitClosure());
      });

  fake_decoder->configure(CreateVideoConfig(), v8_scope.GetExceptionState());
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  run_loop.Run();
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
TEST_F(VideoDecoderTest, BitstreamConversionPreservesKeyFrameFlag) {
  V8TestingScope v8_scope;
  MockFunctionScope mock_functions(v8_scope.GetScriptState());

  auto* fake_decoder = CreateFakeDecoder(
      v8_scope.GetScriptState(),
      CreateVideoDecoderInit(v8_scope.GetScriptState(), mock_functions),
      v8_scope.GetExceptionState());

  constexpr auto kAvcc = std::to_array<uint8_t>(
      {0x01, 0x42, 0x00, 0x28, 0xFF, 0xE1, 0x00, 0x08, 0x67, 0x42, 0x00, 0x28,
       0xE9, 0x05, 0x89, 0xC8, 0x01, 0x00, 0x04, 0x68, 0xCE, 0x06, 0xF2, 0x00});

  auto* config = MakeGarbageCollected<VideoDecoderConfig>();
  config->setCodec("avc1.420028");
  config->setHardwareAcceleration(V8HardwarePreference::Enum::kPreferSoftware);
  config->setDescription(MakeGarbageCollected<AllowSharedBufferSource>(
      DOMArrayBuffer::Create(kAvcc)));

  {
    base::RunLoop run_loop;
    fake_decoder->SetupMockSoftwareDecoder(run_loop.QuitWhenIdleClosure());
    fake_decoder->configure(config, v8_scope.GetExceptionState());
    ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
    run_loop.Run();
  }

  constexpr auto kH264KeyFrame = std::to_array<uint8_t>(
      {0x00, 0x00, 0x00, 0x08, 0x65, 0xB8, 0x40, 0x57, 0x0B, 0xF0, 0xDF, 0xF8});

  auto* chunk_init = MakeGarbageCollected<EncodedVideoChunkInit>();
  chunk_init->setType(V8EncodedVideoChunkType::Enum::kKey);
  chunk_init->setTimestamp(0);
  chunk_init->setData(MakeGarbageCollected<AllowSharedBufferSource>(
      DOMArrayBuffer::Create(kH264KeyFrame)));
  auto* chunk = EncodedVideoChunk::Create(v8_scope.GetScriptState(), chunk_init,
                                          v8_scope.GetExceptionState());
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(*fake_decoder->mock_decoder(), Decode_(_, _))
        .WillOnce([&run_loop](scoped_refptr<media::DecoderBuffer> buffer,
                              media::VideoDecoder::DecodeCB& decode_cb) {
          EXPECT_TRUE(buffer->is_key_frame());
          scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
              FROM_HERE,
              blink::BindOnce(std::move(decode_cb), media::OkStatus()));
          run_loop.Quit();
        });

    fake_decoder->decode(chunk, v8_scope.GetExceptionState());
    ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
    run_loop.Run();
  }
}
#endif

}  // namespace

}  // namespace blink
