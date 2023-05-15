// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/browser_capture_media_stream_track.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_long_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constrainlongrange_long.h"
#include "third_party/blink/renderer/modules/mediastream/crop_target.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/region_capture_crop_id.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"

namespace blink {

namespace {

using ::testing::_;
using ::testing::Args;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;

std::unique_ptr<MockMediaStreamVideoSource> MakeMockMediaStreamVideoSource() {
  return base::WrapUnique(new MockMediaStreamVideoSource(
      media::VideoCaptureFormat(gfx::Size(640, 480), 30.0,
                                media::PIXEL_FORMAT_I420),
      true));
}

BrowserCaptureMediaStreamTrack* MakeTrack(
    V8TestingScope& v8_scope,
    std::unique_ptr<MockMediaStreamVideoSource> media_stream_video_source) {
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

  return MakeGarbageCollected<BrowserCaptureMediaStreamTrack>(
      v8_scope.GetExecutionContext(), component,
      /*callback=*/base::DoNothing());
}

}  // namespace

class BrowserCaptureMediaStreamTrackTest : public testing::Test {
 public:
  ~BrowserCaptureMediaStreamTrackTest() override = default;

  void CheckHistograms(
      int expected_count,
      BrowserCaptureMediaStreamTrack::CropToResult expected_result) {
    histogram_tester_.ExpectTotalCount("Media.RegionCapture.CropTo.Result",
                                       expected_count);
    histogram_tester_.ExpectUniqueSample("Media.RegionCapture.CropTo.Result",
                                         expected_result, expected_count);
    histogram_tester_.ExpectTotalCount("Media.RegionCapture.CropTo.Latency",
                                       expected_count);
  }

  void TearDown() override { WebHeap::CollectAllGarbageForTesting(); }

