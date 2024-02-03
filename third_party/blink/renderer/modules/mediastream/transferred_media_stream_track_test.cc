// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/transferred_media_stream_track.h"

#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_capabilities.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_settings.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_track.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {
using testing::_;
using testing::Return;

class TestObserver : public GarbageCollected<TestObserver>,
                     public MediaStreamTrack::Observer {
 public:
  void TrackChangedState() override { observation_count_++; }
  int ObservationCount() const { return observation_count_; }

 private:
  int observation_count_ = 0;
};

}  // namespace

class TransferredMediaStreamTrackTest : public testing::Test {
 public:
  void CustomSetUp(V8TestingScope& scope) {
    mock_impl_ = MakeGarbageCollected<MockMediaStreamTrack>();
    transferred_track_ = MakeGarbageCollected<TransferredMediaStreamTrack>(
        scope.GetExecutionContext(),
        MediaStreamTrack::TransferredValues{
            .kind = "video",
            .id = "",
            .label = "dummy",
            .enabled = true,
            .muted = false,
            .content_hint = WebMediaStreamTrack::ContentHintType::kNone,
            .ready_state = MediaStreamSource::kReadyStateLive});
  }

  void TearDown() override { WebHeap::CollectAllGarbageForTesting(); }

  test::TaskEnvironment task_environment_;
  WeakPersistent<MockMediaStreamTrack> mock_impl_;
  Persistent<TransferredMediaStreamTrack> transferred_track_;
};

class MockEventListener final : public NativeEventListener {
 public:
  MOCK_METHOD2(Invoke, void(ExecutionContext* executionContext, Event*));
};

TEST_F(TransferredMediaStreamTrackTest, InitialProperties) {
  V8TestingScope scope;
  CustomSetUp(scope);
  EXPECT_EQ(transferred_track_->kind(), "video");
  EXPECT_EQ(transferred_track_->id(), "");
  EXPECT_EQ(transferred_track_->label(), "dummy");
  EXPECT_EQ(transferred_track_->enabled(), true);
  EXPECT_EQ(transferred_track_->muted(), false);
  EXPECT_EQ(transferred_track_->ContentHint(), "");
  EXPECT_EQ(transferred_track_->readyState(), "live");
  EXPECT_EQ(transferred_track_->GetReadyState(),
            MediaStreamSource::kReadyStateLive);
  EXPECT_EQ(transferred_track_->Ended(), false);
  EXPECT_EQ(transferred_track_->device(), std::nullopt);
}

