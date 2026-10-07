// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/user_media_request_provider_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_html_media_stream_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraint_set.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_domexception_overconstrainederror.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_camera_element.h"
#include "third_party/blink/renderer/core/html/html_capability_element_metrics_util.h"
#include "third_party/blink/renderer/core/html/html_microphone_element.h"
#include "third_party/blink/renderer/core/html/html_permission_element_test_helper.h"
#include "third_party/blink/renderer/core/html/html_user_media_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/mediastream/html_media_track_element_media_track.h"
#include "third_party/blink/renderer/modules/mediastream/html_user_media_element_media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_capture_element_constraints.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_track.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class TestEventListener : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event* event) override { fired_ = true; }
  bool fired() const { return fired_; }

 private:
  bool fired_ = false;
};

class UserMediaRequestProviderImplTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    UserMediaRequestProviderImpl::ProvideTo(*GetDocument().domWindow());
  }
};

// Verifies that the provider doesn't crash when attempting to process a request
// without a valid UserMediaClient.
TEST_F(UserMediaRequestProviderImplTest, StartRequestEarlyExitNoClient) {
  V8TestingScope scope;
  auto* provider = UserMediaRequestProvider::From(*GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());

  HTMLMediaStreamConstraints* constraints = HTMLMediaStreamConstraints::Create();
  constraints->setVideo(MediaTrackConstraintSet::Create());
  MediaCaptureElementConstraints::setConstraints(*element, constraints);

  provider->StartRequest(element, element->GetPermissionDescriptors());
  // Test passes if it doesn't crash.
}

// Verifies that StartRequest gracefully exits and makes no changes when an
// active stream is already present.
TEST_F(UserMediaRequestProviderImplTest, StartRequestActiveStreamExists) {
  V8TestingScope scope;
  auto* provider = UserMediaRequestProvider::From(*GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());

  HTMLMediaStreamConstraints* constraints = HTMLMediaStreamConstraints::Create();
  constraints->setVideo(MediaTrackConstraintSet::Create());
  MediaCaptureElementConstraints::setConstraints(*element, constraints);

  auto* stream = MediaStream::Create(GetDocument().GetExecutionContext());
  stream->Descriptor()->SetActive(true);
  HTMLUserMediaElementMediaStream::From(*element).SetMediaStream(stream);

  provider->StartRequest(element, element->GetPermissionDescriptors());
  // Test passes if it doesn't crash and does not change the stream.
  EXPECT_EQ(HTMLUserMediaElementMediaStream::stream(*element), stream);
}

