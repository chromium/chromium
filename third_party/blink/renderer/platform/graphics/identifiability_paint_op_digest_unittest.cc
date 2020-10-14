// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/identifiability_paint_op_digest.h"

#include "cc/test/skia_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings_provider.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkTileMode.h"

namespace blink {

namespace {

// A IdentifiabilityStudySettingsProvider implementation that opts-into study
// participation.
class ActiveSettingsProvider : public IdentifiabilityStudySettingsProvider {
 public:
  bool IsActive() const override { return true; }
  bool IsAnyTypeOrSurfaceBlocked() const override { return false; }
  bool IsSurfaceAllowed(IdentifiableSurface surface) const override {
    return true;
  }
  bool IsTypeAllowed(IdentifiableSurface::Type type) const override {
    return true;
  }
};

// An RAII class that opts into study participation using
// ActiveSettingsProvider.
class StudyParticipationRaii {
 public:
  StudyParticipationRaii() {
    IdentifiabilityStudySettings::SetGlobalProvider(
        std::make_unique<ActiveSettingsProvider>());
  }
  ~StudyParticipationRaii() {
    IdentifiabilityStudySettings::ResetStateForTesting();
  }
};

// Arbitrary non-zero size.
constexpr IntSize kSize(10, 10);

constexpr float kScaleX = 1.0f, kScaleY = 1.0f;
constexpr int64_t kScaleDigest = INT64_C(8258647715449129112);
constexpr int64_t kTokenBuilderInitialDigest = INT64_C(6544625333304541877);

TEST(IdentifiabilityPaintOpDigestTest, Construct) {
  IdentifiabilityPaintOpDigest identifiability_paintop_digest(kSize);
}

TEST(IdentifiabilityPaintOpDigestTest, Construct_InStudy) {
  StudyParticipationRaii study_participation_raii;
  IdentifiabilityPaintOpDigest identifiability_paintop_digest(kSize);
}

TEST(IdentifiabilityPaintOpDigestTest, InitialDigest) {
  StudyParticipationRaii study_participation_raii;
  IdentifiabilityPaintOpDigest identifiability_paintop_digest(kSize);
  auto paint_record = sk_make_sp<cc::PaintRecord>();
  identifiability_paintop_digest.MaybeUpdateDigest(paint_record,
                                                   /*num_ops_to_visit=*/1);
  EXPECT_EQ(kTokenBuilderInitialDigest,
            identifiability_paintop_digest.GetToken().ToUkmMetricValue());

  EXPECT_FALSE(identifiability_paintop_digest.encountered_skipped_ops());
  EXPECT_FALSE(
      identifiability_paintop_digest.encountered_partially_digested_image());
}

TEST(IdentifiabilityPaintOpDigestTest, SimpleDigest) {
  StudyParticipationRaii study_participation_raii;
  IdentifiabilityPaintOpDigest identifiability_paintop_digest(kSize);
  auto paint_record = sk_make_sp<cc::PaintRecord>();
  paint_record->push<cc::ScaleOp>(kScaleX, kScaleY);
  identifiability_paintop_digest.MaybeUpdateDigest(paint_record,
                                                   /*num_ops_to_visit=*/1);
  EXPECT_EQ(kScaleDigest,
            identifiability_paintop_digest.GetToken().ToUkmMetricValue());

  EXPECT_FALSE(identifiability_paintop_digest.encountered_skipped_ops());
  EXPECT_FALSE(
      identifiability_paintop_digest.encountered_partially_digested_image());
}

TEST(IdentifiabilityPaintOpDigestTest, DigestIsInitialIfNotInStudy) {
  IdentifiabilityPaintOpDigest identifiability_paintop_digest(kSize);
  auto paint_record = sk_make_sp<cc::PaintRecord>();
  paint_record->push<cc::ScaleOp>(1.0f, 1.0f);
  identifiability_paintop_digest.MaybeUpdateDigest(paint_record,
                                                   /*num_ops_to_visit=*/1);
  EXPECT_EQ(kTokenBuilderInitialDigest,
            identifiability_paintop_digest.GetToken().ToUkmMetricValue());

  EXPECT_FALSE(identifiability_paintop_digest.encountered_skipped_ops());
  EXPECT_FALSE(
      identifiability_paintop_digest.encountered_partially_digested_image());
}

TEST(IdentifiabilityPaintOpDigestTest, IgnoresTextOpsContents) {
  StudyParticipationRaii study_participation_raii;
  IdentifiabilityPaintOpDigest identifiability_paintop_digest1(kSize);
  IdentifiabilityPaintOpDigest identifiability_paintop_digest2(kSize);
  auto paint_record1 = sk_make_sp<cc::PaintRecord>();
  auto paint_record2 = sk_make_sp<cc::PaintRecord>();
  paint_record1->push<cc::DrawTextBlobOp>(
      SkTextBlob::MakeFromString("abc", SkFont(SkTypeface::MakeDefault())),
      1.0f, 1.0f, cc::PaintFlags());
  paint_record2->push<cc::DrawTextBlobOp>(
      SkTextBlob::MakeFromString("def", SkFont(SkTypeface::MakeDefault())),
      2.0f, 2.0f, cc::PaintFlags());
  identifiability_paintop_digest1.MaybeUpdateDigest(paint_record1,
                                                    /*num_ops_to_visit=*/1);
  identifiability_paintop_digest2.MaybeUpdateDigest(paint_record2,
                                                    /*num_ops_to_visit=*/1);
  EXPECT_EQ(identifiability_paintop_digest1.GetToken().ToUkmMetricValue(),
            identifiability_paintop_digest2.GetToken().ToUkmMetricValue());
  EXPECT_NE(kTokenBuilderInitialDigest,
            identifiability_paintop_digest1.GetToken().ToUkmMetricValue());
  EXPECT_EQ(INT64_C(5364951310489041526),
            identifiability_paintop_digest1.GetToken().ToUkmMetricValue());

  EXPECT_FALSE(identifiability_paintop_digest1.encountered_skipped_ops());
  EXPECT_FALSE(
      identifiability_paintop_digest1.encountered_partially_digested_image());
  EXPECT_FALSE(identifiability_paintop_digest2.encountered_skipped_ops());
  EXPECT_FALSE(
      identifiability_paintop_digest2.encountered_partially_digested_image());
}

TEST(IdentifiabilityPaintOpDigestTest, RelativeOrderingTextOpsAndOtherOps) {
  StudyParticipationRaii study_participation_raii;
  IdentifiabilityPaintOpDigest identifiability_paintop_digest1(kSize);
  IdentifiabilityPaintOpDigest identifiability_paintop_digest2(kSize);
  auto paint_record1 = sk_make_sp<cc::PaintRecord>();
  auto paint_record2 = sk_make_sp<cc::PaintRecord>();
  paint_record1->push<cc::DrawTextBlobOp>(
      SkTextBlob::MakeFromString("abc", SkFont(SkTypeface::MakeDefault())),
      1.0f, 1.0f, cc::PaintFlags());
  paint_record1->push<cc::ScaleOp>(kScaleX, kScaleY);
  paint_record2->push<cc::ScaleOp>(kScaleX, kScaleY);
  paint_record2->push<cc::DrawTextBlobOp>(
      SkTextBlob::MakeFromString("abc", SkFont(SkTypeface::MakeDefault())),
      1.0f, 1.0f, cc::PaintFlags());
  identifiability_paintop_digest1.MaybeUpdateDigest(paint_record1,
                                                    /*num_ops_to_visit=*/1);
  identifiability_paintop_digest2.MaybeUpdateDigest(paint_record2,
                                                    /*num_ops_to_visit=*/1);
  EXPECT_NE(identifiability_paintop_digest1.GetToken().ToUkmMetricValue(),
            identifiability_paintop_digest2.GetToken().ToUkmMetricValue());

  EXPECT_FALSE(identifiability_paintop_digest1.encountered_skipped_ops());
  EXPECT_FALSE(
      identifiability_paintop_digest1.encountered_partially_digested_image());
  EXPECT_FALSE(identifiability_paintop_digest2.encountered_skipped_ops());
  EXPECT_FALSE(
      identifiability_paintop_digest2.encountered_partially_digested_image());
}

TEST(IdentifiabilityPaintOpDigestTest, SkPathDigestStability) {
  StudyParticipationRaii study_participation_raii;
  // These 2 SkPath objects will have different internal generation IDs. We
  // can't use empty paths as in that case the internal SkPathRefs (which holds
  // the ID) for each SkPath would be the same global "empty" SkPathRef, so we
  // make the SkPaths non-empty.
  SkPath path1;
  path1.rLineTo(1.0f, 0.0f);
  SkPath path2;
  path2.rLineTo(1.0f, 0.0f);
  IdentifiabilityPaintOpDigest identifiability_paintop_digest1(kSize);
  IdentifiabilityPaintOpDigest identifiability_paintop_digest2(kSize);
  auto paint_record1 = sk_make_sp<cc::PaintRecord>();
  auto paint_record2 = sk_make_sp<cc::PaintRecord>();
  paint_record1->push<cc::DrawPathOp>(path1, cc::PaintFlags());
  paint_record2->push<cc::DrawPathOp>(path2, cc::PaintFlags());
  identifiability_paintop_digest1.MaybeUpdateDigest(paint_record1,
                                                    /*num_ops_to_visit=*/1);
  identifiability_paintop_digest2.MaybeUpdateDigest(paint_record2,
                                                    /*num_ops_to_visit=*/1);
  EXPECT_EQ(identifiability_paintop_digest1.GetToken().ToUkmMetricValue(),
            identifiability_paintop_digest2.GetToken().ToUkmMetricValue());
  EXPECT_EQ(INT64_C(-1093634256342000670),
            identifiability_paintop_digest1.GetToken().ToUkmMetricValue());

  EXPECT_FALSE(identifiability_paintop_digest1.encountered_skipped_ops());
  EXPECT_FALSE(
      identifiability_paintop_digest1.encountered_partially_digested_image());
  EXPECT_FALSE(identifiability_paintop_digest2.encountered_skipped_ops());
  EXPECT_FALSE(
      identifiability_paintop_digest2.encountered_partially_digested_image());
}

TEST(IdentifiabilityPaintOpDigestTest, PaintShaderStability) {
  StudyParticipationRaii study_participation_raii;
  SkPath path;
  path.rLineTo(1.0f, 0.0f);
  IdentifiabilityPaintOpDigest identifiability_paintop_digest1(kSize);
  IdentifiabilityPaintOpDigest identifiability_paintop_digest2(kSize);
  auto paint_record1 = sk_make_sp<cc::PaintRecord>();
  auto paint_record2 = sk_make_sp<cc::PaintRecord>();
  auto paint_record_shader = sk_make_sp<cc::PaintRecord>();
  paint_record_shader->push<cc::ScaleOp>(2.0f, 2.0f);
  const SkRect tile = SkRect::MakeWH(100, 100);
  // These 2 shaders will have different internal generation IDs.
  cc::PaintFlags paint_flags1;
  paint_flags1.setShader(cc::PaintShader::MakePaintRecord(
      paint_record_shader, tile, SkTileMode::kClamp, SkTileMode::kClamp,
      nullptr));
  cc::PaintFlags paint_flags2;
  paint_flags2.setShader(cc::PaintShader::MakePaintRecord(
      paint_record_shader, tile, SkTileMode::kClamp, SkTileMode::kClamp,
      nullptr));
  paint_record1->push<cc::DrawPathOp>(path, paint_flags1);
  paint_record2->push<cc::DrawPathOp>(path, paint_flags2);
  identifiability_paintop_digest1.MaybeUpdateDigest(paint_record1,
                                                    /*num_ops_to_visit=*/1);
  identifiability_paintop_digest2.MaybeUpdateDigest(paint_record2,
                                                    /*num_ops_to_visit=*/1);
  EXPECT_EQ(identifiability_paintop_digest1.GetToken().ToUkmMetricValue(),
            identifiability_paintop_digest2.GetToken().ToUkmMetricValue());
  EXPECT_EQ(INT64_C(6157094048912696853),
            identifiability_paintop_digest1.GetToken().ToUkmMetricValue());
}

TEST(IdentifiabilityPaintOpDigestTest, BufferLeftoversDontAffectFutureDigests) {
  StudyParticipationRaii study_participation_raii;
  // Make a complex path to make a PaintOp with a large serialization.
  SkPath path;
  path.rLineTo(1.0f, 0.0f);
  path.rLineTo(1.0f, 1.0f);
  path.rLineTo(0.0f, 1.0f);
  path.rLineTo(2.0f, 0.0f);
  path.rLineTo(2.0f, 2.0f);
  path.rLineTo(0.0f, 2.0f);
  path.rLineTo(1.0f, 0.0f);
  IdentifiabilityPaintOpDigest identifiability_paintop_digest1(kSize);
  IdentifiabilityPaintOpDigest identifiability_paintop_digest2(kSize);
  auto paint_record1 = sk_make_sp<cc::PaintRecord>();
  auto paint_record2 = sk_make_sp<cc::PaintRecord>();
  paint_record1->push<cc::DrawPathOp>(path, cc::PaintFlags());
  paint_record2->push<cc::ScaleOp>(kScaleX, kScaleY);
  identifiability_paintop_digest1.MaybeUpdateDigest(paint_record1,
                                                    /*num_ops_to_visit=*/1);
  identifiability_paintop_digest2.MaybeUpdateDigest(paint_record2,
                                                    /*num_ops_to_visit=*/1);
  EXPECT_EQ(INT64_C(-1855817800596177722),
            identifiability_paintop_digest1.GetToken().ToUkmMetricValue());
  EXPECT_EQ(kScaleDigest,
            identifiability_paintop_digest2.GetToken().ToUkmMetricValue());

  EXPECT_FALSE(identifiability_paintop_digest1.encountered_skipped_ops());
  EXPECT_FALSE(
      identifiability_paintop_digest1.encountered_partially_digested_image());
  EXPECT_FALSE(identifiability_paintop_digest2.encountered_skipped_ops());
  EXPECT_FALSE(
      identifiability_paintop_digest2.encountered_partially_digested_image());
}

TEST(IdentifiabilityPaintOpDigestTest,
     BufferLeftoversDontAffectFutureDigests_SameCanvas) {
  StudyParticipationRaii study_participation_raii;
  // Make a complex path to make a PaintOp with a large serialization.
  SkPath path;
  path.rLineTo(1.0f, 0.0f);
  path.rLineTo(1.0f, 1.0f);
  path.rLineTo(0.0f, 1.0f);
  path.rLineTo(2.0f, 0.0f);
  path.rLineTo(2.0f, 2.0f);
  path.rLineTo(0.0f, 2.0f);
  path.rLineTo(1.0f, 0.0f);
  IdentifiabilityPaintOpDigest identifiability_paintop_digest(kSize);
  auto paint_record = sk_make_sp<cc::PaintRecord>();
  paint_record->push<cc::DrawPathOp>(path, cc::PaintFlags());
  paint_record->push<cc::ScaleOp>(kScaleX, kScaleY);
  identifiability_paintop_digest.MaybeUpdateDigest(paint_record,
                                                   /*num_ops_to_visit=*/2);
  EXPECT_EQ(INT64_C(-2635322358402873102),
            identifiability_paintop_digest.GetToken().ToUkmMetricValue());

  EXPECT_FALSE(identifiability_paintop_digest.encountered_skipped_ops());
  EXPECT_FALSE(
      identifiability_paintop_digest.encountered_partially_digested_image());
}

TEST(IdentifiabilityPaintOpDigestTest, IgnoresPrefixAndSuffix) {
  StudyParticipationRaii study_participation_raii;
  SkPath path1;
  path1.rLineTo(1.0f, 0.0f);
  SkPath path2;
  path2.rLineTo(0.0f, 1.0f);
  IdentifiabilityPaintOpDigest identifiability_paintop_digest(kSize);
  auto paint_record = sk_make_sp<cc::PaintRecord>();
  paint_record->push<cc::DrawPathOp>(path1, cc::PaintFlags());
  paint_record->push<cc::ScaleOp>(kScaleX, kScaleY);
  paint_record->push<cc::DrawPathOp>(path2, cc::PaintFlags());
  identifiability_paintop_digest.SetPrefixSkipCount(1);
  identifiability_paintop_digest.MaybeUpdateDigest(paint_record,
                                                   /*num_ops_to_visit=*/2);
  EXPECT_EQ(kScaleDigest,
            identifiability_paintop_digest.GetToken().ToUkmMetricValue());

  EXPECT_FALSE(identifiability_paintop_digest.encountered_skipped_ops());
  EXPECT_FALSE(
      identifiability_paintop_digest.encountered_partially_digested_image());
}

TEST(IdentifiabilityPaintOpDigestTest, IgnoresPrefixAndSuffix_MultipleOps) {
  StudyParticipationRaii study_participation_raii;
  SkPath path1;
  path1.rLineTo(1.0f, 0.0f);
  SkPath path2;
  path2.rLineTo(0.0f, 1.0f);
  SkPath path3;
  path3.rLineTo(1.0f, 1.0f);
  IdentifiabilityPaintOpDigest identifiability_paintop_digest(kSize);
  auto paint_record = sk_make_sp<cc::PaintRecord>();
  paint_record->push<cc::DrawPathOp>(path1, cc::PaintFlags());
  paint_record->push<cc::ScaleOp>(kScaleX, kScaleY);
  paint_record->push<cc::DrawPathOp>(path2, cc::PaintFlags());
  paint_record->push<cc::DrawPathOp>(path3, cc::PaintFlags());
  identifiability_paintop_digest.SetPrefixSkipCount(1);
  identifiability_paintop_digest.MaybeUpdateDigest(paint_record,
                                                   /*num_ops_to_visit=*/2);
  EXPECT_EQ(INT64_C(8258647715449129112),
            identifiability_paintop_digest.GetToken().ToUkmMetricValue());

  EXPECT_FALSE(identifiability_paintop_digest.encountered_skipped_ops());
  EXPECT_FALSE(
      identifiability_paintop_digest.encountered_partially_digested_image());
}

TEST(IdentifiabilityPaintOpDigestTest, RecursesIntoDrawRecords) {
  StudyParticipationRaii study_participation_raii;
  IdentifiabilityPaintOpDigest identifiability_paintop_digest(kSize);
  auto paint_record_inner = sk_make_sp<cc::PaintRecord>();
  auto paint_record_outer = sk_make_sp<cc::PaintRecord>();
  paint_record_inner->push<cc::ScaleOp>(kScaleX, kScaleY);
  paint_record_outer->push<cc::DrawRecordOp>(paint_record_inner);
  identifiability_paintop_digest.MaybeUpdateDigest(paint_record_outer,
                                                   /*num_ops_to_visit=*/1);
  EXPECT_EQ(kScaleDigest,
            identifiability_paintop_digest.GetToken().ToUkmMetricValue());

  EXPECT_FALSE(identifiability_paintop_digest.encountered_skipped_ops());
  EXPECT_FALSE(
      identifiability_paintop_digest.encountered_partially_digested_image());
}

TEST(IdentifiabilityPaintOpDigestTest, RecursesIntoDrawRecords_TwoLevels) {
  StudyParticipationRaii study_participation_raii;
  IdentifiabilityPaintOpDigest identifiability_paintop_digest(kSize);
  auto paint_record_inner = sk_make_sp<cc::PaintRecord>();
  auto paint_record_middle = sk_make_sp<cc::PaintRecord>();
  auto paint_record_outer = sk_make_sp<cc::PaintRecord>();
  paint_record_inner->push<cc::ScaleOp>(kScaleX, kScaleY);
  paint_record_middle->push<cc::DrawRecordOp>(paint_record_inner);
  paint_record_outer->push<cc::DrawRecordOp>(paint_record_middle);
  identifiability_paintop_digest.MaybeUpdateDigest(paint_record_outer,
                                                   /*num_ops_to_visit=*/1);
  EXPECT_EQ(kScaleDigest,
            identifiability_paintop_digest.GetToken().ToUkmMetricValue());

  EXPECT_FALSE(identifiability_paintop_digest.encountered_skipped_ops());
  EXPECT_FALSE(
      identifiability_paintop_digest.encountered_partially_digested_image());
}

TEST(IdentifiabilityPaintOpDigestTest, StopsUpdatingDigestAfterThreshold) {
  StudyParticipationRaii study_participation_raii;
  constexpr int kMaxOperations = 5;
  IdentifiabilityPaintOpDigest identifiability_paintop_digest(kSize,
                                                              kMaxOperations);
  int64_t last_digest = INT64_C(0);
  for (int i = 0; i < kMaxOperations; i++) {
    auto paint_record = sk_make_sp<cc::PaintRecord>();
    paint_record->push<cc::ScaleOp>(kScaleX, kScaleY);
    identifiability_paintop_digest.MaybeUpdateDigest(paint_record,
                                                     /*num_ops_to_visit=*/1);
    EXPECT_NE(last_digest,
              identifiability_paintop_digest.GetToken().ToUkmMetricValue())
        << i;
    last_digest = identifiability_paintop_digest.GetToken().ToUkmMetricValue();
  }

  auto paint_record = sk_make_sp<cc::PaintRecord>();
  paint_record->push<cc::ScaleOp>(kScaleX, kScaleY);
  identifiability_paintop_digest.MaybeUpdateDigest(paint_record,
                                                   /*num_ops_to_visit=*/1);
  EXPECT_EQ(last_digest,
            identifiability_paintop_digest.GetToken().ToUkmMetricValue());

  EXPECT_TRUE(identifiability_paintop_digest.encountered_skipped_ops());
  EXPECT_FALSE(
      identifiability_paintop_digest.encountered_partially_digested_image());
}

TEST(IdentifiabilityPaintOpDigestTest, MassiveOpSkipped) {
  StudyParticipationRaii study_participation_raii;
  IdentifiabilityPaintOpDigest identifiability_paintop_digest(kSize);
  SkPath path;
  // Build a massive PaintOp.
  constexpr size_t kMaxIterations = 1 << 22;
  for (size_t i = 0; i < kMaxIterations; i++)
    path.rLineTo(1.0f, 1.0f);
  auto paint_record = sk_make_sp<cc::PaintRecord>();
  paint_record->push<cc::DrawPathOp>(path, cc::PaintFlags());
  identifiability_paintop_digest.MaybeUpdateDigest(paint_record,
                                                   /*num_ops_to_visit=*/1);
  EXPECT_EQ(kTokenBuilderInitialDigest,
            identifiability_paintop_digest.GetToken().ToUkmMetricValue());

  EXPECT_TRUE(identifiability_paintop_digest.encountered_skipped_ops());
  EXPECT_FALSE(
      identifiability_paintop_digest.encountered_partially_digested_image());
}

TEST(IdentifiabilityPaintOpDigestTest, DigestImageOp) {
  StudyParticipationRaii study_participation_raii;
  IdentifiabilityPaintOpDigest identifiability_paintop_digest(kSize);
  auto paint_record = sk_make_sp<cc::PaintRecord>();
  paint_record->push<cc::DrawImageOp>(
      cc::CreateDiscardablePaintImage(gfx::Size(10, 10)), 10.0f, 10.0f,
      nullptr);
  identifiability_paintop_digest.MaybeUpdateDigest(paint_record,
                                                   /*num_ops_to_visit=*/1);
  EXPECT_EQ(INT64_C(72317288461381383),
            identifiability_paintop_digest.GetToken().ToUkmMetricValue());

  EXPECT_FALSE(identifiability_paintop_digest.encountered_skipped_ops());
  EXPECT_TRUE(
      identifiability_paintop_digest.encountered_partially_digested_image());
}

}  // namespace

}  // namespace blink
