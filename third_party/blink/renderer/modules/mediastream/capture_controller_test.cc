// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/capture_controller.h"

#include <tuple>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
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
#include "third_party/blink/renderer/core/dom/document.h"
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

enum class TestedZoomControlAPI { kIncrease, kDecrease, kReset };

mojom::blink::ZoomLevelAction ToZoomLevelAction(TestedZoomControlAPI input) {
  switch (input) {
    case TestedZoomControlAPI::kIncrease:
      return mojom::blink::ZoomLevelAction::kIncrease;
    case TestedZoomControlAPI::kDecrease:
      return mojom::blink::ZoomLevelAction::kDecrease;
    case TestedZoomControlAPI::kReset:
      return mojom::blink::ZoomLevelAction::kReset;
  }
  NOTREACHED() << "Not a ZoomLevelAction.";
}

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
// to validate the tests themselves against false-positives through
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

MediaStreamTrack* MakeTrack(
    ExecutionContext* execution_context,
    SurfaceType display_surface,
    int initial_zoom_level =
        CaptureController::getSupportedZoomLevelsForTabs()[0],
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
    int initial_zoom_level =
        CaptureController::getSupportedZoomLevelsForTabs()[0],
    bool use_session_id = true) {
  return MakeTrack(testing_scope.GetExecutionContext(), display_surface,
                   initial_zoom_level, use_session_id);
}

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
       ExceptionRaisedIfControllerUnbound) {
  V8TestingScope v8_scope;
  DummyExceptionStateForTesting& exception_state = v8_scope.GetExceptionState();

  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  // Note that this test avoids calling CaptureController::SetIsBound().

  EXPECT_EQ(controller->getSupportedZoomLevels(exception_state), Vector<int>());
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST_F(CaptureControllerGetSupportedZoomLevelsTest,
       ExceptionRaisedIfNoVideoTrack) {
  V8TestingScope v8_scope;
  DummyExceptionStateForTesting& exception_state = v8_scope.GetExceptionState();

  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  // Note that this test avoids calling CaptureController::SetVideoTrack().

  EXPECT_EQ(controller->getSupportedZoomLevels(exception_state), Vector<int>());
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST_F(CaptureControllerGetSupportedZoomLevelsTest,
       CallSucceedsIfActivelyCapturingTabAndResultIsValid) {
  V8TestingScope v8_scope;
  DummyExceptionStateForTesting& exception_state = v8_scope.GetExceptionState();

  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER,
                                      /*initial_zoom_level=*/100);
  controller->SetVideoTrack(track, "descriptor");

  // Call to getSupportedZoomLevels() succeeds.
  const Vector<int> supported_levels =
      controller->getSupportedZoomLevels(exception_state);
  EXPECT_FALSE(exception_state.HadException());

  // The result is monotonically increasing.
  for (wtf_size_t i = 1; i < supported_levels.size(); ++i) {
    EXPECT_LT(supported_levels[i - 1], supported_levels[i]);
  }

  // The value `100` appears in the result. (It is the default zoom level.)
  EXPECT_THAT(controller->getSupportedZoomLevels(exception_state),
              testing::Contains(100));
}

TEST_F(CaptureControllerGetSupportedZoomLevelsTest,
       ExceptionRaisedIfCapturingWindow) {
  V8TestingScope v8_scope;
  DummyExceptionStateForTesting& exception_state = v8_scope.GetExceptionState();

  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::WINDOW,
                                      /*initial_zoom_level=*/100);
  controller->SetVideoTrack(track, "descriptor");

  EXPECT_EQ(controller->getSupportedZoomLevels(exception_state), Vector<int>());
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kNotSupportedError);
}

TEST_F(CaptureControllerGetSupportedZoomLevelsTest,
       ExceptionRaisedIfCapturingScreen) {
  V8TestingScope v8_scope;
  DummyExceptionStateForTesting& exception_state = v8_scope.GetExceptionState();

  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::MONITOR,
                                      /*initial_zoom_level=*/100);
  controller->SetVideoTrack(track, "descriptor");

  EXPECT_EQ(controller->getSupportedZoomLevels(exception_state), Vector<int>());
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kNotSupportedError);
}

