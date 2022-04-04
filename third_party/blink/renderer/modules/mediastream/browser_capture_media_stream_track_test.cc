// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/browser_capture_media_stream_track.h"

#include "base/guid.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/region_capture_crop_id.h"

namespace blink {

namespace {

using ::testing::_;

std::unique_ptr<MockMediaStreamVideoSource> MakeMockMediaStreamVideoSource() {
  return base::WrapUnique(new MockMediaStreamVideoSource(
      media::VideoCaptureFormat(gfx::Size(640, 480), 30.0,
                                media::PIXEL_FORMAT_I420),
      true));
}

BrowserCaptureMediaStreamTrack* MakeTrack(
    V8TestingScope& v8_scope,
    std::unique_ptr<MockMediaStreamVideoSource> media_stream_video_source) {
  MediaStreamSource* const source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::StreamType::kTypeVideo, "name",
      /*remote=*/false, std::move(media_stream_video_source));

  MediaStreamComponent* const component =
      MakeGarbageCollected<MediaStreamComponent>(source);

  return MakeGarbageCollected<BrowserCaptureMediaStreamTrack>(
      v8_scope.GetExecutionContext(), component, /*callback=*/base::DoNothing(),
      "descriptor");
}

}  // namespace

class BrowserCaptureMediaStreamTrackTest : public testing::Test {
 public:
  ~BrowserCaptureMediaStreamTrackTest() override {
    WebHeap::CollectAllGarbageForTesting();
  }
};

#if !BUILDFLAG(IS_ANDROID)
TEST_F(BrowserCaptureMediaStreamTrackTest, CropToOnValidId) {
  V8TestingScope v8_scope;

  const base::GUID valid_id = base::GUID::GenerateRandomV4();

  std::unique_ptr<MockMediaStreamVideoSource> media_stream_video_source =
      MakeMockMediaStreamVideoSource();

  EXPECT_CALL(*media_stream_video_source, Crop(GUIDToToken(valid_id), _, _))
      .Times(1);

  BrowserCaptureMediaStreamTrack* const track =
      MakeTrack(v8_scope, std::move(media_stream_video_source));

  const ScriptPromise promise = track->cropTo(
      v8_scope.GetScriptState(), WTF::String(valid_id.AsLowercaseString()),
      v8_scope.GetExceptionState());
}

TEST_F(BrowserCaptureMediaStreamTrackTest, CropToInvalidIdIsRejected) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockMediaStreamVideoSource> media_stream_video_source =
      MakeMockMediaStreamVideoSource();

  EXPECT_CALL(*media_stream_video_source, Crop(_, _, _)).Times(0);

  BrowserCaptureMediaStreamTrack* const track =
      MakeTrack(v8_scope, std::move(media_stream_video_source));

  const ScriptPromise promise =
      track->cropTo(v8_scope.GetScriptState(), WTF::String("INVALID-ID"),
                    v8_scope.GetExceptionState());
  EXPECT_EQ(promise.V8Promise()->State(), v8::Promise::kRejected);
}

#else

TEST_F(BrowserCaptureMediaStreamTrackTest, CropToFailsOnAndroid) {
  V8TestingScope v8_scope;

  const base::GUID valid_id = base::GUID::GenerateRandomV4();

  std::unique_ptr<MockMediaStreamVideoSource> media_stream_video_source =
      MakeMockMediaStreamVideoSource();

  EXPECT_CALL(*media_stream_video_source, Crop(_, _, _)).Times(0);

  BrowserCaptureMediaStreamTrack* const track =
      MakeTrack(v8_scope, std::move(media_stream_video_source));

  const ScriptPromise promise = track->cropTo(
      v8_scope.GetScriptState(), WTF::String(valid_id.AsLowercaseString()),
      v8_scope.GetExceptionState());
  EXPECT_EQ(promise.V8Promise()->State(), v8::Promise::kRejected);
}
#endif

}  // namespace blink
