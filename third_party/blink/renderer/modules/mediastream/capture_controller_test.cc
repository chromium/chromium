// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/capture_controller.h"

#include "base/functional/overloaded.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_captured_wheel_action.h"
#include "third_party/blink/renderer/modules/mediastream/browser_capture_media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"

namespace blink {

namespace {

using SurfaceType = media::mojom::DisplayCaptureSurfaceType;
using SendWheelResult = MockMediaStreamVideoSource::SendWheelResult;
using GetZoomLevelResult = MockMediaStreamVideoSource::GetZoomLevelResult;

// TODO(crbug.com/1505223): Avoid this helper's duplication throughout Blink.
bool IsDOMException(ScriptState* script_state,
                    const ScriptValue& value,
                    DOMExceptionCode code) {
  DOMException* const dom_exception =
      V8DOMException::ToWrappable(script_state->GetIsolate(), value.V8Value());
  return dom_exception && dom_exception->name() == DOMException(code).name();
}

bool IsDOMException(V8TestingScope& v8_scope,
                    const ScriptValue& value,
                    DOMExceptionCode code) {
  return IsDOMException(v8_scope.GetScriptState(), value, code);
}

// Note that we don't actually care what the message is. We use this as a way
// to sanity-check the tests themselves against false-positives through
// failures on different code paths that yield the same DOMException.
String GetDOMExceptionMessage(V8TestingScope& v8_scope,
                              const ScriptValue& value) {
  ScriptState* const script_state = v8_scope.GetScriptState();
  DOMException* const dom_exception =
      V8DOMException::ToWrappable(script_state->GetIsolate(), value.V8Value());
  CHECK(dom_exception) << "Malformed test.";
  return dom_exception->message();
}

// TODO(crbug.com/1505218): Move to a shared location to avoid duplication.
MediaStreamTrack* MakeTrack(
    V8TestingScope& v8_scope,
    SurfaceType display_surface,
    absl::variant<absl::monostate, SendWheelResult, GetZoomLevelResult>
        mock_source_result = absl::monostate()) {
  std::unique_ptr<MockMediaStreamVideoSource> media_stream_video_source =
      base::WrapUnique(new ::testing::NiceMock<MockMediaStreamVideoSource>(
          media::VideoCaptureFormat(gfx::Size(640, 480), 30.0,
                                    media::PIXEL_FORMAT_I420),
          true));
  absl::visit(
      base::Overloaded{
          [](absl::monostate) {},
          [&media_stream_video_source](SendWheelResult send_wheel_result) {
            media_stream_video_source->SetSendWheelResult(send_wheel_result);
          },
          [&media_stream_video_source](
              GetZoomLevelResult get_zoom_level_result) {
            media_stream_video_source->SetGetZoomLevelResult(
                get_zoom_level_result);
          }},
      mock_source_result);

  // Set the reported SurfaceType.
  MediaStreamDevice device = media_stream_video_source->device();
  device.display_media_info = media::mojom::DisplayMediaInformation::New(
      display_surface,
      /*logical_surface=*/true, media::mojom::CursorCaptureType::NEVER,
      /*capture_handle=*/nullptr);
  media_stream_video_source->SetDevice(device);

  auto media_stream_video_track = std::make_unique<MediaStreamVideoTrack>(
      media_stream_video_source.get(),
      WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
      /*enabled=*/true);

  MediaStreamSource* const source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::StreamType::kTypeVideo, "name",
      /*remote=*/false, std::move(media_stream_video_source));

  MediaStreamComponent* const component =
      MakeGarbageCollected<MediaStreamComponentImpl>(
          "component_id", source, std::move(media_stream_video_track));

  switch (display_surface) {
    case SurfaceType::BROWSER:
      return MakeGarbageCollected<BrowserCaptureMediaStreamTrack>(
          v8_scope.GetExecutionContext(), component,
          /*callback=*/base::DoNothing());
    case SurfaceType::WINDOW:
    case SurfaceType::MONITOR:
      return MakeGarbageCollected<MediaStreamTrackImpl>(
          v8_scope.GetExecutionContext(), component,
          /*callback=*/base::DoNothing());
  }
  NOTREACHED_NORETURN();
}

CapturedWheelAction* MakeCapturedWheelAction(
    absl::optional<int> x = absl::nullopt,
    absl::optional<int> y = absl::nullopt,
    absl::optional<int> wheelDeltaX = absl::nullopt,
    absl::optional<int> wheelDeltaY = absl::nullopt) {
  CapturedWheelAction* const action = CapturedWheelAction::Create();
  if (x) {
    action->setX(*x);
  }
  if (y) {
    action->setY(*y);
  }
  if (wheelDeltaX) {
    action->setWheelDeltaX(*wheelDeltaX);
  }
  if (wheelDeltaY) {
    action->setWheelDeltaY(*wheelDeltaY);
  }
  return action;
}

}  // namespace