TEST_F(CaptureControllerGetSupportedZoomLevelsTest,
       ExceptionRaisedIfTrackedEnded) {
  V8TestingScope v8_scope;
  DummyExceptionStateForTesting& exception_state = v8_scope.GetExceptionState();

  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER,
                                      /*initial_zoom_level=*/100);
  controller->SetVideoTrack(track, "descriptor");

  // Everything up to here still allowed for getSupportedZoomLevels()
  // to be called successfully.
  EXPECT_NE(controller->getSupportedZoomLevels(exception_state), Vector<int>());
  EXPECT_FALSE(exception_state.HadException());

  // Ending the track cuts off access to getSupportedZoomLevels().
  track->stopTrack(v8_scope.GetExecutionContext());  // Ends the track.

  EXPECT_EQ(controller->getSupportedZoomLevels(exception_state), Vector<int>());
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

// Test suite for CaptureController functionality from the
// Captured Surface Control spec, focusing on the zoomLevel attribute.
class CaptureControllerZoomLevelAttributeTest
    : public CaptureControllerBaseTest {
 public:
  ~CaptureControllerZoomLevelAttributeTest() override = default;
};

TEST_F(CaptureControllerZoomLevelAttributeTest,
       NoValueIfCaptureControllerNotBound) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  // Test avoids calling CaptureController::SetIsBound().

  EXPECT_EQ(controller->zoomLevel(), std::nullopt);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
}

TEST_F(CaptureControllerZoomLevelAttributeTest,
       NoValueIfCaptureControllerBoundButNoVideoTrack) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  // Test avoids calling CaptureController::SetVideoTrack().

  EXPECT_EQ(controller->zoomLevel(), std::nullopt);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
}

TEST_F(CaptureControllerZoomLevelAttributeTest,
       CorrectlyReportsInitialZoomLevel) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track =
      MakeTrack(v8_scope, SurfaceType::BROWSER,
                /*initial_zoom_level=*/
                CaptureController::getSupportedZoomLevelsForTabs()[1]);
  controller->SetVideoTrack(track, "descriptor");

  EXPECT_EQ(controller->zoomLevel(),
            CaptureController::getSupportedZoomLevelsForTabs()[1]);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
}

TEST_F(CaptureControllerZoomLevelAttributeTest, CorrectlyUpdatesValue) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track =
      MakeTrack(v8_scope, SurfaceType::BROWSER,
                /*initial_zoom_level=*/
                CaptureController::getSupportedZoomLevelsForTabs()[0]);
  controller->SetVideoTrack(track, "descriptor");
  ASSERT_EQ(controller->zoomLevel(),
            CaptureController::getSupportedZoomLevelsForTabs()[0]);

  track->Component()->Source()->OnZoomLevelChange(
      MediaStreamDevice(),
      CaptureController::getSupportedZoomLevelsForTabs()[1]);

  EXPECT_EQ(controller->zoomLevel(),
            CaptureController::getSupportedZoomLevelsForTabs()[1]);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
}

// Note that the setup differs from that of CorrectlyReportsInitialZoomLevel
// only in the SurfaceType provided to MakeTrack().
TEST_F(CaptureControllerZoomLevelAttributeTest, NoValueIfCapturingWindow) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track =
      MakeTrack(v8_scope, SurfaceType::WINDOW,
                /*initial_zoom_level=*/
                CaptureController::getSupportedZoomLevelsForTabs()[1]);
  controller->SetVideoTrack(track, "descriptor");

  EXPECT_EQ(controller->zoomLevel(), std::nullopt);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
}

// Note that the setup differs from that of CorrectlyReportsInitialZoomLevel
// only in the SurfaceType provided to MakeTrack().
TEST_F(CaptureControllerZoomLevelAttributeTest, NoValueIfCapturingMonitor) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track =
      MakeTrack(v8_scope, SurfaceType::MONITOR,
                /*initial_zoom_level=*/
                CaptureController::getSupportedZoomLevelsForTabs()[1]);
  controller->SetVideoTrack(track, "descriptor");

  EXPECT_EQ(controller->zoomLevel(), std::nullopt);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
}