// Verifies that StartRequest gracefully exits and makes no changes when an
// active track is already present on a single-track element.
TEST_F(UserMediaRequestProviderImplTest, StartRequestActiveTrackExists) {
  V8TestingScope scope;
  ScopedCameraAndMicrophoneElementsForTest scoped_feature(true);
  auto* provider = UserMediaRequestProvider::From(*GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLCameraElement>(GetDocument());
  auto* mock_track = MakeGarbageCollected<MockMediaStreamTrack>();
  mock_track->SetEnded(false);
  HTMLMediaTrackElementMediaTrack::From(*element).SetMediaTrack(mock_track);

  provider->StartRequest(element, element->GetPermissionDescriptors());
  EXPECT_EQ(HTMLMediaTrackElementMediaTrack::track(*element), mock_track);
}

// Confirms that providing a valid stream sets the generated stream onto the
// HTMLUserMediaElementMediaStream successfully.
TEST_F(UserMediaRequestProviderImplTest, CallbacksOnSuccessWithStream) {
  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  auto* callbacks =
      MakeGarbageCollected<UserMediaRequestProviderCallbacks>(element);

  // Set up event listeners
  auto* stream_listener = MakeGarbageCollected<TestEventListener>();
  auto* error_listener = MakeGarbageCollected<TestEventListener>();
  element->addEventListener(event_type_names::kStream, stream_listener);
  element->addEventListener(event_type_names::kError, error_listener);

  EXPECT_EQ(HTMLUserMediaElementMediaStream::stream(*element), nullptr);

  auto* stream = MediaStream::Create(GetDocument().GetExecutionContext());
  MediaStreamVector streams = {stream};

  callbacks->OnSuccess(streams, /*capture_controller=*/nullptr);

  // The stream should have been set on the element.
  EXPECT_EQ(HTMLUserMediaElementMediaStream::stream(*element), stream);

  test::RunPendingTasks();

  // Verify events
  EXPECT_TRUE(stream_listener->fired());
  EXPECT_FALSE(error_listener->fired());
}


TEST_F(UserMediaRequestProviderImplTest, CallbacksOnError) {
  V8TestingScope scope;
  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  auto* callbacks =
      MakeGarbageCollected<UserMediaRequestProviderCallbacks>(element);

  // Set up event listeners
  auto* error_listener = MakeGarbageCollected<TestEventListener>();
  auto* stream_listener = MakeGarbageCollected<TestEventListener>();
  element->addEventListener(event_type_names::kError, error_listener);
  element->addEventListener(event_type_names::kStream, stream_listener);

  EXPECT_FALSE(element->error());

  DOMException* dom_exception =
      DOMException::Create("Some error message", "NotFoundError");
  V8MediaStreamError* error =
      MakeGarbageCollected<V8UnionDOMExceptionOrOverconstrainedError>(
          dom_exception);
  callbacks->OnError(nullptr, error, nullptr,
                     UserMediaRequestResult::kNotFoundError);

  test::RunPendingTasks();

  // Check that the error event was fired and the stream event was not
  EXPECT_TRUE(error_listener->fired());
  EXPECT_FALSE(stream_listener->fired());

  DOMException* stored_error = element->error();
  ASSERT_TRUE(stored_error);
  EXPECT_EQ(stored_error->name(), "NotFoundError");
  EXPECT_EQ(stored_error->message(), "Some error message");
}

TEST_F(UserMediaRequestProviderImplTest, CallbacksOnCancel) {
  V8TestingScope scope;
  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  auto* callbacks =
      MakeGarbageCollected<UserMediaRequestProviderCallbacks>(element);

  // Set up event listeners
  auto* cancel_listener = MakeGarbageCollected<TestEventListener>();
  auto* error_listener = MakeGarbageCollected<TestEventListener>();
  auto* stream_listener = MakeGarbageCollected<TestEventListener>();
  element->addEventListener(event_type_names::kCancel, cancel_listener);
  element->addEventListener(event_type_names::kError, error_listener);
  element->addEventListener(event_type_names::kStream, stream_listener);

  EXPECT_FALSE(element->error());

  DOMException* dom_exception =
      DOMException::Create("User denied", "NotAllowedError");
  V8MediaStreamError* error =
      MakeGarbageCollected<V8UnionDOMExceptionOrOverconstrainedError>(
          dom_exception);
  callbacks->OnError(nullptr, error, nullptr,
                     UserMediaRequestResult::kNotAllowedByUserError);

  test::RunPendingTasks();

  // Check that the cancel event was fired and others were not
  EXPECT_TRUE(cancel_listener->fired());
  EXPECT_FALSE(error_listener->fired());
  EXPECT_FALSE(stream_listener->fired());

  // Cancel event should set the error attribute on the element
  DOMException* stored_error = element->error();
  ASSERT_TRUE(stored_error);
  EXPECT_EQ(stored_error->name(), "NotAllowedError");
  EXPECT_EQ(stored_error->message(), "User denied");
}

TEST_F(UserMediaRequestProviderImplTest, CallbacksOnSuccessWithNullTrack) {
  ScopedCameraAndMicrophoneElementsForTest scoped_feature(true);
  auto* element = MakeGarbageCollected<HTMLCameraElement>(GetDocument());
  auto* callbacks =
      MakeGarbageCollected<UserMediaRequestProviderCallbacks>(element);

  auto* track_listener = MakeGarbageCollected<TestEventListener>();
  auto* error_listener = MakeGarbageCollected<TestEventListener>();
  element->addEventListener(event_type_names::kTrack, track_listener);
  element->addEventListener(event_type_names::kError, error_listener);

  // Empty stream with no video tracks
  auto* stream = MediaStream::Create(GetDocument().GetExecutionContext());
  MediaStreamVector streams = {stream};

  callbacks->OnSuccess(streams, /*capture_controller=*/nullptr);
  test::RunPendingTasks();

  // Track event should NOT fire, error event SHOULD fire
  EXPECT_FALSE(track_listener->fired());
  EXPECT_TRUE(error_listener->fired());
  EXPECT_EQ(HTMLMediaTrackElementMediaTrack::track(*element), nullptr);

  DOMException* stored_error = element->error();
  ASSERT_TRUE(stored_error);
  EXPECT_EQ(stored_error->name(), "NotFoundError");
}

TEST_F(UserMediaRequestProviderImplTest,
       MetricsUserMediaConstraintsValidation) {
  V8TestingScope scope;
  auto* provider = UserMediaRequestProvider::From(*GetDocument().domWindow());

  // 1. Audio and Video: valid
  {
    base::HistogramTester histogram_tester;
    auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
    HTMLMediaStreamConstraints* constraints =
        HTMLMediaStreamConstraints::Create();
    constraints->setVideo(MediaTrackConstraintSet::Create());
    constraints->setAudio(MediaTrackConstraintSet::Create());
    MediaCaptureElementConstraints::setConstraints(*element, constraints);

    provider->StartRequest(element, element->GetPermissionDescriptors());

    histogram_tester.ExpectUniqueSample(
        "Blink.CapabilityElement.UserMedia.Constraints.ValidationError",
        CapabilityElementMediaConstraintsValidationError::kNoError, 1);
  }

  // 2. Audio only: video missing (valid for usermedia)
  {
    base::HistogramTester histogram_tester;
    auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
    auto* missing_video = HTMLMediaStreamConstraints::Create();
    missing_video->setAudio(MediaTrackConstraints::Create());
    MediaCaptureElementConstraints::From(*element).SetConstraints(
        missing_video);

    provider->StartRequest(element, element->GetPermissionDescriptors());

    histogram_tester.ExpectUniqueSample(
        "Blink.CapabilityElement.UserMedia.Constraints.ValidationError",
        CapabilityElementMediaConstraintsValidationError::kNoError, 1);
  }

  // 3. Video only: audio missing (valid for usermedia)
  {
    base::HistogramTester histogram_tester;
    auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
    auto* missing_audio = HTMLMediaStreamConstraints::Create();
    missing_audio->setVideo(MediaTrackConstraints::Create());
    MediaCaptureElementConstraints::From(*element).SetConstraints(
        missing_audio);

    provider->StartRequest(element, element->GetPermissionDescriptors());

    histogram_tester.ExpectUniqueSample(
        "Blink.CapabilityElement.UserMedia.Constraints.ValidationError",
        CapabilityElementMediaConstraintsValidationError::kNoError, 1);
  }

  // 4. Neither: both missing
  {
    base::HistogramTester histogram_tester;
    auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
    auto* missing_both = HTMLMediaStreamConstraints::Create();
    MediaCaptureElementConstraints::From(*element).SetConstraints(missing_both);

    provider->StartRequest(element, element->GetPermissionDescriptors());

    histogram_tester.ExpectUniqueSample(
        "Blink.CapabilityElement.UserMedia.Constraints.ValidationError",
        CapabilityElementMediaConstraintsValidationError::kBothMissing, 1);
    histogram_tester.ExpectUniqueSample(
        "Blink.CapabilityElement.UserMedia.RequestOutcome",
        CapabilityElementMediaRequestOutcome::kConstraintsValidationError, 1);
  }
}

TEST_F(UserMediaRequestProviderImplTest,
       MetricsCameraAndMicrophoneConstraintsValidation) {
  V8TestingScope scope;
  ScopedCameraAndMicrophoneElementsForTest scoped_feature(true);
  auto* provider = UserMediaRequestProvider::From(*GetDocument().domWindow());

  // 1. Camera: valid (video only)
  {
    base::HistogramTester histogram_tester;
    auto* element = MakeGarbageCollected<HTMLCameraElement>(GetDocument());
    auto* constraints = MediaTrackConstraintSet::Create();
    MediaCaptureElementConstraints::setConstraints(*element, constraints);

    provider->StartRequest(element, element->GetPermissionDescriptors());

    histogram_tester.ExpectUniqueSample(
        "Blink.CapabilityElement.Camera.Constraints.ValidationError",
        CapabilityElementMediaConstraintsValidationError::kNoError, 1);
  }

  // 2. Camera: missing video
  {
    base::HistogramTester histogram_tester;
    auto* element = MakeGarbageCollected<HTMLCameraElement>(GetDocument());
    auto* constraints = HTMLMediaStreamConstraints::Create();
    MediaCaptureElementConstraints::From(*element).SetConstraints(constraints);

    provider->StartRequest(element, element->GetPermissionDescriptors());

    histogram_tester.ExpectUniqueSample(
        "Blink.CapabilityElement.Camera.Constraints.ValidationError",
        CapabilityElementMediaConstraintsValidationError::kVideoMissing, 1);
    histogram_tester.ExpectUniqueSample(
        "Blink.CapabilityElement.Camera.RequestOutcome",
        CapabilityElementMediaRequestOutcome::kConstraintsValidationError, 1);
  }

  // 3. Microphone: valid (audio only)
  {
    base::HistogramTester histogram_tester;
    auto* element = MakeGarbageCollected<HTMLMicrophoneElement>(GetDocument());
    auto* constraints = MediaTrackConstraintSet::Create();
    MediaCaptureElementConstraints::setConstraints(*element, constraints);

    provider->StartRequest(element, element->GetPermissionDescriptors());

    histogram_tester.ExpectUniqueSample(
        "Blink.CapabilityElement.Microphone.Constraints.ValidationError",
        CapabilityElementMediaConstraintsValidationError::kNoError, 1);
  }

  // 4. Microphone: missing audio
  {
    base::HistogramTester histogram_tester;
    auto* element = MakeGarbageCollected<HTMLMicrophoneElement>(GetDocument());
    auto* constraints = HTMLMediaStreamConstraints::Create();
    MediaCaptureElementConstraints::From(*element).SetConstraints(constraints);

    provider->StartRequest(element, element->GetPermissionDescriptors());

    histogram_tester.ExpectUniqueSample(
        "Blink.CapabilityElement.Microphone.Constraints.ValidationError",
        CapabilityElementMediaConstraintsValidationError::kAudioMissing, 1);
    histogram_tester.ExpectUniqueSample(
        "Blink.CapabilityElement.Microphone.RequestOutcome",
        CapabilityElementMediaRequestOutcome::kConstraintsValidationError, 1);
  }
}

TEST_F(UserMediaRequestProviderImplTest, MetricsCallbacksOnSuccess) {
  base::HistogramTester histogram_tester;

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->SetMediaStreamRequestStartTimeForTesting(base::TimeTicks::Now());
  auto* callbacks =
      MakeGarbageCollected<UserMediaRequestProviderCallbacks>(element);

  auto* stream = MediaStream::Create(GetDocument().GetExecutionContext());
  MediaStreamVector streams = {stream};

  callbacks->OnSuccess(streams, /*capture_controller=*/nullptr);

  histogram_tester.ExpectUniqueSample(
      "Blink.CapabilityElement.UserMedia.RequestOutcome",
      CapabilityElementMediaRequestOutcome::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "Blink.CapabilityElement.UserMedia.TimeToStreamOrTrack", 1);
}

TEST_F(UserMediaRequestProviderImplTest, MetricsCallbacksOnErrorAndCancel) {
  V8TestingScope scope;
  base::HistogramTester histogram_tester;

  // 1. Error callback with NotFoundError
  {
    auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
    element->SetMediaStreamRequestStartTimeForTesting(base::TimeTicks::Now());
    auto* callbacks =
        MakeGarbageCollected<UserMediaRequestProviderCallbacks>(element);

    DOMException* dom_exception =
        DOMException::Create("Some error message", "NotFoundError");
    V8MediaStreamError* error =
        MakeGarbageCollected<V8UnionDOMExceptionOrOverconstrainedError>(
            dom_exception);
    callbacks->OnError(nullptr, error, nullptr,
                       UserMediaRequestResult::kNotFoundError);

    histogram_tester.ExpectUniqueSample(
        "Blink.CapabilityElement.UserMedia.RequestOutcome",
        CapabilityElementMediaRequestOutcome::kNotFoundError, 1);
    histogram_tester.ExpectTotalCount(
        "Blink.CapabilityElement.UserMedia.TimeToError", 1);
  }

  // 2. Cancel callback with NotAllowedByUserError
  {
    auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
    element->SetMediaStreamRequestStartTimeForTesting(base::TimeTicks::Now());
    auto* callbacks =
        MakeGarbageCollected<UserMediaRequestProviderCallbacks>(element);

    DOMException* cancel_exception =
        DOMException::Create("User denied", "NotAllowedError");
    V8MediaStreamError* cancel_error =
        MakeGarbageCollected<V8UnionDOMExceptionOrOverconstrainedError>(
            cancel_exception);
    callbacks->OnError(nullptr, cancel_error, nullptr,
                       UserMediaRequestResult::kNotAllowedByUserError);

    histogram_tester.ExpectBucketCount(
        "Blink.CapabilityElement.UserMedia.RequestOutcome",
        CapabilityElementMediaRequestOutcome::kNotAllowedError, 1);
    histogram_tester.ExpectTotalCount(
        "Blink.CapabilityElement.UserMedia.TimeToError", 2);
  }
}

}  // namespace blink
