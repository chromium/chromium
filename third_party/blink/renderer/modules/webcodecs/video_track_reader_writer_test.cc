// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_track_writer_parameters.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webcodecs/video_track_reader.h"
#include "third_party/blink/renderer/modules/webcodecs/video_track_writer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"

namespace blink {

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

class VideoTrackReaderWriterTest : public testing::Test {
 public:
  void TearDown() override {
    RunIOUntilIdle();
    ThreadState::Current()->CollectAllGarbageForTesting();
  }

 protected:
  VideoFrame* CreateBlackVideoFrame(ExecutionContext* context) {
    return MakeGarbageCollected<VideoFrame>(
        media::VideoFrame::CreateBlackFrame(gfx::Size(100, 100)), context);
  }

  void RunIOUntilIdle() const {
    // Tracks use the IO thread to send frames to sinks. Make sure that
    // tasks on IO thread are completed before moving on.
    base::RunLoop run_loop;
    platform_->GetIOTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::BindOnce([] {}), run_loop.QuitClosure());
    run_loop.Run();
    base::RunLoop().RunUntilIdle();
  }

  V8VideoFrameOutputCallback* GetCallback(MockFunction* function) {
    return V8VideoFrameOutputCallback::Create(function->Bind());
  }

 private:
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
};

TEST_F(VideoTrackReaderWriterTest, WriteAndRead) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  VideoTrackWriterParameters params;
  params.setReleaseFrames(false);
  auto* writer =
      VideoTrackWriter::Create(script_state, &params, ASSERT_NO_EXCEPTION);

  auto* read_output_function = MockFunction::Create(script_state);
  auto* reader = VideoTrackReader::Create(script_state, writer->track(),
                                          ASSERT_NO_EXCEPTION);

  reader->start(GetCallback(read_output_function), ASSERT_NO_EXCEPTION);

  auto* frame = CreateBlackVideoFrame(scope.GetExecutionContext());
  writer->writable()
      ->getWriter(script_state, ASSERT_NO_EXCEPTION)
      ->write(script_state,
              ScriptValue(scope.GetIsolate(), ToV8(frame, script_state)),
              ASSERT_NO_EXCEPTION);

  ScriptValue v8_frame;
  // We don't care about Call()'s return value, so we use undefined.
  ScriptValue undefined_value =
      ScriptValue::From(script_state, ToV8UndefinedGenerator());
  EXPECT_CALL(*read_output_function, Call(testing::_))
      .WillOnce(
          testing::DoAll(testing::SaveArg<0>(&v8_frame),
                         testing::Return(testing::ByMove(undefined_value))));

  RunIOUntilIdle();

  testing::Mock::VerifyAndClear(read_output_function);

  auto* read_frame =
      V8VideoFrame::ToImplWithTypeCheck(scope.GetIsolate(), v8_frame.V8Value());

  reader->stop(ASSERT_NO_EXCEPTION);

  ASSERT_TRUE(frame);
  EXPECT_EQ(frame->frame(), read_frame->frame());

  // Auto-release turned off
  EXPECT_NE(nullptr, frame->frame());
}

TEST_F(VideoTrackReaderWriterTest, AutoRelease) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  VideoTrackWriterParameters params;
  params.setReleaseFrames(true);
  auto* writer =
      VideoTrackWriter::Create(script_state, &params, ASSERT_NO_EXCEPTION);

  auto* frame = CreateBlackVideoFrame(scope.GetExecutionContext());
  writer->writable()
      ->getWriter(script_state, ASSERT_NO_EXCEPTION)
      ->write(script_state,
              ScriptValue(scope.GetIsolate(), ToV8(frame, script_state)),
              ASSERT_NO_EXCEPTION);

  RunIOUntilIdle();

  // Auto-release worked
  EXPECT_EQ(nullptr, frame->frame());
}

TEST_F(VideoTrackReaderWriterTest, Abort) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  VideoTrackWriterParameters params;
  params.setReleaseFrames(false);
  auto* writer =
      VideoTrackWriter::Create(script_state, &params, ASSERT_NO_EXCEPTION);

  EXPECT_EQ(writer->track()->readyState(), "live");

  writer->writable()->abort(script_state, ASSERT_NO_EXCEPTION);

  RunIOUntilIdle();

  EXPECT_TRUE(writer->track()->Ended());
}

}  // namespace blink