TEST_F(CaptureControllerZoomLevelAttributeTest,
       ReportOldValueIfVideoTrackEndedWithoutUpdate) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  const int initial_zoom_level =
      CaptureController::getSupportedZoomLevelsForTabs()[1];
  MediaStreamTrack* track =
      MakeTrack(v8_scope, SurfaceType::BROWSER, initial_zoom_level);
  controller->SetVideoTrack(track, "descriptor");
  ASSERT_EQ(controller->zoomLevel(), initial_zoom_level);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  track->stopTrack(v8_scope.GetExecutionContext());  // Ends the track.

  EXPECT_EQ(controller->zoomLevel(), initial_zoom_level);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
}

TEST_F(CaptureControllerZoomLevelAttributeTest,
       ReportOldValueIfVideoTrackEndedWithUpdate) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  const int initial_zoom_level =
      CaptureController::getSupportedZoomLevelsForTabs()[1];
  MediaStreamTrack* track =
      MakeTrack(v8_scope, SurfaceType::BROWSER, initial_zoom_level);
  controller->SetVideoTrack(track, "descriptor");
  ASSERT_EQ(controller->zoomLevel(), initial_zoom_level);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  const int new_zoom_level =
      CaptureController::getSupportedZoomLevelsForTabs()[2];
  ASSERT_NE(new_zoom_level, initial_zoom_level);

  track->Component()->Source()->OnZoomLevelChange(MediaStreamDevice(),
                                                  new_zoom_level);
  ASSERT_EQ(controller->zoomLevel(), new_zoom_level);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  track->stopTrack(v8_scope.GetExecutionContext());  // Ends the track.

  EXPECT_EQ(controller->zoomLevel(), new_zoom_level);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
}

TEST_F(CaptureControllerZoomLevelAttributeTest,
       ZoomLevelUpdatesAfterTrackEndedGracefullyIgnored) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  const int original_zoom_level =
      CaptureController::getSupportedZoomLevelsForTabs()[1];
  MediaStreamTrack* track =
      MakeTrack(v8_scope, SurfaceType::BROWSER, original_zoom_level);
  controller->SetVideoTrack(track, "descriptor");
  ASSERT_EQ(controller->zoomLevel(), original_zoom_level);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  const int new_zoom_level =
      CaptureController::getSupportedZoomLevelsForTabs()[2];
  ASSERT_NE(new_zoom_level, original_zoom_level);

  track->stopTrack(v8_scope.GetExecutionContext());  // Ends the track.

  // Should not really happen; nullified.
  track->Component()->Source()->OnZoomLevelChange(MediaStreamDevice(),
                                                  new_zoom_level);
  ASSERT_EQ(controller->zoomLevel(), original_zoom_level);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
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
  controller->addEventListener(event_type_names::kZoomlevelchange,
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
  controller->addEventListener(event_type_names::kZoomlevelchange,
                               event_listener);
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  EXPECT_CALL(*event_listener, Invoke(_, _)).Times(1);
  track->Component()->Source()->OnZoomLevelChange(
      MediaStreamDevice(),
      CaptureController::getSupportedZoomLevelsForTabs()[1]);
}

TEST_F(CaptureControllerOnCapturedZoomLevelChangeTest,
       NoEventWhenSameAsInitValue) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  StrictMock<MockEventListener>* event_listener =
      MakeGarbageCollected<StrictMock<MockEventListener>>();
  controller->addEventListener(event_type_names::kZoomlevelchange,
                               event_listener);
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  EXPECT_CALL(*event_listener, Invoke(_, _)).Times(0);
  track->Component()->Source()->OnZoomLevelChange(
      MediaStreamDevice(),
      CaptureController::getSupportedZoomLevelsForTabs()[0]);
}

TEST_F(CaptureControllerOnCapturedZoomLevelChangeTest,
       EventWhenDifferentFromPreviousUpdate) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  StrictMock<MockEventListener>* event_listener =
      MakeGarbageCollected<StrictMock<MockEventListener>>();
  controller->addEventListener(event_type_names::kZoomlevelchange,
                               event_listener);
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  EXPECT_CALL(*event_listener, Invoke(_, _)).Times(1);
  track->Component()->Source()->OnZoomLevelChange(
      MediaStreamDevice(),
      CaptureController::getSupportedZoomLevelsForTabs()[1]);
  Mock::VerifyAndClearExpectations(event_listener);
  EXPECT_CALL(*event_listener, Invoke(_, _)).Times(1);
  track->Component()->Source()->OnZoomLevelChange(
      MediaStreamDevice(),
      CaptureController::getSupportedZoomLevelsForTabs()[0]);
}