TEST_F(TransferredMediaStreamTrackTest, PropertiesInheritFromImplementation) {
  V8TestingScope scope;
  CustomSetUp(scope);
  const String kKind = "audio";
  const String kId = "id";
  const String kLabel = "label";
  const bool kEnabled = false;
  const bool kMuted = true;
  const String kContentHint = "motion";
  const String kReadyState = "ended";
  const MediaStreamSource::ReadyState kReadyStateEnum =
      MediaStreamSource::kReadyStateEnded;
  const bool kEnded = true;
  base::UnguessableToken kSerializableSessionId =
      base::UnguessableToken::Create();
  MediaStreamDevice kDevice = MediaStreamDevice();
  kDevice.set_session_id(kSerializableSessionId);

  Persistent<MediaTrackCapabilities> capabilities =
      MediaTrackCapabilities::Create();
  Persistent<MediaTrackConstraints> constraints =
      MediaTrackConstraints::Create();
  Persistent<MediaTrackSettings> settings = MediaTrackSettings::Create();
  Persistent<CaptureHandle> capture_handle = CaptureHandle::Create();

  mock_impl_->SetKind(kKind);
  mock_impl_->SetId(kId);
  mock_impl_->SetLabel(kLabel);
  mock_impl_->setEnabled(kEnabled);
  mock_impl_->SetMuted(kMuted);
  mock_impl_->SetContentHint(kContentHint);
  mock_impl_->SetReadyState(kReadyState);
  mock_impl_->SetCapabilities(capabilities);
  mock_impl_->SetConstraints(constraints);
  mock_impl_->SetSettings(settings);
  mock_impl_->SetCaptureHandle(capture_handle);
  mock_impl_->SetReadyState(kReadyStateEnum);
  mock_impl_->SetComponent(nullptr);
  mock_impl_->SetEnded(kEnded);
  mock_impl_->SetDevice(kDevice);
  mock_impl_->SetExecutionContext(scope.GetExecutionContext());

  EXPECT_CALL(*mock_impl_, AddedEventListener(_, _)).Times(4);
  transferred_track_->SetImplementation(mock_impl_);

  EXPECT_EQ(transferred_track_->kind(), kKind);
  EXPECT_EQ(transferred_track_->id(), kId);
  EXPECT_EQ(transferred_track_->label(), kLabel);
  EXPECT_EQ(transferred_track_->enabled(), kEnabled);
  EXPECT_EQ(transferred_track_->muted(), kMuted);
  EXPECT_EQ(transferred_track_->ContentHint(), kContentHint);
  EXPECT_EQ(transferred_track_->readyState(), kReadyState);
  EXPECT_EQ(transferred_track_->GetReadyState(), kReadyStateEnum);
  EXPECT_EQ(transferred_track_->Ended(), kEnded);
  EXPECT_TRUE(transferred_track_->device().has_value());
  EXPECT_EQ(transferred_track_->device()->serializable_session_id(),
            kSerializableSessionId);
}

TEST_F(TransferredMediaStreamTrackTest, EventsArePropagated) {
  V8TestingScope scope;
  CustomSetUp(scope);
  auto* mock_event_handler = MakeGarbageCollected<MockEventListener>();
  transferred_track_->addEventListener(event_type_names::kEnded,
                                       mock_event_handler);

  mock_impl_->SetExecutionContext(scope.GetExecutionContext());
  EXPECT_CALL(*mock_impl_, AddedEventListener(_, _)).Times(4);
  transferred_track_->SetImplementation(mock_impl_);

  // Dispatching an event on the actual track underneath the
  // TransferredMediaStreamTrack should get propagated to be an event fired on
  // the TMST itself.
  EXPECT_CALL(*mock_event_handler, Invoke(_, _));
  ASSERT_EQ(mock_impl_->DispatchEvent(*Event::Create(event_type_names::kEnded)),
            DispatchEventResult::kNotCanceled);
}

TEST_F(TransferredMediaStreamTrackTest,
       ConstraintsAppliedBeforeImplementation) {
  V8TestingScope scope;
  CustomSetUp(scope);

  mock_impl_->SetExecutionContext(scope.GetExecutionContext());
  transferred_track_->applyConstraints(scope.GetScriptState(),
                                       MediaTrackConstraints::Create());
  EXPECT_CALL(*mock_impl_, AddedEventListener(_, _)).Times(4);

  EXPECT_CALL(*mock_impl_, applyConstraintsScriptState(_, _)).Times(0);
  EXPECT_CALL(*mock_impl_, applyConstraintsResolver(_, _)).Times(1);
  transferred_track_->SetImplementation(mock_impl_);
}

TEST_F(TransferredMediaStreamTrackTest, ConstraintsAppliedAfterImplementation) {
  V8TestingScope scope;
  CustomSetUp(scope);

  mock_impl_->SetExecutionContext(scope.GetExecutionContext());
  EXPECT_CALL(*mock_impl_, AddedEventListener(_, _)).Times(4);

  EXPECT_CALL(*mock_impl_, applyConstraintsScriptState(_, _)).Times(1);
  EXPECT_CALL(*mock_impl_, applyConstraintsResolver(_, _)).Times(0);
  transferred_track_->SetImplementation(mock_impl_);

  transferred_track_->applyConstraints(scope.GetScriptState(),
                                       MediaTrackConstraints::Create());
}

