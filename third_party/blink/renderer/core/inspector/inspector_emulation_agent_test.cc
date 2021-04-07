// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_emulation_agent.h"

#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

class InspectorEmulationAgentTest : public testing::Test {};

TEST_F(InspectorEmulationAgentTest, ModifiesAcceptHeader) {
#if BUILDFLAG(ENABLE_AV1_DECODER)
  String expected_default =
      "image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
  String expected_no_webp =
      "image/avif,image/apng,image/svg+xml,image/*,*/*;q=0.8";
  String expected_no_webp_and_avif =
      "image/apng,image/svg+xml,image/*,*/*;q=0.8";
  String expected_no_avif =
      "image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
  String expected_no_jxl =
      "image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
#else
  String expected_default =
      "image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
  String expected_no_webp = "image/apng,image/svg+xml,image/*,*/*;q=0.8";
  String expected_no_webp_and_avif =
      "image/apng,image/svg+xml,image/*,*/*;q=0.8";
  String expected_no_avif =
      "image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
  String expected_no_jxl =
      "image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
#endif

#if BUILDFLAG(ENABLE_JXL_DECODER)
  bool jxl_enabled = base::FeatureList::IsEnabled(features::kJXL);
  if (jxl_enabled) {
#if BUILDFLAG(ENABLE_AV1_DECODER)
    expected_default =
        "image/jxl,image/avif,image/webp,image/apng,image/svg+xml,image/*,*/"
        "*;q=0.8";
    expected_no_webp =
        "image/jxl,image/avif,image/apng,image/svg+xml,image/*,*/*;q=0.8";
    expected_no_webp_and_avif =
        "image/jxl,image/apng,image/svg+xml,image/*,*/*;q=0.8";
    expected_no_avif =
        "image/jxl,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
    expected_no_jxl =
        "image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
#else   // BUILDFLAG(ENABLE_AV1_DECODER)
    expected_default =
        "image/jxl,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
    expected_no_webp = "image/jxl,image/apng,image/svg+xml,image/*,*/*;q=0.8";
    expected_no_webp_and_avif =
        "image/jxl,image/apng,image/svg+xml,image/*,*/*;q=0.8";
    expected_no_avif =
        "image/jxl,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
    expected_no_jxl = "image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
#endif  // BUILDFLAG(ENABLE_AV1_DECODER)
  }
#endif  // BUILDFLAG(ENABLE_JXL_DECODER)

  HashSet<String> disabled_types;
  EXPECT_EQ(InspectorEmulationAgent::OverrideAcceptImageHeader(&disabled_types),
            expected_default);
  disabled_types.insert("image/webp");
  EXPECT_EQ(InspectorEmulationAgent::OverrideAcceptImageHeader(&disabled_types),
            expected_no_webp);
  disabled_types.insert("image/avif");
  EXPECT_EQ(InspectorEmulationAgent::OverrideAcceptImageHeader(&disabled_types),
            expected_no_webp_and_avif);
  disabled_types.erase("image/webp");
  EXPECT_EQ(InspectorEmulationAgent::OverrideAcceptImageHeader(&disabled_types),
            expected_no_avif);
  disabled_types.erase("image/avif");
  disabled_types.insert("image/jxl");
  EXPECT_EQ(InspectorEmulationAgent::OverrideAcceptImageHeader(&disabled_types),
            expected_no_jxl);
}

}  // namespace blink
