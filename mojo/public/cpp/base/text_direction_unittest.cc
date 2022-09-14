// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/text_direction_mojom_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {
namespace text_direction_unittest {

TEST(TextDirectionTest, TextDirection) {
  static constexpr base::i18n::TextDirection kTestDirections[] = {
      base::i18n::LEFT_TO_RIGHT, base::i18n::RIGHT_TO_LEFT,
      base::i18n::UNKNOWN_DIRECTION};

  for (auto direction_in : kTestDirections) {
    base::i18n::TextDirection direction_out;

    mojo_base::mojom::TextDirection serialized_direction =
        mojo::EnumTraits<mojo_base::mojom::TextDirection,
                         base::i18n::TextDirection>::ToMojom(direction_in);
    ASSERT_TRUE((mojo::EnumTraits<
                 mojo_base::mojom::TextDirection,
                 base::i18n::TextDirection>::FromMojom(serialized_direction,
                                                       &direction_out)));
    EXPECT_EQ(direction_in, direction_out);
  }
}

}  // namespace text_direction_unittest
}  // namespace mojo_base