// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/frame_owner_element_type_mojom_traits.h"

#include <string>

#include "base/test/gtest_util.h"
#include "content/common/frame.mojom-shared.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"

using FrameOwnerElementTypeTest = testing::Test;
using FrameOwnerElementTypeDeathTest = FrameOwnerElementTypeTest;

TEST_F(FrameOwnerElementTypeTest, SerializeAndDeserialize) {
  // The following FrameOwnerElementTypes can be represented as a
  // mojom::ChildFrameOwnerElementType.
  constexpr blink::FrameOwnerElementType kConvertibleValues[] = {
      blink::FrameOwnerElementType::kIframe,
      blink::FrameOwnerElementType::kObject,
      blink::FrameOwnerElementType::kEmbed,
      blink::FrameOwnerElementType::kFrame,
  };

  for (const auto type : kConvertibleValues) {
    SCOPED_TRACE(static_cast<int>(type));
    blink::FrameOwnerElementType output;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                content::mojom::ChildFrameOwnerElementType>(type, output));
    EXPECT_EQ(type, output);
  }
}

TEST_F(FrameOwnerElementTypeTest, RejectInvalid) {
  // Create an intentionally garbage value.
  const auto mojo_type =
      static_cast<content::mojom::ChildFrameOwnerElementType>(1234);
  blink::FrameOwnerElementType output;
  // TODO(crbug.com/40246400): Ideally, we would not use EnumTraits
  // directly.
  bool valid =
      mojo::EnumTraits<content::mojom::ChildFrameOwnerElementType,
                       blink::FrameOwnerElementType>::FromMojom(mojo_type,
                                                                &output);
  EXPECT_FALSE(valid);
}

TEST_F(FrameOwnerElementTypeDeathTest, SerializeInvalid) {
  // The following FrameOwnerElementTypes cannot be represented as a
  // mojom::ChildFrameOwnerElementType.
  constexpr blink::FrameOwnerElementType kUnconvertibleValues[] = {
      blink::FrameOwnerElementType::kNone,
      blink::FrameOwnerElementType::kFencedframe,
  };

  for (const auto type : kUnconvertibleValues) {
    SCOPED_TRACE(static_cast<int>(type));
    blink::FrameOwnerElementType output;
    EXPECT_DCHECK_DEATH(
        mojo::test::SerializeAndDeserialize<
            content::mojom::ChildFrameOwnerElementType>(type, output));
  }
}
