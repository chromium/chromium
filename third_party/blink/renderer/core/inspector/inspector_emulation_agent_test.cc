// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_emulation_agent.h"

#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class InspectorEmulationAgentTest : public testing::Test {};

TEST_F(InspectorEmulationAgentTest, ModifiesAcceptHeader) {
#if BUILDFLAG(ENABLE_AV1_DECODER)
  const char kExpectedDefault[] =
      "image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
  const char kExpectedNoWebp[] =
      "image/avif,image/apng,image/svg+xml,image/*,*/*;q=0.8";
#else
  const char kExpectedDefault[] =
      "image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";
  const char kExpectedNoWebp[] = "image/apng,image/svg+xml,image/*,*/*;q=0.8";
#endif
  const char kExpectedNoWebpAndAvif[] =
      "image/apng,image/svg+xml,image/*,*/*;q=0.8";
  const char kExpectedNoAvif[] =
      "image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8";

  HashSet<String> disabled_types;
  EXPECT_EQ(InspectorEmulationAgent::OverrideAcceptImageHeader(&disabled_types),
            kExpectedDefault);
  disabled_types.insert("image/webp");
  EXPECT_EQ(InspectorEmulationAgent::OverrideAcceptImageHeader(&disabled_types),
            kExpectedNoWebp);
  disabled_types.insert("image/avif");
  EXPECT_EQ(InspectorEmulationAgent::OverrideAcceptImageHeader(&disabled_types),
            kExpectedNoWebpAndAvif);
  disabled_types.erase("image/webp");
  EXPECT_EQ(InspectorEmulationAgent::OverrideAcceptImageHeader(&disabled_types),
            kExpectedNoAvif);
}

}  // namespace blink
