// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/capture_controller.h"

#include <tuple>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_wheel_event_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_captured_wheel_action.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/wheel_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/mediastream/browser_capture_media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/mock_mojo_media_stream_dispatcher_host.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

using SurfaceType = ::media::mojom::DisplayCaptureSurfaceType;

using ::base::test::RunOnceCallback;
using ::base::test::RunOnceCallbackRepeatedly;
using ::testing::_;
using ::testing::Combine;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::SaveArgPointee;
using ::testing::StrictMock;
using ::testing::Values;
using ::testing::WithParamInterface;
using CscResult = ::blink::mojom::blink::CapturedSurfaceControlResult;

enum class ScrollDirection {
  kNone,
  kForwards,
  kBackwards,
};

class MockEventListener : public NativeEventListener {
 public:
  MOCK_METHOD(void, Invoke, (ExecutionContext*, Event*));
};

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
String GetDOMExceptionMessage(ScriptState* script_state,
                              const ScriptValue& value) {
  DOMException* const dom_exception =
      V8DOMException::ToWrappable(script_state->GetIsolate(), value.V8Value());
  CHECK(dom_exception) << "Malformed test.";
  return dom_exception->message();
}
String GetDOMExceptionMessage(V8TestingScope& v8_scope,
                              const ScriptValue& value) {
  return GetDOMExceptionMessage(v8_scope.GetScriptState(), value);
}

// Extract the MediaStreamVideoTrack which the test has previously injected
// into the track. CHECKs and casts used here are valid because this is a
// controlled test environment.
MediaStreamVideoTrack* GetMediaStreamVideoTrack(MediaStreamTrack* track) {
  MediaStreamComponent* const component = track->Component();
  CHECK(component);
  return MediaStreamVideoTrack::From(component);
}

MediaStreamTrack* MakeTrack(
    ExecutionContext* execution_context,
    SurfaceType display_surface,
    int initial_zoom_level = CaptureController::getSupportedZoomLevels()[0],
    bool use_session_id = true) {
  std::unique_ptr<MockMediaStreamVideoSource> media_stream_video_source =
      base::WrapUnique(new ::testing::NiceMock<MockMediaStreamVideoSource>(
          media::VideoCaptureFormat(gfx::Size(640, 480), 30.0,
                                    media::PIXEL_FORMAT_I420),
          true));

  // Set the reported SurfaceType.
  MediaStreamDevice device = media_stream_video_source->device();
  device.display_media_info = media::mojom::DisplayMediaInformation::New(
      display_surface,
      /*logical_surface=*/true, media::mojom::CursorCaptureType::NEVER,
      /*capture_handle=*/nullptr, initial_zoom_level);
  if (use_session_id) {
    device.set_session_id(base::UnguessableToken::Create());
  }
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
          execution_context, component,
          /*callback=*/base::DoNothing());
    case SurfaceType::WINDOW:
    case SurfaceType::MONITOR:
      return MakeGarbageCollected<MediaStreamTrackImpl>(
          execution_context, component,
          /*callback=*/base::DoNothing());
  }
  NOTREACHED();
}

MediaStreamTrack* MakeTrack(
    V8TestingScope& testing_scope,
    SurfaceType display_surface,
    int initial_zoom_level = CaptureController::getSupportedZoomLevels()[0],
    bool use_session_id = true) {
  return MakeTrack(testing_scope.GetExecutionContext(), display_surface,
                   initial_zoom_level, use_session_id);
}

void SimulateFrameArrival(MediaStreamTrack* track,
                          gfx::Size frame_size = gfx::Size(1000, 1000)) {
  GetMediaStreamVideoTrack(track)->SetTargetSize(frame_size.width(),
                                                 frame_size.height());
}

}  // namespace

class CaptureControllerTestSupport {
 protected:
  virtual ~CaptureControllerTestSupport() = default;

  MockMojoMediaStreamDispatcherHost& DispatcherHost() {
    return mock_dispatcher_host_;
  }

  CaptureController* MakeController(ExecutionContext* execution_context) {
    auto* controller =
        MakeGarbageCollected<CaptureController>(execution_context);
    controller->SetMediaStreamDispatcherHostForTesting(
        mock_dispatcher_host_.CreatePendingRemoteAndBind());
    return controller;
  }

 private:
  MockMojoMediaStreamDispatcherHost mock_dispatcher_host_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
};

