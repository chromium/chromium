// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/browser_capture_media_stream_track.h"

#include "base/functional/callback_helpers.h"
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
#include "third_party/blink/renderer/modules/mediastream/sub_capture_target.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/region_capture_crop_id.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

using ::testing::_;
using ::testing::Args;
using ::testing::Mock;
using ::testing::Return;

std::unique_ptr<MockMediaStreamVideoSource> MakeMockMediaStreamVideoSource() {
  // TODO(crbug.com/1488083): Remove the NiceMock and explicitly expect
  // only truly expected calls.
  return base::WrapUnique(new ::testing::NiceMock<MockMediaStreamVideoSource>(
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

class BrowserCaptureMediaStreamTrackTest
    : public testing::Test,
      public testing::WithParamInterface<SubCaptureTarget::Type> {
 public:
  BrowserCaptureMediaStreamTrackTest() : type_(GetParam()) {}
  ~BrowserCaptureMediaStreamTrackTest() override = default;

  ScriptPromise<IDLUndefined> ApplySubCaptureTarget(
      V8TestingScope& v8_scope,
      BrowserCaptureMediaStreamTrack& track,
      String id_string) {
    switch (type_) {
      case SubCaptureTarget::Type::kCropTarget:
        return track.cropTo(
            v8_scope.GetScriptState(),
            MakeGarbageCollected<CropTarget>(std::move(id_string)),
            v8_scope.GetExceptionState());
      case SubCaptureTarget::Type::kRestrictionTarget:
        return track.restrictTo(
            v8_scope.GetScriptState(),
            MakeGarbageCollected<RestrictionTarget>(std::move(id_string)),
            v8_scope.GetExceptionState());
    }
    NOTREACHED();
  }

  void CheckHistograms(
      int expected_count,
      BrowserCaptureMediaStreamTrack::ApplySubCaptureTargetResult
          expected_result) {
    std::string uma_latency_name;
    std::string uma_result_name;
    switch (type_) {
      case SubCaptureTarget::Type::kCropTarget:
        uma_latency_name = "Media.RegionCapture.CropTo.Latency";
        uma_result_name = "Media.RegionCapture.CropTo.Result2";
        break;
      case SubCaptureTarget::Type::kRestrictionTarget:
        uma_latency_name = "Media.ElementCapture.RestrictTo.Latency";
        uma_result_name = "Media.ElementCapture.RestrictTo.Result";
        break;
    }

    histogram_tester_.ExpectTotalCount(uma_result_name, expected_count);
    histogram_tester_.ExpectUniqueSample(uma_result_name, expected_result,
                                         expected_count);
    histogram_tester_.ExpectTotalCount(uma_latency_name, expected_count);
  }

  void TearDown() override { WebHeap::CollectAllGarbageForTesting(); }

 protected:
  test::TaskEnvironment task_environment_;
  const SubCaptureTarget::Type type_;
  base::HistogramTester histogram_tester_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
};

INSTANTIATE_TEST_SUITE_P(
    _,
    BrowserCaptureMediaStreamTrackTest,
    testing::Values(SubCaptureTarget::Type::kCropTarget,
                    SubCaptureTarget::Type::kRestrictionTarget));

TEST_P(BrowserCaptureMediaStreamTrackTest,
       ApplySubCaptureTargetOnValidIdResultFirst) {
  V8TestingScope v8_scope;

  const base::Uuid valid_id = base::Uuid::GenerateRandomV4();

  std::unique_ptr<MockMediaStreamVideoSource> media_stream_video_source =
      MakeMockMediaStreamVideoSource();

  EXPECT_CALL(*media_stream_video_source, GetNextCaptureVersion)
      .Times(1)
      .WillOnce(Return(std::make_optional<media::CaptureVersion>(
          /*source=*/0, /*sub_capture=*/1)));

  EXPECT_CALL(*media_stream_video_source,
              ApplySubCaptureTarget(_, type_, GUIDToToken(valid_id), _, _))
      .Times(1)
      .WillOnce(::testing::WithArg<4>(
          [](base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
                 cb) {
            std::move(cb).Run(
                media::mojom::ApplySubCaptureTargetResult::kSuccess);
          }));

  BrowserCaptureMediaStreamTrack* const track =
      MakeTrack(v8_scope, std::move(media_stream_video_source));

  const auto promise = ApplySubCaptureTarget(
      v8_scope, *track, String(valid_id.AsLowercaseString()));

  track->OnCaptureVersionObservedForTesting(
      media::CaptureVersion(/*source=*/0, /*sub_capture=*/1));

  ScriptPromiseTester script_promise_tester(v8_scope.GetScriptState(), promise);
  script_promise_tester.WaitUntilSettled();
  EXPECT_TRUE(script_promise_tester.IsFulfilled());
  CheckHistograms(
      /*expected_count=*/1,
      BrowserCaptureMediaStreamTrack::ApplySubCaptureTargetResult::kOk);
}

TEST_P(BrowserCaptureMediaStreamTrackTest,
       ApplySubCaptureTargetRejectsIfResultFromBrowserProcessIsNotSuccess) {
  V8TestingScope v8_scope;

  const base::Uuid valid_id = base::Uuid::GenerateRandomV4();

  std::unique_ptr<MockMediaStreamVideoSource> media_stream_video_source =
      MakeMockMediaStreamVideoSource();

  EXPECT_CALL(*media_stream_video_source, GetNextCaptureVersion)
      .Times(1)
      .WillOnce(Return(std::make_optional<media::CaptureVersion>(
          /*source=*/0, /*sub_capture=*/1)));

  EXPECT_CALL(*media_stream_video_source,
              ApplySubCaptureTarget(_, type_, GUIDToToken(valid_id), _, _))
      .Times(1)
      .WillOnce(::testing::WithArg<4>(
          [](base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
                 cb) {
            std::move(cb).Run(
                media::mojom::ApplySubCaptureTargetResult::kErrorGeneric);
          }));

  BrowserCaptureMediaStreamTrack* const track =
      MakeTrack(v8_scope, std::move(media_stream_video_source));

  const auto promise = ApplySubCaptureTarget(
      v8_scope, *track, String(valid_id.AsLowercaseString()));

  track->OnCaptureVersionObservedForTesting(
      media::CaptureVersion(/*source=*/0, /*sub_capture=*/1));

  ScriptPromiseTester script_promise_tester(v8_scope.GetScriptState(), promise);
  script_promise_tester.WaitUntilSettled();
  EXPECT_TRUE(script_promise_tester.IsRejected());
  CheckHistograms(
      /*expected_count=*/1,
      BrowserCaptureMediaStreamTrack::ApplySubCaptureTargetResult::
          kRejectedWithErrorGeneric);
}

TEST_P(
    BrowserCaptureMediaStreamTrackTest,
    ApplySubCaptureTargetRejectsIfSourceReturnsNulloptForNextCaptureVersion) {
  V8TestingScope v8_scope;

  const base::Uuid valid_id = base::Uuid::GenerateRandomV4();

  std::unique_ptr<MockMediaStreamVideoSource> media_stream_video_source =
      MakeMockMediaStreamVideoSource();

  EXPECT_CALL(*media_stream_video_source, GetNextCaptureVersion)
      .Times(1)
      .WillOnce(Return(std::nullopt));

  EXPECT_CALL(*media_stream_video_source,
              ApplySubCaptureTarget(_, type_, GUIDToToken(valid_id), _, _))
      .Times(0);

  BrowserCaptureMediaStreamTrack* const track =
      MakeTrack(v8_scope, std::move(media_stream_video_source));

  const auto promise = ApplySubCaptureTarget(
      v8_scope, *track, String(valid_id.AsLowercaseString()));

  ScriptPromiseTester script_promise_tester(v8_scope.GetScriptState(), promise);
  script_promise_tester.WaitUntilSettled();
  EXPECT_TRUE(script_promise_tester.IsRejected());
  CheckHistograms(
      /*expected_count=*/1, BrowserCaptureMediaStreamTrack::
                                ApplySubCaptureTargetResult::kInvalidTarget);
}

TEST_P(BrowserCaptureMediaStreamTrackTest, CloningPreservesConstraints) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockMediaStreamVideoSource> media_stream_video_source =
      MakeMockMediaStreamVideoSource();

  EXPECT_CALL(*media_stream_video_source,
              ApplySubCaptureTarget(_, type_, _, _, _))
      .Times(0);

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

TEST_P(BrowserCaptureMediaStreamTrackTest,
       CloneAllowsIndependentSubCaptureTargets) {
  V8TestingScope v8_scope;

  const base::Uuid valid_id1 = base::Uuid::GenerateRandomV4();
  const base::Uuid valid_id2 = base::Uuid::GenerateRandomV4();

  std::unique_ptr<MockMediaStreamVideoSource> media_stream_video_source =
      MakeMockMediaStreamVideoSource();

  if (type_ == SubCaptureTarget::Type::kCropTarget) {
    EXPECT_CALL(*media_stream_video_source, GetNextCaptureVersion)
        .Times(2)
        .WillOnce(Return(std::make_optional<media::CaptureVersion>(
            /*source=*/0, /*sub_capture=*/1)))
        .WillOnce(Return(std::make_optional<media::CaptureVersion>(
            /*source=*/0, /*sub_capture=*/2)));

    MockMediaStreamVideoSource* mock_source = media_stream_video_source.get();
    BrowserCaptureMediaStreamTrack* const track1 =
        MakeTrack(v8_scope, std::move(media_stream_video_source));
    BrowserCaptureMediaStreamTrack* const track2 =
        static_cast<BrowserCaptureMediaStreamTrack*>(
            track1->clone(v8_scope.GetExecutionContext()));

    MediaStreamVideoTrack* const native_track1 =
        MediaStreamVideoTrack::From(track1->Component());
    MediaStreamVideoTrack* const native_track2 =
        MediaStreamVideoTrack::From(track2->Component());

    EXPECT_CALL(*mock_source,
                ApplySubCaptureTarget(native_track1, type_,
                                      GUIDToToken(valid_id1), 1, _))
        .Times(1)
        .WillOnce(::testing::WithArg<4>(
            [](base::OnceCallback<void(
                   media::mojom::ApplySubCaptureTargetResult)> cb) {
              std::move(cb).Run(
                  media::mojom::ApplySubCaptureTargetResult::kSuccess);
            }));

    EXPECT_CALL(*mock_source,
                ApplySubCaptureTarget(native_track2, type_,
                                      GUIDToToken(valid_id2), 2, _))
        .Times(1)
        .WillOnce(::testing::WithArg<4>(
            [](base::OnceCallback<void(
                   media::mojom::ApplySubCaptureTargetResult)> cb) {
              std::move(cb).Run(
                  media::mojom::ApplySubCaptureTargetResult::kSuccess);
            }));

    const auto promise1 = ApplySubCaptureTarget(
        v8_scope, *track1, String(valid_id1.AsLowercaseString()));
    const auto promise2 = ApplySubCaptureTarget(
        v8_scope, *track2, String(valid_id2.AsLowercaseString()));

    track1->OnCaptureVersionObservedForTesting(
        media::CaptureVersion(/*source=*/0, /*sub_capture=*/1));
    track2->OnCaptureVersionObservedForTesting(
        media::CaptureVersion(/*source=*/0, /*sub_capture=*/2));

    ScriptPromiseTester tester1(v8_scope.GetScriptState(), promise1);
    tester1.WaitUntilSettled();
    EXPECT_TRUE(tester1.IsFulfilled());

    ScriptPromiseTester tester2(v8_scope.GetScriptState(), promise2);
    tester2.WaitUntilSettled();
    EXPECT_TRUE(tester2.IsFulfilled());
  } else {
    // For Element Capture (restrictTo), clones cannot change targets.
    EXPECT_CALL(*media_stream_video_source, GetNextCaptureVersion)
        .Times(1)
        .WillOnce(Return(std::nullopt));

    BrowserCaptureMediaStreamTrack* const track1 =
        MakeTrack(v8_scope, std::move(media_stream_video_source));
    BrowserCaptureMediaStreamTrack* const track2 =
        static_cast<BrowserCaptureMediaStreamTrack*>(
            track1->clone(v8_scope.GetExecutionContext()));

    const auto promise = ApplySubCaptureTarget(
        v8_scope, *track2, String(valid_id1.AsLowercaseString()));

    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsRejected());
  }
}

}  // namespace blink
