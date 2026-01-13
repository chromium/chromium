// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/network_utils.h"

#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/features.h"

namespace blink::network_utils {
namespace {

#if BUILDFLAG(ENABLE_JXL_DECODER)
class JxlImageAcceptHeaderTest : public testing::TestWithParam<bool> {};

TEST_P(JxlImageAcceptHeaderTest, JxlInAcceptHeaderMatchesFeatureFlag) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatureState(blink::features::kJXLImageFormat, GetParam());

  std::string_view header = ImageAcceptHeader();
  bool contains_jxl = header.find("image/jxl") != std::string_view::npos;
  EXPECT_EQ(contains_jxl, GetParam());
}

INSTANTIATE_TEST_SUITE_P(NetworkUtilsTest,
                         JxlImageAcceptHeaderTest,
                         testing::Bool());
#else
TEST(NetworkUtilsTest, JxlNotInAcceptHeaderWhenDecoderDisabled) {
  std::string_view header = ImageAcceptHeader();
  EXPECT_EQ(header.find("image/jxl"), std::string_view::npos);
}
#endif

}  // namespace
}  // namespace blink::network_utils