class CaptureControllerBaseTest : public testing::Test,
                                  public CaptureControllerTestSupport {
 public:
  ~CaptureControllerBaseTest() override = default;

 private:
  test::TaskEnvironment task_environment_;
};

// Test suite for CaptureController functionality from the Captured Surface
// Control spec, focusing on reading the supported zoom levels.
class CaptureControllerGetSupportedZoomLevelsTest
    : public CaptureControllerBaseTest {
 public:
  ~CaptureControllerGetSupportedZoomLevelsTest() override = default;
};

TEST_F(CaptureControllerGetSupportedZoomLevelsTest,
       ReturnsMonotonicallyIncreasingSequence) {
  V8TestingScope v8_scope;
  const Vector<int> supported_levels =
      CaptureController::getSupportedZoomLevels();
  ASSERT_GE(supported_levels.size(), 2u);  // Test holds vacuously otherwise.
  for (wtf_size_t i = 1; i < supported_levels.size(); ++i) {
    EXPECT_LT(supported_levels[i - 1], supported_levels[i]);
  }
}

// Test suite for CaptureController functionality from the
// Captured Surface Control spec, focusing on GetZoomLevel.
class CaptureControllerGetZoomLevelTest : public CaptureControllerBaseTest {
 public:
  ~CaptureControllerGetZoomLevelTest() override = default;
};

TEST_F(CaptureControllerGetZoomLevelTest,
       GetZoomLevelFailsIfCaptureControllerNotBound) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  // Test avoids calling CaptureController::SetIsBound().

  controller->getZoomLevel(v8_scope.GetExceptionState());
  ASSERT_TRUE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(v8_scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(v8_scope.GetExceptionState().Message(),
            "getDisplayMedia() not called yet.");
}

TEST_F(CaptureControllerGetZoomLevelTest,
       GetZoomLevelFailsIfCaptureControllerBoundButNoVideoTrack) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  // Test avoids calling CaptureController::SetVideoTrack().

  controller->getZoomLevel(v8_scope.GetExceptionState());
  ASSERT_TRUE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(v8_scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(v8_scope.GetExceptionState().Message(),
            "Capture-session not started.");
}

TEST_F(CaptureControllerGetZoomLevelTest, GetZoomLevelFailsIfVideoTrackEnded) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");
  track->stopTrack(v8_scope.GetExecutionContext());  // Ends the track.

  controller->getZoomLevel(v8_scope.GetExceptionState());
  ASSERT_TRUE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(v8_scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(v8_scope.GetExceptionState().Message(), "Video track ended.");
}

TEST_F(CaptureControllerGetZoomLevelTest, GetZoomLevelSuccessInitialZoomLevel) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(
      v8_scope, SurfaceType::BROWSER,
      /*initial_zoom_level=*/CaptureController::getSupportedZoomLevels()[1]);
  controller->SetVideoTrack(track, "descriptor");

  int zoom_level = controller->getZoomLevel(v8_scope.GetExceptionState());
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(zoom_level, CaptureController::getSupportedZoomLevels()[1]);
}

TEST_F(CaptureControllerGetZoomLevelTest, GetZoomLevelSuccessZoomLevelUpdate) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(
      v8_scope, SurfaceType::BROWSER,
      /*initial_zoom_level=*/CaptureController::getSupportedZoomLevels()[0]);
  controller->SetVideoTrack(track, "descriptor");

  track->Component()->Source()->OnZoomLevelChange(
      MediaStreamDevice(), CaptureController::getSupportedZoomLevels()[1]);

  int zoom_level = controller->getZoomLevel(v8_scope.GetExceptionState());
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(zoom_level, CaptureController::getSupportedZoomLevels()[1]);
}

// Note that the setup differs from that of GetZoomLevelSuccessInitialZoomLevel
// only in the SurfaceType provided to MakeTrack().
TEST_F(CaptureControllerGetZoomLevelTest, GetZoomLevelFailsIfCapturingWindow) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(
      v8_scope, SurfaceType::WINDOW,
      /*initial_zoom_level=*/CaptureController::getSupportedZoomLevels()[1]);
  controller->SetVideoTrack(track, "descriptor");

  controller->getZoomLevel(v8_scope.GetExceptionState());
  ASSERT_TRUE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(v8_scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kNotSupportedError);

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(v8_scope.GetExceptionState().Message(),
            "Action only supported for tab-capture.");
}

