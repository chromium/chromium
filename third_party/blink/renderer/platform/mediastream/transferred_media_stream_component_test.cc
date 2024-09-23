// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/transferred_media_stream_component.h"

#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"

namespace blink {
using ::testing::Return;

class MockSourceObserver : public GarbageCollected<MockSourceObserver>,
                           public MediaStreamSource::Observer {
 public:
  MOCK_METHOD0(SourceChangedState, void());
  MOCK_METHOD0(SourceChangedCaptureConfiguration, void());
  MOCK_METHOD0(SourceChangedCaptureHandle, void());
  MOCK_METHOD1(SourceChangedZoomLevel, void(int));
};

// TODO(crbug.com/1288839): Move this mock out into a share place.
class MockMediaStreamComponent
    : public GarbageCollected<MockMediaStreamComponent>,
      public MediaStreamComponent {
 public:
  virtual ~MockMediaStreamComponent() = default;
  MOCK_CONST_METHOD0(Clone, MediaStreamComponent*());
  MOCK_CONST_METHOD0(Source, MediaStreamSource*());
  MOCK_CONST_METHOD0(Id, String());
  MOCK_CONST_METHOD0(UniqueId, int());
  MOCK_CONST_METHOD0(GetSourceType, MediaStreamSource::StreamType());
  MOCK_CONST_METHOD0(GetSourceName, const String&());
  MOCK_CONST_METHOD0(GetReadyState, MediaStreamSource::ReadyState());
  MOCK_CONST_METHOD0(Remote, bool());
  MOCK_CONST_METHOD0(Enabled, bool());
  MOCK_METHOD1(SetEnabled, void(bool));
  MOCK_METHOD0(ContentHint, WebMediaStreamTrack::ContentHintType());
  MOCK_METHOD1(SetContentHint, void(WebMediaStreamTrack::ContentHintType));
  MOCK_CONST_METHOD0(GetPlatformTrack, MediaStreamTrackPlatform*());
  MOCK_METHOD1(SetPlatformTrack,
               void(std::unique_ptr<MediaStreamTrackPlatform>));
  MOCK_METHOD1(GetSettings, void(MediaStreamTrackPlatform::Settings&));
  MOCK_METHOD0(GetCaptureHandle, MediaStreamTrackPlatform::CaptureHandle());
  MOCK_METHOD0(CreationFrame, WebLocalFrame*());
  MOCK_METHOD1(SetCreationFrameGetter,
               void(base::RepeatingCallback<WebLocalFrame*()>));
  MOCK_METHOD1(AddSourceObserver, void(MediaStreamSource::Observer*));
  MOCK_METHOD1(AddSink, void(WebMediaStreamAudioSink*));
  MOCK_METHOD4(AddSink,
               void(WebMediaStreamSink*,
                    const VideoCaptureDeliverFrameCB&,
                    MediaStreamVideoSink::IsSecure,
                    MediaStreamVideoSink::UsesAlpha));
  MOCK_CONST_METHOD0(ToString, String());
};

class TransferredMediaStreamComponentTest : public testing::Test {
 public:
  void SetUp() override {
    transferred_component_ =
        MakeGarbageCollected<TransferredMediaStreamComponent>(
            TransferredMediaStreamComponent::TransferredValues{.id = "id"});

    component_ = MakeGarbageCollected<MockMediaStreamComponent>();
    EXPECT_CALL(*component_, GetCaptureHandle())
        .WillRepeatedly(Return(MediaStreamTrackPlatform::CaptureHandle()));
    EXPECT_CALL(*component_, GetReadyState())
        .WillRepeatedly(Return(MediaStreamSource::kReadyStateLive));
  }

  void TearDown() override { WebHeap::CollectAllGarbageForTesting(); }

  Persistent<TransferredMediaStreamComponent> transferred_component_;
  Persistent<MockMediaStreamComponent> component_;

  base::test::TaskEnvironment task_environment_;
};

TEST_F(TransferredMediaStreamComponentTest, InitialProperties) {
  EXPECT_EQ(transferred_component_->Id(), "id");
}

TEST_F(TransferredMediaStreamComponentTest, AddingObserver) {
  MockSourceObserver* observer = MakeGarbageCollected<MockSourceObserver>();

  transferred_component_->AddSourceObserver(observer);

  EXPECT_CALL(*component_, AddSourceObserver(observer));
  transferred_component_->SetImplementation(component_);
}

TEST_F(TransferredMediaStreamComponentTest,
       ObserverStateChangeFiresWhenSettingImplementation) {
  MockSourceObserver* observer = MakeGarbageCollected<MockSourceObserver>();

  transferred_component_->AddSourceObserver(observer);
  ASSERT_EQ(transferred_component_->GetReadyState(),
            MediaStreamSource::kReadyStateLive);

  EXPECT_CALL(*component_, GetReadyState())
      .WillRepeatedly(Return(MediaStreamSource::kReadyStateMuted));

  EXPECT_CALL(*component_, AddSourceObserver(observer));
  EXPECT_CALL(*observer, SourceChangedState());

  transferred_component_->SetImplementation(component_);
}

TEST_F(TransferredMediaStreamComponentTest,
       ObserverCaptureHandleChangeFiresWhenSettingImplementation) {
  MockSourceObserver* observer = MakeGarbageCollected<MockSourceObserver>();

  transferred_component_->AddSourceObserver(observer);
  ASSERT_TRUE(transferred_component_->GetCaptureHandle().IsEmpty());

  EXPECT_CALL(*component_, GetCaptureHandle())
      .WillRepeatedly(
          Return(MediaStreamTrackPlatform::CaptureHandle{.handle = "handle"}));
  EXPECT_CALL(*component_, AddSourceObserver(observer));
  EXPECT_CALL(*observer, SourceChangedCaptureHandle());

  transferred_component_->SetImplementation(component_);
}

}  // namespace blink
