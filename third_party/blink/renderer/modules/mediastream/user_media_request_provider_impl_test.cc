// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/user_media_request_provider_impl.h"

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
#include "third_party/blink/renderer/core/html/html_user_media_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/mediastream/html_user_media_element_media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_element_constraints.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

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
  element->setAttribute(html_names::kTypeAttr, AtomicString("camera"));

  HTMLMediaStreamConstraints* constraints = HTMLMediaStreamConstraints::Create();
  constraints->setVideo(MediaTrackConstraintSet::Create());
  UserMediaElementConstraints::setConstraints(*element, constraints);

  provider->StartRequest(element, element->GetPermissionDescriptors());
  // Test passes if it doesn't crash.
}

// Verifies that StartRequest gracefully exits and makes no changes when an
// active stream is already present.
TEST_F(UserMediaRequestProviderImplTest, StartRequestActiveStreamExists) {
  V8TestingScope scope;
  auto* provider = UserMediaRequestProvider::From(*GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->setAttribute(html_names::kTypeAttr, AtomicString("camera"));

  HTMLMediaStreamConstraints* constraints = HTMLMediaStreamConstraints::Create();
  constraints->setVideo(MediaTrackConstraintSet::Create());
  UserMediaElementConstraints::setConstraints(*element, constraints);

  auto* stream = MediaStream::Create(GetDocument().GetExecutionContext());
  HTMLUserMediaElementMediaStream::From(*element).SetMediaStream(stream);

  provider->StartRequest(element, element->GetPermissionDescriptors());
  // Test passes if it doesn't crash and does not change the stream.
  EXPECT_EQ(HTMLUserMediaElementMediaStream::stream(*element), stream);
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

  EXPECT_FALSE(HTMLUserMediaElementMediaStream::error(*element));

  DOMException* dom_exception =
      DOMException::Create("Some error message", "NotFoundError");
  V8MediaStreamError* error =
      MakeGarbageCollected<V8UnionDOMExceptionOrOverconstrainedError>(
          dom_exception);
  callbacks->OnError(nullptr, error, nullptr, UserMediaRequestResult());

  // Check that the error event was fired and the stream event was not
  EXPECT_TRUE(error_listener->fired());
  EXPECT_FALSE(stream_listener->fired());

  DOMException* stored_error = HTMLUserMediaElementMediaStream::error(*element);
  ASSERT_TRUE(stored_error);
  EXPECT_EQ(stored_error->name(), "NotFoundError");
  EXPECT_EQ(stored_error->message(), "Some error message");
}

TEST_F(UserMediaRequestProviderImplTest, StartRequestNoConstraintsError) {
  V8TestingScope scope;
  auto* provider = UserMediaRequestProvider::From(*GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->setAttribute(html_names::kTypeAttr, AtomicString("camera microphone"));

  HTMLMediaStreamConstraints* constraints = HTMLMediaStreamConstraints::Create();
  UserMediaElementConstraints::setConstraints(*element, constraints);

  // Set up event listeners
  auto* error_listener = MakeGarbageCollected<TestEventListener>();
  auto* stream_listener = MakeGarbageCollected<TestEventListener>();
  element->addEventListener(event_type_names::kError, error_listener);
  element->addEventListener(event_type_names::kStream, stream_listener);

  provider->StartRequest(element, element->GetPermissionDescriptors());

  // Verify events
  EXPECT_TRUE(error_listener->fired());
  EXPECT_FALSE(stream_listener->fired());

  DOMException* stored_error = HTMLUserMediaElementMediaStream::error(*element);
  ASSERT_TRUE(stored_error);
  EXPECT_EQ(stored_error->name(), "NotSupportedError");
  EXPECT_EQ(stored_error->message(), "No constraints set");
}

}  // namespace blink