// Note that the setup differs from that of GetZoomLevelSuccessInitialZoomLevel
// only in the SurfaceType provided to MakeTrack().
TEST_F(CaptureControllerGetZoomLevelTest, GetZoomLevelFailsIfCapturingMonitor) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(
      v8_scope, SurfaceType::MONITOR,
      /*initial_zoom_level=*/CaptureController::getSupportedZoomLevels()[1]);
  controller->SetVideoTrack(track, "descriptor");

  controller->getZoomLevel(v8_scope.GetExceptionState());
  ASSERT_TRUE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(v8_scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kNotSupportedError);

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(v8_scope.GetExceptionState().Message(),
            "Action only supported for tab-capture.");
}

// Test suite for CaptureController functionality from the Captured Surface
// Control spec, focusing on OnCapturedZoomLevelChange events.
class CaptureControllerOnCapturedZoomLevelChangeTest
    : public CaptureControllerBaseTest {
 public:
  ~CaptureControllerOnCapturedZoomLevelChangeTest() override = default;
};

TEST_F(CaptureControllerOnCapturedZoomLevelChangeTest, NoEventOnInit) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());

  StrictMock<MockEventListener>* event_listener =
      MakeGarbageCollected<StrictMock<MockEventListener>>();
  EXPECT_CALL(*event_listener, Invoke(_, _)).Times(0);
  controller->addEventListener(event_type_names::kCapturedzoomlevelchange,
                               event_listener);

  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");
}

TEST_F(CaptureControllerOnCapturedZoomLevelChangeTest,
       EventWhenDifferentFromInitValue) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  StrictMock<MockEventListener>* event_listener =
      MakeGarbageCollected<StrictMock<MockEventListener>>();
  controller->addEventListener(event_type_names::kCapturedzoomlevelchange,
                               event_listener);
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  EXPECT_CALL(*event_listener, Invoke(_, _)).Times(1);
  track->Component()->Source()->OnZoomLevelChange(
      MediaStreamDevice(), CaptureController::getSupportedZoomLevels()[1]);
}

TEST_F(CaptureControllerOnCapturedZoomLevelChangeTest,
       NoEventWhenSameAsInitValue) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  StrictMock<MockEventListener>* event_listener =
      MakeGarbageCollected<StrictMock<MockEventListener>>();
  controller->addEventListener(event_type_names::kCapturedzoomlevelchange,
                               event_listener);
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  EXPECT_CALL(*event_listener, Invoke(_, _)).Times(0);
  track->Component()->Source()->OnZoomLevelChange(
      MediaStreamDevice(), CaptureController::getSupportedZoomLevels()[0]);
}

TEST_F(CaptureControllerOnCapturedZoomLevelChangeTest,
       EventWhenDifferentFromPreviousUpdate) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  StrictMock<MockEventListener>* event_listener =
      MakeGarbageCollected<StrictMock<MockEventListener>>();
  controller->addEventListener(event_type_names::kCapturedzoomlevelchange,
                               event_listener);
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  EXPECT_CALL(*event_listener, Invoke(_, _)).Times(1);
  track->Component()->Source()->OnZoomLevelChange(
      MediaStreamDevice(), CaptureController::getSupportedZoomLevels()[1]);
  Mock::VerifyAndClearExpectations(event_listener);
  EXPECT_CALL(*event_listener, Invoke(_, _)).Times(1);
  track->Component()->Source()->OnZoomLevelChange(
      MediaStreamDevice(), CaptureController::getSupportedZoomLevels()[0]);
}

TEST_F(CaptureControllerOnCapturedZoomLevelChangeTest,
       EventWhenSameAsPreviousUpdate) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  StrictMock<MockEventListener>* event_listener =
      MakeGarbageCollected<StrictMock<MockEventListener>>();
  controller->addEventListener(event_type_names::kCapturedzoomlevelchange,
                               event_listener);
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  EXPECT_CALL(*event_listener, Invoke(_, _)).Times(1);
  track->Component()->Source()->OnZoomLevelChange(
      MediaStreamDevice(), CaptureController::getSupportedZoomLevels()[1]);
  Mock::VerifyAndClearExpectations(event_listener);
  EXPECT_CALL(*event_listener, Invoke(_, _)).Times(0);
  track->Component()->Source()->OnZoomLevelChange(
      MediaStreamDevice(), CaptureController::getSupportedZoomLevels()[1]);
}

