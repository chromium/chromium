// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_encoder.h"

#include "base/run_loop.h"
#include "media/base/mock_filters.h"
#include "media/video/video_encoder_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_cssimagevalue_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_encode_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_init.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/testing/mock_function_scope.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_manager.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_manager_provider.h"
#include "third_party/blink/renderer/modules/webcodecs/video_encoder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using testing::_;
using testing::ByMove;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;
using testing::WithArgs;

ACTION_P(RunClosure, closure) {
  scheduler::GetSequencedTaskRunnerForTesting()->PostTask(FROM_HERE,
                                                          std::move(closure));
}

class MockVideoEncoder : public VideoEncoder {
 public:
  MockVideoEncoder(ScriptState* script_state,
                   const VideoEncoderInit* init,
                   ExceptionState& exception_state)
      : VideoEncoder(script_state, init, exception_state) {}
  ~MockVideoEncoder() override = default;

  MOCK_METHOD((media::EncoderStatus::Or<std::unique_ptr<media::VideoEncoder>>),
              CreateMediaVideoEncoder,
              (const ParsedConfig& config,
               media::GpuVideoAcceleratorFactories* gpu_factories,
               bool& is_platform_encoder),
              (override));
  MOCK_METHOD(std::unique_ptr<media::VideoEncoderMetricsProvider>,
              CreateVideoEncoderMetricsProvider,
              (),
              (const));

  // CallOnMediaENcoderInfoChanged() is necessary for VideoEncoderTest to call
  // VideoEncoder::OnMediaEncoderInfoChanged() because the function is a private
  // and VideoEncoderTest is not a friend of VideoEncoder.
  void CallOnMediaEncoderInfoChanged(
      const media::VideoEncoderInfo& encoder_info) {
    VideoEncoder::OnMediaEncoderInfoChanged(encoder_info);
  }
};

class VideoEncoderTest : public testing::Test {
 public:
  VideoEncoderTest() = default;
  ~VideoEncoderTest() override = default;
  test::TaskEnvironment task_environment_;
};

constexpr gfx::Size kEncodeSize(80, 60);

VideoEncoderConfig* CreateConfig() {
  auto* config = MakeGarbageCollected<VideoEncoderConfig>();
  config->setCodec("vp8");
  config->setWidth(kEncodeSize.width());
  config->setHeight(kEncodeSize.height());
  return config;
}

VideoEncoder* CreateEncoder(ScriptState* script_state,
                            const VideoEncoderInit* init,
                            ExceptionState& exception_state) {
  return MakeGarbageCollected<VideoEncoder>(script_state, init,
                                            exception_state);
}

MockVideoEncoder* CreateMockEncoder(ScriptState* script_state,
                                    VideoEncoderInit* init,
                                    ExceptionState& exception_state) {
  return MakeGarbageCollected<MockVideoEncoder>(script_state, init,
                                                exception_state);
}

VideoEncoderInit* CreateInit(ScriptFunction* output_callback,
                             ScriptFunction* error_callback) {
  auto* init = MakeGarbageCollected<VideoEncoderInit>();
  init->setOutput(
      V8EncodedVideoChunkOutputCallback::Create(output_callback->V8Function()));
  init->setError(
      V8WebCodecsErrorCallback::Create(error_callback->V8Function()));
  return init;
}

VideoFrame* MakeVideoFrame(ScriptState* script_state,
                           int width,
                           int height,
                           int timestamp) {
  std::vector<uint8_t> data(width * height * 4);
  NotShared<DOMUint8ClampedArray> data_u8(DOMUint8ClampedArray::Create(data));

  ImageData* image_data =
      ImageData::Create(data_u8, width, IGNORE_EXCEPTION_FOR_TESTING);

  if (!image_data)
    return nullptr;

  ImageBitmap* image_bitmap = MakeGarbageCollected<ImageBitmap>(
      image_data, std::nullopt, ImageBitmapOptions::Create());

  VideoFrameInit* video_frame_init = VideoFrameInit::Create();
  video_frame_init->setTimestamp(timestamp);

  auto* source = MakeGarbageCollected<V8CanvasImageSource>(image_bitmap);

  return VideoFrame::Create(script_state, source, video_frame_init,
                            IGNORE_EXCEPTION_FOR_TESTING);
}