TEST_F(CaptureControllerOnCapturedZoomLevelChangeTest,
       EventWhenSameAsPreviousUpdate) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  StrictMock<MockEventListener>* event_listener =
      MakeGarbageCollected<StrictMock<MockEventListener>>();
  controller->addEventListener(event_type_names::kZoomlevelchange,
                               event_listener);
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  EXPECT_CALL(*event_listener, Invoke(_, _)).Times(1);
  track->Component()->Source()->OnZoomLevelChange(
      MediaStreamDevice(),
      CaptureController::getSupportedZoomLevelsForTabs()[1]);
  Mock::VerifyAndClearExpectations(event_listener);
  EXPECT_CALL(*event_listener, Invoke(_, _)).Times(0);
  track->Component()->Source()->OnZoomLevelChange(
      MediaStreamDevice(),
      CaptureController::getSupportedZoomLevelsForTabs()[1]);
}

class CaptureControllerUpdateZoomLevelTest
    : public CaptureControllerBaseTest,
      public WithParamInterface<TestedZoomControlAPI> {
 public:
  CaptureControllerUpdateZoomLevelTest() : tested_action_(GetParam()) {}
  ~CaptureControllerUpdateZoomLevelTest() override = default;

  ScriptPromise<IDLUndefined> CallTestedAPI(ScriptState* script_state,
                                            CaptureController* controller) {
    switch (tested_action_) {
      case TestedZoomControlAPI::kIncrease:
        return controller->increaseZoomLevel(script_state);
      case TestedZoomControlAPI::kDecrease:
        return controller->decreaseZoomLevel(script_state);
      case TestedZoomControlAPI::kReset:
        return controller->resetZoomLevel(script_state);
    }
    NOTREACHED();
  }

 protected:
  const TestedZoomControlAPI tested_action_;
};

INSTANTIATE_TEST_SUITE_P(,
                         CaptureControllerUpdateZoomLevelTest,
                         Values(TestedZoomControlAPI::kIncrease,
                                TestedZoomControlAPI::kDecrease,
                                TestedZoomControlAPI::kReset));

TEST_P(CaptureControllerUpdateZoomLevelTest,
       UpdateZoomLevelFailsIfCaptureControllerNotBound) {
  V8TestingScope v8_scope;
  ScriptState* const script_state = v8_scope.GetScriptState();

  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  // Test avoids calling CaptureController::SetIsBound().

  const ScriptPromise<IDLUndefined> promise =
      CallTestedAPI(script_state, controller);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "getDisplayMedia() not called yet.");
}

TEST_P(CaptureControllerUpdateZoomLevelTest,
       UpdateZoomLevelFailsIfCaptureControllerBoundButNoVideoTrack) {
  V8TestingScope v8_scope;
  ScriptState* const script_state = v8_scope.GetScriptState();

  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  // Test avoids calling CaptureController::SetVideoTrack().

  const ScriptPromise<IDLUndefined> promise =
      CallTestedAPI(script_state, controller);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Capture-session not started.");
}

TEST_P(CaptureControllerUpdateZoomLevelTest,
       UpdateZoomLevelFailsIfVideoTrackEnded) {
  V8TestingScope v8_scope;
  ScriptState* const script_state = v8_scope.GetScriptState();

  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");
  track->stopTrack(v8_scope.GetExecutionContext());  // Ends the track.

  const ScriptPromise<IDLUndefined> promise =
      CallTestedAPI(script_state, controller);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Video track ended.");
}

TEST_P(CaptureControllerUpdateZoomLevelTest, UpdateZoomLevelSuccess) {
  V8TestingScope v8_scope;
  ScriptState* const script_state = v8_scope.GetScriptState();

  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  EXPECT_CALL(DispatcherHost(),
              UpdateZoomLevel(_, ToZoomLevelAction(tested_action_), _))
      .WillOnce(RunOnceCallback<2>(CscResult::kSuccess));
  const ScriptPromise<IDLUndefined> promise =
      CallTestedAPI(script_state, controller);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());
}