// Test suite for CaptureController functionality from the
// Captured Surface Control spec, focusing on SetZoomLevel.
class CaptureControllerSetZoomLevelTest : public CaptureControllerBaseTest {
 public:
  ~CaptureControllerSetZoomLevelTest() override = default;
};

TEST_F(CaptureControllerSetZoomLevelTest,
       SetZoomLevelFailsIfCaptureControllerNotBound) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  // Test avoids calling CaptureController::SetIsBound().

  const ScriptPromiseUntyped promise =
      controller->setZoomLevel(v8_scope.GetScriptState(), 125);

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

TEST_F(CaptureControllerSetZoomLevelTest,
       SetZoomLevelFailsIfCaptureControllerBoundButNoVideoTrack) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  // Test avoids calling CaptureController::SetVideoTrack().

  const ScriptPromiseUntyped promise =
      controller->setZoomLevel(v8_scope.GetScriptState(), 125);

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

TEST_F(CaptureControllerSetZoomLevelTest, SetZoomLevelFailsIfVideoTrackEnded) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");
  track->stopTrack(v8_scope.GetExecutionContext());  // Ends the track.

  const ScriptPromiseUntyped promise =
      controller->setZoomLevel(v8_scope.GetScriptState(), 125);

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

TEST_F(CaptureControllerSetZoomLevelTest, SetZoomLevelSuccessIfSupportedValue) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  const Vector<int> supported_levels =
      CaptureController::getSupportedZoomLevels();
  for (int zoom_level : supported_levels) {
    EXPECT_CALL(DispatcherHost(), SetZoomLevel(_, zoom_level, _))
        .WillOnce(RunOnceCallback<2>(CscResult::kSuccess));
    const ScriptPromiseUntyped promise =
        controller->setZoomLevel(v8_scope.GetScriptState(), zoom_level);

    ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
    promise_tester.WaitUntilSettled();
    EXPECT_TRUE(promise_tester.IsFulfilled());
  }
}

TEST_F(CaptureControllerSetZoomLevelTest, SetZoomLevelFailsIfLevelTooLow) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  const ScriptPromiseUntyped promise = controller->setZoomLevel(
      v8_scope.GetScriptState(),
      controller->getSupportedZoomLevels().front() - 1);
  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Only values returned by getSupportedZoomLevels() are valid.");
}

TEST_F(CaptureControllerSetZoomLevelTest, SetZoomLevelFailsIfLevelTooHigh) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  const ScriptPromiseUntyped promise =
      controller->setZoomLevel(v8_scope.GetScriptState(),
                               controller->getSupportedZoomLevels().back() + 1);
  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Only values returned by getSupportedZoomLevels() are valid.");
}

// This test is distinct from SetZoomLevelFailsIfLevelTooLow and
// SetZoomLevelFailsIfLevelTooHigh in that it uses a value that's within the
// permitted range, thereby ensuring that the validation does not just check
// the range, but rather actually uses the supported value as an allowlist.
TEST_F(CaptureControllerSetZoomLevelTest, SetZoomLevelFailsIfUnsupportedValue) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  // Find an unsupported value.
  const Vector<int> supported_levels = controller->getSupportedZoomLevels();
  ASSERT_GE(supported_levels.size(), 2u);
  const int unsupported_level = (supported_levels[0] + supported_levels[1]) / 2;
  ASSERT_FALSE(supported_levels.Contains(unsupported_level));

  const ScriptPromiseUntyped promise =
      controller->setZoomLevel(v8_scope.GetScriptState(), unsupported_level);

  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Only values returned by getSupportedZoomLevels() are valid.");
}

// Note that the setup differs from that of SetZoomLevelSuccess only in the
// SurfaceType provided to MakeTrack().
TEST_F(CaptureControllerSetZoomLevelTest, SetZoomLevelFailsIfCapturingWindow) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::WINDOW);
  controller->SetVideoTrack(track, "descriptor");

  const ScriptPromiseUntyped promise =
      controller->setZoomLevel(v8_scope.GetScriptState(), 125);
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

// Note that the setup differs from that of SetZoomLevelSuccess only in the
// SurfaceType provided to MakeTrack().
TEST_F(CaptureControllerSetZoomLevelTest, SetZoomLevelFailsIfCapturingMonitor) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::MONITOR);
  controller->SetVideoTrack(track, "descriptor");

  const ScriptPromiseUntyped promise =
      controller->setZoomLevel(v8_scope.GetScriptState(), 125);
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

