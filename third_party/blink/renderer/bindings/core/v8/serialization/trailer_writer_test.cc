// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/serialization/trailer_writer.h"

#include "base/containers/span.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialization_tag.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

namespace blink {
namespace {

TEST(TrailerWriterTest, Empty) {
  test::TaskEnvironment task_environment;
  TrailerWriter writer;
  EXPECT_THAT(writer.MakeTrailerData(), ElementsAre());
}

TEST(TrailerWriterTest, ExposedInterfaces) {
  test::TaskEnvironment task_environment;
  TrailerWriter writer;
  writer.RequireExposedInterface(kImageBitmapTag);
  writer.RequireExposedInterface(kCryptoKeyTag);
  writer.RequireExposedInterface(kImageBitmapTag);

  // Duplicates should be removed, but we're otherwise indifferent to the order.
  auto trailer = writer.MakeTrailerData();
  ASSERT_EQ(trailer.size(), 7u);
  EXPECT_THAT(base::make_span(trailer).first(5u),
              ElementsAre(0xA0, 0x00, 0x00, 0x00, 0x02));
  EXPECT_THAT(base::make_span(trailer).subspan(5, 2),
              UnorderedElementsAre(kImageBitmapTag, kCryptoKeyTag));
}

}  // namespace
}  // namespace blink