// Note that the setup differs from that of UpdateZoomLevelSuccess only in the
// SurfaceType provided to MakeTrack().
TEST_P(CaptureControllerUpdateZoomLevelTest,
       UpdateZoomLevelFailsIfCapturingWindow) {
  V8TestingScope v8_scope;
  ScriptState* const script_state = v8_scope.GetScriptState();

  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::WINDOW);
  controller->SetVideoTrack(track, "descriptor");

  const ScriptPromise<IDLUndefined> promise =
      CallTestedAPI(script_state, controller);
  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kNotSupportedError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Action only supported for tab-capture.");
}

// Note that the setup differs from that of UpdateZoomLevelSuccess only in the
// SurfaceType provided to MakeTrack().
TEST_P(CaptureControllerUpdateZoomLevelTest,
       UpdateZoomLevelFailsIfCapturingMonitor) {
  V8TestingScope v8_scope;
  ScriptState* const script_state = v8_scope.GetScriptState();

  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::MONITOR);
  controller->SetVideoTrack(track, "descriptor");

  const ScriptPromise<IDLUndefined> promise =
      CallTestedAPI(script_state, controller);
  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kNotSupportedError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Action only supported for tab-capture.");
}

// Note that the setup differs from that of UpdateZoomLevelSuccess only in the
// simulated result from the browser process.
TEST_P(CaptureControllerUpdateZoomLevelTest,
       SimulatedFailureFromDispatcherHost) {
  V8TestingScope v8_scope;
  ScriptState* const script_state = v8_scope.GetScriptState();

  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track = MakeTrack(v8_scope, SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  EXPECT_CALL(DispatcherHost(), UpdateZoomLevel(_, _, _))
      .WillOnce(RunOnceCallback<2>(CscResult::kUnknownError));
  const ScriptPromise<IDLUndefined> promise =
      CallTestedAPI(script_state, controller);
  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kUnknownError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Unknown error.");
}

TEST_P(CaptureControllerUpdateZoomLevelTest,
       UpdateZoomLevelFailsWithoutSessionId) {
  V8TestingScope v8_scope;
  ScriptState* const script_state = v8_scope.GetScriptState();

  CaptureController* controller =
      MakeController(v8_scope.GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track =
      MakeTrack(v8_scope, SurfaceType::BROWSER,
                CaptureController::getSupportedZoomLevelsForTabs()[0],
                /*use_session_id=*/false);
  controller->SetVideoTrack(track, "descriptor");

  const ScriptPromise<IDLUndefined> promise =
      CallTestedAPI(script_state, controller);
  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(v8_scope, promise_tester.Value(),
                             DOMExceptionCode::kUnknownError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(v8_scope, promise_tester.Value()),
            "Invalid capture");
}

class CaptureControllerForwardWheelTest : public PageTestBase,
                                          public CaptureControllerTestSupport {
 public:
  ~CaptureControllerForwardWheelTest() override = default;
};

TEST_F(CaptureControllerForwardWheelTest, Success) {
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
  auto promise = controller->forwardWheel(script_state, element);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());

  base::RunLoop run_loop;
  EXPECT_CALL(DispatcherHost(), SendWheel(_, _))
      .WillOnce(Invoke(&run_loop, &base::RunLoop::Quit));
  element->DispatchEvent(
      *WheelEvent::Create(event_type_names::kWheel, WheelEventInit::Create()));
  run_loop.Run();

  promise = controller->forwardWheel(script_state, nullptr);
  ScriptPromiseTester promise_tester2(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());

  auto* mock_listener = MakeGarbageCollected<MockEventListener>();
  element->addEventListener(event_type_names::kWheel, mock_listener);
  base::RunLoop run_loop2;
  EXPECT_CALL(DispatcherHost(), SendWheel(_, _)).Times(0);
  EXPECT_CALL(*mock_listener, Invoke)
      .WillOnce(Invoke(&run_loop2, &base::RunLoop::Quit));
  element->DispatchEvent(
      *WheelEvent::Create(event_type_names::kWheel, WheelEventInit::Create()));
  run_loop2.Run();
}

TEST_F(CaptureControllerForwardWheelTest, DropUntrustedEvent) {
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
                      controller->forwardWheel(script_state, element))
      .WaitUntilSettled();

  EXPECT_CALL(DispatcherHost(), SendWheel(_, _)).Times(0);
  DummyExceptionStateForTesting exception_state;
  // Events dispatched with dispatchEventForBindings are always untrusted.
  element->dispatchEventForBindings(
      WheelEvent::Create(event_type_names::kWheel, WheelEventInit::Create()),
      exception_state);

  task_environment().RunUntilIdle();
}