TEST_F(VideoEncoderTest, RejectFlushAfterClose) {
  V8TestingScope v8_scope;
  auto& es = v8_scope.GetExceptionState();
  auto* script_state = v8_scope.GetScriptState();

  MockFunctionScope mock_function(script_state);
  auto* init =
      CreateInit(mock_function.ExpectNoCall(), mock_function.ExpectNoCall());
  auto* encoder = CreateEncoder(script_state, init, es);
  ASSERT_FALSE(es.HadException());

  auto* config = CreateConfig();
  encoder->configure(config, es);
  ASSERT_FALSE(es.HadException());
  {
    // We need this to make sure that configuration has completed.
    auto promise = encoder->flush(es);
    ScriptPromiseTester tester(script_state, promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
  }

  encoder->encode(
      MakeVideoFrame(script_state, config->width(), config->height(), 1),
      MakeGarbageCollected<VideoEncoderEncodeOptions>(), es);

  ScriptPromiseTester tester(script_state, encoder->flush(es));
  ASSERT_FALSE(es.HadException());
  ASSERT_FALSE(tester.IsFulfilled());
  ASSERT_FALSE(tester.IsRejected());

  encoder->close(es);

  ThreadState::Current()->CollectAllGarbageForTesting();

  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsRejected());
}

TEST_F(VideoEncoderTest, CodecReclamation) {
  V8TestingScope v8_scope;
  auto& es = v8_scope.GetExceptionState();
  auto* script_state = v8_scope.GetScriptState();

  MockFunctionScope mock_function(script_state);

  auto& pressure_manager_provider =
      CodecPressureManagerProvider::From(*v8_scope.GetExecutionContext());

  auto* decoder_pressure_manager =
      pressure_manager_provider.GetDecoderPressureManager();
  auto* encoder_pressure_manager =
      pressure_manager_provider.GetEncoderPressureManager();

  // Create a video encoder.
  auto* init =
      CreateInit(mock_function.ExpectNoCall(), mock_function.ExpectNoCall());
  auto* encoder = CreateMockEncoder(script_state, init, es);
  ASSERT_FALSE(es.HadException());

  // Simulate backgrounding to enable reclamation.
  if (!encoder->is_backgrounded_for_testing()) {
    encoder->SimulateLifecycleStateForTesting(
        scheduler::SchedulingLifecycleState::kHidden);
    DCHECK(encoder->is_backgrounded_for_testing());
  }

  // Make sure VideoEncoder doesn't apply pressure by default.
  EXPECT_FALSE(encoder->is_applying_codec_pressure());
  ASSERT_EQ(0u, encoder_pressure_manager->pressure_for_testing());
  ASSERT_EQ(0u, decoder_pressure_manager->pressure_for_testing());

  auto* config = CreateConfig();
  {
    base::RunLoop run_loop;
    auto media_encoder = std::make_unique<media::MockVideoEncoder>();
    media::MockVideoEncoder* mock_media_encoder = media_encoder.get();

    EXPECT_CALL(*encoder, CreateMediaVideoEncoder(_, _, _))
        .WillOnce(DoAll(Invoke([encoder = encoder]() {
                          media::VideoEncoderInfo info;
                          info.implementation_name = "MockEncoderName";
                          info.is_hardware_accelerated = true;
                          encoder->CallOnMediaEncoderInfoChanged(info);
                        }),
                        Return(ByMove(std::unique_ptr<media::VideoEncoder>(
                            std::move(media_encoder))))));
    EXPECT_CALL(*encoder, CreateVideoEncoderMetricsProvider())
        .WillOnce(Return(ByMove(
            std::make_unique<media::MockVideoEncoderMetricsProvider>())));
    EXPECT_CALL(*mock_media_encoder, Initialize(_, _, _, _, _))
        .WillOnce(WithArgs<4>(
            Invoke([quit_closure = run_loop.QuitWhenIdleClosure()](
                       media::VideoEncoder::EncoderStatusCB done_cb) {
              scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
                  FROM_HERE, WTF::BindOnce(std::move(done_cb),
                                           media::EncoderStatus::Codes::kOk));
              scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
                  FROM_HERE, std::move(quit_closure));
            })));

    encoder->configure(config, es);
    ASSERT_FALSE(es.HadException());
    run_loop.Run();
  }

  // Make sure VideoEncoders apply pressure when configured with a HW encoder.
  EXPECT_TRUE(encoder->is_applying_codec_pressure());
  ASSERT_EQ(1u, encoder_pressure_manager->pressure_for_testing());
  ASSERT_EQ(0u, decoder_pressure_manager->pressure_for_testing());

  // Change codec to avoid a pure reconfigure.
  config->setCodec("avc1.42001E");
  {
    base::RunLoop run_loop;

    auto media_encoder = std::make_unique<media::MockVideoEncoder>();
    media::MockVideoEncoder* mock_media_encoder = media_encoder.get();

    EXPECT_CALL(*encoder, CreateMediaVideoEncoder(_, _, _))
        .WillOnce(DoAll(Invoke([encoder = encoder]() {
                          media::VideoEncoderInfo info;
                          info.implementation_name = "MockEncoderName";
                          info.is_hardware_accelerated = false;
                          encoder->CallOnMediaEncoderInfoChanged(info);
                        }),
                        Return(ByMove(std::unique_ptr<media::VideoEncoder>(
                            std::move(media_encoder))))));
    EXPECT_CALL(*mock_media_encoder, Initialize(_, _, _, _, _))
        .WillOnce(WithArgs<4>(
            Invoke([quit_closure = run_loop.QuitWhenIdleClosure()](
                       media::VideoEncoder::EncoderStatusCB done_cb) {
              scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
                  FROM_HERE, WTF::BindOnce(std::move(done_cb),
                                           media::EncoderStatus::Codes::kOk));
              scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
                  FROM_HERE, std::move(quit_closure));
            })));

    encoder->configure(config, es);
    ASSERT_FALSE(es.HadException());
    run_loop.Run();
  }

  // Make sure the pressure is released when reconfigured with a SW encoder.
  EXPECT_FALSE(encoder->is_applying_codec_pressure());
  ASSERT_EQ(0u, encoder_pressure_manager->pressure_for_testing());
  ASSERT_EQ(0u, decoder_pressure_manager->pressure_for_testing());
}

