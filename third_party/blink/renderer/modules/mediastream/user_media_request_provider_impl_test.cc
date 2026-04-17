// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/user_media_request_provider_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
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
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_mediatrackconstraints.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

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

  MediaStreamConstraints* constraints = MediaStreamConstraints::Create();
  constraints->setVideo(
      MakeGarbageCollected<V8UnionBooleanOrMediaTrackConstraints>(true));
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

  MediaStreamConstraints* constraints = MediaStreamConstraints::Create();
  constraints->setVideo(
      MakeGarbageCollected<V8UnionBooleanOrMediaTrackConstraints>(true));
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

  EXPECT_EQ(HTMLUserMediaElementMediaStream::stream(*element), nullptr);

  auto* stream = MediaStream::Create(GetDocument().GetExecutionContext());
  MediaStreamVector streams = {stream};

  callbacks->OnSuccess(streams, /*capture_controller=*/nullptr);

  // The stream should have been set on the element.
  EXPECT_EQ(HTMLUserMediaElementMediaStream::stream(*element), stream);
}

class TestEventListener : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event* event) override { fired_ = true; }
  bool fired() const { return fired_; }

 private:
  bool fired_ = false;
};

TEST_F(UserMediaRequestProviderImplTest, CallbacksOnError) {
  V8TestingScope scope;
  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  auto* callbacks =
      MakeGarbageCollected<UserMediaRequestProviderCallbacks>(element);

  // Set up event listener
  auto* listener = MakeGarbageCollected<TestEventListener>();
  element->addEventListener(event_type_names::kStream, listener);

  EXPECT_TRUE(HTMLUserMediaElementMediaStream::error(scope.GetScriptState(), *element).IsNull());

  DOMException* dom_exception =
      DOMException::Create("Some error message", "NotFoundError");
  V8MediaStreamError* error =
      MakeGarbageCollected<V8UnionDOMExceptionOrOverconstrainedError>(
          dom_exception);
  callbacks->OnError(nullptr, error, nullptr, UserMediaRequestResult());

  // Check that the event was fired and the error was set
  EXPECT_TRUE(listener->fired());
  ScriptValue stored_error = HTMLUserMediaElementMediaStream::error(scope.GetScriptState(), *element);
  EXPECT_FALSE(stored_error.IsEmpty());
  EXPECT_TRUE(stored_error.V8Value()->IsObject());
  EXPECT_EQ(ToCoreString(scope.GetIsolate(), stored_error.V8Value()
                                                 .As<v8::Object>()
                                                 ->Get(scope.GetContext(),
                                                       V8String(scope.GetIsolate(),
                                                                "name"))
                                                 .ToLocalChecked()
                                                 .As<v8::String>()),
            "NotFoundError");
}

TEST_F(UserMediaRequestProviderImplTest, StartRequestNoConstraintsError) {
  V8TestingScope scope;
  auto* provider = UserMediaRequestProvider::From(*GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->setAttribute(html_names::kTypeAttr, AtomicString("camera microphone"));

  MediaStreamConstraints* constraints = MediaStreamConstraints::Create();
  UserMediaElementConstraints::setConstraints(*element, constraints);

  // Set up event listener
  auto* listener = MakeGarbageCollected<TestEventListener>();
  element->addEventListener(event_type_names::kStream, listener);

  provider->StartRequest(element, element->GetPermissionDescriptors());

  EXPECT_TRUE(listener->fired());
  ScriptValue stored_error = HTMLUserMediaElementMediaStream::error(scope.GetScriptState(), *element);
  EXPECT_FALSE(stored_error.IsEmpty());
  EXPECT_TRUE(stored_error.V8Value()->IsObject());

  v8::Local<v8::Object> error_obj = stored_error.V8Value().As<v8::Object>();
  EXPECT_EQ(ToCoreString(scope.GetIsolate(), error_obj->Get(scope.GetContext(),
                                                       V8String(scope.GetIsolate(),
                                                                "name"))
                                                 .ToLocalChecked()
                                                 .As<v8::String>()),
            "TypeError");
  EXPECT_EQ(ToCoreString(scope.GetIsolate(), error_obj->Get(scope.GetContext(),
                                                       V8String(scope.GetIsolate(),
                                                                "message"))
                                                 .ToLocalChecked()
                                                 .As<v8::String>()),
            "No constraints set");
}

}  // namespace blink