TEST_F(CaptureControllerForwardWheelTest, SuccessWithNoElement) {
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
  auto promise = controller->forwardWheel(script_state, nullptr);
  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());
}

TEST_F(CaptureControllerForwardWheelTest, NoSessionId) {
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
  auto promise = controller->forwardWheel(script_state, element);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(script_state, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));
  EXPECT_EQ(GetDOMExceptionMessage(script_state, promise_tester.Value()),
            "Invalid capture.");
}

TEST_F(CaptureControllerForwardWheelTest, NoTrack) {
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
  auto promise = controller->forwardWheel(script_state, element);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(script_state, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));
  EXPECT_EQ(GetDOMExceptionMessage(script_state, promise_tester.Value()),
            "Invalid capture.");
}

TEST_F(CaptureControllerForwardWheelTest, StoppedTrack) {
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
  auto promise = controller->forwardWheel(script_state, element);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(script_state, promise_tester.Value(),
                             DOMExceptionCode::kInvalidStateError));
  EXPECT_EQ(GetDOMExceptionMessage(script_state, promise_tester.Value()),
            "Invalid capture.");
}

class CaptureControllerForwardWheelBackendErrorTest
    : public CaptureControllerForwardWheelTest,
      public WithParamInterface<CscResult> {
 public:
  CaptureControllerForwardWheelBackendErrorTest() : error_(GetParam()) {}
  ~CaptureControllerForwardWheelBackendErrorTest() override = default;

 protected:
  const CscResult error_;
};

INSTANTIATE_TEST_SUITE_P(,
                         CaptureControllerForwardWheelBackendErrorTest,
                         Values(CscResult::kUnknownError,
                                CscResult::kNoPermissionError,
                                CscResult::kCapturerNotFoundError,
                                CscResult::kCapturedSurfaceNotFoundError,
                                CscResult::kDisallowedForSelfCaptureError,
                                CscResult::kCapturerNotFocusedError));

TEST_P(CaptureControllerForwardWheelBackendErrorTest, Test) {
  ExecutionContext* execution_context = GetDocument().GetExecutionContext();
  CaptureController* controller = MakeController(execution_context);
  controller->SetIsBound(true);
  MediaStreamTrack* track =
      MakeTrack(GetDocument().GetExecutionContext(), SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  HTMLDivElement* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());

  EXPECT_CALL(DispatcherHost(), SendWheel(_, _)).Times(0);

  ScriptState::Scope scope(script_state);
  EXPECT_CALL(DispatcherHost(), RequestCapturedSurfaceControlPermission(_, _))
      .WillOnce(RunOnceCallback<1>(error_));
  const auto promise = controller->forwardWheel(script_state, element);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());

  auto* mock_listener = MakeGarbageCollected<MockEventListener>();
  element->addEventListener(event_type_names::kWheel, mock_listener);
  base::RunLoop run_loop;

  EXPECT_CALL(*mock_listener, Invoke)
      .WillRepeatedly(Invoke(&run_loop, &base::RunLoop::Quit));
  element->DispatchEvent(
      *WheelEvent::Create(event_type_names::kWheel, WheelEventInit::Create()));
  run_loop.Run();
}

class CaptureControllerForwardWheelUnsupportedSurfacesTest
    : public CaptureControllerForwardWheelTest,
      public WithParamInterface<SurfaceType> {
 public:
  CaptureControllerForwardWheelUnsupportedSurfacesTest()
      : surface_type_(GetParam()) {}
  ~CaptureControllerForwardWheelUnsupportedSurfacesTest() override = default;

 protected:
  const SurfaceType surface_type_;
};

INSTANTIATE_TEST_SUITE_P(,
                         CaptureControllerForwardWheelUnsupportedSurfacesTest,
                         Values(SurfaceType::WINDOW, SurfaceType::MONITOR));