TEST_F(
    VideoEncoderTest,
    ConfigureAndEncode_CallVideoEncoderMetricsProviderInitializeAndIncrementEncodedFrameCount) {
  V8TestingScope v8_scope;
  auto& es = v8_scope.GetExceptionState();
  auto* script_state = v8_scope.GetScriptState();

  MockFunctionScope mock_function(script_state);

  // Create a video encoder.
  auto* init =
      CreateInit(mock_function.ExpectCall(), mock_function.ExpectNoCall());
  auto* encoder = CreateMockEncoder(script_state, init, es);

  auto* config = CreateConfig();
  base::RunLoop run_loop;
  media::VideoEncoder::OutputCB output_cb;
  auto media_encoder = std::make_unique<media::MockVideoEncoder>();
  media::MockVideoEncoder* mock_media_encoder = media_encoder.get();
  auto encoder_metrics_provider =
      std::make_unique<media::MockVideoEncoderMetricsProvider>();
  media::MockVideoEncoderMetricsProvider* mock_encoder_metrics_provider =
      encoder_metrics_provider.get();
  EXPECT_CALL(*encoder, CreateMediaVideoEncoder(_, _, _))
      .WillOnce(DoAll(Invoke([encoder = encoder]() {
                        media::VideoEncoderInfo info;
                        info.implementation_name = "MockEncoderName";
                        info.is_hardware_accelerated = false;
                        encoder->CallOnMediaEncoderInfoChanged(info);
                      }),
                      Return(ByMove(std::unique_ptr<media::VideoEncoder>(
                          std::move(media_encoder))))));
  EXPECT_CALL(*encoder, CreateVideoEncoderMetricsProvider())
      .WillOnce(Return(ByMove(std::move(encoder_metrics_provider))));
  EXPECT_CALL(
      *mock_encoder_metrics_provider,
      MockInitialize(media::VideoCodecProfile::VP8PROFILE_ANY, kEncodeSize,
                     false, media::SVCScalabilityMode::kL1T1));
  EXPECT_CALL(*mock_media_encoder, Initialize(_, _, _, _, _))
      .WillOnce(DoAll(
          SaveArg<3>(&output_cb),
          WithArgs<4>(Invoke([quit_closure = run_loop.QuitWhenIdleClosure()](
                                 media::VideoEncoder::EncoderStatusCB done_cb) {
            scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
                FROM_HERE, WTF::BindOnce(std::move(done_cb),
                                         media::EncoderStatus::Codes::kOk));
          }))));
  encoder->configure(config, es);
  EXPECT_CALL(*mock_media_encoder, Encode(_, _, _))
      .WillOnce(
          WithArgs<2>(Invoke([output_cb = &output_cb](
                                 media::VideoEncoder::EncoderStatusCB done_cb) {
            scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
                FROM_HERE, WTF::BindOnce(std::move(done_cb),
                                         media::EncoderStatus::Codes::kOk));
            media::VideoEncoderOutput out;
            out.data = base::HeapArray<uint8_t>::Uninit(100);
            out.key_frame = true;
            scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
                FROM_HERE,
                WTF::BindOnce(*output_cb, std::move(out), std::nullopt));
          })));

  EXPECT_CALL(*mock_encoder_metrics_provider, MockIncrementEncodedFrameCount())
      .WillOnce([quit_closure = run_loop.QuitWhenIdleClosure()] {
        scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
            FROM_HERE, std::move(quit_closure));
      });
  encoder->encode(
      MakeVideoFrame(script_state, config->width(), config->height(), 1),
      MakeGarbageCollected<VideoEncoderEncodeOptions>(), es);
  run_loop.Run();
}

