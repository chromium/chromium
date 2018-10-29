// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/remoteplayback/remote_playback.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/remoteplayback/web_remote_playback_state.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_remote_playback_availability_callback.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/presentation/presentation_controller.h"
#include "third_party/blink/renderer/modules/remoteplayback/html_media_element_remote_playback.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class MockFunction : public ScriptFunction {
 public:
  static testing::StrictMock<MockFunction>* Create(ScriptState* script_state) {
    return new testing::StrictMock<MockFunction>(script_state);
  }

  v8::Local<v8::Function> Bind() { return BindToV8Function(); }

  MOCK_METHOD1(Call, ScriptValue(ScriptValue));

 protected:
  explicit MockFunction(ScriptState* script_state)
      : ScriptFunction(script_state) {}
};

class MockEventListenerForRemotePlayback : public EventListener {
 public:
  MockEventListenerForRemotePlayback() : EventListener(kCPPEventListenerType) {}

  bool operator==(const EventListener& other) const final {
    return this == &other;
  }

  MOCK_METHOD2(handleEvent, void(ExecutionContext* executionContext, Event*));
};

class MockPresentationController final : public PresentationController {
 public:
  explicit MockPresentationController(LocalFrame& frame)
      : PresentationController(frame) {}
  ~MockPresentationController() override = default;

  MOCK_METHOD1(AddAvailabilityObserver,
               void(PresentationAvailabilityObserver*));
  MOCK_METHOD1(RemoveAvailabilityObserver,
               void(PresentationAvailabilityObserver*));
};

class RemotePlaybackTest : public testing::Test,
                           private ScopedRemotePlaybackBackendForTest {
 public:
  RemotePlaybackTest() : ScopedRemotePlaybackBackendForTest(true) {}

 protected:
  void CancelPrompt(RemotePlayback* remote_playback) {
    remote_playback->PromptCancelled();
  }

  void SetState(RemotePlayback* remote_playback, WebRemotePlaybackState state) {
    remote_playback->StateChanged(state);
  }

  bool IsListening(RemotePlayback* remote_playback) {
    return remote_playback->is_listening_;
  }
};

TEST_F(RemotePlaybackTest, PromptCancelledRejectsWithNotAllowedError) {
  V8TestingScope scope;

  auto page_holder = DummyPageHolder::Create();

  HTMLMediaElement* element =
      HTMLVideoElement::Create(page_holder->GetDocument());
  RemotePlayback* remote_playback =
      HTMLMediaElementRemotePlayback::remote(*element);

  auto* resolve = MockFunction::Create(scope.GetScriptState());
  auto* reject = MockFunction::Create(scope.GetScriptState());

  EXPECT_CALL(*resolve, Call(testing::_)).Times(0);
  EXPECT_CALL(*reject, Call(testing::_)).Times(1);

  std::unique_ptr<UserGestureIndicator> indicator =
      LocalFrame::NotifyUserActivation(&page_holder->GetFrame(),
                                       UserGestureToken::kNewGesture);
  remote_playback->prompt(scope.GetScriptState())
      .Then(resolve->Bind(), reject->Bind());
  CancelPrompt(remote_playback);

  // Runs pending promises.
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  // Verify mock expectations explicitly as the mock objects are garbage
  // collected.
  testing::Mock::VerifyAndClear(resolve);
  testing::Mock::VerifyAndClear(reject);
}

TEST_F(RemotePlaybackTest, PromptConnectedRejectsWhenCancelled) {
  V8TestingScope scope;

  auto page_holder = DummyPageHolder::Create();

  HTMLMediaElement* element =
      HTMLVideoElement::Create(page_holder->GetDocument());
  RemotePlayback* remote_playback =
      HTMLMediaElementRemotePlayback::remote(*element);

  auto* resolve = MockFunction::Create(scope.GetScriptState());
  auto* reject = MockFunction::Create(scope.GetScriptState());

  EXPECT_CALL(*resolve, Call(testing::_)).Times(0);
  EXPECT_CALL(*reject, Call(testing::_)).Times(1);

  SetState(remote_playback, WebRemotePlaybackState::kConnected);

  std::unique_ptr<UserGestureIndicator> indicator =
      LocalFrame::NotifyUserActivation(&page_holder->GetFrame(),
                                       UserGestureToken::kNewGesture);
  remote_playback->prompt(scope.GetScriptState())
      .Then(resolve->Bind(), reject->Bind());
  CancelPrompt(remote_playback);

  // Runs pending promises.
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  // Verify mock expectations explicitly as the mock objects are garbage
  // collected.
  testing::Mock::VerifyAndClear(resolve);
  testing::Mock::VerifyAndClear(reject);
}

