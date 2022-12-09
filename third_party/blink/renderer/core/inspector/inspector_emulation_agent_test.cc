// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_emulation_agent.h"

#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/buildflags.h"

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
#else
  String expected_default =
      "image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
  String expected_no_webp = "image/apng,image/svg+xml,image/*,*/*;q=0.8";
  String expected_no_webp_and_avif =
      "image/apng,image/svg+xml,image/*,*/*;q=0.8";
  String expected_no_avif =
      "image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
#endif

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
}

}  // namespace blink