 protected:
  base::HistogramTester histogram_tester_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
};

#if !BUILDFLAG(IS_ANDROID)
TEST_F(BrowserCaptureMediaStreamTrackTest, CropToOnValidIdResultFirst) {
  V8TestingScope v8_scope;

  const base::Uuid valid_id = base::Uuid::GenerateRandomV4();

  std::unique_ptr<MockMediaStreamVideoSource> media_stream_video_source =
      MakeMockMediaStreamVideoSource();

  EXPECT_CALL(*media_stream_video_source, GetNextCropVersion)
      .Times(1)
      .WillOnce(Return(absl::optional<uint32_t>(1)));

  EXPECT_CALL(*media_stream_video_source, Crop(GUIDToToken(valid_id), _, _))
      .Times(1)
      .WillOnce(::testing::WithArg<2>(::testing::Invoke(
          [](base::OnceCallback<void(media::mojom::CropRequestResult)> cb) {
            std::move(cb).Run(media::mojom::CropRequestResult::kSuccess);
          })));

  BrowserCaptureMediaStreamTrack* const track =
      MakeTrack(v8_scope, std::move(media_stream_video_source));

  const ScriptPromise promise =
      track->cropTo(v8_scope.GetScriptState(),
                    MakeGarbageCollected<CropTarget>(
                        WTF::String(valid_id.AsLowercaseString())),
                    v8_scope.GetExceptionState());

  track->OnCropVersionObservedForTesting(/*crop_version=*/1);

  ScriptPromiseTester script_promise_tester(v8_scope.GetScriptState(), promise);
  script_promise_tester.WaitUntilSettled();
  EXPECT_TRUE(script_promise_tester.IsFulfilled());
  CheckHistograms(
      /*expected_count=*/1, BrowserCaptureMediaStreamTrack::CropToResult::kOk);
}

TEST_F(BrowserCaptureMediaStreamTrackTest,
       CropToRejectsIfResultFromBrowserProcessIsNotSuccess) {
  V8TestingScope v8_scope;

  const base::Uuid valid_id = base::Uuid::GenerateRandomV4();

  std::unique_ptr<MockMediaStreamVideoSource> media_stream_video_source =
      MakeMockMediaStreamVideoSource();

  EXPECT_CALL(*media_stream_video_source, GetNextCropVersion)
      .Times(1)
      .WillOnce(Return(absl::optional<uint32_t>(1)));

  EXPECT_CALL(*media_stream_video_source, Crop(GUIDToToken(valid_id), _, _))
      .Times(1)
      .WillOnce(::testing::WithArg<2>(::testing::Invoke(
          [](base::OnceCallback<void(media::mojom::CropRequestResult)> cb) {
            std::move(cb).Run(media::mojom::CropRequestResult::kErrorGeneric);
          })));

  BrowserCaptureMediaStreamTrack* const track =
      MakeTrack(v8_scope, std::move(media_stream_video_source));

  const ScriptPromise promise =
      track->cropTo(v8_scope.GetScriptState(),
                    MakeGarbageCollected<CropTarget>(
                        WTF::String(valid_id.AsLowercaseString())),
                    v8_scope.GetExceptionState());

  track->OnCropVersionObservedForTesting(/*crop_version=*/1);

  ScriptPromiseTester script_promise_tester(v8_scope.GetScriptState(), promise);
  script_promise_tester.WaitUntilSettled();
  EXPECT_TRUE(script_promise_tester.IsRejected());
  CheckHistograms(
      /*expected_count=*/1,
      BrowserCaptureMediaStreamTrack::CropToResult::kRejectedWithErrorGeneric);
}

TEST_F(BrowserCaptureMediaStreamTrackTest,
       CropToRejectsIfSourceReturnsNulloptForNextCropVersion) {
  V8TestingScope v8_scope;

  const base::Uuid valid_id = base::Uuid::GenerateRandomV4();

  std::unique_ptr<MockMediaStreamVideoSource> media_stream_video_source =
      MakeMockMediaStreamVideoSource();

  EXPECT_CALL(*media_stream_video_source, GetNextCropVersion)
      .Times(1)
      .WillOnce(Return(absl::nullopt));

  EXPECT_CALL(*media_stream_video_source, Crop(GUIDToToken(valid_id), _, _))
      .Times(0);

  BrowserCaptureMediaStreamTrack* const track =
      MakeTrack(v8_scope, std::move(media_stream_video_source));

  const ScriptPromise promise =
      track->cropTo(v8_scope.GetScriptState(),
                    MakeGarbageCollected<CropTarget>(
                        WTF::String(valid_id.AsLowercaseString())),
                    v8_scope.GetExceptionState());

  ScriptPromiseTester script_promise_tester(v8_scope.GetScriptState(), promise);
  script_promise_tester.WaitUntilSettled();
  EXPECT_TRUE(script_promise_tester.IsRejected());
  CheckHistograms(
      /*expected_count=*/1,
      BrowserCaptureMediaStreamTrack::CropToResult::kInvalidCropTarget);
}

#else

TEST_F(BrowserCaptureMediaStreamTrackTest, CropToFailsOnAndroid) {
  V8TestingScope v8_scope;

  const base::Uuid valid_id = base::Uuid::GenerateRandomV4();

  std::unique_ptr<MockMediaStreamVideoSource> media_stream_video_source =
      MakeMockMediaStreamVideoSource();

  EXPECT_CALL(*media_stream_video_source, Crop(_, _, _)).Times(0);

  BrowserCaptureMediaStreamTrack* const track =
      MakeTrack(v8_scope, std::move(media_stream_video_source));

  const ScriptPromise promise =
      track->cropTo(v8_scope.GetScriptState(),
                    MakeGarbageCollected<CropTarget>(
                        WTF::String(valid_id.AsLowercaseString())),
                    v8_scope.GetExceptionState());

  ScriptPromiseTester script_promise_tester(v8_scope.GetScriptState(), promise);
  script_promise_tester.WaitUntilSettled();
  EXPECT_TRUE(script_promise_tester.IsRejected());
  CheckHistograms(
      /*expected_count=*/1,
      BrowserCaptureMediaStreamTrack::CropToResult::kUnsupportedPlatform);
}
#endif

TEST_F(BrowserCaptureMediaStreamTrackTest, CloningPreservesConstraints) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockMediaStreamVideoSource> media_stream_video_source =
      MakeMockMediaStreamVideoSource();

  EXPECT_CALL(*media_stream_video_source, Crop(_, _, _)).Times(0);

  BrowserCaptureMediaStreamTrack* const track =
      MakeTrack(v8_scope, std::move(media_stream_video_source));

  MediaConstraints constraints;
  MediaTrackConstraintSetPlatform basic;
  basic.width.SetMax(240);
  constraints.Initialize(basic, Vector<MediaTrackConstraintSetPlatform>());
  track->SetInitialConstraints(constraints);

  MediaStreamTrack* clone = track->clone(v8_scope.GetExecutionContext());
  MediaTrackConstraints* clone_constraints = clone->getConstraints();
  EXPECT_TRUE(clone_constraints->hasWidth());
  EXPECT_EQ(clone_constraints->width()->GetAsConstrainLongRange()->max(), 240);
}

}  // namespace blink