// Note that the setup differs from that of SetZoomLevelSuccess only in the
// simulated result from the browser process.
TEST_F(CaptureControllerSetZoomLevelTest, SimulatedFailureFromDispatcherHost) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  EXPECT_CALL(DispatcherHost(), SetZoomLevel(_, _, _))
      .WillOnce(RunOnceCallback<2>(CscResult::kUnknownError));
  const ScriptPromiseUntyped promise =
      controller->setZoomLevel(v8_scope.GetScriptState(), 125);
  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kUnknownError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Unknown error.");
}

TEST_F(CaptureControllerSetZoomLevelTest, SetZoomLevelFailsWithoutSessionId) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(
      v8_scope, SurfaceType::BROWSER,
      CaptureController::getSupportedZoomLevels()[0], /*use_session_id=*/false);
  controller->SetVideoTrack(track, "descriptor");

  const ScriptPromiseUntyped promise =
      controller->setZoomLevel(v8_scope.GetScriptState(), 100);
  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kUnknownError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Invalid capture");
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
      MakeController(v8_scope.GetExecutionContext());
  // Test avoids calling CaptureController::SetIsBound().

  const ScriptPromiseUntyped promise = controller->sendWheel(
      v8_scope.GetScriptState(), CapturedWheelAction::Create());

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
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  // Test avoids calling CaptureController::SetVideoTrack().

  const ScriptPromiseUntyped promise = controller->sendWheel(
      v8_scope.GetScriptState(), CapturedWheelAction::Create());

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
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");
  track->stopTrack(v8_scope.GetExecutionContext());  // Ends the track.

  const ScriptPromiseUntyped promise = controller->sendWheel(
      v8_scope.GetScriptState(), CapturedWheelAction::Create());

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
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");
  SimulateFrameArrival(track);

  EXPECT_CALL(DispatcherHost(), SendWheel(_, _, _))
      .WillOnce(RunOnceCallback<2>(CscResult::kSuccess));
  const ScriptPromiseUntyped promise = controller->sendWheel(
      v8_scope.GetScriptState(), CapturedWheelAction::Create());

  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());
}

// Note that the setup differs from that of SendWheelSuccess only in the
// SurfaceType provided to MakeTrack().
TEST_F(CaptureControllerScrollTest, SendWheelFailsIfCapturingWindow) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::WINDOW);
  controller->SetVideoTrack(track, "descriptor");

  const ScriptPromiseUntyped promise = controller->sendWheel(
      v8_scope.GetScriptState(), CapturedWheelAction::Create());
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
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::MONITOR);
  controller->SetVideoTrack(track, "descriptor");

  const ScriptPromiseUntyped promise = controller->sendWheel(
      v8_scope.GetScriptState(), CapturedWheelAction::Create());
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
// simulated result from the browser process.
TEST_F(CaptureControllerScrollTest, SimulatedFailureFromDispatcherHost) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");
  SimulateFrameArrival(track);

  EXPECT_CALL(DispatcherHost(), SendWheel(_, _, _))
      .WillOnce(RunOnceCallback<2>(CscResult::kUnknownError));
  const ScriptPromiseUntyped promise = controller->sendWheel(
      v8_scope.GetScriptState(), CapturedWheelAction::Create());

  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kUnknownError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Unknown error.");
}

// Note that the setup differs from that of SendWheelSuccess only in the
// absence of a call to SimulateFrameArrival().
TEST_F(CaptureControllerScrollTest, SendWheelFailsBeforeReceivingFrames) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");
  // Intentionally avoid calling SimulateFrameArrival().

  const ScriptPromiseUntyped promise = controller->sendWheel(
      v8_scope.GetScriptState(), CapturedWheelAction::Create());
  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "No frames observed yet.");
}