TEST_P(CaptureControllerForwardWheelUnsupportedSurfacesTest, Fails) {
  CaptureController* controller =
      MakeController(GetDocument().GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track =
      MakeTrack(GetDocument().GetExecutionContext(), surface_type_);
  controller->SetVideoTrack(track, "descriptor");

  HTMLDivElement* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());

  ScriptState::Scope scope(script_state);
  ON_CALL(DispatcherHost(), RequestCapturedSurfaceControlPermission(_, _))
      .WillByDefault(RunOnceCallback<1>(CscResult::kSuccess));
  auto promise = controller->forwardWheel(script_state, element);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_TRUE(IsDOMException(script_state, promise_tester.Value(),
                             DOMExceptionCode::kNotSupportedError));

  // Avoid false-positives through different error paths terminating in
  // exception with the same code.
  EXPECT_EQ(GetDOMExceptionMessage(script_state, promise_tester.Value()),
            "Action only supported for tab-capture.");
}

// Test the validation of forwarded wheel parameters.
using ScrollTestParams =
    std::tuple<gfx::Point, gfx::Point, ScrollDirection, ScrollDirection>;
class CaptureControllerScrollParametersValidationTest
    : public PageTestBase,
      public CaptureControllerTestSupport,
      public WithParamInterface<ScrollTestParams> {
 public:
  static constexpr int kDivWidth = 100;
  static constexpr int kDivHeight = 200;

  static constexpr gfx::Point kDivAtOrigin = gfx::Point(0, 0);
  static constexpr gfx::Point kDivAtOffset = gfx::Point(40, 80);

  static constexpr gfx::Point kTopLeft = gfx::Point(0, 0);
  static constexpr gfx::Point kTopRight = gfx::Point(kDivWidth - 1, 0);
  static constexpr gfx::Point kBottomLeft = gfx::Point(0, kDivHeight - 1);
  static constexpr gfx::Point kBottomRight =
      gfx::Point(kDivWidth - 1, kDivHeight - 1);
  static constexpr gfx::Point kCenter =
      gfx::Point((kDivWidth - 1) / 2, (kDivHeight - 1) / 2);

  CaptureControllerScrollParametersValidationTest()
      : div_origin_(std::get<0>(GetParam())),
        gesture_coordinates_(std::get<1>(GetParam())),
        horizontal_scroll_direction_(std::get<2>(GetParam())),
        vertical_scroll_direction_(std::get<3>(GetParam())) {}
  ~CaptureControllerScrollParametersValidationTest() override = default;

  static std::string DescribeParams(
      const testing::TestParamInfo<ScrollTestParams>& info) {
    std::string result;

    // div_origin
    for (const auto& named_param :
         {std::make_pair(kDivAtOrigin, "DivAtOrigin"),
          std::make_pair(kDivAtOffset, "DivAtOffset")}) {
      if (std::get<0>(info.param) == named_param.first) {
        result += named_param.second;
        break;
      }
    }

    // gesture_coordinates
    for (const auto& named_param : {std::make_pair(kTopLeft, "TopLeft"),
                                    std::make_pair(kTopRight, "TopRight"),
                                    std::make_pair(kBottomLeft, "BottomLeft"),
                                    std::make_pair(kBottomRight, "BottomRight"),
                                    std::make_pair(kCenter, "Center")}) {
      if (std::get<1>(info.param) == named_param.first) {
        result += std::string("GestureAt") + named_param.second;
        break;
      }
    }

    // horizontal_scroll_direction
    for (const auto& named_param :
         {std::make_pair(ScrollDirection::kNone, "None"),
          std::make_pair(ScrollDirection::kForwards, "Forwards"),
          std::make_pair(ScrollDirection::kBackwards, "Backwards")}) {
      if (std::get<2>(info.param) == named_param.first) {
        result += std::string("HorizontalScroll") + named_param.second;
        break;
      }
    }

    // vertical_scroll_direction
    for (const auto& named_param :
         {std::make_pair(ScrollDirection::kNone, "None"),
          std::make_pair(ScrollDirection::kForwards, "Forwards"),
          std::make_pair(ScrollDirection::kBackwards, "Backwards")}) {
      if (std::get<3>(info.param) == named_param.first) {
        result += std::string("VerticalScroll") + named_param.second;
        break;
      }
    }

    return result;
  }

 protected:
  gfx::Point div_origin() const { return div_origin_; }

  gfx::Point gesture_coordinates() const { return gesture_coordinates_; }

  int wheel_deltax_x() const {
    return GetScrollValue(horizontal_scroll_direction_);
  }

  int wheel_deltax_y() const {
    return GetScrollValue(vertical_scroll_direction_);
  }

 private:
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

  const gfx::Point div_origin_;
  const gfx::Point gesture_coordinates_;
  const ScrollDirection horizontal_scroll_direction_;
  const ScrollDirection vertical_scroll_direction_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    CaptureControllerScrollParametersValidationTest,
    Combine(
        // div_origin_
        Values(CaptureControllerScrollParametersValidationTest::kDivAtOrigin,
               CaptureControllerScrollParametersValidationTest::kDivAtOffset),
        // gesture_coordinates_
        Values(CaptureControllerScrollParametersValidationTest::kTopLeft,
               CaptureControllerScrollParametersValidationTest::kTopRight,
               CaptureControllerScrollParametersValidationTest::kBottomLeft,
               CaptureControllerScrollParametersValidationTest::kBottomRight,
               CaptureControllerScrollParametersValidationTest::kCenter),
        // horizontal_scroll_direction_
        Values(ScrollDirection::kNone,
               ScrollDirection::kForwards,
               ScrollDirection::kBackwards),
        // vertical_scroll_direction_
        Values(ScrollDirection::kNone,
               ScrollDirection::kForwards,
               ScrollDirection::kBackwards)),
    CaptureControllerScrollParametersValidationTest::DescribeParams);

