// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_request_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"

namespace blink {

// Check all of the resource types that are NOT supposed to be loaded
// incrementally
TEST(ShouldLoadIncrementalTest, PriorityNotIncremental) {
  constexpr ResourceType kResNotIncremental[] = {
      ResourceType::kCSSStyleSheet, ResourceType::kScript, ResourceType::kFont,
      ResourceType::kXSLStyleSheet, ResourceType::kManifest};
  for (auto res_type : kResNotIncremental) {
    const bool incremental = ShouldLoadIncremental(res_type);
    EXPECT_EQ(incremental, false);
  }
}

// Check all of the resource types that ARE supposed to be loaded
// incrementally
TEST(ShouldLoadIncrementalTest, PriorityIncremental) {
  constexpr ResourceType kResIncremental[] = {
      ResourceType::kImage,       ResourceType::kRaw,
      ResourceType::kSVGDocument, ResourceType::kLinkPrefetch,
      ResourceType::kTextTrack,   ResourceType::kAudio,
      ResourceType::kVideo,       ResourceType::kSpeculationRules};
  for (auto res_type : kResIncremental) {
    const bool incremental = ShouldLoadIncremental(res_type);
    EXPECT_EQ(incremental, true);
  }
}

}  // namespace blink