class CaptureControllerBaseTest : public testing::Test {
 public:
  ~CaptureControllerBaseTest() override = default;

 private:
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
};

// Test suite for CaptureController functionality from the
// Captured Surface Control spec, focusing on zoom-control.
class CaptureControllerZoomTest : public CaptureControllerBaseTest {
 public:
  ~CaptureControllerZoomTest() override = default;
};

TEST_F(CaptureControllerZoomTest, ReasonableMinimumAndMaximum) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeGarbageCollected<CaptureController>(v8_scope.GetExecutionContext());
  EXPECT_LT(controller->getMinZoomLevel(), controller->getMaxZoomLevel());
}

TEST_F(CaptureControllerZoomTest,
       GetZoomLevelFailsIfCaptureControllerNotBound) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeGarbageCollected<CaptureController>(v8_scope.GetExecutionContext());
  // Test avoids calling CaptureController::SetIsBound().

  const ScriptPromise promise =
      controller->getZoomLevel(v8_scope.GetScriptState());

  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "getDisplayMedia() not called yet.");
}

TEST_F(CaptureControllerZoomTest,
       GetZoomLevelFailsIfCaptureControllerBoundButNoVideoTrack) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeGarbageCollected<CaptureController>(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  // Test avoids calling CaptureController::SetVideoTrack().

  const ScriptPromise promise =
      controller->getZoomLevel(v8_scope.GetScriptState());

  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Capture-session not started.");
}

TEST_F(CaptureControllerZoomTest, GetZoomLevelFailsIfVideoTrackEnded) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeGarbageCollected<CaptureController>(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");
  track->stopTrack(v8_scope.GetExecutionContext());  // Ends the track.

  const ScriptPromise promise =
      controller->getZoomLevel(v8_scope.GetScriptState());

  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Video track ended.");
}

TEST_F(CaptureControllerZoomTest, GetZoomLevelSuccess) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeGarbageCollected<CaptureController>(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER,
                                      GetZoomLevelResult(/*zoom_level=*/90,
                                                         /*error=*/""));
  controller->SetVideoTrack(track, "descriptor");

  const ScriptPromise promise =
      controller->getZoomLevel(v8_scope.GetScriptState());

  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());
  ASSERT_TRUE(promise_tester.Value().V8Value()->IsNumber());
  EXPECT_EQ(90, promise_tester.Value().V8Value().As<v8::Number>()->Value());
}

// Note that the setup differs from that of GetZoomLevelSuccess only in the
// SurfaceType provided to MakeTrack().
TEST_F(CaptureControllerZoomTest, GetZoomLevelFailsIfCapturingWindow) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeGarbageCollected<CaptureController>(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::WINDOW,
                                      GetZoomLevelResult(/*zoom_level=*/90,
                                                         /*error=*/""));
  controller->SetVideoTrack(track, "descriptor");

  const ScriptPromise promise =
      controller->getZoomLevel(v8_scope.GetScriptState());

  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kNotSupportedError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Action only supported for tab-capture.");
}

// Note that the setup differs from that of GetZoomLevelSuccess only in the
// SurfaceType provided to MakeTrack().
TEST_F(CaptureControllerZoomTest, GetZoomLevelFailsIfCapturingMonitor) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeGarbageCollected<CaptureController>(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::MONITOR,
                                      GetZoomLevelResult(/*zoom_level=*/90,
                                                         /*error=*/""));
  controller->SetVideoTrack(track, "descriptor");

  const ScriptPromise promise =
      controller->getZoomLevel(v8_scope.GetScriptState());

  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kNotSupportedError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Action only supported for tab-capture.");
}

// Note that the setup differs from that of GetZoomLevelSuccess only in the
// simulated result from the browser process.
TEST_F(CaptureControllerZoomTest, SimulatedFailureFromDispatcherHost) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeGarbageCollected<CaptureController>(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  const String error = "Simulated error from dispatcher-host.";
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER,
                                      GetZoomLevelResult(absl::nullopt, error));
  controller->SetVideoTrack(track, "descriptor");

  const ScriptPromise promise =
      controller->getZoomLevel(v8_scope.GetScriptState());

  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kUnknownError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()), error);
}

// Test suite for CaptureController functionality from the
// Captured Surface Control spec, focusing on scroll-control.
class CaptureControllerScrollTest : public CaptureControllerBaseTest {
 public:
  ~CaptureControllerScrollTest() override = default;
};

