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
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using testing::_;
using testing::Return;
using testing::Unused;

class FakeVideoEncoder : public VideoEncoder {
 public:
  FakeVideoEncoder(ScriptState* script_state,
                   const VideoEncoderInit* init,
                   ExceptionState& exception_state)
      : VideoEncoder(script_state, init, exception_state) {}
  ~FakeVideoEncoder() override = default;

  void SetupMockEncoderCreation(bool is_hw_accelerated,
                                base::RepeatingClosure quit_closure) {
    next_mock_encoder_ = std::make_unique<media::MockVideoEncoder>();
    mock_encoder_is_hw_ = is_hw_accelerated;
    SetupExpectations(quit_closure);
  }

 private:
  void SetupExpectations(base::RepeatingClosure quit_closure) {
    EXPECT_CALL(*next_mock_encoder_, Initialize(_, _, _, _, _))
        .WillOnce([quit_closure](Unused, Unused, Unused, Unused,
                                 media::VideoEncoder::EncoderStatusCB done_cb) {
          scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
              FROM_HERE, WTF::BindOnce(std::move(done_cb),
                                       media::EncoderStatus::Codes::kOk));
          scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
              FROM_HERE, std::move(quit_closure));
        });
  }

  std::unique_ptr<media::VideoEncoder> CreateMediaVideoEncoder(
      const ParsedConfig& config,
      media::GpuVideoAcceleratorFactories* gpu_factories) override {
    EXPECT_TRUE(next_mock_encoder_);

    media::VideoEncoderInfo info;
    info.implementation_name = "MockEncoderName";
    info.is_hardware_accelerated = mock_encoder_is_hw_;
    OnMediaEncoderInfoChanged(info);

    return std::move(next_mock_encoder_);
  }

  bool mock_encoder_is_hw_;
  std::unique_ptr<media::MockVideoEncoder> next_mock_encoder_;
};

class VideoEncoderTest : public testing::Test {
 public:
  VideoEncoderTest() = default;
  ~VideoEncoderTest() override = default;
};

VideoEncoderConfig* CreateConfig() {
  auto* config = MakeGarbageCollected<VideoEncoderConfig>();
  config->setCodec("vp8");
  config->setWidth(80);
  config->setHeight(60);
  return config;
}

VideoEncoder* CreateEncoder(ScriptState* script_state,
                            const VideoEncoderInit* init,
                            ExceptionState& exception_state) {
  return MakeGarbageCollected<VideoEncoder>(script_state, init,
                                            exception_state);
}

FakeVideoEncoder* CreateFakeEncoder(ScriptState* script_state,
                                    VideoEncoderInit* init,
                                    ExceptionState& exception_state) {
  return MakeGarbageCollected<FakeVideoEncoder>(script_state, init,
                                                exception_state);
}

VideoEncoderInit* CreateInit(v8::Local<v8::Function> output_callback,
                             v8::Local<v8::Function> error_callback) {
  auto* init = MakeGarbageCollected<VideoEncoderInit>();
  init->setOutput(V8EncodedVideoChunkOutputCallback::Create(output_callback));
  init->setError(V8WebCodecsErrorCallback::Create(error_callback));
  return init;
}

VideoFrame* MakeVideoFrame(ScriptState* script_state,
                           int width,
                           int height,
                           int timestamp) {
  std::vector<uint8_t> data;
  data.resize(width * height * 4);
  NotShared<DOMUint8ClampedArray> data_u8(DOMUint8ClampedArray::Create(
      reinterpret_cast<const unsigned char*>(data.data()), data.size()));

  ImageData* image_data =
      ImageData::Create(data_u8, width, IGNORE_EXCEPTION_FOR_TESTING);

  if (!image_data)
    return nullptr;

  ImageBitmap* image_bitmap = MakeGarbageCollected<ImageBitmap>(
      image_data, absl::nullopt, ImageBitmapOptions::Create());

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
  auto* encoder = CreateFakeEncoder(script_state, init, es);
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
    encoder->SetupMockEncoderCreation(true, run_loop.QuitClosure());

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
    encoder->SetupMockEncoderCreation(false, run_loop.QuitClosure());

    encoder->configure(config, es);
    ASSERT_FALSE(es.HadException());
    run_loop.Run();
  }

  // Make sure the pressure is released when reconfigured with a SW encoder.
  EXPECT_FALSE(encoder->is_applying_codec_pressure());
  ASSERT_EQ(0u, encoder_pressure_manager->pressure_for_testing());
  ASSERT_EQ(0u, decoder_pressure_manager->pressure_for_testing());
}

}  // namespace

}  // namespace blink
