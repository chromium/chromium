// Copyright 2022 The Chromium Authors. All rights reserved.
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

namespace blink {

namespace {
using testing::_;

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
  EXPECT_EQ(transferred_track_->serializable_session_id(), absl::nullopt);
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
  const absl::optional<base::UnguessableToken> kSerializableSessionId =
      base::UnguessableToken::Create();

  Persistent<MediaTrackCapabilities> capabilities =
      MediaTrackCapabilities::Create();
  Persistent<MediaTrackConstraints> constraints =
      MediaTrackConstraints::Create();
  Persistent<MediaTrackSettings> settings = MediaTrackSettings::Create();
  Persistent<CaptureHandle> capture_handle = CaptureHandle::Create();

  MockMediaStreamTrack* mock_impl =
      MakeGarbageCollected<MockMediaStreamTrack>();
  mock_impl->SetKind(kKind);
  mock_impl->SetId(kId);
  mock_impl->SetLabel(kLabel);
  mock_impl->setEnabled(kEnabled);
  mock_impl->SetMuted(kMuted);
  mock_impl->SetContentHint(kContentHint);
  mock_impl->SetReadyState(kReadyState);
  mock_impl->SetCapabilities(capabilities);
  mock_impl->SetConstraints(constraints);
  mock_impl->SetSettings(settings);
  mock_impl->SetCaptureHandle(capture_handle);
  mock_impl->SetReadyState(kReadyStateEnum);
  mock_impl->SetComponent(nullptr);
  mock_impl->SetEnded(kEnded);
  mock_impl->SetSerializableSessionId(kSerializableSessionId);
  mock_impl->SetExecutionContext(scope.GetExecutionContext());

  EXPECT_CALL(*mock_impl, AddedEventListener(_, _)).Times(4);
  transferred_track_->SetImplementation(mock_impl);

  EXPECT_EQ(transferred_track_->kind(), kKind);
  EXPECT_EQ(transferred_track_->id(), kId);
  EXPECT_EQ(transferred_track_->label(), kLabel);
  EXPECT_EQ(transferred_track_->enabled(), kEnabled);
  EXPECT_EQ(transferred_track_->muted(), kMuted);
  EXPECT_EQ(transferred_track_->ContentHint(), kContentHint);
  EXPECT_EQ(transferred_track_->readyState(), kReadyState);
  EXPECT_EQ(transferred_track_->GetReadyState(), kReadyStateEnum);
  EXPECT_EQ(transferred_track_->Ended(), kEnded);
  EXPECT_EQ(transferred_track_->serializable_session_id(),
            kSerializableSessionId);
}

TEST_F(TransferredMediaStreamTrackTest, EventsArePropagated) {
  V8TestingScope scope;
  CustomSetUp(scope);
  auto* mock_event_handler = MakeGarbageCollected<MockEventListener>();
  transferred_track_->addEventListener(event_type_names::kEnded,
                                       mock_event_handler);

  MockMediaStreamTrack* mock_impl =
      MakeGarbageCollected<MockMediaStreamTrack>();
  mock_impl->SetExecutionContext(scope.GetExecutionContext());
  EXPECT_CALL(*mock_impl, AddedEventListener(_, _)).Times(4);
  transferred_track_->SetImplementation(mock_impl);

  // Dispatching an event on the actual track underneath the
  // TransferredMediaStreamTrack should get propagated to be an event fired on
  // the TMST itself.
  EXPECT_CALL(*mock_event_handler, Invoke(_, _));
  ASSERT_EQ(mock_impl->DispatchEvent(*Event::Create(event_type_names::kEnded)),
            DispatchEventResult::kNotCanceled);
}

TEST_F(TransferredMediaStreamTrackTest,
       ConstraintsAppliedBeforeImplementation) {
  V8TestingScope scope;
  CustomSetUp(scope);

  MockMediaStreamTrack* mock_impl =
      MakeGarbageCollected<MockMediaStreamTrack>();
  mock_impl->SetExecutionContext(scope.GetExecutionContext());
  transferred_track_->applyConstraints(scope.GetScriptState(),
                                       MediaTrackConstraints::Create());
  EXPECT_CALL(*mock_impl, AddedEventListener(_, _)).Times(4);

  EXPECT_CALL(*mock_impl, applyConstraintsScriptState(_, _)).Times(0);
  EXPECT_CALL(*mock_impl, applyConstraintsResolver(_, _)).Times(1);
  transferred_track_->SetImplementation(mock_impl);
}

TEST_F(TransferredMediaStreamTrackTest, ConstraintsAppliedAfterImplementation) {
  V8TestingScope scope;
  CustomSetUp(scope);

  MockMediaStreamTrack* mock_impl =
      MakeGarbageCollected<MockMediaStreamTrack>();
  mock_impl->SetExecutionContext(scope.GetExecutionContext());
  EXPECT_CALL(*mock_impl, AddedEventListener(_, _)).Times(4);

  EXPECT_CALL(*mock_impl, applyConstraintsScriptState(_, _)).Times(1);
  EXPECT_CALL(*mock_impl, applyConstraintsResolver(_, _)).Times(0);
  transferred_track_->SetImplementation(mock_impl);

  transferred_track_->applyConstraints(scope.GetScriptState(),
                                       MediaTrackConstraints::Create());
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
  MockMediaStreamTrack* mock_impl =
      MakeGarbageCollected<testing::NiceMock<MockMediaStreamTrack>>();
  EXPECT_CALL(*mock_impl, AddObserver(_));
  transferred_track_->SetImplementation(mock_impl);
}

}  // namespace blink
