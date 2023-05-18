// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/browsing_context_group_info_mojom_traits.h"

#include "base/unguessable_token.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/browsing_context_group_info.h"
#include "third_party/blink/public/mojom/page/browsing_context_group_info.mojom.h"

namespace blink {

TEST(BrowsingContextGroupInfoTest, ValidMojoSerialization) {
  auto bcg_info = BrowsingContextGroupInfo::CreateUnique();
  auto bcg_info_clone = BrowsingContextGroupInfo::CreateUnique();
  EXPECT_NE(bcg_info.browsing_context_group_token,
            bcg_info_clone.browsing_context_group_token);
  EXPECT_NE(bcg_info.coop_related_group_token,
            bcg_info_clone.coop_related_group_token);

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<
          blink::mojom::BrowsingContextGroupInfo>(bcg_info, bcg_info_clone));

  EXPECT_EQ(bcg_info.browsing_context_group_token,
            bcg_info_clone.browsing_context_group_token);
  EXPECT_EQ(bcg_info.coop_related_group_token,
            bcg_info_clone.coop_related_group_token);
}

}  // namespace blink