TEST_F(RemotePlaybackTest, PromptConnectedResolvesWhenDisconnected) {
  V8TestingScope scope;

  auto page_holder = DummyPageHolder::Create();

  HTMLMediaElement* element =
      HTMLVideoElement::Create(page_holder->GetDocument());
  RemotePlayback* remote_playback =
      HTMLMediaElementRemotePlayback::remote(*element);

  auto* resolve = MockFunction::Create(scope.GetScriptState());
  auto* reject = MockFunction::Create(scope.GetScriptState());

  EXPECT_CALL(*resolve, Call(testing::_)).Times(1);
  EXPECT_CALL(*reject, Call(testing::_)).Times(0);

  SetState(remote_playback, WebRemotePlaybackState::kConnected);

  std::unique_ptr<UserGestureIndicator> indicator =
      LocalFrame::NotifyUserActivation(&page_holder->GetFrame(),
                                       UserGestureToken::kNewGesture);
  remote_playback->prompt(scope.GetScriptState())
      .Then(resolve->Bind(), reject->Bind());

  SetState(remote_playback, WebRemotePlaybackState::kDisconnected);

  // Runs pending promises.
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  // Verify mock expectations explicitly as the mock objects are garbage
  // collected.
  testing::Mock::VerifyAndClear(resolve);
  testing::Mock::VerifyAndClear(reject);
}

TEST_F(RemotePlaybackTest, StateChangeEvents) {
  V8TestingScope scope;

  auto page_holder = DummyPageHolder::Create();

  HTMLMediaElement* element =
      HTMLVideoElement::Create(page_holder->GetDocument());
  RemotePlayback* remote_playback =
      HTMLMediaElementRemotePlayback::remote(*element);

  auto* connecting_handler =
      new testing::StrictMock<MockEventListenerForRemotePlayback>();
  auto* connect_handler =
      new testing::StrictMock<MockEventListenerForRemotePlayback>();
  auto* disconnect_handler =
      new testing::StrictMock<MockEventListenerForRemotePlayback>();

  remote_playback->addEventListener(EventTypeNames::connecting,
                                    connecting_handler);
  remote_playback->addEventListener(EventTypeNames::connect, connect_handler);
  remote_playback->addEventListener(EventTypeNames::disconnect,
                                    disconnect_handler);

  EXPECT_CALL(*connecting_handler, handleEvent(testing::_, testing::_))
      .Times(1);
  EXPECT_CALL(*connect_handler, handleEvent(testing::_, testing::_)).Times(1);
  EXPECT_CALL(*disconnect_handler, handleEvent(testing::_, testing::_))
      .Times(1);

  SetState(remote_playback, WebRemotePlaybackState::kConnecting);
  SetState(remote_playback, WebRemotePlaybackState::kConnecting);
  SetState(remote_playback, WebRemotePlaybackState::kConnected);
  SetState(remote_playback, WebRemotePlaybackState::kConnected);
  SetState(remote_playback, WebRemotePlaybackState::kDisconnected);
  SetState(remote_playback, WebRemotePlaybackState::kDisconnected);

  // Verify mock expectations explicitly as the mock objects are garbage
  // collected.
  testing::Mock::VerifyAndClear(connecting_handler);
  testing::Mock::VerifyAndClear(connect_handler);
  testing::Mock::VerifyAndClear(disconnect_handler);
}