TEST_F(TransferredMediaStreamTrackTest, ContentHintSetBeforeImplementation) {
  V8TestingScope scope;
  CustomSetUp(scope);

  mock_impl_->SetExecutionContext(scope.GetExecutionContext());
  const String kContentHint = "music";
  transferred_track_->SetContentHint(kContentHint);
  ASSERT_EQ(transferred_track_->ContentHint(), "");
  transferred_track_->SetImplementation(mock_impl_);
  EXPECT_EQ(transferred_track_->ContentHint(), kContentHint);
}

TEST_F(TransferredMediaStreamTrackTest, ContentHintSetAfterImplementation) {
  V8TestingScope scope;
  CustomSetUp(scope);

  mock_impl_->SetExecutionContext(scope.GetExecutionContext());
  const String kContentHint = "speech";
  transferred_track_->SetImplementation(mock_impl_);
  ASSERT_TRUE(transferred_track_->ContentHint().IsNull());
  transferred_track_->SetContentHint(kContentHint);
  EXPECT_EQ(transferred_track_->ContentHint(), kContentHint);
}

TEST_F(TransferredMediaStreamTrackTest, SetEnabledBeforeImplementation) {
  V8TestingScope scope;
  CustomSetUp(scope);

  mock_impl_->SetExecutionContext(scope.GetExecutionContext());
  transferred_track_->setEnabled(/*enabled=*/true);
  ASSERT_TRUE(transferred_track_->enabled());
  ASSERT_FALSE(mock_impl_->enabled());
  transferred_track_->SetImplementation(mock_impl_);
  EXPECT_TRUE(transferred_track_->enabled());
}

TEST_F(TransferredMediaStreamTrackTest, SetEnabledAfterImplementation) {
  V8TestingScope scope;
  CustomSetUp(scope);

  mock_impl_->SetExecutionContext(scope.GetExecutionContext());
  ASSERT_TRUE(transferred_track_->enabled());
  transferred_track_->SetImplementation(mock_impl_);
  EXPECT_FALSE(transferred_track_->enabled());
  transferred_track_->setEnabled(/*enabled=*/true);
  EXPECT_TRUE(transferred_track_->enabled());
}

TEST_F(TransferredMediaStreamTrackTest, MultipleSetterFunctions) {
  V8TestingScope scope;
  CustomSetUp(scope);

  EXPECT_CALL(*mock_impl_, applyConstraintsResolver(_, _)).Times(1);
  mock_impl_->SetExecutionContext(scope.GetExecutionContext());
  transferred_track_->SetContentHint("speech");
  transferred_track_->applyConstraints(scope.GetScriptState(),
                                       MediaTrackConstraints::Create());
  transferred_track_->setEnabled(/*enabled=*/true);
  transferred_track_->SetContentHint("music");
  transferred_track_->setEnabled(/*enabled=*/false);
  ASSERT_TRUE(transferred_track_->enabled());
  ASSERT_EQ(transferred_track_->ContentHint(), "");
  transferred_track_->SetImplementation(mock_impl_);
  EXPECT_EQ(transferred_track_->ContentHint(), "music");
  EXPECT_FALSE(transferred_track_->enabled());
}

TEST_F(TransferredMediaStreamTrackTest, SetImplementationTriggersObservers) {
  V8TestingScope scope;
  CustomSetUp(scope);
  TestObserver* testObserver = MakeGarbageCollected<TestObserver>();
  transferred_track_->AddObserver(testObserver);
  transferred_track_->SetImplementation(
      MakeGarbageCollected<testing::NiceMock<MockMediaStreamTrack>>());
  EXPECT_EQ(testObserver->ObservationCount(), 1);
}

TEST_F(TransferredMediaStreamTrackTest, ObserversAddedToImpl) {
  V8TestingScope scope;
  CustomSetUp(scope);
  transferred_track_->AddObserver(MakeGarbageCollected<TestObserver>());
  MockMediaStreamTrack* mock_impl_ =
      MakeGarbageCollected<testing::NiceMock<MockMediaStreamTrack>>();
  EXPECT_CALL(*mock_impl_, AddObserver(_));
  transferred_track_->SetImplementation(mock_impl_);
}

