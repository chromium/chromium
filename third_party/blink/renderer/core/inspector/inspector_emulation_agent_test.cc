// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_emulation_agent.h"

#include "base/feature_list.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

String BuildExpectedAcceptHeader(bool include_webp, bool include_avif) {
  StringBuilder sb;
#if BUILDFLAG(ENABLE_JXL_DECODER)
  if (base::FeatureList::IsEnabled(blink::features::kJXLImageFormat)) {
    sb.Append("image/jxl,");
  }
#endif
#if BUILDFLAG(ENABLE_AV1_DECODER)
  if (include_avif) {
    sb.Append("image/avif,");
  }
#endif
  if (include_webp) {
    sb.Append("image/webp,");
  }
  sb.Append("image/apng,image/svg+xml,image/*,*/*;q=0.8");
  return sb.ToString();
}

}  // namespace

class InspectorEmulationAgentTest : public testing::Test {};

TEST_F(InspectorEmulationAgentTest, ModifiesAcceptHeader) {
  HashSet<String> disabled_types;

  EXPECT_EQ(InspectorEmulationAgent::OverrideAcceptImageHeader(&disabled_types),
            BuildExpectedAcceptHeader(/*include_webp=*/true,
                                      /*include_avif=*/true));

  disabled_types.insert("image/webp");
  EXPECT_EQ(InspectorEmulationAgent::OverrideAcceptImageHeader(&disabled_types),
            BuildExpectedAcceptHeader(/*include_webp=*/false,
                                      /*include_avif=*/true));

  disabled_types.insert("image/avif");
  EXPECT_EQ(InspectorEmulationAgent::OverrideAcceptImageHeader(&disabled_types),
            BuildExpectedAcceptHeader(/*include_webp=*/false,
                                      /*include_avif=*/false));

  disabled_types.erase("image/webp");
  EXPECT_EQ(InspectorEmulationAgent::OverrideAcceptImageHeader(&disabled_types),
            BuildExpectedAcceptHeader(/*include_webp=*/true,
                                      /*include_avif=*/false));
}

}  // namespace blink