// This test:
// * Simulates the arrival of a frame of a given size.
// * Simulates a call to sendWheel() at a specific point.
// * Expects scaling.
TEST_F(CaptureControllerScrollTest, SendWheelScalesCorrectly) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");
  SimulateFrameArrival(track, gfx::Size(200, 4000));

  CapturedWheelAction* const action = CapturedWheelAction::Create();
  action->setX(100);
  action->setY(250);
  action->setWheelDeltaX(111);
  action->setWheelDeltaY(222);

  mojom::blink::CapturedWheelAction dispatcher_action;
  EXPECT_CALL(DispatcherHost(), SendWheel(_, _, _))
      .WillOnce(DoAll(SaveArgPointee<1>(&dispatcher_action),
                      RunOnceCallbackRepeatedly<2>(CscResult::kSuccess)));
  const ScriptPromiseUntyped promise =
      controller->sendWheel(v8_scope.GetScriptState(), action);
  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());
  EXPECT_EQ(dispatcher_action.relative_x, 100.0 / 200.0);
  EXPECT_EQ(dispatcher_action.relative_y, 250.0 / 4000.0);
  EXPECT_EQ(dispatcher_action.wheel_delta_x, 111);
  EXPECT_EQ(dispatcher_action.wheel_delta_y, 222);
}

TEST_F(CaptureControllerScrollTest, SendWheelFailsWithoutSessionId) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(
      v8_scope, SurfaceType::BROWSER,
      CaptureController::getSupportedZoomLevels()[0], /*use_session_id=*/false);
  controller->SetVideoTrack(track, "descriptor");
  SimulateFrameArrival(track);

  const ScriptPromiseUntyped promise = controller->sendWheel(
      v8_scope.GetScriptState(), CapturedWheelAction::Create());
  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kUnknownError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Invalid capture");
}

class CaptureConstrollerCaptureWheelTest : public PageTestBase,
                                           public CaptureControllerTestSupport {
};

TEST_F(CaptureConstrollerCaptureWheelTest, Success) {
  CaptureController* controller =
      MakeController(GetDocument().GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track =
      MakeTrack(GetDocument().GetExecutionContext(), SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  HTMLDivElement* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());

  ScriptState::Scope scope(script_state);
  EXPECT_CALL(DispatcherHost(), RequestCapturedSurfaceControlPermission(_, _))
      .WillOnce(RunOnceCallback<1>(CscResult::kSuccess));
  ScriptPromiseUntyped promise =
      controller->captureWheel(script_state, element);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());

  base::RunLoop run_loop;
  EXPECT_CALL(DispatcherHost(), SendWheel(_, _, _))
      .WillOnce(DoAll(Invoke(&run_loop, &base::RunLoop::Quit),
                      RunOnceCallback<2>(CscResult::kSuccess)));
  element->DispatchEvent(
      *WheelEvent::Create(event_type_names::kWheel, WheelEventInit::Create()));
  run_loop.Run();

  promise = controller->captureWheel(script_state, nullptr);
  ScriptPromiseTester promise_tester2(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());

  auto* mock_listener = MakeGarbageCollected<MockEventListener>();
  element->addEventListener(event_type_names::kWheel, mock_listener);
  base::RunLoop run_loop2;
  EXPECT_CALL(DispatcherHost(), SendWheel(_, _, _)).Times(0);
  EXPECT_CALL(*mock_listener, Invoke)
      .WillOnce(Invoke(&run_loop2, &base::RunLoop::Quit));
  element->DispatchEvent(
      *WheelEvent::Create(event_type_names::kWheel, WheelEventInit::Create()));
  run_loop2.Run();
}