TEST_F(RemotePlaybackTest,
       DisableRemotePlaybackRejectsPromptWithInvalidStateError) {
  V8TestingScope scope;

  auto page_holder = DummyPageHolder::Create();

  HTMLMediaElement* element =
      HTMLVideoElement::Create(page_holder->GetDocument());
  RemotePlayback* remote_playback =
      HTMLMediaElementRemotePlayback::remote(*element);

  MockFunction* resolve = MockFunction::Create(scope.GetScriptState());
  MockFunction* reject = MockFunction::Create(scope.GetScriptState());

  EXPECT_CALL(*resolve, Call(testing::_)).Times(0);
  EXPECT_CALL(*reject, Call(testing::_)).Times(1);

  std::unique_ptr<UserGestureIndicator> indicator =
      LocalFrame::NotifyUserActivation(&page_holder->GetFrame(),
                                       UserGestureToken::kNewGesture);
  remote_playback->prompt(scope.GetScriptState())
      .Then(resolve->Bind(), reject->Bind());
  HTMLMediaElementRemotePlayback::SetBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, *element, true);

  // Runs pending promises.
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  // Verify mock expectations explicitly as the mock objects are garbage
  // collected.
  testing::Mock::VerifyAndClear(resolve);
  testing::Mock::VerifyAndClear(reject);
}

TEST_F(RemotePlaybackTest, DisableRemotePlaybackCancelsAvailabilityCallbacks) {
  V8TestingScope scope;

  auto page_holder = DummyPageHolder::Create();

  HTMLMediaElement* element =
      HTMLVideoElement::Create(page_holder->GetDocument());
  RemotePlayback* remote_playback =
      HTMLMediaElementRemotePlayback::remote(*element);

  MockFunction* callback_function =
      MockFunction::Create(scope.GetScriptState());
  V8RemotePlaybackAvailabilityCallback* availability_callback =
      V8RemotePlaybackAvailabilityCallback::Create(callback_function->Bind());

  // The initial call upon registering will not happen as it's posted on the
  // message loop.
  EXPECT_CALL(*callback_function, Call(testing::_)).Times(0);

  MockFunction* resolve = MockFunction::Create(scope.GetScriptState());
  MockFunction* reject = MockFunction::Create(scope.GetScriptState());

  EXPECT_CALL(*resolve, Call(testing::_)).Times(1);
  EXPECT_CALL(*reject, Call(testing::_)).Times(0);

  remote_playback
      ->watchAvailability(scope.GetScriptState(), availability_callback)
      .Then(resolve->Bind(), reject->Bind());

  HTMLMediaElementRemotePlayback::SetBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, *element, true);

  // Runs pending promises.
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  // Verify mock expectations explicitly as the mock objects are garbage
  // collected.
  testing::Mock::VerifyAndClear(resolve);
  testing::Mock::VerifyAndClear(reject);
  testing::Mock::VerifyAndClear(callback_function);
}

TEST_F(RemotePlaybackTest, PromptThrowsWhenBackendDisabled) {
  ScopedRemotePlaybackBackendForTest remote_playback_backend(false);
  V8TestingScope scope;

  auto page_holder = DummyPageHolder::Create();

  HTMLMediaElement* element =
      HTMLVideoElement::Create(page_holder->GetDocument());
  RemotePlayback* remote_playback =
      HTMLMediaElementRemotePlayback::remote(*element);

  auto* resolve = MockFunction::Create(scope.GetScriptState());
  auto* reject = MockFunction::Create(scope.GetScriptState());

  EXPECT_CALL(*resolve, Call(testing::_)).Times(0);
  EXPECT_CALL(*reject, Call(testing::_)).Times(1);

  std::unique_ptr<UserGestureIndicator> indicator =
      LocalFrame::NotifyUserActivation(&page_holder->GetFrame(),
                                       UserGestureToken::kNewGesture);
  remote_playback->prompt(scope.GetScriptState())
      .Then(resolve->Bind(), reject->Bind());

  // Runs pending promises.
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  // Verify mock expectations explicitly as the mock objects are garbage
  // collected.
  testing::Mock::VerifyAndClear(resolve);
  testing::Mock::VerifyAndClear(reject);
}

