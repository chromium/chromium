// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/filter_operation.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {
namespace {

TEST(BlurFilterOperationTest, IsotropicStdDeviation) {
  const Length fixedLength{1.5, Length::kFixed};
  BlurFilterOperation* filter =
      MakeGarbageCollected<BlurFilterOperation>(fixedLength);

  // We expect that the single-argument constructor makes an isotropic blur,
  // such that the X and Y axis values both contain the passed-in length.
  EXPECT_EQ(filter->StdDeviation(), fixedLength);
  EXPECT_EQ(filter->StdDeviationXY().X(), fixedLength);
  EXPECT_EQ(filter->StdDeviationXY().Y(), fixedLength);
}

TEST(BlurFilterOperationTest, AnisotropicStdDeviation) {
  const Length kFixedLength0{0.0, Length::kFixed};
  const Length kFixedLength3{3.0, Length::kFixed};
  BlurFilterOperation* filter =
      MakeGarbageCollected<BlurFilterOperation>(kFixedLength0, kFixedLength3);

  // We expect that the two-argument constructor makes a blur with the X and Y
  // standard-deviation axis values holding the passed-in lengths.
  // StdDeviation() would DCHECK if it were called, since X and Y do not match.
  EXPECT_EQ(filter->StdDeviationXY(),
            LengthPoint(kFixedLength0, kFixedLength3));
}

}  // namespace
}  // namespace blink