TEST_F(VideoEncoderTest,
       ConfigureTwice_CallVideoEncoderMetricsProviderInitializeTwice) {
  V8TestingScope v8_scope;
  auto& es = v8_scope.GetExceptionState();
  auto* script_state = v8_scope.GetScriptState();

  MockFunctionScope mock_function(script_state);

  // Create a video encoder.
  auto* init =
      CreateInit(mock_function.ExpectNoCall(), mock_function.ExpectNoCall());
  auto* encoder = CreateMockEncoder(script_state, init, es);

  auto* config = CreateConfig();
  base::RunLoop run_loop;
  media::VideoEncoder::OutputCB output_cb;
  auto media_encoder = std::make_unique<media::MockVideoEncoder>();
  media::MockVideoEncoder* mock_media_encoder = media_encoder.get();
  auto encoder_metrics_provider =
      std::make_unique<media::MockVideoEncoderMetricsProvider>();
  media::MockVideoEncoderMetricsProvider* mock_encoder_metrics_provider =
      encoder_metrics_provider.get();
  EXPECT_CALL(*encoder, CreateMediaVideoEncoder(_, _, _))
      .WillOnce(DoAll(Invoke([encoder = encoder]() {
                        media::VideoEncoderInfo info;
                        info.implementation_name = "MockEncoderName";
                        info.is_hardware_accelerated = false;
                        encoder->CallOnMediaEncoderInfoChanged(info);
                      }),
                      Return(ByMove(std::unique_ptr<media::VideoEncoder>(
                          std::move(media_encoder))))));
  EXPECT_CALL(*encoder, CreateVideoEncoderMetricsProvider())
      .WillOnce(Return(ByMove(std::move(encoder_metrics_provider))));
  EXPECT_CALL(
      *mock_encoder_metrics_provider,
      MockInitialize(media::VideoCodecProfile::VP8PROFILE_ANY, kEncodeSize,
                     false, media::SVCScalabilityMode::kL1T1));
  EXPECT_CALL(*mock_media_encoder, Initialize(_, _, _, _, _))
      .WillOnce(DoAll(
          SaveArg<3>(&output_cb),
          WithArgs<4>(Invoke([](media::VideoEncoder::EncoderStatusCB done_cb) {
            scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
                FROM_HERE, WTF::BindOnce(std::move(done_cb),
                                         media::EncoderStatus::Codes::kOk));
          }))));
  encoder->configure(config, es);
  EXPECT_CALL(*mock_media_encoder, Flush(_))
      .WillOnce([](media::VideoEncoder::EncoderStatusCB done_cb) {
        scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
            FROM_HERE, WTF::BindOnce(std::move(done_cb),
                                     media::EncoderStatus::Codes::kOk));
      });
  EXPECT_CALL(
      *mock_encoder_metrics_provider,
      MockInitialize(media::VideoCodecProfile::VP8PROFILE_ANY, kEncodeSize,
                     false, media::SVCScalabilityMode::kL1T1));
  EXPECT_CALL(*mock_media_encoder, ChangeOptions(_, _, _))
      .WillOnce(
          WithArgs<2>(Invoke([quit_closure = run_loop.QuitWhenIdleClosure()](
                                 media::VideoEncoder::EncoderStatusCB done_cb) {
            scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
                FROM_HERE, WTF::BindOnce(std::move(done_cb),
                                         media::EncoderStatus::Codes::kOk));
            scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
                FROM_HERE, std::move(quit_closure));
          })));
  encoder->configure(config, es);
  run_loop.Run();
}

