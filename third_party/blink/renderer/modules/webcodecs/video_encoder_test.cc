// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_encoder.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/video_encoder.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

class MockFunction : public ScriptFunction {
 public:
  static testing::StrictMock<MockFunction>* Create(ScriptState* script_state) {
    return MakeGarbageCollected<testing::StrictMock<MockFunction>>(
        script_state);
  }

  v8::Local<v8::Function> Bind() { return BindToV8Function(); }

  MOCK_METHOD1(Call, ScriptValue(ScriptValue));

 protected:
  explicit MockFunction(ScriptState* script_state)
      : ScriptFunction(script_state) {}
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

VideoEncoderInit* CreateInit(MockFunction* output_callback,
                             MockFunction* error_callback) {
  auto* init = MakeGarbageCollected<VideoEncoderInit>();
  init->setOutput(
      V8EncodedVideoChunkOutputCallback::Create(output_callback->Bind()));
  init->setError(V8WebCodecsErrorCallback::Create(error_callback->Bind()));
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

  auto* init = CreateInit(MockFunction::Create(script_state),
                          MockFunction::Create(script_state));
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

  auto promise = encoder->flush(es);
  ScriptPromiseTester tester(script_state, promise);
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

  // Create a video encoder.
  auto* output_callback = MockFunction::Create(script_state);
  auto* error_callback = MockFunction::Create(script_state);

  auto* init = CreateInit(output_callback, error_callback);
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

  // The encoder should be active, for reclamation purposes.
  ASSERT_TRUE(encoder->IsReclamationTimerActiveForTesting());

  // Resetting the encoder should prevent codec reclamation, silently.
  EXPECT_CALL(*error_callback, Call(testing::_)).Times(0);
  encoder->reset(es);
  ASSERT_FALSE(encoder->IsReclamationTimerActiveForTesting());

  testing::Mock::VerifyAndClearExpectations(error_callback);

  // Reconfiguring the encoder should restart the reclamation timer.
  encoder->configure(config, es);
  ASSERT_FALSE(es.HadException());
  {
    // We need this to make sure that configuration has completed.
    auto promise = encoder->flush(es);
    ScriptPromiseTester tester(script_state, promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
  }

  ASSERT_TRUE(encoder->IsReclamationTimerActiveForTesting());

  // Reclaiming a configured encoder should call the error callback.
  EXPECT_CALL(*error_callback, Call(testing::_)).Times(1);
  encoder->SimulateCodecReclaimedForTesting();
  ASSERT_FALSE(encoder->IsReclamationTimerActiveForTesting());

  testing::Mock::VerifyAndClearExpectations(error_callback);
}

}  // namespace

}  // namespace blink
