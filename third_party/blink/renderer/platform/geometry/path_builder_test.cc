// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/path_builder.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/contoured_rect.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/geometry/path_types.h"

namespace blink {

TEST(PathBuilderTest, Empty) {
  PathBuilder builder;

  EXPECT_TRUE(builder.IsEmpty());
  EXPECT_TRUE(builder.CurrentPath().IsEmpty());

  builder.LineTo({1, 2});
  EXPECT_FALSE(builder.IsEmpty());
  EXPECT_FALSE(builder.CurrentPath().IsEmpty());

  builder.Finalize();
  EXPECT_TRUE(builder.IsEmpty());
  EXPECT_TRUE(builder.CurrentPath().IsEmpty());

  builder.LineTo({1, 2});
  EXPECT_FALSE(builder.IsEmpty());
  EXPECT_FALSE(builder.CurrentPath().IsEmpty());

  builder.Reset();
  EXPECT_TRUE(builder.IsEmpty());
  EXPECT_TRUE(builder.CurrentPath().IsEmpty());
}

TEST(PathBuilderTest, CurrentPath) {
  PathBuilder builder;

  const Path p0 = builder.CurrentPath();
  EXPECT_TRUE(p0.IsEmpty());

  builder.MoveTo({1, 2});
  const Path p1 = builder.CurrentPath();
  EXPECT_FALSE(p1.IsEmpty());
  EXPECT_NE(p0, p1);

  builder.LineTo({2, 3});
  const Path p2 = builder.CurrentPath();
  EXPECT_FALSE(p2.IsEmpty());
  EXPECT_NE(p1, p2);

  builder.Close();
  const Path p3 = builder.CurrentPath();
  EXPECT_FALSE(p3.IsEmpty());
  EXPECT_NE(p2, p3);

  const Path p4 = builder.Finalize();
  EXPECT_FALSE(p4.IsEmpty());
  EXPECT_EQ(p3, p4);

  const Path p5 = builder.CurrentPath();
  EXPECT_TRUE(p5.IsEmpty());
  EXPECT_NE(p4, p5);
}

TEST(PathBuilderTest, AsymmetricConcaveOutsetIsSingleContour) {
  FloatRoundedRect::Radii origin_radii;
  origin_radii.SetTopLeft(gfx::SizeF(10, 50));
  const FloatRoundedRect origin(gfx::RectF(0, 0, 80, 80), origin_radii);

  FloatRoundedRect::Radii target_radii;
  target_radii.SetTopLeft(gfx::SizeF(25, 65));
  ContouredRect target(
      FloatRoundedRect(gfx::RectF(-15, -15, 110, 110), target_radii),
      ContouredRect::CornerCurvature(0.70710677f, 2, 2, 2));
  target.SetOriginRect(origin);

  PathBuilder builder;
  builder.AddContouredRect(target);
  const SkPath path = builder.Finalize().GetSkPath();

  int contour_count = 0;
  SkPath::Iter iterator(path, false);
  SkPoint points[4];
  for (SkPath::Verb verb;
       (verb = iterator.next(points)) != SkPath::kDone_Verb;) {
    if (verb == SkPath::kMove_Verb) {
      ++contour_count;
    }
  }
  EXPECT_EQ(contour_count, 1);
  EXPECT_EQ(path.getBounds(), SkRect::MakeLTRB(-15, -15, 95, 95));
}

TEST(PathBuilderTest, WindRule) {
  PathBuilder builder;

  // Default.
  EXPECT_EQ(
      SkFillTypeToWindRule(builder.CurrentPath().GetSkPath().getFillType()),
      WindRule::RULE_NONZERO);

  builder.SetWindRule(WindRule::RULE_EVENODD);
  EXPECT_EQ(
      SkFillTypeToWindRule(builder.CurrentPath().GetSkPath().getFillType()),
      WindRule::RULE_EVENODD);
}

}  // namespace blink
