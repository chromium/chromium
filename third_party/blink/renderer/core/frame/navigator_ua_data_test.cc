// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/navigator_ua_data.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigator_ua_brand_version.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {
namespace {

TEST(NavigatorUADataTest, DetachedContextBrandsPopulatedLazilyOnce) {
  auto* ua_data = MakeGarbageCollected<NavigatorUAData>(nullptr);
  UserAgentBrandList brand_list = {{"Chromium", "120"}};
  ua_data->SetBrandVersionList(brand_list);

  // A detached context returns the single {"", ""} placeholder instead of
  // `brand_list`.
  const auto& first = ua_data->brands();
  ASSERT_EQ(first.size(), 1u);
  EXPECT_EQ(first[0]->brand(), "");
  EXPECT_EQ(first[0]->version(), "");

  // Repeated access reuses the populated vector instead of appending again.
  const auto& second = ua_data->brands();
  ASSERT_EQ(second.size(), 1u);
  EXPECT_EQ(first[0], second[0]);
}

}  // namespace
}  // namespace blink