TEST_F(CaptureConstrollerCaptureWheelTest, DropUntrustedEvent) {
  CaptureController* controller =
      MakeController(GetDocument().GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track =
      MakeTrack(GetDocument().GetExecutionContext(), SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  HTMLDivElement* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());

  ScriptState::Scope scope(script_state);
  EXPECT_CALL(DispatcherHost(), RequestCapturedSurfaceControlPermission(_, _))
      .WillOnce(RunOnceCallback<1>(CscResult::kSuccess));
  ScriptPromiseTester(script_state,
                      controller->captureWheel(script_state, element))
      .WaitUntilSettled();

  EXPECT_CALL(DispatcherHost(), SendWheel(_, _, _)).Times(0);
  DummyExceptionStateForTesting exception_state;
  // Events dispatched with dispatchEventForBindings are always untrusted.
  element->dispatchEventForBindings(
      WheelEvent::Create(event_type_names::kWheel, WheelEventInit::Create()),
      exception_state);

  task_environment().RunUntilIdle();
}

TEST_F(CaptureConstrollerCaptureWheelTest, SuccessWithNoElement) {
  CaptureController* controller =
      MakeController(GetDocument().GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track =
      MakeTrack(GetDocument().GetExecutionContext(), SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
  ScriptState::Scope scope(script_state);

  EXPECT_CALL(DispatcherHost(), RequestCapturedSurfaceControlPermission(_, _))
      .Times(0);
  ScriptPromiseUntyped promise =
      controller->captureWheel(script_state, nullptr);
  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());
}

TEST_F(CaptureConstrollerCaptureWheelTest, BackendError) {
  ExecutionContext* execution_context = GetDocument().GetExecutionContext();
  CaptureController* controller = MakeController(execution_context);
  controller->SetIsBound(true);
  MediaStreamTrack* track =
      MakeTrack(GetDocument().GetExecutionContext(), SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  HTMLDivElement* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());

  EXPECT_CALL(DispatcherHost(), SendWheel(_, _, _)).Times(0);

  ScriptState::Scope scope(script_state);
  for (CscResult csc_error_result :
       {CscResult::kUnknownError, CscResult::kNoPermissionError,
        CscResult::kCapturerNotFoundError,
        CscResult::kCapturedSurfaceNotFoundError,
        CscResult::kDisallowedForSelfCaptureError,
        CscResult::kCapturerNotFocusedError}) {
    EXPECT_CALL(DispatcherHost(), RequestCapturedSurfaceControlPermission(_, _))
        .WillOnce(RunOnceCallback<1>(csc_error_result));
    const ScriptPromiseUntyped promise =
        controller->captureWheel(script_state, element);

    ScriptPromiseTester promise_tester(script_state, promise);
    promise_tester.WaitUntilSettled();
    EXPECT_TRUE(promise_tester.IsRejected());

    auto* mock_listener = MakeGarbageCollected<MockEventListener>();
    element->addEventListener(event_type_names::kWheel, mock_listener);
    base::RunLoop run_loop;

    EXPECT_CALL(*mock_listener, Invoke)
        .WillRepeatedly(Invoke(&run_loop, &base::RunLoop::Quit));
    element->DispatchEvent(*WheelEvent::Create(event_type_names::kWheel,
                                               WheelEventInit::Create()));
    run_loop.Run();
  }
}

TEST_F(CaptureConstrollerCaptureWheelTest, NoSessionId) {
  CaptureController* controller =
      MakeController(GetDocument().GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track =
      MakeTrack(GetDocument().GetExecutionContext(), SurfaceType::BROWSER,
                /*initial_zoom_level=*/100, /*use_session_id=*/false);
  controller->SetVideoTrack(track, "descriptor");

  HTMLDivElement* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());

  ScriptState::Scope scope(script_state);
  ScriptPromiseUntyped promise =
      controller->captureWheel(script_state, element);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(script_state, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));
  EXPECT_EQ(GetDOMExceptionMessage(script_state, promise_tester.Value()),
            "Invalid capture.");
}

TEST_F(CaptureConstrollerCaptureWheelTest, NoTrack) {
  CaptureController* controller =
      MakeController(GetDocument().GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track =
      MakeTrack(GetDocument().GetExecutionContext(), SurfaceType::BROWSER,
                /*initial_zoom_level=*/100, /*use_session_id=*/false);
  controller->SetVideoTrack(track, "descriptor");

  HTMLDivElement* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());

  ScriptState::Scope scope(script_state);
  ScriptPromiseUntyped promise =
      controller->captureWheel(script_state, element);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(script_state, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));
  EXPECT_EQ(GetDOMExceptionMessage(script_state, promise_tester.Value()),
            "Invalid capture.");
}

TEST_F(CaptureConstrollerCaptureWheelTest, StoppedTrack) {
  CaptureController* controller =
      MakeController(GetDocument().GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track =
      MakeTrack(GetDocument().GetExecutionContext(), SurfaceType::BROWSER,
                /*initial_zoom_level=*/100, /*use_session_id=*/false);
  controller->SetVideoTrack(track, "descriptor");

  HTMLDivElement* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());

  ScriptState::Scope scope(script_state);
  track->stopTrack(GetDocument().GetExecutionContext());
  ScriptPromiseUntyped promise =
      controller->captureWheel(script_state, element);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(script_state, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));
  EXPECT_EQ(GetDOMExceptionMessage(script_state, promise_tester.Value()),
            "Invalid capture.");
}