TEST_F(CaptureControllerScrollTest, SendWheelFailsIfCaptureControllerNotBound) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeGarbageCollected<CaptureController>(v8_scope.GetExecutionContext());
  // Test avoids calling CaptureController::SetIsBound().

  const ScriptPromise promise = controller->sendWheel(
      v8_scope.GetScriptState(), MakeCapturedWheelAction());

  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "getDisplayMedia() not called yet.");
}

TEST_F(CaptureControllerScrollTest,
       SendWheelFailsIfCaptureControllerBoundButNoVideoTrack) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeGarbageCollected<CaptureController>(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  // Test avoids calling CaptureController::SetVideoTrack().

  const ScriptPromise promise = controller->sendWheel(
      v8_scope.GetScriptState(), MakeCapturedWheelAction());

  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Capture-session not started.");
}

TEST_F(CaptureControllerScrollTest, SendWheelFailsIfVideoTrackEnded) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeGarbageCollected<CaptureController>(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");
  track->stopTrack(v8_scope.GetExecutionContext());  // Ends the track.

  const ScriptPromise promise = controller->sendWheel(
      v8_scope.GetScriptState(), MakeCapturedWheelAction());

  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Video track ended.");
}

TEST_F(CaptureControllerScrollTest, SendWheelSuccess) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeGarbageCollected<CaptureController>(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER,
                                      SendWheelResult(/*success=*/true,
                                                      /*error=*/""));
  controller->SetVideoTrack(track, "descriptor");

  const ScriptPromise promise = controller->sendWheel(
      v8_scope.GetScriptState(), MakeCapturedWheelAction());

  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());
}

// Note that the setup differs from that of SendWheelSuccess only in the
// SurfaceType provided to MakeTrack().
TEST_F(CaptureControllerScrollTest, SendWheelFailsIfCapturingWindow) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeGarbageCollected<CaptureController>(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::WINDOW,
                                      SendWheelResult(/*success=*/true,
                                                      /*error=*/""));
  controller->SetVideoTrack(track, "descriptor");

  const ScriptPromise promise = controller->sendWheel(
      v8_scope.GetScriptState(), MakeCapturedWheelAction());

  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kNotSupportedError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Action only supported for tab-capture.");
}

// Note that the setup differs from that of SendWheelSuccess only in the
// SurfaceType provided to MakeTrack().
TEST_F(CaptureControllerScrollTest, SendWheelFailsIfCapturingMonitor) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeGarbageCollected<CaptureController>(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::MONITOR,
                                      SendWheelResult(/*success=*/true,
                                                      /*error=*/""));
  controller->SetVideoTrack(track, "descriptor");

  const ScriptPromise promise = controller->sendWheel(
      v8_scope.GetScriptState(), MakeCapturedWheelAction());

  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kNotSupportedError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Action only supported for tab-capture.");
}

// Note that the setup differs from that of SendWheelSuccess only in the
// action provided to sendWheel().
TEST_F(CaptureControllerScrollTest,
       SendWheelFailsIfInvalidCaputredWheelActionX) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeGarbageCollected<CaptureController>(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER,
                                      SendWheelResult(/*success=*/true,
                                                      /*error=*/""));
  controller->SetVideoTrack(track, "descriptor");

  const ScriptPromise promise = controller->sendWheel(
      v8_scope.GetScriptState(), MakeCapturedWheelAction(/*x=*/-1));

  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Invalid action.");
}

// Note that the setup differs from that of SendWheelSuccess only in the
// action provided to sendWheel().
TEST_F(CaptureControllerScrollTest,
       SendWheelFailsIfInvalidCaputredWheelActionY) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeGarbageCollected<CaptureController>(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER,
                                      SendWheelResult(/*success=*/true,
                                                      /*error=*/""));
  controller->SetVideoTrack(track, "descriptor");

  const ScriptPromise promise = controller->sendWheel(
      v8_scope.GetScriptState(),
      MakeCapturedWheelAction(/*x=*/absl::nullopt, /*y=*/-1));

  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Invalid action.");
}

// Note that the setup differs from that of SendWheelSuccess only in the
// simulated result from the browser process.
TEST_F(CaptureControllerScrollTest, SimulatedFailureFromDispatcherHost) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeGarbageCollected<CaptureController>(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  const String error = "Simulated error from dispatcher-host.";
  MediaStreamTrack* track =
      MakeTrack(v8_scope, SurfaceType::BROWSER,
                SendWheelResult(/*success=*/false, error));
  controller->SetVideoTrack(track, "descriptor");

  const ScriptPromise promise = controller->sendWheel(
      v8_scope.GetScriptState(), MakeCapturedWheelAction());

  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kUnknownError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()), error);
}

}  // namespace blink