TEST_F(TransferredMediaStreamTrackTest, CloneInitialProperties) {
  V8TestingScope scope;
  CustomSetUp(scope);

  MediaStreamTrack* clone =
      transferred_track_->clone(scope.GetExecutionContext());

  EXPECT_EQ(clone->kind(), "video");
  EXPECT_EQ(clone->id(), "");
  EXPECT_EQ(clone->label(), "dummy");
  EXPECT_EQ(clone->enabled(), true);
  EXPECT_EQ(clone->muted(), false);
  EXPECT_EQ(clone->ContentHint(), "");
  EXPECT_EQ(clone->readyState(), "live");
  EXPECT_EQ(clone->GetReadyState(), MediaStreamSource::kReadyStateLive);
  EXPECT_EQ(clone->Ended(), false);
  EXPECT_EQ(clone->device(), std::nullopt);
}

TEST_F(TransferredMediaStreamTrackTest, CloneSetImplementation) {
  V8TestingScope scope;
  CustomSetUp(scope);
  TransferredMediaStreamTrack* clone =
      static_cast<TransferredMediaStreamTrack*>(
          transferred_track_->clone(scope.GetExecutionContext()));
  MockMediaStreamTrack* mock_impl_ =
      MakeGarbageCollected<testing::NiceMock<MockMediaStreamTrack>>();
  EXPECT_CALL(*mock_impl_, clone(_))
      .WillOnce(Return(
          MakeGarbageCollected<testing::NiceMock<MockMediaStreamTrack>>()));

  transferred_track_->SetImplementation(mock_impl_);

  EXPECT_TRUE(clone->HasImplementation());
}

TEST_F(TransferredMediaStreamTrackTest, CloneMutationsReplayed) {
  V8TestingScope scope;
  CustomSetUp(scope);

  transferred_track_->setEnabled(false);

  TransferredMediaStreamTrack* clone =
      static_cast<TransferredMediaStreamTrack*>(
          transferred_track_->clone(scope.GetExecutionContext()));

  MockMediaStreamTrack* mock_impl_ =
      MakeGarbageCollected<testing::NiceMock<MockMediaStreamTrack>>();
  EXPECT_CALL(*mock_impl_, clone(_))
      .WillOnce(Return(
          MakeGarbageCollected<testing::NiceMock<MockMediaStreamTrack>>()));
  transferred_track_->SetImplementation(mock_impl_);

  EXPECT_EQ(transferred_track_->enabled(), false);
  EXPECT_EQ(clone->enabled(), false);
}

TEST_F(TransferredMediaStreamTrackTest, CloneDoesntIncludeLaterMutations) {
  V8TestingScope scope;
  CustomSetUp(scope);

  // Clone, the track, then disable the original. The clone should still be
  // enabled.
  transferred_track_->setEnabled(true);
  MediaStreamTrack* clone =
      transferred_track_->clone(scope.GetExecutionContext());
  transferred_track_->setEnabled(false);

  MockMediaStreamTrack* mock_impl_ =
      MakeGarbageCollected<testing::NiceMock<MockMediaStreamTrack>>();
  EXPECT_CALL(*mock_impl_, clone(_)).WillOnce([&](ExecutionContext*) {
    MockMediaStreamTrack* mock_clone_impl_ =
        MakeGarbageCollected<testing::NiceMock<MockMediaStreamTrack>>();
    mock_clone_impl_->setEnabled(mock_impl_->enabled());
    return mock_clone_impl_;
  });
  transferred_track_->SetImplementation(mock_impl_);

  EXPECT_EQ(transferred_track_->enabled(), false);
  EXPECT_EQ(clone->enabled(), true);
}

}  // namespace blink