TEST_P(CaptureControllerScrollParametersValidationTest, ValidateCoordinates) {
  SetHtmlInnerHTML(base::StringPrintf(
      R"HTML(
        <body style="margin: 0;">
          <div id="div"
               style="position: absolute; left: %d; top: %d;
               width: %dpx; height: %dpx;" />
        </body>
      )HTML",
      div_origin().x(), div_origin().x(), kDivWidth, kDivHeight));

  CaptureController* controller =
      MakeController(GetDocument().GetExecutionContext());
  controller->SetIsBound(true);
  MediaStreamTrack* track =
      MakeTrack(GetDocument().GetExecutionContext(), SurfaceType::BROWSER);
  controller->SetVideoTrack(track, "descriptor");

  HTMLElement* const element = reinterpret_cast<HTMLElement*>(
      GetDocument().getElementById(AtomicString("div")));
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());

  ScriptState::Scope scope(script_state);
  EXPECT_CALL(DispatcherHost(), RequestCapturedSurfaceControlPermission(_, _))
      .WillOnce(RunOnceCallback<1>(CscResult::kSuccess));
  auto promise = controller->forwardWheel(script_state, element);

  ScriptPromiseTester promise_tester(script_state, promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());

  mojom::blink::CapturedWheelAction dispatcher_action;
  base::RunLoop run_loop;
  EXPECT_CALL(DispatcherHost(), SendWheel(_, _))
      .WillOnce(DoAll(SaveArgPointee<1>(&dispatcher_action),
                      Invoke(&run_loop, &base::RunLoop::Quit)));

  // Deliver the wheel event.
  WheelEventInit* const init = WheelEventInit::Create();
  init->setClientX(gesture_coordinates().x());
  init->setClientY(gesture_coordinates().y());
  init->setDeltaX(wheel_deltax_x());
  init->setDeltaY(wheel_deltax_y());
  WheelEvent* const wheel_event =
      WheelEvent::Create(event_type_names::kWheel, init);

  element->DispatchEvent(*wheel_event);
  run_loop.Run();

  EXPECT_EQ(dispatcher_action.relative_x,
            1.0 * gesture_coordinates().x() / kDivWidth);
  EXPECT_EQ(dispatcher_action.relative_y,
            1.0 * gesture_coordinates().y() / kDivHeight);
  EXPECT_EQ(dispatcher_action.wheel_delta_x, -wheel_deltax_x());
  EXPECT_EQ(dispatcher_action.wheel_delta_y, -wheel_deltax_y());
}

}  // namespace
}  // namespace blink