TEST_F(RemotePlaybackTest, WatchAvailabilityWorksWhenBackendDisabled) {
  ScopedRemotePlaybackBackendForTest remote_playback_backend(false);
  V8TestingScope scope;

  auto page_holder = DummyPageHolder::Create();

  HTMLMediaElement* element =
      HTMLVideoElement::Create(page_holder->GetDocument());
  RemotePlayback* remote_playback =
      HTMLMediaElementRemotePlayback::remote(*element);

  MockFunction* callback_function =
      MockFunction::Create(scope.GetScriptState());
  V8RemotePlaybackAvailabilityCallback* availability_callback =
      V8RemotePlaybackAvailabilityCallback::Create(callback_function->Bind());

  // The initial call upon registering will not happen as it's posted on the
  // message loop.
  EXPECT_CALL(*callback_function, Call(testing::_)).Times(0);

  MockFunction* resolve = MockFunction::Create(scope.GetScriptState());
  MockFunction* reject = MockFunction::Create(scope.GetScriptState());

  EXPECT_CALL(*resolve, Call(testing::_)).Times(1);
  EXPECT_CALL(*reject, Call(testing::_)).Times(0);

  remote_playback
      ->watchAvailability(scope.GetScriptState(), availability_callback)
      .Then(resolve->Bind(), reject->Bind());

  // Runs pending promises.
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  // Verify mock expectations explicitly as the mock objects are garbage
  // collected.
  testing::Mock::VerifyAndClear(resolve);
  testing::Mock::VerifyAndClear(reject);
  testing::Mock::VerifyAndClear(callback_function);
}

TEST_F(RemotePlaybackTest, IsListening) {
  ScopedNewRemotePlaybackPipelineForTest new_remote_playback_pipeline(true);
  V8TestingScope scope;

  auto page_holder = DummyPageHolder::Create();

  HTMLMediaElement* element =
      HTMLVideoElement::Create(page_holder->GetDocument());
  RemotePlayback* remote_playback =
      HTMLMediaElementRemotePlayback::remote(*element);

  LocalFrame& frame = page_holder->GetFrame();
  MockPresentationController* mock_controller =
      new MockPresentationController(frame);
  Supplement<LocalFrame>::ProvideTo(
      frame, static_cast<PresentationController*>(mock_controller));

  EXPECT_CALL(*mock_controller,
              AddAvailabilityObserver(testing::Eq(remote_playback)))
      .Times(2);
  EXPECT_CALL(*mock_controller,
              RemoveAvailabilityObserver(testing::Eq(remote_playback)))
      .Times(2);

  MockFunction* callback_function =
      MockFunction::Create(scope.GetScriptState());
  V8RemotePlaybackAvailabilityCallback* availability_callback =
      V8RemotePlaybackAvailabilityCallback::Create(callback_function->Bind());

  // The initial call upon registering will not happen as it's posted on the
  // message loop.
  EXPECT_CALL(*callback_function, Call(testing::_)).Times(2);

  remote_playback->watchAvailability(scope.GetScriptState(),
                                     availability_callback);

  ASSERT_TRUE(remote_playback->Urls().IsEmpty());
  ASSERT_FALSE(IsListening(remote_playback));

  remote_playback->SourceChanged(WebURL(KURL("http://www.example.com")), true);
  ASSERT_EQ((size_t)1, remote_playback->Urls().size());
  ASSERT_TRUE(IsListening(remote_playback));
  remote_playback->AvailabilityChanged(mojom::ScreenAvailability::AVAILABLE);

  remote_playback->cancelWatchAvailability(scope.GetScriptState());
  ASSERT_EQ((size_t)1, remote_playback->Urls().size());
  ASSERT_FALSE(IsListening(remote_playback));

  remote_playback->watchAvailability(scope.GetScriptState(),
                                     availability_callback);
  ASSERT_EQ((size_t)1, remote_playback->Urls().size());
  ASSERT_TRUE(IsListening(remote_playback));
  remote_playback->AvailabilityChanged(mojom::ScreenAvailability::AVAILABLE);

  remote_playback->SourceChanged(WebURL(), false);
  ASSERT_TRUE(remote_playback->Urls().IsEmpty());
  ASSERT_FALSE(IsListening(remote_playback));

  remote_playback->SourceChanged(WebURL(KURL("@$@#@#")), true);
  ASSERT_TRUE(remote_playback->Urls().IsEmpty());
  ASSERT_FALSE(IsListening(remote_playback));

  // Runs pending promises.
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  // Verify mock expectations explicitly as the mock objects are garbage
  // collected.
  testing::Mock::VerifyAndClear(callback_function);
  testing::Mock::VerifyAndClear(mock_controller);
}

}  // namespace blink
