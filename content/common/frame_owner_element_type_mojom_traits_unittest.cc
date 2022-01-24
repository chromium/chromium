// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/frame_owner_element_type_mojom_traits.h"

#include <string>

#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/common/frame.mojom-shared.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"

// Parameterized on FencedFrames implementation.
class FrameOwnerElementTypeTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::string> {
 public:
  FrameOwnerElementTypeTest() {
    if (GetParam() == "disabled") {
      scoped_feature_list_.InitAndDisableFeature(
          blink::features::kFencedFrames);
    } else {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          blink::features::kFencedFrames,
          {{"implementation_type", GetParam()}});
    }
  }

  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    return info.param;
  }

  bool CanSerializeFencedFrameType() const {
    return blink::features::IsFencedFramesEnabled() &&
           blink::features::kFencedFramesImplementationTypeParam.Get() ==
               blink::features::FencedFramesImplementationType::kShadowDOM;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

using FrameOwnerElementTypeDeathTest = FrameOwnerElementTypeTest;

INSTANTIATE_TEST_SUITE_P(All,
                         FrameOwnerElementTypeTest,
                         ::testing::Values("disabled", "shadow_dom", "mparch"),
                         &FrameOwnerElementTypeTest::DescribeParams);
INSTANTIATE_TEST_SUITE_P(All,
                         FrameOwnerElementTypeDeathTest,
                         ::testing::Values("disabled", "shadow_dom", "mparch"),
                         &FrameOwnerElementTypeDeathTest::DescribeParams);

TEST_P(FrameOwnerElementTypeTest, SerializeAndDeserialize) {
  // The following FrameOwnerElementTypes can be represented as a
  // mojom::ChildFrameOwnerElementType.
  constexpr blink::FrameOwnerElementType kConvertableValues[] = {
      blink::FrameOwnerElementType::kIframe,
      blink::FrameOwnerElementType::kObject,
      blink::FrameOwnerElementType::kEmbed,
      blink::FrameOwnerElementType::kFrame,
  };

  for (const auto type : kConvertableValues) {
    SCOPED_TRACE(static_cast<int>(type));
    blink::FrameOwnerElementType output;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                content::mojom::ChildFrameOwnerElementType>(type, output));
    EXPECT_EQ(type, output);
  }

  if (CanSerializeFencedFrameType()) {
    const blink::FrameOwnerElementType type =
        blink::FrameOwnerElementType::kFencedframe;
    blink::FrameOwnerElementType output;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                content::mojom::ChildFrameOwnerElementType>(type, output));
    EXPECT_EQ(type, output);
  }
}

TEST_P(FrameOwnerElementTypeTest, RejectInvalid) {
  {
    // Create an intentionally garbage value.
    const auto mojo_type =
        static_cast<content::mojom::ChildFrameOwnerElementType>(1234);
    blink::FrameOwnerElementType output;
    bool valid =
        mojo::EnumTraits<content::mojom::ChildFrameOwnerElementType,
                         blink::FrameOwnerElementType>::FromMojom(mojo_type,
                                                                  &output);
    EXPECT_FALSE(valid);
  }

  if (!CanSerializeFencedFrameType()) {
    const auto mojo_type =
        content::mojom::ChildFrameOwnerElementType::kFencedframe;
    blink::FrameOwnerElementType output;
    bool valid =
        mojo::EnumTraits<content::mojom::ChildFrameOwnerElementType,
                         blink::FrameOwnerElementType>::FromMojom(mojo_type,
                                                                  &output);
    EXPECT_FALSE(valid);
  }
}

TEST_P(FrameOwnerElementTypeDeathTest, SerializeInvalid) {
  // The following FrameOwnerElementTypes cannot be represented as a
  // mojom::ChildFrameOwnerElementType.
  constexpr blink::FrameOwnerElementType kUnconvertableValues[] = {
      blink::FrameOwnerElementType::kNone,
      blink::FrameOwnerElementType::kPortal,
  };

  for (const auto type : kUnconvertableValues) {
    SCOPED_TRACE(static_cast<int>(type));
    blink::FrameOwnerElementType output;
    EXPECT_DCHECK_DEATH(
        mojo::test::SerializeAndDeserialize<
            content::mojom::ChildFrameOwnerElementType>(type, output));
  }

  if (!CanSerializeFencedFrameType()) {
    const blink::FrameOwnerElementType type =
        blink::FrameOwnerElementType::kFencedframe;
    blink::FrameOwnerElementType output;
    EXPECT_DCHECK_DEATH(
        mojo::test::SerializeAndDeserialize<
            content::mojom::ChildFrameOwnerElementType>(type, output));
  }
}