// Test the validation of sendWheel() parameters.
class CaptureControllerScrollParametersValidationTest
    : public CaptureControllerScrollTest,
      public WithParamInterface<
          std::tuple<std::tuple<ScrollDirection, ScrollDirection>,
                     std::tuple<gfx::Point, bool>>> {
 public:
  static constexpr int kWidth = 1000;
  static constexpr int kHeight = 2000;

  CaptureControllerScrollParametersValidationTest()
      : vertical_scroll_direction_(std::get<0>(std::get<0>(GetParam()))),
        horizontal_scroll_direction_(std::get<1>(std::get<0>(GetParam()))),
        scroll_coordinates_(std::get<0>(std::get<1>(GetParam()))),
        expect_success_(std::get<1>(std::get<1>(GetParam()))) {}
  ~CaptureControllerScrollParametersValidationTest() override = default;

  static int GetScrollValue(ScrollDirection direction) {
    switch (direction) {
      case ScrollDirection::kNone:
        return 0;
      case ScrollDirection::kForwards:
        return 10;
      case ScrollDirection::kBackwards:
        return -10;
    }
  }

  int wheel_deltax_x() const {
    return GetScrollValue(horizontal_scroll_direction_);
  }

  int wheel_deltax_y() const {
    return GetScrollValue(vertical_scroll_direction_);
  }

 protected:
  const ScrollDirection vertical_scroll_direction_;
  const ScrollDirection horizontal_scroll_direction_;
  const gfx::Point scroll_coordinates_;
  const bool expect_success_;
};

namespace {
constexpr int kLeftmost = 0;
constexpr int kRightmost =
    CaptureControllerScrollParametersValidationTest::kWidth - 1;
constexpr int kTop = 0;
constexpr int kBottom =
    CaptureControllerScrollParametersValidationTest::kHeight - 1;

INSTANTIATE_TEST_SUITE_P(
    ,
    CaptureControllerScrollParametersValidationTest,
    Combine(
        // Scroll direction.
        Combine(
            // Vertical scroll.
            Values(ScrollDirection::kNone,
                   ScrollDirection::kForwards,
                   ScrollDirection::kBackwards),
            // Horizontal scroll.
            Values(ScrollDirection::kNone,
                   ScrollDirection::kForwards,
                   ScrollDirection::kBackwards)),
        // Scroll coordinates and expectation.
        Values(
            // Corners
            std::make_tuple(gfx::Point(kLeftmost, kTop), true),
            std::make_tuple(gfx::Point(kLeftmost, kBottom), true),
            std::make_tuple(gfx::Point(kRightmost, kTop), true),
            std::make_tuple(gfx::Point(kRightmost, kBottom), true),
            // Just beyond top-left
            std::make_tuple(gfx::Point(kLeftmost - 1, kTop), false),
            std::make_tuple(gfx::Point(kLeftmost, kTop - 1), false),
            // Just beyond bottom-left
            std::make_tuple(gfx::Point(kLeftmost - 1, kBottom), false),
            std::make_tuple(gfx::Point(kLeftmost, kBottom + 1), false),
            // Just beyond top-right
            std::make_tuple(gfx::Point(kRightmost + 1, kTop), false),
            std::make_tuple(gfx::Point(kRightmost, kTop - 1), false),
            // Just beyond bottom-right
            std::make_tuple(gfx::Point(kRightmost + 1, kBottom), false),
            std::make_tuple(gfx::Point(kRightmost, kBottom + 1), false))));
}  // namespace

TEST_P(CaptureControllerScrollParametersValidationTest, ValidateCoordinates) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");
  SimulateFrameArrival(track, gfx::Size(kWidth, kHeight));

  if (expect_success_) {
    EXPECT_CALL(DispatcherHost(), SendWheel(_, _, _))
        .WillOnce(RunOnceCallback<2>(CscResult::kSuccess));
  }
  CapturedWheelAction* const action = CapturedWheelAction::Create();
  action->setX(scroll_coordinates_.x());
  action->setY(scroll_coordinates_.y());
  action->setWheelDeltaX(wheel_deltax_x());
  action->setWheelDeltaY(wheel_deltax_y());
  const ScriptPromiseUntyped promise =
      controller->sendWheel(v8_scope.GetScriptState(), action);

  ScriptPromiseTester promise_tester(v8_scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(expect_success_ ? promise_tester.IsFulfilled()
                              : promise_tester.IsRejected());

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  if (!expect_success_) {
    EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
              "Coordinates out of bounds.");
  }
}

}  // namespace blink