TEST_F(VideoEncoderTest,
       InitializeFailure_CallVideoEncoderMetricsProviderSetError) {
  V8TestingScope v8_scope;
  auto& es = v8_scope.GetExceptionState();
  auto* script_state = v8_scope.GetScriptState();

  MockFunctionScope mock_function(script_state);

  // Create a video encoder.
  auto* init =
      CreateInit(mock_function.ExpectNoCall(), mock_function.ExpectCall());
  auto* encoder = CreateMockEncoder(script_state, init, es);

  auto* config = CreateConfig();
  base::RunLoop run_loop;
  media::VideoEncoder::OutputCB output_cb;
  auto media_encoder = std::make_unique<media::MockVideoEncoder>();
  media::MockVideoEncoder* mock_media_encoder = media_encoder.get();
  auto encoder_metrics_provider =
      std::make_unique<media::MockVideoEncoderMetricsProvider>();
  media::MockVideoEncoderMetricsProvider* mock_encoder_metrics_provider =
      encoder_metrics_provider.get();
  EXPECT_CALL(*encoder, CreateMediaVideoEncoder(_, _, _))
      .WillOnce(DoAll(Invoke([encoder = encoder]() {
                        media::VideoEncoderInfo info;
                        info.implementation_name = "MockEncoderName";
                        info.is_hardware_accelerated = false;
                        encoder->CallOnMediaEncoderInfoChanged(info);
                      }),
                      Return(ByMove(std::unique_ptr<media::VideoEncoder>(
                          std::move(media_encoder))))));
  EXPECT_CALL(*encoder, CreateVideoEncoderMetricsProvider())
      .WillOnce(Return(ByMove(std::move(encoder_metrics_provider))));
  EXPECT_CALL(
      *mock_encoder_metrics_provider,
      MockInitialize(media::VideoCodecProfile::VP8PROFILE_ANY, kEncodeSize,
                     false, media::SVCScalabilityMode::kL1T1));
  EXPECT_CALL(*mock_media_encoder, Initialize(_, _, _, _, _))
      .WillOnce(
          WithArgs<4>(Invoke([quit_closure = run_loop.QuitWhenIdleClosure()](
                                 media::VideoEncoder::EncoderStatusCB done_cb) {
            scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
                FROM_HERE,
                WTF::BindOnce(
                    std::move(done_cb),
                    media::EncoderStatus::Codes::kEncoderUnsupportedConfig));
          })));
  EXPECT_CALL(*mock_encoder_metrics_provider, MockSetError(_))
      .WillOnce(RunClosure(run_loop.QuitWhenIdleClosure()));
  encoder->configure(config, es);
  run_loop.Run();
}

TEST_F(VideoEncoderTest, NoAvailableMediaVideoEncoder) {
  V8TestingScope v8_scope;
  auto& es = v8_scope.GetExceptionState();
  auto* script_state = v8_scope.GetScriptState();

  MockFunctionScope mock_function(script_state);

  // Create a video encoder.
  auto* init =
      CreateInit(mock_function.ExpectNoCall(), mock_function.ExpectCall());
  auto* encoder = CreateMockEncoder(script_state, init, es);
  auto* config = CreateConfig();
  EXPECT_CALL(*encoder, CreateMediaVideoEncoder(_, _, _))
      .WillOnce(Return(media::EncoderStatus(
          media::EncoderStatus::Codes::kEncoderUnsupportedProfile)));
  encoder->configure(config, es);
}
}  // namespace

}  // namespace blink
